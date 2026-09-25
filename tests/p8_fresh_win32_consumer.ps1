param(
    [Parameter(Mandatory=$true)][string]$SourceDir,
    [Parameter(Mandatory=$true)][string]$BuildDir,
    [string]$Config = "Debug"
)

$ErrorActionPreference="Stop"

$stage=Join-Path $env:TEMP ("AuroraGlass-P8-Win32-Stage-" + $PID)
$work=Join-Path $env:TEMP ("AuroraGlass-P8-Win32-Consumer-" + $PID)

foreach($p in @($stage,$work)){
    if(Test-Path $p){Remove-Item $p -Recurse -Force}
}

& cmake --install $BuildDir --config $Config --prefix $stage
if($LASTEXITCODE){throw "INSTALL_FAIL"}

Copy-Item (Join-Path $SourceDir "tests/p8_consumers/win32") $work -Recurse

& cmake -S $work -B (Join-Path $work "build") "-DCMAKE_PREFIX_PATH=$stage"
if($LASTEXITCODE){throw "CONFIG_FAIL"}

& cmake --build (Join-Path $work "build") --config $Config
if($LASTEXITCODE){throw "BUILD_FAIL"}

$out=Join-Path $work "build/$Config"

Copy-Item (Join-Path $stage "shaders") (Join-Path $out "shaders") -Recurse -Force

$exe=Join-Path $out "P8FreshWin32.exe"

$p=Start-Process -FilePath $exe -WorkingDirectory $out -PassThru

if(!$p.WaitForExit(15000)){
    $p.Kill()
    $p.WaitForExit()
    throw "RUNTIME_TIMEOUT"
}

$p.Refresh()

if($p.ExitCode -ne 0){
    throw ("RUNTIME_FAIL=" + $p.ExitCode)
}

Write-Output "P8_FRESH_WIN32_CONSUMER=PASS"

Remove-Item $stage -Recurse -Force
Remove-Item $work -Recurse -Force