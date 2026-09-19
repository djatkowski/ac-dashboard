# Assetto Corsa dashboard on an M5Stack Tab5

Assetto Corsa telemetry on a Tab5 screen (1280x720), over a USB-C cable.
Two touch-switchable screens: a classic **RACE** layout and a **DRIFT** one
built around body slip angle.

```
  Assetto Corsa                pc-app/                      firmware/
  (shared memory)  ────────>   ac_bridge.py    ──USB-C──>   Tab5
   physics 333 Hz              reads shared                 draws at 45 fps
   graphics 60 Hz              memory, packs
   static                      into 270 B frames
```

## Why a bridge program

AC does not broadcast the full picture over the network - its built-in UDP
telemetry (port 9996) omits fuel and tyre temperatures, among other things.
The complete data lives only in shared memory, and that is reachable only
locally on the PC. Hence the bridge: it reads shared memory and pushes
everything to the Tab5.

## Quick start

**1. PC (Windows, wherever the game runs)**

```bash
cd pc-app
pip install -r requirements.txt
python ac_bridge.py
```

Prefer a single .exe with no Python install? Run `pc-app\build_exe.bat` on
Windows, or grab the artifact from GitHub Actions - see `pc-app/README.md`.

**2. Tab5**

```bash
cd firmware
pio run -t upload
```

Plug the Tab5 in over USB-C. The bridge finds the port on its own - there is
nothing to configure.

## Trying it without the game

To see the screen working before you launch AC (or if you are on a Mac):

```bash
python ac_bridge.py --demo
```

The simulator fakes a car periodically falling into a drift, so every gauge
gets exercised across its full range.

## The two screens

**RACE** - full-width rev bar with shift lights, speed, gear, revs,
ABS/TC/DRS/PIT indicators, fuel, three lap times, pedals and steering.

**DRIFT** - large slip angle gauge with a peak marker, angle history for the
last ~5 s, a countersteer indicator (are you catching the slide or feeding
it), yaw rate, a g-force ball, per-tyre slip and temperature, surface grip.

Switching: tap a tab at the bottom, or swipe sideways.

## Layout

```
pc-app/                the PC bridge (Python, pyserial only)
  acbridge/protocol.py   <- packet format: THE single source of truth
  acbridge/shared_memory.py
  acbridge/serial_link.py
  acbridge/simulator.py
  ac_bridge.py
  ac_bridge.spec         <- packaging into one .exe
  build_exe.bat          <- one-click .exe build (Windows)
firmware/              PlatformIO, Arduino + M5Unified
  src/protocol.h         <- mirror of protocol.py, keep them in sync!
  src/frame_parser.h     <- frame assembly, testable on a PC
  src/page_race.cpp
  src/page_drift.cpp
  tools/                 <- parser test that runs on a PC
```

Changing the data format? Edit `protocol.py` **and** `protocol.h`, bump
`VERSION`, then run the contract test:

```bash
cd firmware
python3 tools/make_stream.py /tmp/stream.bin
c++ -std=c++17 -I src -o /tmp/test_parser tools/test_parser.cpp
/tmp/test_parser /tmp/stream.bin
```
