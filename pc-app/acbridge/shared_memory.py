"""Czytnik pamieci wspoldzielonej Assetto Corsa (AC1).

AC wystawia trzy nazwane mapowania pamieci:
    Local\\acpmf_physics   - 333 Hz, dane fizyki
    Local\\acpmf_graphics  - ~60 Hz, stan sesji / czasy
    Local\\acpmf_static    - stale sesji (auto, tor, maxRpm, maxFuel)

Uklady struktur ponizej odpowiadaja Assetto Corsa (nie ACC - tam po polu
carCoordinates uklad SPageFileGraphic sie rozjezdza; patrz README).

wchar_t jest tu celowo zamapowany na c_uint16 zamiast ctypes.c_wchar -
dzieki temu rozmiary struktur sa takie same niezaleznie od platformy,
wiec modul da sie zaimportowac (i przetestowac) takze poza Windows.
"""

from __future__ import annotations

import ctypes
import math
import mmap
import struct
import sys

from .protocol import (
    F_ABS_ACTIVE,
    F_DRS,
    F_ENGINE_LIMITER,
    F_IN_PIT,
    F_LIVE,
    F_PIT_LIMITER,
    F_TC_ACTIVE,
    F_VALID_LAP,
    Frame,
)

WCHAR = ctypes.c_uint16


def _wstr(arr) -> str:
    """Dekoduje tablice UTF-16LE zakonczona zerem."""
    out = bytearray()
    for code in arr:
        if code == 0:
            break
        out += struct.pack("<H", code)
    return out.decode("utf-16-le", "replace")


class SPageFilePhysics(ctypes.Structure):
    _pack_ = 4
    _fields_ = [
        ("packetId", ctypes.c_int32),
        ("gas", ctypes.c_float),
        ("brake", ctypes.c_float),
        ("fuel", ctypes.c_float),
        ("gear", ctypes.c_int32),
        ("rpms", ctypes.c_int32),
        ("steerAngle", ctypes.c_float),
        ("speedKmh", ctypes.c_float),
        ("velocity", ctypes.c_float * 3),
        ("accG", ctypes.c_float * 3),
        ("wheelSlip", ctypes.c_float * 4),
        ("wheelLoad", ctypes.c_float * 4),
        ("wheelsPressure", ctypes.c_float * 4),
        ("wheelAngularSpeed", ctypes.c_float * 4),
        ("tyreWear", ctypes.c_float * 4),
        ("tyreDirtyLevel", ctypes.c_float * 4),
        ("tyreCoreTemperature", ctypes.c_float * 4),
        ("camberRAD", ctypes.c_float * 4),
        ("suspensionTravel", ctypes.c_float * 4),
        ("drs", ctypes.c_float),
        ("tc", ctypes.c_float),
        ("heading", ctypes.c_float),
        ("pitch", ctypes.c_float),
        ("roll", ctypes.c_float),
        ("cgHeight", ctypes.c_float),
        ("carDamage", ctypes.c_float * 5),
        ("numberOfTyresOut", ctypes.c_int32),
        ("pitLimiterOn", ctypes.c_int32),
        ("abs", ctypes.c_float),
        ("kersCharge", ctypes.c_float),
        ("kersInput", ctypes.c_float),
        ("autoShifterOn", ctypes.c_int32),
        ("rideHeight", ctypes.c_float * 2),
        ("turboBoost", ctypes.c_float),
        ("ballast", ctypes.c_float),
        ("airDensity", ctypes.c_float),
        ("airTemp", ctypes.c_float),
        ("roadTemp", ctypes.c_float),
        ("localAngularVel", ctypes.c_float * 3),
        ("finalFF", ctypes.c_float),
        ("performanceMeter", ctypes.c_float),
        ("engineBrake", ctypes.c_int32),
        ("ersRecoveryLevel", ctypes.c_int32),
        ("ersPowerLevel", ctypes.c_int32),
        ("ersHeatCharging", ctypes.c_int32),
        ("ersIsCharging", ctypes.c_int32),
        ("kersCurrentKJ", ctypes.c_float),
        ("drsAvailable", ctypes.c_int32),
        ("drsEnabled", ctypes.c_int32),
        ("brakeTemp", ctypes.c_float * 4),
        ("clutch", ctypes.c_float),
        ("tyreTempI", ctypes.c_float * 4),
        ("tyreTempM", ctypes.c_float * 4),
        ("tyreTempO", ctypes.c_float * 4),
        ("isAIControlled", ctypes.c_int32),
        ("tyreContactPoint", (ctypes.c_float * 3) * 4),
        ("tyreContactNormal", (ctypes.c_float * 3) * 4),
        ("tyreContactHeading", (ctypes.c_float * 3) * 4),
        ("brakeBias", ctypes.c_float),
        ("localVelocity", ctypes.c_float * 3),
    ]


