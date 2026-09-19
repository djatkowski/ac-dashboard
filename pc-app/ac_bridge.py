#!/usr/bin/env python3
"""Most telemetryczny Assetto Corsa -> M5Stack Tab5 (po USB).

Uzycie:
    python ac_bridge.py                  # autodetekcja portu, dane z gry
    python ac_bridge.py --demo           # sztuczne dane, dziala tez bez AC
    python ac_bridge.py --port COM5      # z pominieciem autodetekcji
    python ac_bridge.py --list-ports     # co widzi system
    python ac_bridge.py --print          # podglad danych w konsoli
"""

from __future__ import annotations

import argparse
import sys
import time

from acbridge.protocol import BAUD, FRAME_SIZE, Frame
from acbridge.serial_link import SerialLink, describe, list_candidates
from acbridge.simulator import Simulator

IDLE_RATE_HZ = 10.0  # gdy AC nie dziala, podtrzymujemy lacze rzadszymi ramkami


def fmt_time(ms: int) -> str:
    if ms <= 0:
        return "--:--.---"
    m, rest = divmod(ms, 60000)
    s, milli = divmod(rest, 1000)
    return f"{m}:{s:02d}.{milli:03d}"


def gear_label(g: int) -> str:
    return "R" if g < 0 else ("N" if g == 0 else str(g))


def main() -> int:
    ap = argparse.ArgumentParser(description="Assetto Corsa -> M5Stack Tab5 (USB)")
    ap.add_argument("--port", default=None, help="port szeregowy Tab5, np. COM5 albo /dev/cu.usbmodem1101")
    ap.add_argument("--rate", type=float, default=60.0, help="czestotliwosc wysylki w Hz (domyslnie 60)")
    ap.add_argument("--demo", action="store_true", help="sztuczna telemetria zamiast gry")
    ap.add_argument("--print", dest="show", action="store_true", help="wypisuj telemetrie w konsoli")
    ap.add_argument("--list-ports", action="store_true", help="wypisz porty szeregowe i zakoncz")
    args = ap.parse_args()

    if args.list_ports:
        ports = list_candidates()
        if not ports:
            print("Brak portow szeregowych.")
        for p in ports:
            print("   ", describe(p))
        return 0

    link = SerialLink(device=args.port, verbose=True)
    period = 1.0 / max(1.0, args.rate)
    idle_period = 1.0 / IDLE_RATE_HZ

    print(f"Transport : USB CDC @ {BAUD} (ramka {FRAME_SIZE} B)")
    print(f"Wysylka   : {args.rate:.0f} Hz gdy gra dziala, {IDLE_RATE_HZ:.0f} Hz na jalowym biegu")
    print(f"Port      : {args.port if args.port else 'autodetekcja'}")

    source = None
    sim = Simulator() if args.demo else None
    if args.demo:
        print("Tryb DEMO - dane sa zmyslone.")
    else:
        from acbridge.shared_memory import ACSharedMemory
        source = ACSharedMemory()

    next_tick = time.monotonic()
    next_retry = 0.0
    next_status = 0.0
    was_live = False

    try:
        while True:
            now = time.monotonic()

            if sim is not None:
                frame: Frame | None = sim.read()
            else:
                if not source.connected and now >= next_retry:
                    next_retry = now + 2.0
                    if source.open():
                        print("[+] Podlaczono do Assetto Corsa")
                frame = source.read() if source.connected else None

            live = frame is not None
            if was_live and not live:
                print("[-] Assetto Corsa zniknelo")
            was_live = live

            # Nawet bez gry slemy pusta ramke, zeby Tab5 wiedzial, ze kabel
            # zyje, i pokazal "czekam na gre" zamiast "brak polaczenia".
            link.send(frame if live else Frame())

            if args.show and live and now >= next_status:
                next_status = now + 0.25
                sys.stdout.write(
                    f"\r{frame.speed_kmh:6.1f} km/h  {frame.rpm:5.0f} rpm  "
                    f"bieg {gear_label(frame.gear):>2}  drift {frame.drift_angle:+6.1f}st  "
                    f"yaw {frame.yaw_rate:+6.1f}st/s  okr {fmt_time(frame.lap_ms)}  "
                    f"ramek {link.frames_sent}   "
                )
                sys.stdout.flush()
            elif not args.show and now >= next_status:
                next_status = now + 5.0
                usb = link.device if link.connected else "brak"
                print(f"[i] gra: {'tak' if live else 'nie'}, Tab5: {usb}, ramek: {link.frames_sent}")

            next_tick += period if live else idle_period
            sleep = next_tick - time.monotonic()
            if sleep > 0:
                time.sleep(sleep)
            else:
                next_tick = time.monotonic()  # nadganianie po zacieciu
    except KeyboardInterrupt:
        print("\nKoniec.")
    finally:
        link.close()
        if source is not None:
            source.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
