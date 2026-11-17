"""Passive Wi-Fi / Bluetooth scans on the UNO Q Linux side.

Each function returns a list of (name, rssi_dbm) and never raises: if a tool is
missing the list is empty and the base's own scan is used instead.
"""
import asyncio
import re
import subprocess


def _run(cmd, timeout=15):
    try:
        return subprocess.run(cmd, capture_output=True, text=True, timeout=timeout).stdout
    except (OSError, subprocess.SubprocessError):
        return ""


def parse_nmcli(text: str):
    """nmcli -t -f SSID,SIGNAL: 'name:percent' per line; ':' inside names is escaped as '\\:'."""
    out = []
    for line in text.splitlines():
        m = re.match(r"^(.*):(\d+)$", line)
        if not m:
            continue
        pct = max(0, min(100, int(m.group(2))))
        out.append((m.group(1).replace("\\:", ":"), pct / 2.0 - 100.0))  # NetworkManager: percent = 2 * (dBm + 100)
    return out


def parse_iw(text: str):
    out, name, sig = [], "", None
    for line in text.splitlines():
        line = line.strip()
        if line.startswith("BSS "):
            if sig is not None:
                out.append((name, sig))
            name, sig = "", None
        elif line.startswith("signal:"):
            m = re.search(r"(-?\d+(\.\d+)?)", line)
            sig = float(m.group(1)) if m else None
        elif line.startswith("SSID:"):
            name = line[5:].strip()
    if sig is not None:
        out.append((name, sig))
    return out


def scan_wifi():
    found = parse_nmcli(_run(["nmcli", "-t", "-f", "SSID,SIGNAL", "dev", "wifi", "list", "--rescan", "yes"]))
    if not found:
        found = parse_iw(_run(["iw", "dev", "wlan0", "scan"]))
    return found


def scan_ble(seconds: float = 3.0):
    try:
        from bleak import BleakScanner
    except ImportError:
        return []

    async def go():
        devs = await BleakScanner.discover(timeout=seconds, return_adv=True)
        return [(adv.local_name or "", float(adv.rssi)) for _, adv in devs.values()]

    try:
        return asyncio.run(go())
    except Exception:  # no adapter, no permission, bluetoothd not running
        return []
