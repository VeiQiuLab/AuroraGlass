param(
    [Parameter(Mandatory=$true)][string]$SourceDir,
    [Parameter(Mandatory=$true)][string]$BuildDir,
    [string]$Config = "Debug"
)

$ErrorActionPreference="Stop"

$stage=Join-Path $env:TEMP ("AuroraGlass-P8-WinUI-Stage-" + $PID)
$work=Join-Path $env:TEMP ("AuroraGlass-P8-WinUI-Consumer-" + $PID)

foreach($p in @($stage,$work)){
    if(Test-Path $p){
        Remove-Item $p -Recurse -Force
    }
}

& cmake --install $BuildDir --config $Config --prefix $stage
if($LASTEXITCODE){throw "INSTALL_FAIL"}

Copy-Item (Join-Path $SourceDir "tests/p8_consumers/winui") $work -Recurse

New-Item -ItemType Directory -Force (Join-Path $work "sdk/shaders") | Out-Null

Copy-Item (Join-Path $stage "managed/WinUI/$Config/AuroraGlass.WinUI.dll") (Join-Path $work "sdk/")
Copy-Item (Join-Path $stage "bin/$Config/AuroraGlassWinUIInterop.dll") (Join-Path $work "sdk/")
Copy-Item (Join-Path $stage "bin/$Config/AuroraGlassWpfInterop.dll") (Join-Path $work "sdk/")
Copy-Item (Join-Path $stage "shaders/*") (Join-Path $work "sdk/shaders/")

$proj=Join-Path $work "P8FreshWinUI.csproj"

& dotnet build $proj -c $Config --nologo
if($LASTEXITCODE){throw "BUILD_FAIL"}

$exe=Get-ChildItem (Join-Path $work "bin") -Recurse -File -Filter "P8FreshWinUI.exe" |
    Select-Object -First 1

if(!$exe){throw "EXE_MISSING"}

$result=Join-Path $exe.DirectoryName "p8-result.txt"
Remove-Item $result -Force -ErrorAction SilentlyContinue

$p=Start-Process -FilePath $exe.FullName -WorkingDirectory $exe.DirectoryName -PassThru

if(!$p.WaitForExit(20000)){
    $p.Kill()
    $p.WaitForExit()
    throw "RUNTIME_TIMEOUT"
}

$p.Refresh()

if(!(Test-Path $result)){throw "RESULT_MISSING"}

$content=Get-Content $result -Raw

if($content -notmatch "ATTACHED=False"){throw "TEARDOWN_FAIL"}
if($content -notmatch "EXIT=0"){throw "RENDER_FAIL"}
if($p.ExitCode -ne 0){throw ("PROCESS_FAIL=" + $p.ExitCode)}

Write-Output "P8_FRESH_WINUI_CONSUMER=PASS"

Remove-Item $stage -Recurse -Force
Remove-Item $work -Recurse -Force