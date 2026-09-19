@echo off
REM Budowanie ac_bridge.exe na Windows. Wymaga tylko Pythona 3.9+.
REM Uruchom dwuklikiem albo z wiersza polecen w tym katalogu.

setlocal
cd /d "%~dp0"

echo === Instalacja zaleznosci ===
python -m pip install --upgrade pip
python -m pip install -r requirements.txt pyinstaller
if errorlevel 1 goto :error

echo.
echo === Budowanie ===
python -m PyInstaller --clean --noconfirm ac_bridge.spec
if errorlevel 1 goto :error

echo.
echo === Gotowe ===
echo Plik: %~dp0dist\ac_bridge.exe
echo Skopiuj go gdziekolwiek i uruchom - nie wymaga Pythona.
goto :end

:error
echo.
echo BLAD budowania. Sprawdz komunikaty powyzej.
exit /b 1

:end
endlocal
pause
