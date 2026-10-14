param([string]$Compiler = '', [switch]$Test)
$ErrorActionPreference = 'Stop'
$taskRoot = $PSScriptRoot
if (-not $Compiler) {
    $taskZig = Get-ChildItem -LiteralPath (Join-Path $taskRoot 'tools') -Filter 'zig.exe' -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($taskZig) { $Compiler = $taskZig.FullName }
    else {
        $taskZigCommand = Get-Command zig -ErrorAction SilentlyContinue
        if ($taskZigCommand) { $Compiler = $taskZigCommand.Source }
        else { throw 'Run ./download-toolchain.ps1 or pass -Compiler with the full path to Zig 0.15.2. Zig is needed only to build.' }
    }
}
$taskVersion = (& $Compiler version | Out-String).Trim()
if ($LASTEXITCODE -ne 0 -or $taskVersion -ne '0.15.2') { throw "Expected Zig 0.15.2, got '$taskVersion'." }
$taskOutput = Join-Path $taskRoot 'AgentGuild.exe'
$taskResources = Join-Path $taskRoot 'AgentGuild.res'
& $Compiler rc /fo $taskResources /i $taskRoot (Join-Path $taskRoot 'AgentGuild.rc')
if ($LASTEXITCODE -ne 0) { throw "Resource compilation failed ($LASTEXITCODE)" }
& $Compiler cc -std=c99 -Os -s '-Wl,--subsystem,windows' '-Wl,--dynamicbase' '-Wl,--nxcompat' -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable (Join-Path $taskRoot 'main.c') $taskResources -o $taskOutput -luser32 -lgdi32 -lws2_32 -ladvapi32
if ($LASTEXITCODE -ne 0) { throw "Compilation failed ($LASTEXITCODE)" }
Get-Item -LiteralPath $taskOutput | Select-Object FullName, Length
if ($Test) {
    $taskProcess = Start-Process -FilePath $taskOutput -ArgumentList '--self-test' -PassThru -Wait -WindowStyle Hidden
    if ($taskProcess.ExitCode -ne 0) { throw "Self-test failed ($($taskProcess.ExitCode))" }
    Write-Output 'JSON/status, battle, economy, and save/reload self-tests passed.'
}
