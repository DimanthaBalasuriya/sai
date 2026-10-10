param(
    [ValidateSet('debug','release')][string]$Preset='debug',
    [switch]$Smoke,
    [int]$SmokeFrames=0,
    [string]$Gltf='',
    [string]$Theme='',
    [float]$UiDpi=0,
    [switch]$ThemePanel,
    [switch]$UiSmoke,
    [switch]$InputSmoke,
    [switch]$EntityBenchmark,
    [string]$Bindings='',
    [string]$Gpu=''
)
$ErrorActionPreference='Stop'
$workspace=Split-Path -Parent $PSScriptRoot
$previousLayerPath=$env:VK_LAYER_PATH
try {
    $layerDirectory=Join-Path $workspace '.tools/validation/bin'
    if (Test-Path -LiteralPath (Join-Path $layerDirectory 'VkLayer_khronos_validation.json')) {
        $env:VK_LAYER_PATH=if ($previousLayerPath) { "$layerDirectory;$previousLayerPath" } else { $layerDirectory }
    }
    $editorArguments=@()
    if($EntityBenchmark){$editorArguments+='--entity-benchmark'}
    if ($InputSmoke) {$editorArguments+= '--input-smoke'}
    elseif ($UiSmoke) {$editorArguments+= '--ui-smoke'}
    elseif ($Smoke) { $editorArguments+= '--smoke' }
    elseif ($SmokeFrames -gt 0) { $editorArguments+= @('--smoke-frames',"$SmokeFrames") }
    if ($Gpu) { $editorArguments+= "--gpu=$Gpu" }
    if ($Gltf) { $editorArguments+= @('--gltf',$Gltf) }
    if ($Theme) { $editorArguments+= @('--theme',$Theme) }
    if ($Bindings) { $editorArguments+= @('--bindings',$Bindings) }
    if ($UiDpi -gt 0) { $editorArguments+= @('--ui-dpi',$UiDpi.ToString([System.Globalization.CultureInfo]::InvariantCulture)) }
    if ($ThemePanel) { $editorArguments+= '--theme-panel' }
    & (Join-Path $workspace "out/build/$Preset/editor/engine_editor.exe") @editorArguments
    $exitCode=$LASTEXITCODE
} finally { $env:VK_LAYER_PATH=$previousLayerPath }
exit $exitCode