class SPageFileGraphic(ctypes.Structure):
    _pack_ = 4
    _fields_ = [
        ("packetId", ctypes.c_int32),
        ("status", ctypes.c_int32),          # 0 OFF, 1 REPLAY, 2 LIVE, 3 PAUSE
        ("session", ctypes.c_int32),
        ("currentTime", WCHAR * 15),
        ("lastTime", WCHAR * 15),
        ("bestTime", WCHAR * 15),
        ("split", WCHAR * 15),
        ("completedLaps", ctypes.c_int32),
        ("position", ctypes.c_int32),
        ("iCurrentTime", ctypes.c_int32),
        ("iLastTime", ctypes.c_int32),
        ("iBestTime", ctypes.c_int32),
        ("sessionTimeLeft", ctypes.c_float),
        ("distanceTraveled", ctypes.c_float),
        ("isInPit", ctypes.c_int32),
        ("currentSectorIndex", ctypes.c_int32),
        ("lastSectorTime", ctypes.c_int32),
        ("numberOfLaps", ctypes.c_int32),
        ("tyreCompound", WCHAR * 33),
        ("replayTimeMultiplier", ctypes.c_float),
        ("normalizedCarPosition", ctypes.c_float),
        ("carCoordinates", ctypes.c_float * 3),
        ("penaltyTime", ctypes.c_float),
        ("flag", ctypes.c_int32),
        ("idealLineOn", ctypes.c_int32),
        ("isInPitLane", ctypes.c_int32),
        ("surfaceGrip", ctypes.c_float),
        ("mandatoryPitDone", ctypes.c_int32),
        ("windSpeed", ctypes.c_float),
        ("windDirection", ctypes.c_float),
    ]


class SPageFileStatic(ctypes.Structure):
    _pack_ = 4
    _fields_ = [
        ("smVersion", WCHAR * 15),
        ("acVersion", WCHAR * 15),
        ("numberOfSessions", ctypes.c_int32),
        ("numCars", ctypes.c_int32),
        ("carModel", WCHAR * 33),
        ("track", WCHAR * 33),
        ("playerName", WCHAR * 33),
        ("playerSurname", WCHAR * 33),
        ("playerNick", WCHAR * 33),
        ("sectorCount", ctypes.c_int32),
        ("maxTorque", ctypes.c_float),
        ("maxPower", ctypes.c_float),
        ("maxRpm", ctypes.c_int32),
        ("maxFuel", ctypes.c_float),
        ("suspensionMaxTravel", ctypes.c_float * 4),
        ("tyreRadius", ctypes.c_float * 4),
        ("maxTurboBoost", ctypes.c_float),
        ("deprecated_1", ctypes.c_float),
        ("deprecated_2", ctypes.c_float),
        ("penaltiesEnabled", ctypes.c_int32),
        ("aidFuelRate", ctypes.c_float),
        ("aidTireRate", ctypes.c_float),
        ("aidMechanicalDamage", ctypes.c_float),
        ("aidAllowTyreBlankets", ctypes.c_int32),
        ("aidStability", ctypes.c_float),
        ("aidAutoClutch", ctypes.c_int32),
        ("aidAutoBlip", ctypes.c_int32),
    ]


