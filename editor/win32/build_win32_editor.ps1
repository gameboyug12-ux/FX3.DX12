$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
Set-Location $root
$vswhere = Join-Path $env:ProgramFiles(x86) "Microsoft Visual Studio/Installer/vswhere.exe"
if (Test-Path $vswhere) {
    $vs = & $vswhere -latest -products * -property installationPath
    $devShell = Join-Path $vs "Common7/Tools/Launch-VsDevShell.ps1"
    if (Test-Path $devShell) { . $devShell -Arch amd64 -HostArch amd64 }
}
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    throw "cl.exe not found. Run from a Visual Studio 2022 Developer PowerShell."
}
$dxc = Get-Command dxc.exe -ErrorAction SilentlyContinue
if (-not $dxc) { throw "dxc.exe is required for the DXR viewport. Add the DirectX Shader Compiler to PATH." }
$out = Join-Path $root "out/editor"
$shaderDir = Join-Path $out "shaders"
New-Item -ItemType Directory -Force -Path $out,$shaderDir | Out-Null
& $dxc.Source -nologo -T lib_6_3 -HV 2021 -Ges -Fo (Join-Path $shaderDir "pathtrace.dxil") (Join-Path $root "shaders/pathtrace.hlsl")
if ($LASTEXITCODE -ne 0) { throw "DXC shader compilation failed." }
$shaderLiteral = 'L"' + $shaderDir.Replace('\','\\') + '"'
$define = "/DXWE_SHADER_DIR=$shaderLiteral"
$sources = @(
    (Join-Path $PSScriptRoot "main.cpp"),
    (Join-Path $PSScriptRoot "MainWindow.cpp"),
    (Join-Path $PSScriptRoot "SceneDocument.cpp"),
    (Join-Path $PSScriptRoot "DXRViewport.cpp")
)
& cl.exe /nologo /std:c++20 /EHsc /W4 /permissive- /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX $define $sources "/Fe:$out/FX3Win32Editor.exe" /link /SUBSYSTEM:WINDOWS d3d12.lib dxgi.lib dxguid.lib d3dcompiler.lib comctl32.lib ole32.lib shell32.lib uuid.lib user32.lib gdi32.lib
if ($LASTEXITCODE -ne 0) { throw "Win32 editor compilation failed." }
Write-Host "Built $out/FX3Win32Editor.exe"
