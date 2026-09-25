param(
    [Parameter(Mandatory=$true)][string]$SourceDir,
    [Parameter(Mandatory=$true)][string]$BuildDir,
    [string]$Config = "Debug"
)

$ErrorActionPreference="Stop"

$version=(Get-Content (Join-Path $SourceDir "VERSION") -Raw).Trim()
$stage=Join-Path $env:TEMP ("AuroraGlass-P8-SDK-Layout-" + $PID)

if(Test-Path $stage){
    Remove-Item $stage -Recurse -Force
}

& cmake --install $BuildDir --config $Config --prefix $stage
if($LASTEXITCODE){
    throw "INSTALL_FAILED"
}

function Need([string]$p) {
    if(!(Test-Path $p)){
        throw ("MISSING=" + $p)
    }
}

Need (Join-Path $stage "VERSION")

if((Get-Content (Join-Path $stage "VERSION") -Raw).Trim() -ne $version){
    throw "VERSION_MISMATCH"
}

$manifest=Get-Content (Join-Path $SourceDir "api/native_public_headers.txt") |
    Where-Object {$_ -and !$_.StartsWith("#")}

$expected=@()

foreach($entry in $manifest){
    $rel=$entry.Replace("\","/")

    if($rel.StartsWith("src/")){
        $rel=$rel.Substring(4)
    }elseif($rel.StartsWith("adapters/")){
        $rel=$rel.Substring(9)
    }

    $expected += $rel

    Need (Join-Path (Join-Path $stage "include") $rel)
}

Need (Join-Path $stage "include/auroraglass/version.h")

$includeRoot=Join-Path $stage "include"

$installed=Get-ChildItem $includeRoot -Recurse -File -Filter "*.h" |
    ForEach-Object {
        $_.FullName.Substring($includeRoot.Length + 1).Replace("\","/")
    }

$allowed=@(
    $expected + "auroraglass/version.h" |
    Sort-Object -Unique
)

$extra=@(
    $installed |
    Where-Object {$_ -notin $allowed}
)

if($extra.Count -ne 0){
    throw ("UNEXPECTED_PUBLIC_HEADER=" + ($extra -join ","))
}

foreach($internal in @(
    "core/d3d11_device.h",
    "core/glass_renderer.h",
    "core/shader_library.h"
)){
    if(Test-Path (Join-Path $includeRoot $internal)){
        throw ("INTERNAL_HEADER_LEAK=" + $internal)
    }
}

foreach($file in @(
    "lib/$Config/AuroraGlassMaterial.lib",
    "lib/$Config/AuroraGlassCore.lib",
    "lib/$Config/AuroraGlassControls.lib",
    "lib/$Config/AuroraGlassMotion.lib",
    "lib/$Config/AuroraGlassMotionControls.lib",
    "lib/$Config/AuroraGlassWin32Adapter.lib",
    "bin/$Config/AuroraGlassWpfInterop.dll",
    "bin/$Config/AuroraGlassWinUIInterop.dll",
    "managed/WPF/$Config/AuroraGlass.Wpf.dll",
    "managed/WinUI/$Config/AuroraGlass.WinUI.dll",
    "lib/cmake/AuroraGlass/AuroraGlassConfig.cmake",
    "lib/cmake/AuroraGlass/AuroraGlassConfigVersion.cmake",
    "lib/cmake/AuroraGlass/AuroraGlassTargets.cmake",
    "shaders/glass.hlsl"
)){
    Need (Join-Path $stage $file)
}

# Expected managed assembly version derives from canonical VERSION (x.y.z -> x.y.z.0).
$vp=$version.Split(".")
$expMajor=[int]$vp[0]; $expMinor=[int]$vp[1]; $expBuild=[int]$vp[2]

foreach($managed in @(
    "managed/WPF/$Config/AuroraGlass.Wpf.dll",
    "managed/WinUI/$Config/AuroraGlass.WinUI.dll"
)){
    $path=Resolve-Path (Join-Path $stage $managed)

    $av=[Reflection.AssemblyName]::GetAssemblyName(
        $path
    ).Version

    if(
        $av.Major -ne $expMajor -or
        $av.Minor -ne $expMinor -or
        $av.Build -ne $expBuild
    ){
        throw (
            "MANAGED_VERSION_MISMATCH=" +
            $managed +
            ":" +
            $av +
            " expected=" + $version
        )
    }
}

Write-Output "P8_SDK_INSTALL_LAYOUT=PASS"
Write-Output ("VERSION=" + $version)
Write-Output ("PUBLIC_HEADERS=" + $expected.Count)
Write-Output "INTERNAL_HEADERS=ABSENT"
Write-Output "NATIVE_ARTIFACTS=PASS"
Write-Output "MANAGED_ARTIFACTS=PASS"
Write-Output "RUNTIME_RESOURCES=PASS"

Remove-Item $stage -Recurse -Force