"""Signal Scout sled - UNO Q Linux (QRB2210) side.

Scans Wi-Fi and Bluetooth with the UNO Q's own radio, compares the result with
the broadband detector level reported by the microcontroller, and sends a
verdict back (quiet / explained / unexplained). Receive-only: nothing here
transmits on Wi-Fi or Bluetooth beyond the normal scan requests of the OS.
"""
import threading
import time

from arduino.app_utils import App, Bridge

import correlate
import scanners

SCAN_PERIOD_S = 8.0
VERDICT_PERIOD_S = 1.0

state_lock = threading.Lock()
state = {"rf_dbm": None, "floor_dbm": None, "t_rf": 0.0,
         "own_rssi": [], "n_wifi": 0, "n_ble": 0,
         "base_sum_dbm": -200.0, "base_wifi": 0, "base_ble": 0, "t_base": 0.0}


def on_rf(avg_cdbm: int, floor_cdbm: int):
    with state_lock:
        state["rf_dbm"], state["floor_dbm"], state["t_rf"] = avg_cdbm / 100.0, floor_cdbm / 100.0, time.monotonic()


def on_base_scan(sum_cdbm: int, n_wifi: int, n_ble: int):
    with state_lock:
        state["base_sum_dbm"], state["base_wifi"], state["base_ble"], state["t_base"] = sum_cdbm / 100.0, n_wifi, n_ble, time.monotonic()


def scan_loop():
    while True:
        wifi = scanners.scan_wifi()
        ble = scanners.scan_ble()
        with state_lock:
            state["own_rssi"] = [r for _, r in wifi] + [r for _, r in ble]
            state["n_wifi"], state["n_ble"] = len(wifi), len(ble)
        time.sleep(SCAN_PERIOD_S)


def known_emitters(now: float):
    """Best estimate of the visible emitters: our own scan, or the base's if it sees more."""
    with state_lock:
        own = correlate.sum_dbm(state["own_rssi"]) if state["own_rssi"] else -200.0
        base_fresh = now - state["t_base"] < 15.0
        base = state["base_sum_dbm"] if base_fresh else -200.0
        if base > own:
            return base, state["base_wifi"], state["base_ble"]
        return own, state["n_wifi"], state["n_ble"]


def loop():
    time.sleep(VERDICT_PERIOD_S)
    now = time.monotonic()
    with state_lock:
        rf, floor, fresh = state["rf_dbm"], state["floor_dbm"], now - state["t_rf"] < 2.0
    if rf is None or not fresh:
        return
    known, n_wifi, n_ble = known_emitters(now)
    r = correlate.classify(rf, floor, known)
    Bridge.call("verdict", r.state, r.confidence, int(round(r.excess_db * 100)),
                int(round(r.expected_dbm * 100)), min(n_wifi, 255), min(n_ble, 255))


Bridge.provide("rf", on_rf)
Bridge.provide("base_scan", on_base_scan)
threading.Thread(target=scan_loop, daemon=True).start()
App.run(user_loop=loop)
