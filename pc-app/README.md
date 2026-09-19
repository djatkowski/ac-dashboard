# Mostek PC: Assetto Corsa -> Tab5

Czyta pamiec wspoldzielona AC i wysyla telemetrie po USB do Tab5.
Zaleznosci: tylko `pyserial`.

## Uruchomienie

```bash
pip install -r requirements.txt
python ac_bridge.py
```

Kolejnosc wlaczania jest dowolna - mostek czeka i na gre, i na tablet,
i sam sie podnosi po wyciagnieciu kabla.

## Wersja .exe (bez instalowania Pythona)

```bat
build_exe.bat
```

Wynik: `dist\ac_bridge.exe` - jeden plik, ~9 MB, bez zaleznosci. Skopiuj
gdziekolwiek i uruchom.

**PyInstaller nie kompiluje skrosnie.** Plik dla Windows musi powstac na
Windows. Jesli nie masz go pod reka, zrobi to CI: wypchnij repo na GitHuba,
a workflow `.github/workflows/build-exe.yml` zbuduje `.exe` na maszynie
`windows-latest` i wystawi go jako artefakt (zakladka Actions -> przebieg ->
Artifacts). Mozna go tez odpalic recznie przyciskiem "Run workflow".

Antywirusy lubia zglaszac falszywe alarmy na binarkach z PyInstallera.
Dlatego UPX jest w `ac_bridge.spec` wylaczony - z kompresja jest jeszcze gorzej.

## Opcje

| Flaga | Do czego |
|---|---|
| `--demo` | sztuczna telemetria; dziala tez na macOS/Linux, bez gry |
| `--port COM5` | z pominieciem autodetekcji portu |
| `--list-ports` | co widzi system |
| `--print` | podglad danych w konsoli (predkosc, obroty, kat driftu) |
| `--rate 60` | czestotliwosc wysylki, domyslnie 60 Hz |

## Jak znajduje Tab5

Tab5 wypisuje w kolko `ACT5HELLO`, dopoki nie dostanie telemetrii. Mostek
otwiera po kolei porty szeregowe (zaczynajac od tych z VID Espressifu,
`0x303A`) i czeka 1,5 s na to haslo. Dlatego zwykle nie trzeba podawac `--port`.

## Format danych

`acbridge/protocol.py` jest jedynym zrodlem prawdy o ukladzie bajtow.
Ramka ma 270 B:

```
[4B "ACT5"] [264B pakiet] [2B CRC16-CCITT]
```

Magic sluzy za slowo synchronizujace (strumien bajtow nie ma granic pakietow),
CRC odsiewa przypadkowe bajty danych, ktore wygladaja jak magic.

Po stronie Tab5 odpowiada temu `firmware/src/protocol.h`. **Zmiana w jednym
pliku wymaga zmiany w drugim** - inaczej `static_assert` w firmware przestanie
sie zgadzac albo, gorzej, dane beda czytane z przesunieciem.

## Skad biora sie dane driftowe

- **Kat poslizgu** - z `localVelocity` (predkosc w ukladzie auta):
  `atan2(vx, |vz|)`. Dodatni = wektor predkosci ucieka w prawo wzgledem osi
  auta, czyli tyl wyszedl w prawo. Ponizej 0,8 m/s kat jest smieciem, wiec
  zwracamy zero. Jest tez zapasowa sciezka przez `heading` dla buildow, ktore
  nie wypelniaja `localVelocity`.
- **Predkosc obrotu** - `localAngularVel[1]`, zamieniona na stopnie/s.
- **Poslizg kol** - `wheelSlip[4]` prosto z gry.
- **Kat poslizgu kola** - AC go nie wystawia. Skalujemy `wheelSlip`, zeby dalo
  sie zrobic czytelny wskaznik. To wskaznik, nie pomiar.

## Uwaga: AC, nie ACC

Struktury w `shared_memory.py` opisuja **Assetto Corsa**. W Assetto Corsa
Competizione uklad `SPageFileGraphic` rozjezdza sie za polem `carCoordinates`
(ACC ma tam tablice dla wielu aut), a `SPageFilePhysics` ma dodatkowe pola na
koncu. Pod ACC trzeba by dopisac osobny zestaw struktur.
