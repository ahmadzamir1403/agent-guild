# Agent Guild: instructions for AI assistants

Agent Guild is a portable Windows desktop app written in C99 with Win32/GDI.
It observes a running local Codex daemon. It is not an agent skill, MCP server,
web app, npm package, or Python app.

## Install or troubleshoot

Read [INSTALL.md](INSTALL.md) for prerequisites, release verification, source
build commands, launch, troubleshooting, and uninstall. Finish installation
only after the executable self-test exits 0. A live connection requires Codex
to be running; disconnected status is not an installation failure.

## Develop

Use Zig 0.15.2. Run `./build.ps1 -Test` in PowerShell after C/header changes,
then `python test_transport.py` for transport changes. The Python fixture
uses an isolated socket and must never target a real Codex home. Run probes
against the real daemon only when the user asks for live diagnostics; the
resulting screenshot may reveal task titles and project names.

Keep the runtime read-only: initialization and `thread/loaded/list` +
`thread/read` with `includeTurns:false`. Preserve the distinction between
idle, waiting, failed, and disconnected. Game combat and gold are decorative.

Read [SECURITY.md](SECURITY.md) before changing transport, persistence,
diagnostics, or release packaging. Publish source from the explicit file list
in `package.ps1`; keep toolchains, diagnostics, local video output, and
credentials out of commits and release archives.
