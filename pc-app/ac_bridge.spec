# -*- mode: python ; coding: utf-8 -*-
"""PyInstaller configuration: the bridge as a single executable.

On Windows this produces ac_bridge.exe, which needs no launcher - the user
does not have to have Python or pyserial installed.

NOTE: PyInstaller does NOT cross-compile. The .exe has to be produced on
Windows (or in CI - see .github/workflows/build-exe.yml). Building this spec
on macOS/Linux yields a macOS/Linux binary, not a Windows one.
"""

a = Analysis(
    ["ac_bridge.py"],
    pathex=[],
    binaries=[],
    datas=[],
    # pyserial picks its backend dynamically, so static import analysis can
    # miss it. List them explicitly.
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
    # Skip tkinter and the test modules - they only inflate the size.
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
    # UPX tends to trigger antivirus false positives - not worth it.
    upx=False,
    runtime_tmpdir=None,
    console=True,   # this is a console tool, the window should be visible
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)
