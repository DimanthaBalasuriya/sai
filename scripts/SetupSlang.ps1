$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$workspace = Split-Path -Parent $PSScriptRoot
$toolsDirectory = Join-Path $workspace '.tools'
$destination = Join-Path $toolsDirectory 'slang-2026.19'
if (Test-Path -LiteralPath (Join-Path $destination 'bin/slangc.exe')) { return }
New-Item -ItemType Directory -Path $toolsDirectory -Force | Out-Null
$archive = Join-Path $toolsDirectory 'slang.zip'
Invoke-WebRequest -Uri 'https://github.com/shader-slang/slang/releases/download/v2026.19/slang-2026.19-windows-x86_64.zip' -OutFile $archive
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne 'fc922f214b5d115f663932e59e80e0f6b6cc6fae140979cf307da6459b6f8f33') { throw 'Slang checksum mismatch' }
Expand-Archive -LiteralPath $archive -DestinationPath $destination -Force
