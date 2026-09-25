param(
    [Parameter(Mandatory=$true)][string]$SourceDir,
    [Parameter(Mandatory=$true)][string]$BuildDir
)

$ErrorActionPreference="Stop"

$samples=@(
    "p5_win32_sample",
    "p6_wpf_sample",
    "p7_winui_sample"
)

foreach($s in $samples){
    $dir=Join-Path $SourceDir ("samples/" + $s)
    if(!(Test-Path $dir)){
        throw ("SAMPLE_MISSING=" + $s)
    }
}

# P6 / P7 managed samples must only reference their adapter project,
# never Core sources or the tests directory.
foreach($pair in @(
    @("samples/p6_wpf_sample/P6.WpfSample.csproj","AuroraGlass.Wpf/AuroraGlass.Wpf.csproj"),
    @("samples/p7_winui_sample/P7.WinUISample.csproj","AuroraGlass.WinUI/AuroraGlass.WinUI.csproj")
)){
    $rel=$pair[0]
    $expected=$pair[1]

    $text=Get-Content (Join-Path $SourceDir $rel) -Raw

    if($text -notmatch [regex]::Escape($expected)){
        throw ("ADAPTER_REF_MISSING=" + $rel)
    }

    if($text -match "tests/") {
        throw ("TESTS_DEPENDENCY=" + $rel)
    }

    if($text -match "src/core/") {
        throw ("CORE_SOURCE_DEPENDENCY=" + $rel)
    }
}

# P5 Win32 sample must not include internal production headers.
$p5header=Join-Path $SourceDir "samples/p5_win32_sample/sample_d3d11_host.h"
if(!(Test-Path $p5header)){
    throw "P5_SAMPLE_HOST_MISSING"
}

$p5dir=Join-Path $SourceDir "samples/p5_win32_sample"

$violations=@()

Get-ChildItem $p5dir -File -Include "*.h","*.cpp" -Recurse |
    ForEach-Object {
        $t=Get-Content $_.FullName -Raw

        if($t -match "core/d3d11_device\.h"){
            $violations += $_.Name
        }
    }

if($violations.Count -ne 0){
    throw ("P5_INTERNAL_INCLUDE=" + ($violations -join ","))
}

Write-Output "P8_SAMPLE_MATURITY=PASS"
