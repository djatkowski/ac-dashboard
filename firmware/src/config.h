// Konfiguracja uzytkownika. Jedyny plik, ktory warto edytowac przed wgraniem.
#pragma once

// --- Polaczenie ---------------------------------------------------------------
// Dane ida po USB-C (tym samym kablem, ktorym wgrywasz firmware).
// Nie ma tu nic do ustawiania - PC sam znajduje Tab5 po porcie szeregowym.

// Po ilu ms bez ramki uznajemy, ze kabel padl
#define LINK_TIMEOUT_MS 1500

// Co ile ms wolamy "ACT5HELLO", dopoki PC nas nie znajdzie
#define HELLO_PERIOD_MS 500

// --- Wyglad -------------------------------------------------------------------
#define SCREEN_BRIGHTNESS 200  // 0..255
#define TARGET_FPS 45

// Ekran startowy: 0 = RACE (predkosciomierz/obrotomierz), 1 = DRIFT
#define DEFAULT_PAGE 0

// Progi swiatel zmiany biegu jako ulamek maxRpm
#define SHIFT_LIGHT_START 0.80f
#define SHIFT_LIGHT_RED 0.93f

// Koniec skali wskaznika kata driftu (stopnie)
#define DRIFT_GAUGE_MAX 70.0f
