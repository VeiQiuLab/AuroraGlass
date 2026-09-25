param(
    [Parameter(Mandatory=$true)][string]$SourceDir,
    [Parameter(Mandatory=$true)][string]$BuildDir
)

$ErrorActionPreference="Stop"

$doc=Join-Path $SourceDir "docs/VERSIONING.md"
if(!(Test-Path $doc)){ throw "VERSIONING_DOC_MISSING" }
$text=Get-Content $doc -Raw

# Required sections.
foreach($needle in @(
    "## 1. Version authority",
    "## 2. What counts as public contract",
    "## 3. PATCH change",
    "## 4. MINOR change",
    "## 5. MAJOR change",
    "## 6. Pre-1.0 policy",
    "## 7. ABI rules",
    "## 8. C++ ABI limitation",
    "## 9. Managed API rules",
    "## 10. Package contract",
    "## 11. Deprecation policy",
    "## 12. Version bump checklist"
)){
    if($text -notmatch [regex]::Escape($needle)){ throw ("SECTION_MISSING=" + $needle) }
}

# Single version authority.
if($text -notmatch [regex]::Escape("root ``VERSION`` file is the canonical")){ throw "VERSION_AUTHORITY_MISSING" }

# PATCH/MINOR/MAJOR rule statements.
if($text -notmatch [regex]::Escape("A PATCH MUST remain backward-compatible.")){ throw "PATCH_RULE_MISSING" }
if($text -notmatch [regex]::Escape("only at a MINOR boundary")){ throw "MINOR_BREAKING_RULE_MISSING" }
if($text -notmatch [regex]::Escape("MAJOR denotes a formally breaking public contract change")){ throw "MAJOR_RULE_MISSING" }

# Pre-1.0: breaking PATCH forbidden.
if($text -notmatch [regex]::Escape("Breaking PATCH is NOT allowed")){ throw "PATCH_BREAKING_FORBIDDEN_MISSING" }

# Public contract classification.
foreach($n in @("api/native_public_headers.txt","api/wpf_interop_exports.txt","api/winui_interop_exports.txt")){
    if($text -notmatch [regex]::Escape($n)){ throw ("CONTRACT_MISSING=" + $n) }
}
if($text -notmatch [regex]::Escape("build directory layout")){ throw "INTERNAL_CLASSIFICATION_MISSING" }

# Native ABI / C++ ABI limitation.
if($text -notmatch [regex]::Escape("changing calling convention")){ throw "ABI_BREAK_RULE_MISSING" }
if($text -notmatch [regex]::Escape("not guaranteed")){ throw "CPP_ABI_LIMITATION_MISSING" }

# Managed rules.
if($text -notmatch [regex]::Escape("remove a public class")){ throw "MANAGED_BREAK_MISSING" }
if($text -notmatch [regex]::Escape("add a new class")){ throw "MANAGED_NONBREAK_MISSING" }

# Package contract + deprecation.
if($text -notmatch [regex]::Escape("CMake target names")){ throw "PACKAGE_CONTRACT_MISSING" }
if($text -notmatch [regex]::Escape("deprecated before removal")){ throw "DEPRECATION_MISSING" }

# Bump checklist.
foreach($c in @("update the root ``VERSION`` only","run public API baseline checks","update upgrade notes if breaking")){
    if($text -notmatch [regex]::Escape($c)){ throw ("BUMP_CHECKLIST_MISSING=" + $c) }
}

Write-Output "P8_SEMVER_RULES=PASS"
