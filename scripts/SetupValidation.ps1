# Optional workspace-local build when the Vulkan SDK is not installed.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'DeveloperEnvironment.ps1')
$workspace = Split-Path -Parent $PSScriptRoot
$source = Join-Path $workspace '.tools/validation-src'
$build = Join-Path $workspace '.tools/validation-build'
$install = Join-Path $workspace '.tools/validation'
if (!(Test-Path -LiteralPath $source)) {
    & git clone --depth 1 --branch vulkan-sdk-1.4.313.0 https://github.com/KhronosGroup/Vulkan-ValidationLayers.git $source
    if ($LASTEXITCODE -ne 0) { throw 'Validation source download failed' }
}
$revision = & git -C $source rev-parse HEAD
if ($revision -ne '50b87dd4be883b63c10e3c4f7b9c5aac0c82efd3') { throw 'Unexpected validation source revision' }
& cmake -S $source -B $build -G Ninja -DUPDATE_DEPS=ON -DBUILD_TESTS=OFF -DBUILD_WERROR=OFF -DCMAKE_BUILD_TYPE=Release "-DCMAKE_INSTALL_PREFIX=$install"
if ($LASTEXITCODE -ne 0) { throw 'Validation configure failed' }
& cmake --build $build --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Validation build failed' }
& cmake --install $build
if ($LASTEXITCODE -ne 0) { throw 'Validation install failed' }
