@echo off
REM Builds ac_bridge.exe on Windows. Needs nothing but Python 3.9+.
REM Double-click it, or run it from a command prompt in this directory.

setlocal
cd /d "%~dp0"

echo === Installing dependencies ===
python -m pip install --upgrade pip
python -m pip install -r requirements.txt pyinstaller
if errorlevel 1 goto :error

echo.
echo === Building ===
python -m PyInstaller --clean --noconfirm ac_bridge.spec
if errorlevel 1 goto :error

echo.
echo === Done ===
echo File: %~dp0dist\ac_bridge.exe
echo Copy it anywhere and run it - no Python required.
goto :end

:error
echo.
echo BUILD FAILED. Check the messages above.
exit /b 1

:end
endlocal
pause
