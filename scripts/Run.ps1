param(
    [ValidateSet('debug', 'release')][string]$Preset = 'debug',
    [int]$SmokeFrames = 0,
    [switch]$ResizeSmoke,
    [switch]$Instances,
    [string]$Gltf='',
    [switch]$LightingGrid,
    [switch]$LightingSmoke,
    [switch]$LightingAllSmoke,
    [switch]$LightingCompare,
    [switch]$OrbitalSmoke,
    [float]$EV100=15,
    [string]$Gpu=''
)
$ErrorActionPreference = 'Stop'
$workspace = Split-Path -Parent $PSScriptRoot
$layerDirectory = Join-Path $workspace '.tools/validation/bin'
$previousLayerPath = $env:VK_LAYER_PATH
try {
    if (Test-Path -LiteralPath (Join-Path $layerDirectory 'VkLayer_khronos_validation.json')) {
        $env:VK_LAYER_PATH = if ($previousLayerPath) { "$layerDirectory;$previousLayerPath" } else { $layerDirectory }
    }
    $runtimeArguments = @()
    if ($ResizeSmoke) { $runtimeArguments = @('--resize-smoke') }
    elseif ($SmokeFrames -gt 0) { $runtimeArguments = @('--smoke-frames', "$SmokeFrames") }
    if ($Instances -and $Gltf) { throw 'Choose either Instances or Gltf' }
    if ($Instances) { $runtimeArguments+= '--instances' }
    if ($Gltf) { $runtimeArguments+= @('--gltf',$Gltf) }
    if ($LightingGrid) { $runtimeArguments+= '--lighting-grid' }
    if ($LightingCompare) { $runtimeArguments+= '--lighting-path-compare' }
    if ($OrbitalSmoke) { $runtimeArguments+= '--orbital-smoke' }
    if ($LightingAllSmoke) { $runtimeArguments+= '--lighting-all-smoke' }
    if ($LightingSmoke) { $runtimeArguments+= '--lighting-smoke' }
    if ($PSBoundParameters.ContainsKey('EV100')) { $runtimeArguments+= "--ev100=$($EV100.ToString([System.Globalization.CultureInfo]::InvariantCulture))" }
    if ($Gpu) { $runtimeArguments+= "--gpu=$Gpu" }
    & (Join-Path $workspace "out/build/$Preset/apps/runtime/engine_runtime.exe") @runtimeArguments
    $exitCode = $LASTEXITCODE
} finally { $env:VK_LAYER_PATH = $previousLayerPath }
exit $exitCode
