# Dashboard Assetto Corsa na M5Stack Tab5

Telemetria z Assetto Corsa na ekranie Tab5 (1280x720), przez kabel USB-C.
Dwa ekrany przelaczane dotykiem: klasyczny **RACE** i **DRIFT** z katem poslizgu.

```
  Assetto Corsa                pc-app/                      firmware/
  (shared memory)  ────────>   ac_bridge.py    ──USB-C──>   Tab5
   physics 333 Hz              czyta pamiec                 rysuje 45 fps
   graphics 60 Hz              wspoldzielona,
   static                      pakuje do 270 B
```

## Dlaczego przez program posredni

AC nie wysyla przez siec kompletu danych - wbudowana telemetria UDP (port 9996)
nie zawiera m.in. paliwa i temperatur opon. Pelny obraz jest tylko w pamieci
wspoldzielonej, a ta jest dostepna wylacznie lokalnie na pececie. Stad mostek:
czyta pamiec i przepycha wszystko do Tab5.

## Szybki start

**1. PC (Windows, tam gdzie chodzi gra)**

```bash
cd pc-app
pip install -r requirements.txt
python ac_bridge.py
```

Wolisz jeden plik .exe bez instalowania Pythona? Uruchom `pc-app\build_exe.bat`
(na Windows) albo pobierz artefakt z GitHub Actions - szczegoly w `pc-app/README.md`.

**2. Tab5**

```bash
cd firmware
pio run -t upload
```

Podepnij Tab5 kablem USB-C. Mostek sam znajdzie port - nic nie konfigurujesz.

## Test bez gry

Chcesz zobaczyc dzialajacy ekran, zanim odpalisz AC (albo pracujesz na Macu):

```bash
python ac_bridge.py --demo
```

Symulator udaje auto wpadajace cyklicznie w drift, wiec widac cala skale
wskaznikow.

## Uklad ekranow

**RACE** - pasek obrotow na calej szerokosci ze swiatlami zmiany biegu,
predkosc, bieg, obroty, ABS/TC/DRS/PIT, paliwo, trzy czasy okrazen,
pedaly i kierownica.

**DRIFT** - duzy wskaznik kata poslizgu ze znacznikiem szczytu, historia kata
z ostatnich ~5 s, wskaznik kontry (czy lapiesz poslizg, czy go poglebiasz),
predkosc obrotu, kula przeciazen, poslizg i temperatury kazdej opony,
przyczepnosc nawierzchni.

Przelaczanie: dotknij zakladki na dole albo przesun palcem w bok.

## Struktura

```
pc-app/                mostek na PC (Python, tylko pyserial)
  acbridge/protocol.py   <- format pakietu: JEDYNE zrodlo prawdy
  acbridge/shared_memory.py
  acbridge/serial_link.py
  acbridge/simulator.py
  ac_bridge.py
  ac_bridge.spec         <- pakowanie do jednego .exe
  build_exe.bat          <- budowanie .exe jednym kliknieciem (Windows)
firmware/              PlatformIO, Arduino + M5Unified
  src/protocol.h         <- lustro protocol.py, trzymac zgodne!
  src/frame_parser.h     <- skladanie ramek, testowalne na PC
  src/page_race.cpp
  src/page_drift.cpp
  tools/                 <- test parsera uruchamiany na PC
```

Zmieniasz format danych? Edytuj `protocol.py` **i** `protocol.h`, podbij
`VERSION`, potem uruchom test zgodnosci:

```bash
cd firmware
python3 tools/make_stream.py /tmp/stream.bin
c++ -std=c++17 -I src -o /tmp/test_parser tools/test_parser.cpp
/tmp/test_parser /tmp/stream.bin
```
