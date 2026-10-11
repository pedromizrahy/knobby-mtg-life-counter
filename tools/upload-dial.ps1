param([string]$Port='COM4')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$build=Join-Path $root '.dial-build\arduino'
$report=Join-Path $root '.dial-build\assets-build-report.json'
$binary=Join-Path $build 'knobby.ino.bin'
if (-not (Test-Path $binary)) { throw 'Nao ha firmware compilado neste diretório. Execute BUILD.cmd primeiro.' }
if (-not (Test-Path $report)) { throw 'Falta o relatorio dos assets. Execute BUILD.cmd primeiro.' }
$recorded = @(Get-Content -LiteralPath $report -Raw | ConvertFrom-Json)
foreach ($item in $recorded) {
  $file = Join-Path $root ("knobby\src\" + $item.name)
  if (-not (Test-Path -LiteralPath $file)) { throw "Asset removido depois do build: $($item.name). Execute BUILD.cmd." }
  $current = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant()
  if ($current -ne $item.sha256) { throw "Asset alterado apos build: $($item.name). Execute BUILD.cmd novamente antes do upload." }
}
if ((Get-Item -LiteralPath $binary).Length -gt 0xC80000) { throw 'Firmware maior que particao app0 de 12.5 MiB.' }

$cli='C:\Program Files\Arduino CLI\arduino-cli.exe'
if (-not (Test-Path $cli)) {
  $c=Get-Command arduino-cli -ErrorAction SilentlyContinue
  if (-not $c) { throw 'Arduino CLI nao encontrado.' }
  $cli=$c.Source
}
$fqbn='esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=qio,PartitionScheme=custom'
Write-Host "Upload da compilacao validada, porta $Port"
& $cli upload --fqbn $fqbn --input-dir $build -p $Port (Join-Path $root 'knobby')
if ($LASTEXITCODE -ne 0) { throw "Upload falhou (codigo $LASTEXITCODE)" }
Write-Host 'UPLOAD CONCLUIDO.'
