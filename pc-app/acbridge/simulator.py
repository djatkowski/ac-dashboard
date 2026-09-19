"""Generator sztucznej telemetrii (tryb --demo).

Sluzy do dopiecia UI na Tab5 bez odpalania gry - takze z Maca/Linuksa.
Symuluje auto jezdzace w kolko i wpadajace cyklicznie w dluzszy drift,
zeby bylo widac cala skale wskaznikow.
"""

from __future__ import annotations

import math
import time

from .protocol import F_LIVE, F_TC_ACTIVE, Frame

_GEAR_RATIOS = [0.0, 0.0, 3.2, 2.1, 1.55, 1.2, 1.0]  # index = gear+1 (AC-like)


class Simulator:
    def __init__(self) -> None:
        self.t0 = time.monotonic()
        self.lap_start = self.t0
        self.laps = 0
        self.best = 0
        self.last = 0
        self.fuel = 42.0

    def read(self) -> Frame:
        t = time.monotonic() - self.t0
        f = Frame()
        f.flags = F_LIVE

        # 24-sekundowy cykl: prosta -> hamowanie -> drift -> wyjscie
        phase = (t % 24.0) / 24.0
        drift_env = max(0.0, math.sin(phase * math.pi * 2 - 0.6))  # 0..1

        base = 120 + 55 * math.sin(t * 0.21)
        f.speed_kmh = max(8.0, base - 45 * drift_env)

        f.gear = 2 if f.speed_kmh < 60 else (3 if f.speed_kmh < 110 else 4)
        ratio = _GEAR_RATIOS[f.gear + 1]
        f.max_rpm = 7800.0
        f.rpm = min(f.max_rpm, 1100 + f.speed_kmh * ratio * 22 + 900 * drift_env)

        f.throttle = min(1.0, 0.35 + 0.65 * drift_env + 0.25 * math.sin(t * 1.7))
        f.brake = max(0.0, 0.7 * math.sin(phase * math.pi * 2 - 0.2)) if drift_env < 0.15 else 0.0
        f.clutch = 0.0
        f.fuel_l = self.fuel = max(2.0, self.fuel - 0.0008)
        f.fuel_pct = int(f.fuel_l / 60.0 * 100)

        # --- drift ---
        angle = 52.0 * drift_env * math.sin(t * 0.37 + 1.0)
        f.drift_angle = angle
        f.yaw_rate = -angle * 1.15 + 14 * math.sin(t * 2.3) * drift_env
        f.steer = max(-1.0, min(1.0, -angle / 45.0 + 0.08 * math.sin(t * 3.1)))

        f.g_lat = angle / 32.0
        f.g_lon = -f.brake * 1.1 + f.throttle * 0.4
        f.g_vert = 1.0 + 0.08 * math.sin(t * 5.0)

        rear = 0.10 + 0.85 * drift_env
        front = 0.05 + 0.30 * drift_env
        f.wheel_slip = [front, front * 0.9, rear, rear * 0.96]
        f.slip_angle = [min(90.0, s * 12.0) for s in f.wheel_slip]
        f.tyre_temp = [
            72 + 14 * front, 71 + 14 * front,
            88 + 46 * rear, 86 + 46 * rear,
        ]
        f.tyre_press = [26.5, 26.6, 27.4, 27.3]
        f.brake_temp = [220 + 180 * f.brake, 218 + 180 * f.brake, 140, 138]
        f.tyre_dirt = [0.0, 0.0, 0.4 * drift_env, 0.4 * drift_env]
        f.wheel_load = [3100, 3050, 2600 + 900 * abs(f.g_lat), 2580]

        if rear > 0.8:
            f.flags |= F_TC_ACTIVE
        f.tc_level = 3
        f.abs_level = 2
        f.surface_grip = 0.98
        f.air_temp = 21.5
        f.road_temp = 31.0
        f.turbo_boost = 0.0

        now = time.monotonic()
        lap_time = now - self.lap_start
        if lap_time > 88.0:
            self.last = int(lap_time * 1000)
            self.best = self.last if self.best == 0 else min(self.best, self.last)
            self.laps += 1
            self.lap_start = now
            lap_time = 0.0
        f.lap_ms = int(lap_time * 1000)
        f.last_lap_ms = self.last
        f.best_lap_ms = self.best
        f.lap_count = self.laps
        f.position = 1
        f.norm_pos = (lap_time / 88.0) % 1.0
        f.distance_m = t * 30.0

        f.car_name = "ks_toyota_ae86_drift"
        f.track_name = "ek_ebisu_minami"
        return f
