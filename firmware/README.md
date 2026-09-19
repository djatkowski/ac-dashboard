# Tab5 firmware

Arduino + M5Unified/M5GFX, built with PlatformIO.

## Flashing

```bash
pio run -t upload
```

Then start the bridge on the PC (`../pc-app`). The same USB-C cable carries
both the firmware upload and the telemetry.

### Two things that will cost you an afternoon otherwise

**Flash the merged image at `0x0`, not `0x2000`.** `firmware.factory.bin`
already contains the padding from address zero, so writing it at the
bootloader offset shifts everything by 8 KB and the ROM answers with
`invalid header: 0xffffffff` forever.

**On a Mac, go through a USB hub.** Connected straight to a MacBook's USB-C
port the data comes back corrupted - `Invalid head of packet`, truncated
streams, stub uploads that never finish. The same cable and the same board
flash first time through a hub. Once behind a hub, `--before usb_reset`
works too, so no button press is needed:

```bash
esptool.py --chip esp32p4 --port /dev/cu.usbmodemXXXX \
  --before usb_reset --after hard_reset \
  write_flash --flash_size 16MB --flash_mode qio --flash_freq 80m \
  0x0 .pio/build/tab5/firmware.factory.bin
```

If a connection attempt still fails, put the board into download mode by hand.
The Tab5 has no separate BOOT button - hold RESET for about 2 seconds until
the internal green LED blinks rapidly, release, then retry.

## Why this configuration

**Platform.** The official `platform-espressif32` does not support the
ESP32-P4 inside the Tab5. We use the
[pioarduino](https://github.com/pioarduino/platform-espressif32) fork - it is
the only way to build this at all.

**Board definition.** pioarduino has no entry for the Tab5, so the project
carries its own: `boards/m5stack_tab5.json`. It is derived from
`esp32-p4_r3-evboard` with the PSRAM size corrected to 32 MB.

**Libraries.** M5Unified 0.2.22 is the first release that knows about the
Tab5 (`board_M5Tab5`). Older ones will not detect the display.

**Silicon revision.** The board definition pins `chip_variant` to
`esp32p4_es` at 360 MHz. Tab5 units report as `esp32p4-eco2`, i.e. pre
rev.300, and the framework ships different prebuilt libraries per variant -
building as the rev.301 part produces an image that panics with an illegal
instruction on its very first instruction.

The versions in `platformio.ini` are pinned deliberately. If you bump them,
check that the board is still detected.

## How it works

`main.cpp` collects telemetry on **every** loop pass but only draws
`TARGET_FPS` times per second. If reception were tied to drawing, a long
frame would overflow the USB buffer.

Every screen tile is its own PSRAM sprite, repainted only when its data
changed. Repainting the full 1280x720 each frame would waste framebuffer
bandwidth on pixels that did not move.

`frame_parser.h` deliberately has no Arduino dependency, so the very code
that assembles frames on the board can be tested on a PC:

```bash
python3 tools/make_stream.py /tmp/stream.bin
c++ -std=c++17 -I src -o /tmp/test_parser tools/test_parser.cpp
/tmp/test_parser /tmp/stream.bin
```

The test injects garbage and a false `ACT5` into the stream to verify the
parser recovers without losing frames.

## Settings

Everything worth touching lives in `src/config.h`: brightness, target fps,
startup screen, shift light thresholds, gauge range.

## Signs and conventions

- `drift_angle > 0` - the rear stepped out to the **right** (nose points left).
- `steer > 0` - steering wheel turned **right**.
- The countersteer is correct when the signs match, because catching a slide
  means steering into it. That is what drives the `COUNTER OK` / `INTO SLIDE!`
  indicator.

If the steering reads backwards on your setup (AC varies with controller
configuration), flip the sign of `f.steer` in
`pc-app/acbridge/shared_memory.py`.

## Touch

- tap a tab in the bottom bar - switch screen,
- swipe sideways (180 px minimum) - same thing, anywhere on the screen.

The bottom bar also shows link state: `NO USB` / `WAITING FOR GAME` /
`CONNECTED`, plus frame rate, fps and a dropped-packet counter.
