param([ValidateSet('debug','release')][string]$Preset='debug')
$ErrorActionPreference='Stop'
$workspace=Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'DeveloperEnvironment.ps1')
Push-Location $workspace
try {
    $combinations=@(
        @{Name='No skybox';Sky='OFF';Post='ON';Debug='ON'},
        @{Name='No postprocess';Sky='ON';Post='OFF';Debug='ON'},
        @{Name='No debug draw';Sky='ON';Post='ON';Debug='OFF'},
        @{Name='No optional modules';Sky='OFF';Post='OFF';Debug='OFF'}
    )
    foreach ($combination in $combinations) {
        Write-Output "Checking $($combination.Name)"
        & cmake --preset $Preset "-DENGINE_FEATURE_SKYBOX=$($combination.Sky)" "-DENGINE_FEATURE_POSTPROCESS=$($combination.Post)" "-DENGINE_FEATURE_DEBUG_DRAW=$($combination.Debug)"
        if ($LASTEXITCODE -ne 0) { throw 'Module-removal configure failed' }
        & cmake --build "out/build/$Preset" --target engine_runtime engine_editor engine_feature_tests --parallel
        if ($LASTEXITCODE -ne 0) { throw 'Module-removal build failed' }
        & "./out/build/$Preset/tests/engine_feature_tests.exe"
        if ($LASTEXITCODE -ne 0) { throw 'Module-removal CPU tests failed' }
        & powershell -NoProfile -ExecutionPolicy Bypass -File ./scripts/RunEditor.ps1 -Preset $Preset -Smoke
        if ($LASTEXITCODE -ne 0) { throw 'Module-removal GPU tests failed' }
    }
} finally {
    & cmake --preset $Preset -DENGINE_FEATURE_SKYBOX=ON -DENGINE_FEATURE_POSTPROCESS=ON -DENGINE_FEATURE_DEBUG_DRAW=ON
    if ($LASTEXITCODE -ne 0) { throw 'Restore feature configuration failed' }
    & cmake --build --preset $Preset --parallel
    if ($LASTEXITCODE -ne 0) { throw 'Restore feature build failed' }
    Pop-Location
}
