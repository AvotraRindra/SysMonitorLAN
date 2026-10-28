@echo off
setlocal
cd /d "%~dp0\.."
echo ===== Tests Windows SysMonitor Option B =====
call build.bat
if errorlevel 1 exit /b 1
SysMonitor.exe --selftest
if errorlevel 1 exit /b 1
SysMonitor.exe --demo
if errorlevel 1 exit /b 1
SysMonitor.exe --check
if errorlevel 1 exit /b 1
echo [INFO] Lancer SysMonitor.exe --setup pour ajouter une VM.
echo [OK] Tests locaux termines. --once necessite SSH et Kali en ligne.
