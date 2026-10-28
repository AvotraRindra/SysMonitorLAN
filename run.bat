@echo off
cd /d "%~dp0"
if not exist "SysMonitor.exe" (
  echo [INFO] Compilation necessaire pour cette premiere execution.
  call build.bat
  if errorlevel 1 (
    echo [ERREUR] Compiler C Windows requis : voir README.md
    pause
    exit /b 1
  )
)
SysMonitor.exe %*
