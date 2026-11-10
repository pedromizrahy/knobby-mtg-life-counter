@echo off
setlocal EnableExtensions
cd /d "%~dp0..\.."
set "PROJECT=%CD%"
set "BLENDER_EXE="
where blender >nul 2>&1
if not errorlevel 1 for /f "delims=" %%B in ('where blender') do if not defined BLENDER_EXE set "BLENDER_EXE=%%B"
if not defined BLENDER_EXE (
  for /d %%D in ("C:\Program Files\Blender Foundation\Blender *") do (
    if exist "%%~fD\blender.exe" set "BLENDER_EXE=%%~fD\blender.exe"
  )
)
if not defined BLENDER_EXE (
  echo ERRO: Blender nao encontrado. Instale Blender 4.x e rode novamente.
  echo Os sprites antigos NAO serao alterados.
  pause
  exit /b 1
)
echo Blender: "%BLENDER_EXE%"
echo Gerando D6 3D real com o modelo Blender Dice Gen...
"%BLENDER_EXE%" -b -t 4 --python "%PROJECT%\tools\dice3d\render_d6.py" -- --output "%PROJECT%\knobby\src\dial_d6_frames.h"
if errorlevel 1 (
  echo FALHA na renderizacao. Nao compilar/upload.
  pause
  exit /b 1
)
findstr /C:"DIAL_D6_BLENDER_V1" "%PROJECT%\knobby\src\dial_d6_frames.h" >nul
if errorlevel 1 (
  echo ERRO: assinatura de frames 3D nao encontrada.
  pause
  exit /b 1
)
echo Modelo D6 3D gerado. Compilando SEM upload...
"C:\Program Files\Arduino CLI\arduino-cli.exe" compile --clean --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=qio,PartitionScheme=custom" "%PROJECT%\knobby"
if errorlevel 1 (
  echo ERRO: compilacao falhou. Nao fazer upload.
  pause
  exit /b 1
)
echo SUCESSO: compilacao do D6 3D concluiu. Verifique tamanho da particao ANTES do upload.
pause
