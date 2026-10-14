# Official archive, pinned to the compiler tested by this project.
$ErrorActionPreference = 'Stop'
$taskTools = Join-Path $PSScriptRoot 'tools'
$taskCompiler = Join-Path $taskTools 'zig-x86_64-windows-0.15.2\zig.exe'
if (Test-Path -LiteralPath $taskCompiler) {
    Write-Output "Using existing toolchain: $taskCompiler"
    exit 0
}
New-Item -ItemType Directory -Force -Path $taskTools | Out-Null
$taskArchive = Join-Path $taskTools 'zig-0.15.2.zip'
Invoke-WebRequest -Uri 'https://ziglang.org/download/0.15.2/zig-x86_64-windows-0.15.2.zip' -OutFile $taskArchive -UseBasicParsing
$taskExpected = '3a0ed1e8799a2f8ce2a6e6290a9ff22e6906f8227865911fb7ddedc3cc14cb0c'
if ((Get-FileHash -LiteralPath $taskArchive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $taskExpected) {
    throw 'Zig archive checksum mismatch. Archive was not extracted.'
}
Expand-Archive -LiteralPath $taskArchive -DestinationPath $taskTools -Force
Write-Output "Verified Zig 0.15.2: $taskCompiler"
