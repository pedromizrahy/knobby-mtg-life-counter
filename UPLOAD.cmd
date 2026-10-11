@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\upload-dial.ps1" %*
if errorlevel 1 (
  echo.
  echo FALHA: upload nao concluido.
  pause
  exit /b 1
)
echo.
pause
