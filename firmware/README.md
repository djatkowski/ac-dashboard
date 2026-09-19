# Firmware Tab5

Arduino + M5Unified/M5GFX, budowane przez PlatformIO.

## Wgranie

```bash
pio run -t upload
```

Potem odpal mostek na pececie (`../pc-app`). Ten sam kabel USB-C sluzy do
wgrywania i do telemetrii.

## Dlaczego taka konfiguracja

**Platforma.** Oficjalny `platform-espressif32` nie obsluguje ESP32-P4, ktory
siedzi w Tab5. Uzywamy forka [pioarduino](https://github.com/pioarduino/platform-espressif32) -
to jedyny sposob, zeby to w ogole zbudowac.

**Definicja plytki.** pioarduino nie ma wpisu dla Tab5, wiec projekt wozi
wlasny: `boards/m5stack_tab5.json`. Wyprowadzony z `esp32-p4_r3-evboard`,
z poprawiona iloscia PSRAM (32 MB).

**Biblioteki.** M5Unified 0.2.22 to pierwsze wydanie, ktore zna Tab5
(`board_M5Tab5`). Starsze nie wykryja ekranu.

Wersje w `platformio.ini` sa przypiete swiadomie. Jesli bedziesz je podnosic,
sprawdz, czy plytka dalej sie wykrywa.

## Jak to dziala

`main.cpp` zbiera telemetrie w **kazdym** obiegu petli, ale rysuje tylko
`TARGET_FPS` razy na sekunde. Gdyby odbior byl zwiazany z rysowaniem, dluzsza
klatka przepelnialaby bufor USB.

Kazdy kafelek ekranu to osobny sprite w PSRAM, przerysowywany dopiero wtedy,
gdy jego dane sie zmienily. Przerysowywanie calych 1280x720 co klatke
zmarnowaloby pasmo do framebufora na dane, ktore sie nie ruszyly.

`frame_parser.h` celowo nie zalezy od Arduino - dzieki temu ten sam kod, ktory
sklada ramki na plytce, da sie przetestowac na PC:

```bash
python3 tools/make_stream.py /tmp/stream.bin
c++ -std=c++17 -I src -o /tmp/test_parser tools/test_parser.cpp
/tmp/test_parser /tmp/stream.bin
```

Test wstrzykuje w strumien smieci i falszywy `ACT5`, zeby sprawdzic, czy parser
sie z tego podnosi bez gubienia ramek.

## Ustawienia

Wszystko, co warto ruszac, jest w `src/config.h`: jasnosc, docelowy fps, ekran
startowy, progi swiatel zmiany biegu, zakres wskaznika kata.

## Znaki i konwencje

- `drift_angle > 0` - tyl wyszedl w **prawo** (nos patrzy w lewo).
- `steer > 0` - kierownica w **prawo**.
- Kontra jest prawidlowa, gdy znaki sie zgadzaja - lapanie poslizgu to skret
  w strone poslizgu. Na tym opiera sie wskaznik `KONTRA OK` / `W POSLIZG!`.

Jesli u Ciebie kierownica wychodzi odwrotnie (AC bywa rozne w zaleznosci od
konfiguracji kontrolera), odwroc znak `f.steer` w `pc-app/acbridge/shared_memory.py`.

## Dotyk

- zakladka na dolnym pasku - zmiana ekranu,
- przesuniecie palcem w bok (min. 180 px) - to samo, gdziekolwiek po ekranie.

Dolny pasek pokazuje tez stan: `BRAK USB` / `CZEKAM NA GRE` / `POLACZONO`,
czestotliwosc ramek, fps i licznik zgubionych pakietow.
