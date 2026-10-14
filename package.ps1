param([string]$Version = '0.3.0')
$ErrorActionPreference = 'Stop'
if ($Version -notmatch '^\d+\.\d+\.\d+$') { throw 'Use a numeric major.minor.patch version.' }
$taskRoot = $PSScriptRoot
$taskOutput = Join-Path $taskRoot 'dist\release'
New-Item -ItemType Directory -Force -Path $taskOutput | Out-Null
# Explicit source allowlist: no recursive upload of the development folder.
$taskSourceFiles = @(
    '.gitignore', '.gitattributes', 'AGENTS.md', 'INSTALL.md', 'LICENSE',
    'README.md', 'SECURITY.md', 'THIRD-PARTY-NOTICES.txt',
    'main.c', 'model.h', 'transport.h', 'world.h', 'arena.h', 'economy.h',
    'jsmn.h', 'AgentGuild.rc', 'adventurer.ico', 'adventurer.png',
    'build.ps1', 'download-toolchain.ps1', 'package.ps1',
    'generate_icon.py', 'integration-research.md', 'test_transport.py',
    '.github\workflows\ci.yml'
)
$taskRuntimeFiles = @('AgentGuild.exe', 'README.md', 'INSTALL.md', 'LICENSE', 'SECURITY.md', 'THIRD-PARTY-NOTICES.txt')
foreach ($taskFile in ($taskSourceFiles + $taskRuntimeFiles)) {
    if (-not (Test-Path -LiteralPath (Join-Path $taskRoot $taskFile) -PathType Leaf)) { throw "Missing package input: $taskFile" }
}
$taskCheck = Start-Process -FilePath (Join-Path $taskRoot 'AgentGuild.exe') -ArgumentList '--self-test' -PassThru -Wait -WindowStyle Hidden
if ($taskCheck.ExitCode -ne 0) { throw "Release self-test failed: $($taskCheck.ExitCode)" }
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
function Write-GuildArchive([string]$Name, [string[]]$Files) {
    $taskPath = Join-Path $taskOutput $Name
    # Replace just this generated ZIP, preserving unrelated artifacts.
    $taskStream = [System.IO.File]::Open($taskPath, [System.IO.FileMode]::Create)
    $taskZip = [System.IO.Compression.ZipArchive]::new($taskStream, [System.IO.Compression.ZipArchiveMode]::Create, $false)
    try {
        foreach ($taskFile in $Files) {
            [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($taskZip, (Join-Path $taskRoot $taskFile), $taskFile.Replace('\', '/'), [System.IO.Compression.CompressionLevel]::Optimal) | Out-Null
        }
    } finally { $taskZip.Dispose(); $taskStream.Dispose() }
    return $taskPath
}
$taskRuntimeZip = Write-GuildArchive 'AgentGuild-windows-x64.zip' $taskRuntimeFiles
$taskSourceZip = Write-GuildArchive 'AgentGuild-source.zip' $taskSourceFiles
$taskChecksum = foreach ($taskFile in @($taskRuntimeZip, $taskSourceZip, (Join-Path $taskRoot 'AgentGuild.exe'))) {
    $taskHash = (Get-FileHash -LiteralPath $taskFile -Algorithm SHA256).Hash.ToLowerInvariant()
    "$taskHash  $([System.IO.Path]::GetFileName($taskFile))"
}
[System.IO.File]::WriteAllLines((Join-Path $taskOutput 'SHA256SUMS.txt'), $taskChecksum, [System.Text.UTF8Encoding]::new($false))
Write-Output "Release $Version archives and checksums: $taskOutput"
