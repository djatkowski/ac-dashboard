"""USB transport: sends telemetry frames to the Tab5 over a serial port.

The Tab5 announces itself with an "ACT5HELLO" line until telemetry starts
arriving. That lets us find the right port without asking the user for a
name: we open candidates one by one and listen for who answers.
"""

from __future__ import annotations

import time

try:
    import serial
    from serial.tools import list_ports
except ImportError as exc:  # pragma: no cover
    raise SystemExit(
        "pyserial is missing. Install it with:  pip install -r requirements.txt"
    ) from exc

from .protocol import BAUD, HELLO_LINE, Frame

# The ESP32-P4 enumerates as Espressif's native USB Serial/JTAG device.
ESPRESSIF_VID = 0x303A

PROBE_TIMEOUT = 1.5  # seconds spent listening for "hello" on one port


def list_candidates() -> list:
    """Serial ports, most likely one first."""
    ports = list(list_ports.comports())
    # Drop the obvious non-candidates: on macOS every port has a /dev/tty.*
    # twin whose open() can block waiting for carrier detect. Prefer /dev/cu.*.
    ports = [p for p in ports if not p.device.startswith("/dev/tty.")]
    ports.sort(key=lambda p: (p.vid != ESPRESSIF_VID, p.device))
    return ports


def describe(port) -> str:
    vid = f"{port.vid:04X}" if port.vid is not None else "----"
    pid = f"{port.pid:04X}" if port.pid is not None else "----"
    return f"{port.device}  [{vid}:{pid}]  {port.description}"


def _open(device: str, timeout: float, write_timeout: float | None = None) -> serial.Serial:
    """Open a port WITHOUT resetting the board.

    pyserial asserts DTR and RTS on open by default. The ESP32 USB Serial/JTAG
    peripheral reads those control lines as a reset request - it is the very
    mechanism esptool uses to drop the chip into its bootloader. Leaving the
    default in place makes the Tab5 reboot on every open, and with autodetect
    retrying every 2 s that means a reboot loop.

    Setting the flags before open() records them, so they are applied as the
    port comes up rather than toggled afterwards.
    """
    ser = serial.Serial()
    ser.port = device
    ser.baudrate = BAUD
    ser.timeout = timeout
    if write_timeout is not None:
        ser.write_timeout = write_timeout
    ser.dtr = False
    ser.rts = False
    ser.open()
    return ser


def probe(device: str) -> bool:
    """Is our dashboard sitting on this port?"""
    try:
        with _open(device, timeout=0.2) as ser:
            ser.reset_input_buffer()
            deadline = time.monotonic() + PROBE_TIMEOUT
            buf = bytearray()
            while time.monotonic() < deadline:
                chunk = ser.read(256)
                if chunk:
                    buf += chunk
                    if HELLO_LINE in buf:
                        return True
                    if len(buf) > 4096:
                        del buf[:2048]
    except (OSError, serial.SerialException):
        return False
    return False


def autodetect(verbose: bool = True) -> str | None:
    for port in list_candidates():
        if verbose:
            print(f"    probing {describe(port)}")
        if probe(port.device):
            return port.device
    return None


class SerialLink:
    """Keeps the connection to the Tab5 alive and sends frames. Recovers on
    its own after the cable is unplugged and plugged back in."""

    def __init__(self, device: str | None = None, verbose: bool = True) -> None:
        self.device = device        # None = autodetect
        self.fixed = device is not None
        self.verbose = verbose
        self.ser: serial.Serial | None = None
        self.seq = 0
        self.frames_sent = 0
        self.dropped_frames = 0
        self.timeouts = 0
        self._next_retry = 0.0
        # The search repeats every 2 s. Report the full scan only the first
        # time, so we do not flood the console with the same lines forever.
        self._complained = False

    @property
    def connected(self) -> bool:
        return self.ser is not None and self.ser.is_open

    def ensure_open(self) -> bool:
        if self.connected:
            return True
        now = time.monotonic()
        if now < self._next_retry:
            return False
        self._next_retry = now + 2.0

        device = self.device
        first = not self._complained
        if device is None:
            if self.verbose and first:
                print("[i] Looking for the Tab5 on the serial ports...")
            device = autodetect(self.verbose and first)
            if device is None:
                if self.verbose and first:
                    print("[-] Tab5 not found. Plug in the USB-C cable or pass --port.")
                    print("    Still searching in the background...")
                self._complained = True
                return False

        try:
            # write_timeout keeps the loop from hanging if the Tab5 stops
            # reading (a crash, or a reset mid-write).
            self.ser = _open(device, timeout=0.05, write_timeout=0.5)
            self.ser.reset_input_buffer()
            if not self.fixed:
                self.device = device
            self._complained = False  # the next disconnect should report again
            if self.verbose:
                print(f"[+] Tab5 connected: {device}")
            return True
        except (OSError, serial.SerialException) as exc:
            if self.verbose and first:
                print(f"[-] Cannot open {device}: {exc}")
                print("    Retrying in the background...")
            self._complained = True
            self.ser = None
            if not self.fixed:
                self.device = None
            return False

    def send(self, frame: Frame) -> bool:
        if not self.ensure_open():
            return False
        payload = frame.frame(self.seq)
        self.seq = (self.seq + 1) & 0xFFFF
        try:
            # Drain whatever the Tab5 printed (its "hello" line). If nobody
            # reads it, the device's TX buffer fills and its writes start
            # blocking, which stalls its main loop and stops it reading us.
            waiting = self.ser.in_waiting
            if waiting:
                self.ser.read(waiting)
            self.ser.write(payload)
            self.frames_sent += 1
            self.timeouts = 0
            return True
        except serial.SerialTimeoutException:
            # A write timeout means the device was busy for a moment, NOT that
            # it went away. Closing the port here would be actively harmful:
            # re-opening it resets the ESP32, so reconnecting on every hiccup
            # put the board in a reboot loop. Drop the frame and move on -
            # at 60 Hz the next one is 16 ms away anyway.
            self.timeouts += 1
            self.dropped_frames += 1
            if self.verbose and self.timeouts == 20:
                print("[!] Tab5 is not keeping up - dropping frames. Try a lower --rate.")
            return False
        except (OSError, serial.SerialException) as exc:
            if self.verbose:
                print(f"[-] Lost the Tab5 ({exc}). Waiting for it to come back.")
            self.close()
            if not self.fixed:
                self.device = None
            return False

    def close(self) -> None:
        if self.ser is not None:
            try:
                self.ser.close()
            except OSError:
                pass
            self.ser = None
