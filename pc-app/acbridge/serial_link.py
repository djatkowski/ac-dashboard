"""Transport USB: wysylka ramek telemetrii do Tab5 przez port szeregowy.

Tab5 zglasza sie linia "ACT5HELLO", dopoki nie dostanie telemetrii. Dzieki
temu mozemy znalezc wlasciwy port bez pytania uzytkownika o nazwe: otwieramy
kandydatow po kolei i sluchamy, ktory sie odezwie.
"""

from __future__ import annotations

import time

try:
    import serial
    from serial.tools import list_ports
except ImportError as exc:  # pragma: no cover
    raise SystemExit(
        "Brakuje biblioteki pyserial. Zainstaluj:  pip install -r requirements.txt"
    ) from exc

from .protocol import BAUD, HELLO_LINE, Frame

# ESP32-P4 zglasza sie natywnym USB Serial/JTAG Espressifu.
ESPRESSIF_VID = 0x303A

PROBE_TIMEOUT = 1.5  # s nasluchu na "hello" na jednym porcie


def list_candidates() -> list:
    """Porty szeregowe posortowane od najbardziej prawdopodobnego."""
    ports = list(list_ports.comports())
    # Odsiewamy oczywiste nie-to: na Macu kazdy port ma blizniaka /dev/tty.*,
    # z ktorego zapis potrafi sie zablokowac - wolimy /dev/cu.*.
    ports = [p for p in ports if not p.device.startswith("/dev/tty.")]
    ports.sort(key=lambda p: (p.vid != ESPRESSIF_VID, p.device))
    return ports


def describe(port) -> str:
    vid = f"{port.vid:04X}" if port.vid is not None else "----"
    pid = f"{port.pid:04X}" if port.pid is not None else "----"
    return f"{port.device}  [{vid}:{pid}]  {port.description}"


def probe(device: str) -> bool:
    """Czy na tym porcie siedzi nasz dashboard?"""
    try:
        with serial.Serial(device, BAUD, timeout=0.2) as ser:
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
            print(f"    sprawdzam {describe(port)}")
        if probe(port.device):
            return port.device
    return None


class SerialLink:
    """Utrzymuje polaczenie z Tab5 i wysyla ramki. Sam sie odtwarza po
    wyciagnieciu i wpieciu kabla."""

    def __init__(self, device: str | None = None, verbose: bool = True) -> None:
        self.device = device        # None = autodetekcja
        self.fixed = device is not None
        self.verbose = verbose
        self.ser: serial.Serial | None = None
        self.seq = 0
        self.frames_sent = 0
        self._next_retry = 0.0
        # Szukanie powtarza sie co 2 s. Pelny raport ze skanu pokazujemy tylko
        # za pierwszym razem, zeby nie zasypac konsoli w kolko tym samym.
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
                print("[i] Szukam Tab5 na portach szeregowych...")
            device = autodetect(self.verbose and first)
            if device is None:
                if self.verbose and first:
                    print("[-] Nie znalazlem Tab5. Podepnij kabel USB-C albo podaj --port.")
                    print("    Szukam dalej w tle...")
                self._complained = True
                return False

        try:
            # write_timeout chroni przed zawieszeniem sie petli, gdyby Tab5
            # przestal odbierac (np. zawis albo reset w trakcie zapisu).
            self.ser = serial.Serial(device, BAUD, timeout=0.05, write_timeout=0.5)
            self.ser.reset_input_buffer()
            if not self.fixed:
                self.device = device
            self._complained = False  # nastepna utrata polaczenia znow zaraportuje
            if self.verbose:
                print(f"[+] Tab5 podlaczony: {device}")
            return True
        except (OSError, serial.SerialException) as exc:
            if self.verbose and first:
                print(f"[-] Nie moge otworzyc {device}: {exc}")
                print("    Probuje dalej w tle...")
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
            self.ser.write(payload)
            self.frames_sent += 1
            return True
        except (OSError, serial.SerialException, serial.SerialTimeoutException) as exc:
            if self.verbose:
                print(f"[-] Utracono Tab5 ({exc}). Czekam na ponowne podlaczenie.")
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
