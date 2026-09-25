param(
    [Parameter(Mandatory=$true)][string]$SourceDir,
    [Parameter(Mandatory=$true)][string]$BuildDir
)

$ErrorActionPreference="Stop"

# 1. Root LICENSE exists and is canonical Apache-2.0.
$license=Join-Path $SourceDir "LICENSE"
if(!(Test-Path $license)){ throw "ROOT_LICENSE_MISSING" }

$text=Get-Content $license -Raw

foreach($marker in @(
    "Apache License",
    "Version 2.0, January 2004",
    "TERMS AND CONDITIONS FOR USE, REPRODUCTION, AND DISTRIBUTION",
    "END OF TERMS AND CONDITIONS"
)){
    if($text -notmatch [regex]::Escape($marker)){ throw ("LICENSE_MARKER_MISSING=" + $marker) }
}

# 2. README declares Apache-2.0 and links LICENSE.
$readme=Get-Content (Join-Path $SourceDir "README.md") -Raw
if($readme -notmatch "Apache"){ throw "README_LICENSE_MISSING" }
if($readme -notmatch "Apache-2.0"){ throw "README_SPDX_MISSING" }

# 3. VERSION is a well-formed x.y.z (canonical source of truth).
$version=(Get-Content (Join-Path $SourceDir "VERSION") -Raw).Trim()
$parsed=$null
if(-not [version]::TryParse($version, [ref]$parsed)){ throw ("VERSION_MALFORMED=" + $version) }

# 4. Staged SDK includes LICENSE (only if a stage exists).
$stage=Join-Path $BuildDir ("sdk-stage/AuroraGlass-" + $version)
if(Test-Path $stage){
    if(!(Test-Path (Join-Path $stage "LICENSE"))){ throw "STAGE_LICENSE_MISSING" }
}

Write-Output "P8_LICENSE=PASS"
