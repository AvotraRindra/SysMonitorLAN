@echo off
setlocal
cd /d "%~dp0"
echo [SysMonitor] Compilation C pour Windows...
where gcc >nul 2>&1
if not errorlevel 1 (
  gcc -std=c11 -Wall -Wextra -O2 -o SysMonitor.exe src\sysmonitor.c -luser32
  if errorlevel 1 (echo [ERREUR] Compilation GCC echouee.& exit /b 1)
  echo [OK] SysMonitor.exe compile avec GCC
  exit /b 0
)
where cl >nul 2>&1
if not errorlevel 1 (
  cl /nologo /W3 /O2 /TC src\sysmonitor.c /Fe:SysMonitor.exe user32.lib
  if errorlevel 1 (echo [ERREUR] Compilation MSVC echouee.& exit /b 1)
  echo [OK] SysMonitor.exe compile avec MSVC
  exit /b 0
)
echo [ERREUR] Aucun compilateur C trouve dans PATH.
echo Installer MinGW-w64 GCC ou ouvrir le terminal developpeur Visual Studio avec cl.
exit /b 1
