param(
    [Parameter(Mandatory=$true)][string]$SourceDir,
    [Parameter(Mandatory=$true)][string]$BuildDir
)

$ErrorActionPreference="Stop"

$doc=Join-Path $SourceDir "docs/UPGRADING.md"
if(!(Test-Path $doc)){ throw "UPGRADING_DOC_MISSING" }
$text=Get-Content $doc -Raw

# Required sections.
foreach($needle in @(
    "## Current version status",
    "## Version source",
    "## What to check when upgrading",
    "## Native consumers",
    "## WPF",
    "## WinUI 3",
    "## Public API baseline",
    "## Pre-1.0 warning"
)){
    if($text -notmatch [regex]::Escape($needle)){ throw ("SECTION_MISSING=" + $needle) }
}

# Current version / pre-1.0 status must be accurate.
if($text -notmatch [regex]::Escape("SDK version: **0.8.0**")){ throw "VERSION_STATUS_MISSING" }
if($text -notmatch [regex]::Escape("Status: **pre-1.0**")){ throw "PRE1_STATUS_MISSING" }
if($text -match [regex]::Escape("1.0 stable")){ throw "FALSE_STABLE_CLAIM" }

# Version source.
if($text -notmatch [regex]::Escape("VERSION")){ throw "VERSION_SOURCE_MISSING" }

# Public API baseline manifest names.
foreach($m in @(
    "api/native_public_headers.txt",
    "api/wpf_public_api.txt",
    "api/winui_public_api.txt",
    "api/wpf_interop_exports.txt",
    "api/winui_interop_exports.txt"
)){
    if($text -notmatch [regex]::Escape($m)){ throw ("MANIFEST_MISSING=" + $m) }
}

# Upgrade checklist present.
foreach($c in @(
    "refresh the installed SDK stage",
    "replace the native runtime DLLs",
    "replace the managed assemblies",
    "replace shaders / runtime resources",
    "rerun an integration smoke test"
)){
    if($text -notmatch [regex]::Escape($c)){ throw ("CHECKLIST_MISSING=" + $c) }
}

# No fabricated previous release.
if($text -match "0\.7\.0"){ throw "FABRICATED_PRIOR_RELEASE" }
if($text -notmatch [regex]::Escape("first formally versioned")){ throw "FIRST_BASELINE_STMT_MISSING" }

# WPF / WinUI matched-pair guidance.
foreach($n in @("AuroraGlassWpfInterop.dll","AuroraGlassWinUIInterop.dll","Microsoft.WindowsAppSDK 2.5.1")){
    if($text -notmatch [regex]::Escape($n)){ throw ("PAIR_GUIDANCE_MISSING=" + $n) }
}

Write-Output "P8_UPGRADE_NOTES=PASS"
