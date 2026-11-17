"""Pogo-link protocol, Python side. Mirrors firmware/lib/ss_core/proto.h (docs/PROTOCOL.md)."""
import struct

SOF = b"\xA5\x5A"
VERSION = 1
MAX_PAYLOAD = 64
T_HELLO, T_PING, T_PONG = 0x01, 0x02, 0x03
T_RF_SAMPLE, T_VERDICT, T_SCAN_SUMMARY, T_SLEEP, T_WAKE = 0x10, 0x11, 0x20, 0x30, 0x31

RF_SAMPLE = struct.Struct("<Ihhhh")   # t_ms, avg, min, max, floor (centi-dBm)
VERDICT = struct.Struct("<BBhhBB")    # state, confidence, excess cdB, expected cdBm, n_wifi, n_ble
SCAN_SUMMARY = struct.Struct("<BBhhh")
HELLO = struct.Struct("<BBH")


def crc16(data: bytes) -> int:
    c = 0xFFFF
    for b in data:
        c ^= b << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x1021) & 0xFFFF if c & 0x8000 else (c << 1) & 0xFFFF
    return c


def encode(ftype: int, seq: int, payload: bytes = b"") -> bytes:
    if len(payload) > MAX_PAYLOAD:
        raise ValueError("payload too long")
    body = bytes([VERSION, ftype, seq & 0xFF, len(payload)]) + payload
    return SOF + body + struct.pack("<H", crc16(body))


class Parser:
    """Streaming decoder. feed(bytes) yields (type, seq, payload) tuples."""

    def __init__(self):
        self.buf = bytearray()
        self.crc_errors = 0

    def feed(self, data: bytes):
        self.buf += data
        while True:
            i = self.buf.find(SOF)
            if i < 0:
                del self.buf[: max(0, len(self.buf) - 1)]
                return
            del self.buf[:i]
            if len(self.buf) < 6:
                return
            n = self.buf[5]
            if n > MAX_PAYLOAD:
                del self.buf[:2]
                continue
            if len(self.buf) < 8 + n:
                return
            body = bytes(self.buf[2 : 6 + n])
            crc = self.buf[6 + n] | (self.buf[7 + n] << 8)
            if crc != crc16(body) or body[0] != VERSION:
                self.crc_errors += 1
                del self.buf[:2]
                continue
            del self.buf[: 8 + n]
            yield body[1], body[2], body[4:]
