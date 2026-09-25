param(
    [Parameter(Mandatory=$true)][string]$SourceDir,
    [Parameter(Mandatory=$true)][string]$BuildDir
)

$ErrorActionPreference="Stop"

$doc=Join-Path $SourceDir "docs/BENCHMARKS.md"

if(!(Test-Path $doc)){ throw "BENCHMARK_DOC_MISSING" }

$text=Get-Content $doc -Raw

# Required sections.
foreach($needle in @(
    "## Purpose",
    "## Environment",
    "## Methodology",
    "## Benchmarks",
    "## Results",
    "## Interpretation",
    "## Limitations",
    "## How to reproduce"
)){
    if($text -notmatch [regex]::Escape($needle)){
        throw ("SECTION_MISSING=" + $needle)
    }
}

# Honest-benchmark requirements.
foreach($needle in @(
    "frame-rate guarantee",
    "NOT a rendering throughput",
    "This is isolated GPU render throughput, NOT a real application FPS",
    "single machine"
)){
    if($text -notmatch [regex]::Escape($needle)){
        throw ("HONESTY_MISSING=" + $needle)
    }
}

# P2 stress must be explicitly framed as NOT a throughput benchmark.
if($text -notmatch [regex]::Escape("It is NOT a rendering throughput")){
    throw "P2_STRESS_FRAMING_MISSING"
}

# The benchmark harness source must exist.
$harness=Join-Path $SourceDir "tests/p8_benchmark/main.cpp"
if(!(Test-Path $harness)){ throw "BENCHMARK_HARNESS_MISSING" }

# Environment must record build config / SDK version (from canonical VERSION).
$version=(Get-Content (Join-Path $SourceDir "VERSION") -Raw).Trim()
foreach($needle in @("SDK version", $version)){
    if($text -notmatch [regex]::Escape($needle)){
        throw ("ENV_MISSING=" + $needle)
    }
}

Write-Output "P8_BENCHMARK_DOCS=PASS"
