@echo off
setlocal
if /i "%~1"=="--help" (
 echo Rozpakuj ZIP i uruchom ten plik na Windows.
 exit /b 0
)
cd /d "%~dp0"
title Jerzy MultiFX - instalacja na Raspberry Pi
echo.
echo Wlacz Raspberry Pi i HeadRush. Scisz glosniki.
echo Komputer musi byc w tej samej sieci co Raspberry Pi.
echo Gdy zapyta o haslo, wpisz haslo uzytkownika jerzy.
echo Podczas wpisywania hasla nie widac znakow - to normalne.
echo.
pause
where ssh >nul 2>nul
if errorlevel 1 goto no_ssh
where scp >nul 2>nul
if errorlevel 1 goto no_ssh
if not exist "JerzyMultiFX-AKTUALIZUJ.py" goto missing
scp "JerzyMultiFX-AKTUALIZUJ.py" jerzy@bassambient.local:~/
if errorlevel 1 goto failed
ssh -t jerzy@bassambient.local "python3 ~/JerzyMultiFX-AKTUALIZUJ.py"
if errorlevel 1 goto failed
echo.
echo GOTOWE. Panel otworzy sie w przegladarce.
start "" "http://bassambient.local:8097"
pause
exit /b 0
:no_ssh
echo Brakuje skladnika OpenSSH w Windows. Zrob zdjecie tego okna.
goto failed
:missing
echo Najpierw rozpakuj CALY ZIP do folderu. Nie uruchamiaj pliku wewnatrz ZIP.
:failed
echo.
echo Instalacja nie zostala potwierdzona. Zrob zdjecie komunikatu.
pause
exit /b 1
