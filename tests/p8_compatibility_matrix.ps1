param(
    [Parameter(Mandatory=$true)][string]$SourceDir,
    [Parameter(Mandatory=$true)][string]$BuildDir
)

$ErrorActionPreference="Stop"

$doc=Join-Path $SourceDir "docs/COMPATIBILITY.md"

if(!(Test-Path $doc)){
    throw "COMPATIBILITY_DOC_MISSING"
}

$text=Get-Content $doc -Raw

foreach($needle in @(
    "VALIDATED",
    "SUPPORTED BY DESIGN BUT NOT VALIDATED",
    "NOT TESTED",
    "NOT SUPPORTED"
)){
    if($text -notmatch [regex]::Escape($needle)){
        throw ("VOCAB_MISSING=" + $needle)
    }
}

foreach($needle in @(
    "Motion managed boundary | NOT EXPOSED",
    "Cross-monitor DPI transition | NOT TESTED",
    "Release | NOT TESTED"
)){
    if($text -notmatch [regex]::Escape($needle)){
        throw ("FACT_MISSING=" + $needle)
    }
}

# Both managed motion boundaries must appear.
$motion=([regex]::Matches($text,"Motion managed boundary | NOT EXPOSED")).Count
if($motion -lt 2){
    throw ("MOTION_BOUNDARY_COUNT=" + $motion)
}

Write-Output "P8_COMPATIBILITY_MATRIX=PASS"
