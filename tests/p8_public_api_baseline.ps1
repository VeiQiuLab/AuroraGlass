param([string]$SourceDir,[string]$BuildDir)
$ErrorActionPreference="Stop"
function A([bool]$ok,[string]$why){if(!$ok){throw $why}}
$version=(Get-Content (Join-Path $SourceDir "VERSION") -Raw).Trim()
$header=Join-Path $BuildDir "generated\auroraglass\version.h"
A (Test-Path $header) "VERSION_HEADER_MISSING"
$h=Get-Content $header -Raw
A ($h.Contains($version)) "VERSION_STRING_MISSING"
A ($h.Contains("GetAuroraGlassVersion")) "VERSION_API_MISSING"
$pf=[Environment]::GetFolderPath([Environment+SpecialFolder]::ProgramFilesX86)
$vw=Join-Path $pf "Microsoft Visual Studio\Installer\vswhere.exe"
$install=(& $vw -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
$vc=(Get-Content (Join-Path $install "VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt") -Raw).Trim()
$dumpbin=Join-Path $install ("VC\Tools\MSVC\"+$vc+"\bin\Hostx64\x64\dumpbin.exe")
function E([string]$dll){
 $raw=& $dumpbin /nologo /exports $dll 2>&1
 $names=@()
 foreach($line in $raw){
  $m=[regex]::Match([string]$line,"^\s*\d+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+(.+?)\s*$")
  if($m.Success){
   $n=$m.Groups[1].Value.Trim()
   if($n.Contains(" = ")){$n=($n -split "\s+=\s+")[0].Trim()}
   if($n){$names += $n}
  }
 }
 return @($names|Sort-Object -Unique)
}
foreach($entry in @(@("AuroraGlassWpfInterop.dll","api\wpf_interop_exports.txt"),@("AuroraGlassWinUIInterop.dll","api\winui_interop_exports.txt"))){
 $actual=@(E (Join-Path $BuildDir ("Debug\"+$entry[0])))
 $expected=@(Get-Content (Join-Path $SourceDir $entry[1])|Where-Object {$_}|Sort-Object -Unique)
 A (@(Compare-Object $expected $actual).Count -eq 0) ("EXPORT_DRIFT="+$entry[0])
}
$wpf=@(Get-ChildItem (Join-Path $BuildDir "managed") -Recurse -File -Filter "AuroraGlass.Wpf.dll"|Where-Object {$_.FullName -match "\\bin\\" -and $_.FullName -notmatch "\\obj\\"}|Sort-Object LastWriteTime -Descending)|Select-Object -First 1
$winui=@(Get-ChildItem (Join-Path $BuildDir "managed") -Recurse -File -Filter "AuroraGlass.WinUI.dll"|Where-Object {$_.FullName -match "\\bin\\" -and $_.FullName -notmatch "\\obj\\"}|Sort-Object LastWriteTime -Descending)|Select-Object -First 1
A ($null -ne $wpf) "WPF_RUNTIME_DLL_MISSING"
A ($null -ne $winui) "WINUI_RUNTIME_DLL_MISSING"
$tool=Join-Path $SourceDir "tests\p8_api_tool\P8.ApiTool.csproj"
& dotnet run --project $tool -c Debug --no-build -- verify $wpf.FullName (Join-Path $SourceDir "api\wpf_public_api.txt") $version "AuroraGlass.Wpf"
A ($LASTEXITCODE -eq 0) "WPF_PUBLIC_API_FAIL"
& dotnet run --project $tool -c Debug --no-build -- verify $winui.FullName (Join-Path $SourceDir "api\winui_public_api.txt") $version "AuroraGlass.WinUI"
A ($LASTEXITCODE -eq 0) "WINUI_PUBLIC_API_FAIL"
$headers=Get-Content (Join-Path $SourceDir "api\native_public_headers.txt")|Where-Object {$_}
$text=($headers|ForEach-Object {Get-Content (Join-Path $SourceDir $_) -Raw}) -join [Environment]::NewLine
foreach($n in @("GlassMaterial","GlassSurface","GlassButton","GlassToggle","GlassSlider","Win32HostAttachment","Win32ControlInputBridge","Win32HostMetrics")){
 A ($text.Contains($n)) ("NATIVE_API_MISSING="+$n)
}
Write-Output ("SDK_VERSION="+$version)
Write-Output "PUBLIC_API_BASELINE=PASS"
