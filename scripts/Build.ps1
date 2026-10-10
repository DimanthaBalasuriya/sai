param(
    [ValidateSet('debug', 'release')][string]$Preset = 'debug',
    [switch]$Smoke,
    [switch]$ResizeSmoke
)
$ErrorActionPreference = 'Stop'
$workspace = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'DeveloperEnvironment.ps1')
Push-Location $workspace
try {
    & cmake --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
    & cmake --build --preset $Preset --parallel
    if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
    & ctest --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
    if ($Smoke) {
        & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'Run.ps1') -Preset $Preset -SmokeFrames 12
        if ($LASTEXITCODE -ne 0) { throw 'Window smoke test failed' }
    }
    if ($ResizeSmoke) {
        & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'Run.ps1') -Preset $Preset -ResizeSmoke
        if ($LASTEXITCODE -ne 0) { throw 'Resize smoke test failed' }
    }
} finally { Pop-Location }