AC_STATUS_OFF, AC_STATUS_REPLAY, AC_STATUS_LIVE, AC_STATUS_PAUSE = 0, 1, 2, 3


def _wrap_pi(a: float) -> float:
    while a > math.pi:
        a -= 2 * math.pi
    while a < -math.pi:
        a += 2 * math.pi
    return a


class ACSharedMemory:
    """Otwiera mapowania AC i produkuje gotowe ramki `Frame`."""

    def __init__(self) -> None:
        self._maps: dict[str, mmap.mmap] = {}
        self.connected = False
        self._last_packet_id = -1
        self._stale_reads = 0

    # -- cykl zycia ----------------------------------------------------------
    def open(self) -> bool:
        if sys.platform != "win32":
            raise RuntimeError(
                "Shared memory Assetto Corsa jest dostepne tylko na Windows. "
                "Do testow bez gry uzyj trybu --demo."
            )
        specs = (
            ("physics", "Local\\acpmf_physics", ctypes.sizeof(SPageFilePhysics)),
            ("graphics", "Local\\acpmf_graphics", ctypes.sizeof(SPageFileGraphic)),
            ("static", "Local\\acpmf_static", ctypes.sizeof(SPageFileStatic)),
        )
        try:
            for key, tag, size in specs:
                if key not in self._maps:
                    self._maps[key] = mmap.mmap(-1, size, tag, access=mmap.ACCESS_READ)
        except OSError:
            self.close()
            return False

        # mmap z tagname tworzy mapowanie, jesli go nie ma - wiec samo
        # otwarcie niczego nie dowodzi. Dopiero sensowne maxRpm znaczy,
        # ze po drugiej stronie faktycznie siedzi AC.
        st = self._read(SPageFileStatic, "static")
        if st.maxRpm <= 0:
            self.close()
            return False

        self.connected = True
        return True

    def close(self) -> None:
        for m in self._maps.values():
            try:
                m.close()
            except Exception:
                pass
        self._maps.clear()
        self.connected = False

    def _read(self, struct_cls, key):
        buf = self._maps[key]
        buf.seek(0)
        return struct_cls.from_buffer_copy(buf.read(ctypes.sizeof(struct_cls)))

    # -- odczyt --------------------------------------------------------------
    def read(self) -> Frame | None:
        if not self.connected:
            return None
        try:
            ph = self._read(SPageFilePhysics, "physics")
            gr = self._read(SPageFileGraphic, "graphics")
            st = self._read(SPageFileStatic, "static")
        except (OSError, ValueError):
            self.close()
            return None

        # gra zamknieta -> packetId przestaje rosnac
        if ph.packetId == self._last_packet_id:
            self._stale_reads += 1
            if self._stale_reads > 600:  # ~10 s przy 60 Hz
                self.close()
                return None
        else:
            self._stale_reads = 0
        self._last_packet_id = ph.packetId

        return self._build(ph, gr, st)

    def _build(self, ph, gr, st) -> Frame:
        f = Frame()

        live = gr.status == AC_STATUS_LIVE
        f.flags |= F_LIVE if live else 0
        f.flags |= F_IN_PIT if (gr.isInPit or gr.isInPitLane) else 0
        f.flags |= F_PIT_LIMITER if ph.pitLimiterOn else 0
        f.flags |= F_ABS_ACTIVE if ph.abs > 0.02 else 0
        f.flags |= F_TC_ACTIVE if ph.tc > 0.02 else 0
        f.flags |= F_DRS if ph.drsEnabled else 0
        f.flags |= F_VALID_LAP if gr.numberOfLaps >= 0 else 0

        max_rpm = float(st.maxRpm) if st.maxRpm > 0 else 8000.0
        f.speed_kmh = ph.speedKmh
        f.rpm = float(ph.rpms)
        f.max_rpm = max_rpm
        if ph.rpms >= max_rpm - 50:
            f.flags |= F_ENGINE_LIMITER

        f.gear = int(ph.gear) - 1  # AC: 0=R, 1=N, 2=1 bieg
        f.fuel_l = ph.fuel
        f.fuel_pct = int(round(100.0 * ph.fuel / st.maxFuel)) if st.maxFuel > 0 else 0
        f.tc_level = int(round(ph.tc * 10))
        f.abs_level = int(round(ph.abs * 10))

        f.throttle = ph.gas
        f.brake = ph.brake
        f.clutch = 1.0 - ph.clutch  # AC: 1.0 = sprzeglo puszczone
        f.steer = ph.steerAngle

        # --- dane driftowe ---------------------------------------------------
        f.drift_angle = self._drift_angle(ph)
        f.yaw_rate = math.degrees(ph.localAngularVel[1])
        f.g_lat = ph.accG[0]
        f.g_vert = ph.accG[1]
        f.g_lon = ph.accG[2]

        f.wheel_slip = list(ph.wheelSlip)
        f.slip_angle = self._slip_angles(ph)
        f.tyre_temp = list(ph.tyreCoreTemperature)
        f.tyre_press = list(ph.wheelsPressure)
        f.brake_temp = list(ph.brakeTemp)
        f.tyre_dirt = list(ph.tyreDirtyLevel)
        f.wheel_load = list(ph.wheelLoad)

        f.surface_grip = gr.surfaceGrip
        f.air_temp = ph.airTemp
        f.road_temp = ph.roadTemp
        f.turbo_boost = ph.turboBoost
        f.tyres_out = max(0, min(255, ph.numberOfTyresOut))

        f.lap_ms = max(0, gr.iCurrentTime)
        f.last_lap_ms = max(0, gr.iLastTime) if gr.iLastTime < 1 << 31 else 0
        f.best_lap_ms = max(0, gr.iBestTime) if gr.iBestTime < 1 << 31 else 0
        f.lap_count = max(0, gr.completedLaps)
        f.position = max(0, min(255, gr.position))
        f.norm_pos = gr.normalizedCarPosition
        f.distance_m = gr.distanceTraveled

        f.car_name = _wstr(st.carModel)
        f.track_name = _wstr(st.track)
        return f

    @staticmethod
    def _drift_angle(ph) -> float:
        """Kat poslizgu nadwozia w stopniach.

        Dodatni = wektor predkosci ucieka w prawo wzgledem osi auta,
        czyli tyl wychodzi w prawo (kontra w lewo).
        """
        vx, _vy, vz = ph.localVelocity
        if abs(vx) > 1e-4 or abs(vz) > 1e-4:
            if math.hypot(vx, vz) < 0.8:  # stoimy - kat jest smieciem
                return 0.0
            return math.degrees(math.atan2(vx, abs(vz)))

        # Fallback dla buildow, ktore nie wypelniaja localVelocity:
        # kierunek jazdy w swiecie minus heading auta.
        wx, _wy, wz = ph.velocity
        if math.hypot(wx, wz) < 0.8:
            return 0.0
        return math.degrees(_wrap_pi(math.atan2(wx, wz) - ph.heading))

    @staticmethod
    def _slip_angles(ph) -> list:
        """Przyblizony kat poslizgu kazdego kola.

        AC nie wystawia slip angle per kolo, wiec skalujemy wheelSlip
        (ktore jest miara laczna) tak, zeby dalo sie z tego zrobic
        czytelny wskaznik na ekranie. To wskaznik, nie pomiar.
        """
        return [min(90.0, s * 12.0) for s in ph.wheelSlip]
