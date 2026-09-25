param(
    [Parameter(Mandatory=$true)][string]$SourceDir,
    [Parameter(Mandatory=$true)][string]$BuildDir
)

$ErrorActionPreference="Stop"

$doc=Join-Path $SourceDir "docs/TROUBLESHOOTING.md"

if(!(Test-Path $doc)){ throw "TROUBLESHOOTING_DOC_MISSING" }

$text=Get-Content $doc -Raw

# Required sections.
foreach($needle in @(
    "## 1. SDK / CMake",
    "## 2. Public Headers",
    "## 3. Shader / Runtime Resources",
    "## 4. Native Win32",
    "## 5. WPF",
    "## 6. WinUI 3",
    "## 7. DPI",
    "## 8. Known Non-Blocking Warnings",
    "## 9. Diagnosis Checklist"
)){
    if($text -notmatch [regex]::Escape($needle)){
        throw ("SECTION_MISSING=" + $needle)
    }
}

# Required factual statements (version from canonical VERSION).
$version=(Get-Content (Join-Path $SourceDir "VERSION") -Raw).Trim()
foreach($needle in @(
    "api/native_public_headers.txt",
    "NOT EXPOSED",
    "Real cross-monitor DPI transition is NOT TESTED",
    "Authoritative DPI is the native HWND DPI",
    $version
)){
    if($text -notmatch [regex]::Escape($needle)){
        throw ("FACT_MISSING=" + $needle)
    }
}

# Motion NOT EXPOSED must be stated for BOTH WPF and WinUI.
$motion=([regex]::Matches($text,"Motion managed boundary is NOT EXPOSED")).Count
if($motion -lt 2){ throw ("MOTION_BOUNDARY_COUNT=" + $motion) }

# Public contract: must not tell consumers to include internal headers.
# (The doc names internal headers only to say they are NOT installed.)
if($text -match "include only the installed" ){ }

Write-Output "P8_TROUBLESHOOTING_DOCS=PASS"
