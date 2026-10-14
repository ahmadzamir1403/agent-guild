# Install Agent Guild

## Requirements

- Windows 10 version 1803 or newer, or Windows 11, on x64.
- A running local Codex daemon exposing the app-server control socket.
- No administrator access, API key, Node.js, Python, or compiler is needed to run.
- The local integration was originally verified with Codex 0.162.1 on
  13 October 2026. The protocol is experimental; other versions may differ.

## Option 1: portable release

1. Open [GitHub Releases](https://github.com/ahmadzamir1403/agent-guild/releases).
2. Download `AgentGuild-windows-x64.zip` and `SHA256SUMS.txt` from the same release.
3. Compare the ZIP hash with its entry in the checksum file:

   ```powershell
   Get-FileHash .\AgentGuild-windows-x64.zip -Algorithm SHA256
   Get-Content .\SHA256SUMS.txt
   ```

   Stop if the values differ. A checksum is not a signature or malware scan.
4. Extract into a folder you own:

   ```powershell
   Expand-Archive .\AgentGuild-windows-x64.zip -DestinationPath .\AgentGuild
   Set-Location .\AgentGuild
   $taskTest = Start-Process .\AgentGuild.exe -ArgumentList '--self-test' -Wait -PassThru -WindowStyle Hidden
   if ($taskTest.ExitCode -ne 0) { throw "Self-test failed: $($taskTest.ExitCode)" }
   Start-Process .\AgentGuild.exe
   ```

5. Keep Codex open and start a task. The app should show a live connection and
   loaded agents. A quiet camp means there may be no loaded tasks.

The app is portable: extraction is installation. It adds no startup entry,
service, browser extension, or Codex hook. Releases are unsigned; if Windows
blocks a download, keep security protections enabled and use a source build.

## Option 2: build from source

Git and PowerShell are needed for this route. The helper downloads the pinned
Zig 0.15.2 x64 compiler from ziglang.org and verifies its SHA-256 against
[Zig's official download index](https://ziglang.org/download/index.json).

```powershell
git clone https://github.com/ahmadzamir1403/agent-guild.git
Set-Location agent-guild
.\download-toolchain.ps1
.\build.ps1 -Test
Start-Process .\AgentGuild.exe
```

If Zig 0.15.2 is already installed:

```powershell
.\build.ps1 -Compiler 'C:\path\to\zig.exe' -Test
```

If PowerShell blocks local scripts, review the script first, then run just that
invocation with a process-scoped policy, for example
`powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Test`.
This does not change the machine's execution policy.

Installation is verified when the build succeeds and the self-test exits 0.
Connection verification additionally requires a running compatible Codex host.
The optional development test `python test_transport.py` requires Python 3
and uses an isolated local socket; Python is not needed to run the app.

## Help an AI assistant install it

Paste this into your coding assistant:

> Install Agent Guild from https://github.com/ahmadzamir1403/agent-guild.
> Read AGENTS.md and INSTALL.md first. Check this is Windows x64. Prefer a
> release ZIP and verify its SHA-256, or build with the pinned Zig toolchain.
> Run --self-test and check the exit code before launching. Run as my normal
> user. Keep Windows security protections enabled. Do not request API keys,
> alter Codex settings, or use --probe unless I ask for diagnostics.

## Troubleshooting

| Symptom | What to check |
| --- | --- |
| Connecting / disconnected | Open Codex. This app never starts its daemon. Confirm your Codex version exposes the control socket. |
| Custom Codex home | Set CODEX_HOME for this process before launching. The socket must be under app-server-control/app-server-control.sock. |
| Socket path too long | The UTF-8 socket path including its terminator must fit 108 bytes. Choose a shorter trusted Codex home. |
| Empty roster | Start a Codex task; only agents loaded on the current local daemon are shown. |
| Idle hero | Idle means loaded and at camp; it does not prove the task succeeded. |
| Build cannot replace executable | Close Agent Guild, then rebuild. |
| Compiler missing | Run download-toolchain.ps1 or supply -Compiler with Zig 0.15.2. |
| Release not available yet | Build from source. |

## Diagnostics and uninstall

`AgentGuild.exe --probe` writes counts to `probe.json` and a screenshot to
`preview.bmp` beside the executable. Screenshots may show private task titles
and project names. Exit 0 means connected; exit 20 means unavailable. This is
optional and is not needed to install.

To uninstall, close Agent Guild and delete its extracted folder. If you also
want to erase game progress, delete only `%LOCALAPPDATA%\AgentGuild`. Your
Codex installation and conversations are not modified.
