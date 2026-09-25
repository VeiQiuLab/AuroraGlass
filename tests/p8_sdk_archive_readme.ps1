param(
    [Parameter(Mandatory=$true)][string]$SourceDir,
    [Parameter(Mandatory=$true)][string]$BuildDir
)

$ErrorActionPreference="Stop"

# 1. Source README exists.
$srcDoc=Join-Path $SourceDir "docs/SDK_ARCHIVE_README.md"
if(!(Test-Path $srcDoc)){ throw "SDK_ARCHIVE_README_SOURCE_MISSING" }
$src=Get-Content $srcDoc -Raw

# 2. Required accuracy markers (version read from canonical VERSION).
$version=(Get-Content (Join-Path $SourceDir "VERSION") -Raw).Trim()
foreach($m in @("AuroraGlass",$version,"Windows","x64","Debug","Apache-2.0")){
    if($src -notmatch [regex]::Escape($m)){ throw ("SOURCE_MARKER_MISSING=" + $m) }
}

# 3. Must not contain fabricated claims / install commands.
foreach($bad in @("v1.0","NuGet install","dotnet add package","Install-Package")){
    if($src -match [regex]::Escape($bad)){ throw ("FABRICATED_CLAIM=" + $bad) }
}
if($src -match "production Release configuration"){ throw "FALSE_RELEASE_CLAIM" }

# 4. Installed SDK root README (only if a stage exists).
$stage=Join-Path $BuildDir ("sdk-stage/AuroraGlass-" + $version)
if(Test-Path $stage){
    $inst=Join-Path $stage "README.md"
    if(!(Test-Path $inst)){ throw "INSTALLED_README_MISSING" }
    $t=Get-Content $inst -Raw
    if($t -notmatch [regex]::Escape("AuroraGlass")){ throw "INSTALLED_README_CONTENT" }
}

# 5. VERSION is well-formed.
$parsed=$null
if(-not [version]::TryParse($version, [ref]$parsed)){ throw ("VERSION_MALFORMED=" + $version) }

Write-Output "P8_SDK_ARCHIVE_README=PASS"
