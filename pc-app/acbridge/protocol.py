"""Format pakietu telemetrii AC -> M5Stack Tab5.

Ten plik jest JEDYNYM zrodlem prawdy o ukladzie bajtow.
Odpowiednik po stronie Tab5: firmware/src/protocol.h
Kazda zmiana musi byc wprowadzona w OBU plikach (wersja +1).

Wszystko little-endian, bez paddingu (w C: #pragma pack(1)).
Pola sa i tak ulozone tak, zeby kazdy float/u32 lezal na granicy 4 bajtow.

Transport: USB CDC (port szeregowy). Strumien bajtow nie ma granic pakietow,
wiec kazda ramka to:

    [4B magic "ACT5"] [264B reszta pakietu] [2B CRC16-CCITT]  = 270 B

Magic sluzy za slowo synchronizujace przy odzyskiwaniu sie po smieciach,
CRC potwierdza, ze trafilismy w prawdziwy poczatek ramki, a nie w przypadkowe
bajty danych, ktore wygladaja jak magic.
"""

from __future__ import annotations

import struct
import time
from dataclasses import dataclass, field

MAGIC = b"ACT5"
VERSION = 2

# Tab5 wypisuje to w petli, dopoki nie dostanie telemetrii - po tym
# rozpoznajemy wlasciwy port szeregowy przy autodetekcji.
HELLO_LINE = b"ACT5HELLO"

# Nominalna predkosc portu. USB CDC i tak ja ignoruje (leci pelna predkoscia
# USB), ale pyserial wymaga podania liczby.
BAUD = 921600

# --- flagi (bitmaska w polu `flags`) -----------------------------------------
F_LIVE = 1 << 0  # gra dziala i jedziemy (nie menu / nie pauza)
F_IN_PIT = 1 << 1
F_PIT_LIMITER = 1 << 2
F_ABS_ACTIVE = 1 << 3
F_TC_ACTIVE = 1 << 4
F_DRS = 1 << 5
F_VALID_LAP = 1 << 6
F_ENGINE_LIMITER = 1 << 7  # obroty na ograniczniku

_FMT = (
    "<"
    "4s B B H I"          # magic, version, flags, seq, ts_ms
    "3f"                  # speed_kmh, rpm, max_rpm
    "b 3B"                # gear (-1=R, 0=N, 1..n), fuel_pct, tc_level, abs_level
    "5f"                  # throttle, brake, clutch, steer, fuel_l
    "2f"                  # drift_angle_deg, yaw_rate_dps
    "3f"                  # g_lat, g_lon, g_vert
    "4f"                  # slip_angle[4]  (stopnie, przyblizenie)
    "4f"                  # wheel_slip[4]  (0..1+, poslizg kola z AC)
    "4f"                  # tyre_temp[4]   (C, rdzen)
    "4f"                  # tyre_press[4]  (psi)
    "4f"                  # brake_temp[4]  (C)
    "4f"                  # tyre_dirt[4]   (0..5)
    "4f"                  # wheel_load[4]  (N)
    "f"                   # surface_grip
    "2f"                  # air_temp, road_temp
    "f"                   # turbo_boost
    "3I"                  # lap_ms, last_lap_ms, best_lap_ms
    "H 2B"                # lap_count, position, tyres_out
    "2f"                  # norm_pos (0..1), distance_m
    "24s 24s"             # car_name, track_name (ascii, dopelnione zerami)
)

PACKET_SIZE = struct.calcsize(_FMT)
assert PACKET_SIZE == 268, f"nieoczekiwany rozmiar pakietu: {PACKET_SIZE}"

CRC_SIZE = 2
FRAME_SIZE = PACKET_SIZE + CRC_SIZE  # 270

_PACKER = struct.Struct(_FMT)


def crc16(data: bytes) -> int:
    """CRC16-CCITT (wielomian 0x1021, init 0xFFFF).

    Ten sam algorytm co crc16() w firmware/src/protocol.h.
    """
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def _ascii24(s: str) -> bytes:
    return s.encode("ascii", "replace")[:23].ljust(24, b"\0")


