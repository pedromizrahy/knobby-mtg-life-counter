@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\build-dial.ps1" %*
if errorlevel 1 (
  echo.
  echo FALHA: build interrompido. NAO fazer upload.
  pause
  exit /b 1
)
echo.
pause
