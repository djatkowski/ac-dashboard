#!/usr/bin/env python3
"""Assetto Corsa -> M5Stack Tab5 telemetry bridge (over USB).

Usage:
    python ac_bridge.py                  # autodetect the port, read the game
    python ac_bridge.py --demo           # synthetic data, works without AC
    python ac_bridge.py --port COM5      # skip autodetection
    python ac_bridge.py --list-ports     # what the system can see
    python ac_bridge.py --print          # live data in the console
"""

from __future__ import annotations

import argparse
import sys
import time

from acbridge.protocol import BAUD, FRAME_SIZE, Frame
from acbridge.serial_link import SerialLink, describe, list_candidates
from acbridge.simulator import Simulator

IDLE_RATE_HZ = 10.0  # when AC is not running, keep the link warm more slowly


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
    ap.add_argument("--port", default=None,
                    help="Tab5 serial port, e.g. COM5 or /dev/cu.usbmodem1101")
    ap.add_argument("--rate", type=float, default=60.0, help="send rate in Hz (default 60)")
    ap.add_argument("--demo", action="store_true", help="synthetic telemetry instead of the game")
    ap.add_argument("--print", dest="show", action="store_true", help="print live telemetry")
    ap.add_argument("--list-ports", action="store_true", help="list serial ports and exit")
    args = ap.parse_args()

    if args.list_ports:
        ports = list_candidates()
        if not ports:
            print("No serial ports found.")
        for p in ports:
            print("   ", describe(p))
        return 0

    link = SerialLink(device=args.port, verbose=True)
    period = 1.0 / max(1.0, args.rate)
    idle_period = 1.0 / IDLE_RATE_HZ

    print(f"Transport : USB CDC @ {BAUD} ({FRAME_SIZE} B frames)")
    print(f"Send rate : {args.rate:.0f} Hz while the game runs, {IDLE_RATE_HZ:.0f} Hz when idle")
    print(f"Port      : {args.port if args.port else 'autodetect'}")

    source = None
    sim = Simulator() if args.demo else None
    if args.demo:
        print("DEMO mode - the data is made up.")
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
                        print("[+] Connected to Assetto Corsa")
                frame = source.read() if source.connected else None

            live = frame is not None
            if was_live and not live:
                print("[-] Assetto Corsa went away")
            was_live = live

            # Even without the game we send an empty frame, so the Tab5 knows
            # the cable is alive and shows "waiting for game" instead of
            # "no connection".
            link.send(frame if live else Frame())

            if args.show and live and now >= next_status:
                next_status = now + 0.25
                sys.stdout.write(
                    f"\r{frame.speed_kmh:6.1f} km/h  {frame.rpm:5.0f} rpm  "
                    f"gear {gear_label(frame.gear):>2}  drift {frame.drift_angle:+6.1f}deg  "
                    f"yaw {frame.yaw_rate:+6.1f}deg/s  lap {fmt_time(frame.lap_ms)}  "
                    f"frames {link.frames_sent}   "
                )
                sys.stdout.flush()
            elif not args.show and now >= next_status:
                next_status = now + 5.0
                usb = link.device if link.connected else "none"
                print(f"[i] game: {'yes' if live else 'no'}, Tab5: {usb}, frames: {link.frames_sent}")

            next_tick += period if live else idle_period
            sleep = next_tick - time.monotonic()
            if sleep > 0:
                time.sleep(sleep)
            else:
                next_tick = time.monotonic()  # catch up after a stall
    except KeyboardInterrupt:
        print("\nDone.")
    finally:
        link.close()
        if source is not None:
            source.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