def _q4(v) -> list:
    """Normalizuje cokolwiek do listy 4 floatow."""
    if v is None:
        return [0.0] * 4
    out = [float(x) for x in v][:4]
    return out + [0.0] * (4 - len(out))


@dataclass
class Frame:
    """Jedna ramka telemetrii. Wypelniana przez czytnik shared memory
    albo przez symulator, potem pakowana do UDP."""

    flags: int = 0
    seq: int = 0
    ts_ms: int = 0

    speed_kmh: float = 0.0
    rpm: float = 0.0
    max_rpm: float = 8000.0

    gear: int = 0
    fuel_pct: int = 0
    tc_level: int = 0
    abs_level: int = 0

    throttle: float = 0.0
    brake: float = 0.0
    clutch: float = 0.0
    steer: float = 0.0
    fuel_l: float = 0.0

    drift_angle: float = 0.0   # stopnie, + = tyl ucieka w prawo
    yaw_rate: float = 0.0      # stopnie/s
    g_lat: float = 0.0
    g_lon: float = 0.0
    g_vert: float = 0.0

    slip_angle: list = field(default_factory=lambda: [0.0] * 4)
    wheel_slip: list = field(default_factory=lambda: [0.0] * 4)
    tyre_temp: list = field(default_factory=lambda: [0.0] * 4)
    tyre_press: list = field(default_factory=lambda: [0.0] * 4)
    brake_temp: list = field(default_factory=lambda: [0.0] * 4)
    tyre_dirt: list = field(default_factory=lambda: [0.0] * 4)
    wheel_load: list = field(default_factory=lambda: [0.0] * 4)

    surface_grip: float = 1.0
    air_temp: float = 0.0
    road_temp: float = 0.0
    turbo_boost: float = 0.0

    lap_ms: int = 0
    last_lap_ms: int = 0
    best_lap_ms: int = 0
    lap_count: int = 0
    position: int = 0
    tyres_out: int = 0
    norm_pos: float = 0.0
    distance_m: float = 0.0

    car_name: str = ""
    track_name: str = ""

    def frame(self, seq: int) -> bytes:
        """Gotowa ramka do wyslania po USB: pakiet + CRC16."""
        body = self.pack(seq)
        return body + struct.pack("<H", crc16(body))

    def pack(self, seq: int) -> bytes:
        return _PACKER.pack(
            MAGIC,
            VERSION,
            self.flags & 0xFF,
            seq & 0xFFFF,
            (self.ts_ms or int(time.monotonic() * 1000)) & 0xFFFFFFFF,
            self.speed_kmh,
            self.rpm,
            self.max_rpm,
            max(-1, min(9, int(self.gear))),
            max(0, min(255, int(self.fuel_pct))),
            self.tc_level & 0xFF,
            self.abs_level & 0xFF,
            self.throttle,
            self.brake,
            self.clutch,
            self.steer,
            self.fuel_l,
            self.drift_angle,
            self.yaw_rate,
            self.g_lat,
            self.g_lon,
            self.g_vert,
            *_q4(self.slip_angle),
            *_q4(self.wheel_slip),
            *_q4(self.tyre_temp),
            *_q4(self.tyre_press),
            *_q4(self.brake_temp),
            *_q4(self.tyre_dirt),
            *_q4(self.wheel_load),
            self.surface_grip,
            self.air_temp,
            self.road_temp,
            self.turbo_boost,
            int(self.lap_ms) & 0xFFFFFFFF,
            int(self.last_lap_ms) & 0xFFFFFFFF,
            int(self.best_lap_ms) & 0xFFFFFFFF,
            int(self.lap_count) & 0xFFFF,
            int(self.position) & 0xFF,
            int(self.tyres_out) & 0xFF,
            self.norm_pos,
            self.distance_m,
            _ascii24(self.car_name),
            _ascii24(self.track_name),
        )


if __name__ == "__main__":
    print(f"PACKET_SIZE = {PACKET_SIZE}")
    print(f"FRAME_SIZE  = {FRAME_SIZE}")
    print(f"len(frame)  = {len(Frame().frame(0))}")
    print(f"crc16(b'123456789') = 0x{crc16(b'123456789'):04X}  (oczekiwane 0x29B1)")
