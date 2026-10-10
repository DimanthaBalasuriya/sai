param(
    [ValidateSet('debug','release')][string]$Preset='debug',
    [ValidateRange(0,2)][int]$Quality=1,
    [ValidateRange(0,23.999)][float]$Time=12,
    [ValidateRange(0,10)][int]$DebugView=0,
    [switch]$Smoke,
    [switch]$Benchmark
)
$ErrorActionPreference='Stop'
$workspace=Split-Path -Parent $PSScriptRoot
$previousLayerPath=$env:VK_LAYER_PATH
try {
    $layerDirectory=Join-Path $workspace '.tools/validation/bin'
    if(Test-Path -LiteralPath (Join-Path $layerDirectory 'VkLayer_khronos_validation.json')){$env:VK_LAYER_PATH=$layerDirectory}
    $skyArguments=@('--sky-time',$Time.ToString([System.Globalization.CultureInfo]::InvariantCulture),'--sky-quality',"$Quality",'--sky-debug',"$DebugView")
    if($Smoke){$skyArguments+='--sky-smoke'}elseif($Benchmark){$skyArguments+='--sky-benchmark'}
    & (Join-Path $workspace "out/build/$Preset/apps/runtime/engine_runtime.exe") @skyArguments
    $exitCode=$LASTEXITCODE
}finally{$env:VK_LAYER_PATH=$previousLayerPath}
exit $exitCode
