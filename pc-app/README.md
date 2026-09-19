# PC bridge: Assetto Corsa -> Tab5

Reads Assetto Corsa's shared memory and streams telemetry to the Tab5 over
USB. The only dependency is `pyserial`.

## Running it

```bash
pip install -r requirements.txt
python ac_bridge.py
```

Start order does not matter - the bridge waits for both the game and the
tablet, and recovers on its own when the cable is pulled.

## The .exe (no Python install needed)

```bat
build_exe.bat
```

Result: `dist\ac_bridge.exe` - a single ~9 MB file with no dependencies.
Copy it anywhere and run it.

**PyInstaller cannot cross-compile.** A Windows binary has to be produced on
Windows. If you do not have one to hand, CI will do it: push the repo to
GitHub and the `.github/workflows/build-exe.yml` workflow builds the `.exe`
on a `windows-latest` runner and publishes it as an artifact (Actions tab ->
pick the run -> Artifacts). You can also trigger it by hand with
"Run workflow".

Antivirus software likes to raise false positives on PyInstaller binaries.
That is why UPX is disabled in `ac_bridge.spec` - compression makes it worse.

## Options

| Flag | What it does |
|---|---|
| `--demo` | synthetic telemetry; works on macOS/Linux, with no game |
| `--port COM5` | skip port autodetection |
| `--list-ports` | show what the system can see |
| `--print` | live telemetry in the console (speed, revs, drift angle) |
| `--rate 60` | send rate, 60 Hz by default |

## How it finds the Tab5

The Tab5 prints `ACT5HELLO` in a loop until telemetry arrives. The bridge
opens serial ports one at a time (starting with those carrying Espressif's
VID, `0x303A`) and listens 1.5 s for that keyword. That is why `--port` is
usually unnecessary.

## Troubleshooting

**The Tab5 reboots over and over while the bridge runs.**
Opening a serial port asserts DTR and RTS by default, and the ESP32 USB
Serial/JTAG peripheral reads those lines as a reset request - the same
mechanism esptool uses to enter the bootloader. With autodetection retrying
every 2 s that becomes a reboot loop. The bridge clears both lines before
opening (see `_open()` in `serial_link.py`), so this should not happen here;
if you write your own tool against this port, do the same.

**The port is invisible on macOS and a VM is running.**
Parallels and VMware can claim a USB device for the guest the moment it
enumerates, which also happens after every board reset. macOS then shows the
device in `ioreg` but publishes no serial node. Check with:

```bash
ioreg -p IOUSB -l -w 0 | grep UsbExclusiveOwner
```

If a VM process owns it, detach the device from the guest (Devices -> USB &
Bluetooth) or run the bridge inside the VM instead - which is often what you
want anyway, since AC runs on Windows.

**`--list-ports` shows nothing on macOS.**
Approve the accessory when macOS asks. Without that the device enumerates but
never gets configured.

## Data format

`acbridge/protocol.py` is the single source of truth for the byte layout.
A frame is 270 B:

```
[4B "ACT5"] [264B packet] [2B CRC16-CCITT]
```

The magic doubles as a sync word (a byte stream has no packet boundaries)
and the CRC rejects random payload bytes that happen to look like the magic.

On the Tab5 side the counterpart is `firmware/src/protocol.h`. **A change in
one file requires a change in the other** - otherwise the `static_assert` in
the firmware stops matching or, worse, data gets read at an offset.

## Where the drift data comes from

- **Slip angle** - from `localVelocity` (velocity in the car's frame):
  `atan2(vx, |vz|)`. Positive means the velocity vector points right of the
  car's axis, i.e. the rear stepped out right. Below 0.8 m/s the angle is
  noise, so we return zero. There is also a fallback path via `heading` for
  builds that do not populate `localVelocity`.
- **Yaw rate** - `localAngularVel[1]`, converted to degrees/s.
- **Wheel slip** - `wheelSlip[4]` straight from the game.
- **Per-wheel slip angle** - AC does not expose it. We scale `wheelSlip` into
  something readable on screen. It is an indicator, not a measurement.

## Note: AC, not ACC

The structures in `shared_memory.py` describe **Assetto Corsa**. In Assetto
Corsa Competizione the `SPageFileGraphic` layout diverges after the
`carCoordinates` field (ACC has an array there covering many cars) and
`SPageFilePhysics` carries extra trailing fields. Supporting ACC would mean
adding a separate set of structures.
