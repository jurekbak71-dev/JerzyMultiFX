@echo off
cd /d "%~dp0"
if /i "%~1"=="--help" (
 echo Rozpakuj ZIP i uruchom ten plik, aby zainstalowac VST3.
 exit /b 0
)
echo Zamknij FL Studio lub inny program muzyczny.
echo Instalator poprosi Windows o zgode na instalacje w folderze wtyczek.
pause
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0ZAINSTALUJ-VST3.ps1"
if errorlevel 1 pause
