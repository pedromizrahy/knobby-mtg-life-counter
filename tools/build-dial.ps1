param([switch]$Clean)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$src = Join-Path $root 'knobby\src'
$assetsDir = Join-Path $root '.dial-build'
$buildDir = Join-Path $assetsDir 'arduino'
$manifestPath = Join-Path $PSScriptRoot 'assets-manifest.json'
$fqbn = 'esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=qio,PartitionScheme=custom'
$cli = 'C:\Program Files\Arduino CLI\arduino-cli.exe'
if (-not (Test-Path -LiteralPath $cli)) {
  $cmd = Get-Command arduino-cli -ErrorAction SilentlyContinue
  if (-not $cmd) { throw 'Arduino CLI nao localizado. Instale o Arduino CLI ou corrija o caminho.' }
  $cli = $cmd.Source
}
New-Item -ItemType Directory -Force -Path $assetsDir | Out-Null

# D6 must use outcome-matched 3D frames, not the legacy shared roll + flat
# numbered landing sprites. Generate locally only if obsolete, no Blender.
$d6Path = Join-Path $src 'dial_d6_frames.h'
$d6Current = (Test-Path -LiteralPath $d6Path) -and
             (Select-String -LiteralPath $d6Path -SimpleMatch '#define DIAL_D6_PYTHON_MESH_V1 1' -Quiet)
if (-not $d6Current) {
  Write-Host 'D6 antigo detectado. Gerando seis trajetorias 3D e seis resultados correspondentes...'
  $python = Get-Command py -ErrorAction SilentlyContinue
  if ($python) { $pythonExe = $python.Source; $pythonPrefix = @('-3') }
  else {
    $python = Get-Command python -ErrorAction SilentlyContinue
    if (-not $python) { throw 'Python 3 nao encontrado. Instale Python 3 com py launcher antes de compilar o D6 3D.' }
    $pythonExe = $python.Source; $pythonPrefix = @()
  }
  Write-Host 'Conferindo dependencias do gerador Python...'
  & $pythonExe @pythonPrefix -m pip install numpy scipy Pillow opencv-python-headless
  if ($LASTEXITCODE -ne 0) { throw 'Falha ao preparar dependencias Python para o D6.' }
  $script = Join-Path $root 'tools\dice3d\render_d6_python.py'
  $tmp = Join-Path $assetsDir 'dial_d6_frames.h'
  & $pythonExe @pythonPrefix $script $tmp
  if ($LASTEXITCODE -ne 0) { throw 'Gerador D6 3D falhou; assets antigos preservados.' }
  if (-not (Select-String -LiteralPath $tmp -SimpleMatch '#define DIAL_D6_TOTAL 78' -Quiet)) { throw 'D6 gerado sem os 78 frames esperados.' }
  if (-not (Select-String -LiteralPath $tmp -SimpleMatch '#define DIAL_D6_PYTHON_MESH_V1 1' -Quiet)) { throw 'D6 gerado sem assinatura Python Mesh.' }
  Move-Item -LiteralPath $tmp -Destination $d6Path -Force
  Write-Host 'D6 substituido pelos 78 frames 3D correspondentes ao sorteio.'
}

$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$required = @($manifest.required)
$locked = @($manifest.locked)
$report = @()
Write-Host '=== DIAL: VERIFICACAO DE ASSETS ==='
foreach ($name in $required) {
  $path = Join-Path $src $name
  $entry = $locked | Where-Object { $_.name -eq $name } | Select-Object -First 1
  if (-not (Test-Path -LiteralPath $path) -and $entry) {
    $url = [string]$entry.url
    if ($url -notmatch '^https://github\.com/[^/]+/[^/]+/releases/download/') {
      throw "URL de release invalida para $name"
    }
    Write-Host "Baixando asset fixado: $name"
    $tmp = "$path.download"
    try {
      Invoke-WebRequest -Uri $url -OutFile $tmp
      $h = (Get-FileHash -LiteralPath $tmp -Algorithm SHA256).Hash.ToLowerInvariant()
      if ($h -ne ([string]$entry.sha256).ToLowerInvariant()) { throw "SHA-256 incorreto: $name" }
      Move-Item -LiteralPath $tmp -Destination $path -Force
    } finally {
      if (Test-Path -LiteralPath $tmp) { Remove-Item -LiteralPath $tmp -Force }
    }
  }
  if (-not (Test-Path -LiteralPath $path)) {
    throw "Asset ausente: $name. Os arquivos V9 ja instalados servem para o primeiro build. Nao foi configurada uma release para baixar este arquivo automaticamente."
  }
  $hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
  if ($entry -and $hash -ne ([string]$entry.sha256).ToLowerInvariant()) {
    throw "Asset de versao diferente: $name; hash esperado $($entry.sha256), recebido $hash"
  }
  $f = Get-Item -LiteralPath $path
  $report += [pscustomobject]@{ name=$name; bytes=$f.Length; sha256=$hash; source=$(if($entry){'github-release-locked'}else{'local-unlocked'}) }
  Write-Host ("OK {0} ({1} bytes) [{2}]" -f $name,$f.Length,$hash.Substring(0,12))
}
$report | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $assetsDir 'assets-build-report.json') -Encoding UTF8
if ($locked.Count -eq 0) {
  Write-Warning 'Assets locais verificados por hash, mas ainda SEM hashes oficiais no repositorio. Para garantir reproducibilidade entre PCs, publicar release e preencher assets-manifest.json.'
}
$part = Get-Content -LiteralPath (Join-Path $root 'knobby\partitions.csv') -Raw
if ($part -notmatch '(?m)^app0,\s*app,\s*ota_0,\s*0x10000,\s*0xC80000,') { throw 'Particao app0 de 12.5 MiB nao encontrada. Compilacao bloqueada.' }
Write-Host '=== COMPILANDO PARA DIRETORIO FIXO ==='
$compileArgs = @('compile','--fqbn',$fqbn,'--build-path',$buildDir)
if ($Clean) { $compileArgs += '--clean' }
$compileArgs += (Join-Path $root 'knobby')
& $cli @compileArgs
if ($LASTEXITCODE -ne 0) { throw "Compilacao falhou (codigo $LASTEXITCODE)" }
$binary = Join-Path $buildDir 'knobby.ino.bin'
if (-not (Test-Path -LiteralPath $binary)) {
  $match = Get-ChildItem -LiteralPath $buildDir -Filter '*.ino.bin' | Select-Object -First 1
  if (-not $match) { throw 'Binario nao encontrado apos compilacao. Nao fazer upload.' }
  $binary = $match.FullName
}
$size = (Get-Item -LiteralPath $binary).Length
$limit = 0xC80000
if ($size -gt $limit) { throw "FIRMWARE EXCEDE PARTICAO: $size > $limit bytes. Nao fazer upload." }
Write-Host ("BUILD OK: {0:N0} bytes de {1:N0} bytes ({2:N1}%)" -f $size,$limit,(100*$size/$limit))
Write-Host 'Upload separado: execute UPLOAD.cmd depois de verificar o resultado.'
