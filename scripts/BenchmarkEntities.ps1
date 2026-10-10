param([ValidateSet('debug','release')][string]$Preset='release',[switch]$Editor)
$ErrorActionPreference='Stop'
$workspace=Split-Path -Parent $PSScriptRoot
if($Editor){& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'RunEditor.ps1') -Preset $Preset -EntityBenchmark}
else{& (Join-Path $workspace "out/build/$Preset/tests/engine_entity_benchmark.exe")}
exit $LASTEXITCODE
