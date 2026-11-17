"""Sweep-mode correlation. Mirrors firmware/lib/ss_core/correlate.cpp.

Both implementations are checked against tests/vectors/correlate.csv, so the
sled (Linux side) and the base (fallback) always agree on a verdict.
"""
import math
from dataclasses import dataclass

QUIET, EXPLAINED, UNEXPLAINED = 0, 1, 2
STATE_NAMES = {QUIET: "quiet", EXPLAINED: "explained", UNEXPLAINED: "unexplained"}


@dataclass
class Config:
    quiet_margin_db: float = 3.0
    explain_margin_db: float = 6.0
    activity_factor: float = 1.0


@dataclass
class Result:
    state: int
    excess_db: float
    expected_dbm: float
    confidence: int


def dbm_to_mw(dbm: float) -> float:
    return 10.0 ** (dbm / 10.0)


def mw_to_dbm(mw: float) -> float:
    return -200.0 if mw <= 0 else 10.0 * math.log10(mw)


def sum_dbm(rssi_list) -> float:
    return mw_to_dbm(sum(dbm_to_mw(r) for r in rssi_list))


def _round_half_away(x: float) -> int:
    return int(math.floor(x + 0.5)) if x >= 0 else -int(math.floor(-x + 0.5))


def classify(measured_dbm: float, floor_dbm: float, known_sum_dbm: float, cfg: Config = Config()) -> Result:
    known_mw = 0.0 if known_sum_dbm <= -199.0 else dbm_to_mw(known_sum_dbm)
    expected = mw_to_dbm(dbm_to_mw(floor_dbm) + cfg.activity_factor * known_mw)
    excess = measured_dbm - expected
    if measured_dbm - floor_dbm < cfg.quiet_margin_db:
        return Result(QUIET, excess, expected, 0)
    if excess <= cfg.explain_margin_db:
        k = (cfg.explain_margin_db - excess) / cfg.explain_margin_db
        conf = 100 if k >= 1 else (0 if k <= 0 else _round_half_away(k * 100))
        return Result(EXPLAINED, excess, expected, conf)
    k = (excess - cfg.explain_margin_db) / 14.0
    return Result(UNEXPLAINED, excess, expected, 100 if k >= 1 else _round_half_away(k * 100))


class FloorTracker:
    """Rolling noise floor: follows drops at once, rises slowly."""

    def __init__(self, rise_db_per_s: float = 0.05):
        self.rise = rise_db_per_s
        self.value = None

    def update(self, dbm: float, dt_s: float) -> float:
        if self.value is None or dbm < self.value:
            self.value = dbm
        else:
            self.value = min(dbm, self.value + self.rise * dt_s)
        return self.value
