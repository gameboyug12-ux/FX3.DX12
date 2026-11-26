param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [string]$BuildDirectory = "build-engine"
)
$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$build = Join-Path $root $BuildDirectory
Write-Host "Configuring XWe engine DLL only; standalone renderer disabled."
cmake -S $root -B $build -A x64 -DFX3_BUILD_RENDERER=OFF -DXWE_BUILD_ENGINE=ON
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed: $LASTEXITCODE" }
cmake --build $build --config $Configuration --target xwe_engine
if ($LASTEXITCODE -ne 0) { throw "Engine DLL build failed: $LASTEXITCODE" }
$dll = Join-Path $build "bin/$Configuration/xwe_engine.dll"
if (-not (Test-Path $dll)) { throw "DLL not found: $dll" }
Write-Host "Engine DLL: $dll"
Write-Host "Run the Python smoke test: py -3 tests/python/engine_dll_test.py --smoke"
