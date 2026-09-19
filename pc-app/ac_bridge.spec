# -*- mode: python ; coding: utf-8 -*-
"""Konfiguracja PyInstallera: mostek jako jeden plik wykonywalny.

Na Windows produkuje ac_bridge.exe, ktorego nie trzeba niczym poprzedzac -
uzytkownik nie musi miec zainstalowanego Pythona ani pyserial.

UWAGA: PyInstaller NIE kompiluje skrosnie. Plik .exe musi powstac na Windows
(albo w CI - patrz .github/workflows/build-exe.yml). Budowanie tego spec-a na
macOS/Linux da binarke dla macOS/Linux, nie dla Windows.
"""

a = Analysis(
    ["ac_bridge.py"],
    pathex=[],
    binaries=[],
    datas=[],
    # pyserial wybiera backend dynamicznie, wiec statyczna analiza importow
    # moze go przeoczyc. Wymieniamy je wprost.
    hiddenimports=[
        "serial",
        "serial.tools.list_ports",
        "serial.tools.list_ports_windows",
        "serial.serialwin32",
        "serial.win32",
        "acbridge.shared_memory",
        "acbridge.serial_link",
        "acbridge.simulator",
        "acbridge.protocol",
    ],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    # Nie wciagamy tkintera ani testow - niepotrzebnie pompuja rozmiar.
    excludes=["tkinter", "unittest", "pydoc", "doctest", "test"],
    noarchive=False,
)

pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.datas,
    [],
    name="ac_bridge",
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    # UPX potrafi podnosic falszywe alarmy antywirusow - nie warto.
    upx=False,
    runtime_tmpdir=None,
    console=True,   # to jest narzedzie konsolowe, okno ma byc widoczne
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)
