# Agent Guild

A moving 2D RPG world and read-only Windows monitor for your local Codex agents.
Native C + Win32/GDI. Pixel adventurers are drawn with code. No browser, Electron,
WebView, Python runtime, background relay, or API key is needed to run the app.

## Install

Windows x64. Download the portable ZIP from [Releases](https://github.com/ahmadzamir1403/agent-guild/releases) and verify its checksum, or build from source.
See [INSTALL.md](INSTALL.md) for exact commands, troubleshooting, uninstall,
and a prompt you can give an AI assistant. AI assistants should begin with
[AGENTS.md](AGENTS.md).

Read [SECURITY.md](SECURITY.md) for privacy, diagnostics, and trust boundaries.
Agent Guild is an independent project and is not affiliated with OpenAI.

## Run

Double-click **AgentGuild.exe**. Keep Codex open. The app connects to the existing
Codex daemon and lists loaded agents and subagents. It never starts the daemon.

- **On a quest**: active turn with no waiting flag.
- **Needs your help**: waiting for approval or user input.
- **At camp**: loaded, idle thread. This does not prove successful completion.
- **Unknown / Error**: host-supplied state that needs inspection.

In **World**, working agents walk to random regions and battle Moss Munchers,
Twig Goblins, and Acorn Bandits. Hits, health bars, spell or sword effects, and
monster KOs are decorative game activity, not measurements of task completion.
Idle heroes wander around the campfire; agents waiting on you display `!` at camp.
Lost connections freeze the scene. The map displays up to 96 heroes.

Gold is scarce: 25% of monsters drop none, 55% drop one coin, 16% drop two, and
4% drop three. Heroes review purchases after several fights, keep a personal
savings reserve, wait 45-90 seconds between purchases, and sometimes decide to
keep saving. They aim for a preferred upgrade instead of buying any cheap option.
Swordsmen favor Power, mages favor Luck, and rangers favor Boots. Choices run
locally without making AI requests; eligible shopping reviews have a 35-55% buy
chance. Existing wallets and upgrades survive balance changes.

- **Power**: +1 damage per tier.
- **Boots**: +12% movement speed per tier.
- **Luck**: +12% chance of one bonus coin per monster per tier.

Upgrades cost 20, 45, 85, then 140 gold; each stat caps at tier 4. 10% of monsters drop
12-second power-ups: **Fury** (+2 damage), **Haste** (faster movement and attacks),
and **Fortune** (double gold). Power-ups activate automatically when collected.
Click a hero to see its wallet, tiers, shopping choice, and active power-up.

Wallets, permanent upgrades, and monster KOs are saved per Codex thread ID in
`%LOCALAPPDATA%\AgentGuild\progress.json`. Only game stats and IDs are stored there.
The game saves on changes at most once per 15 seconds and when closing normally;
temporary buffs are not saved. Starting a new Codex thread creates a new hero.
Up to 1,024 hero records are retained. Launching the app again focuses its existing
window, keeping one game instance per Windows session.

Click a hero or party card to inspect it. Scroll the party cards with the mouse
wheel or up/down arrows. Choose **Roster** for the original status list and filters.
Click **Pause** or press **Space** to pause/resume animation independently of live
status polling. Click Refresh or press **R** to refresh immediately.
Polling runs every two seconds and slows to ten seconds when minimized.
Animation uses 20 FPS while agents work and 10 FPS at camp. It stops when minimized,
paused, or in Roster. Static scenery and the drawing buffer are cached.
Disconnected counts show `--`; any displayed rows are explicitly last-known states.

Coverage is the current local daemon, not every machine or agent product. Up to
512 loaded agents are supported; the UI identifies limited coverage if exceeded.
No turn history is requested. Thread summaries may include prompt previews;
the app discards those previews. Task titles and project folder names appear
in memory and on screen.

## Requirements and integration

- Windows 10 version 1803 or newer / Windows 11 with Winsock AF_UNIX support.
- A running local Codex daemon with the installed app-server protocol.
- Verified here with Codex CLI/app-server **0.162.1** on 13 October 2026.

Socket discovery uses `CODEX_HOME` if set; otherwise `%USERPROFILE%\.codex`, then
`app-server-control\app-server-control.sock`. Custom daemon socket locations are
not supported by this initial UI. Long paths exceeding the AF_UNIX pathname limit
cannot connect. The protocol is experimental and may change with Codex releases.

The client verifies the WebSocket handshake, masks client frames, handles ping/pong
and fragmented text, bounds messages, and reconnects after failures. It polls
`thread/loaded/list`, then `thread/read` with `includeTurns:false`. Listing stored
threads alone missed a live subagent during research. No `turn/start`, resume,
approval response, hook installation, settings edit, or cloud inference is issued.

See [integration-research.md](integration-research.md) for verified evidence and
official-source links. Research helpers and generated schemas are development
artifacts, not runtime dependencies.

## Build

The source is `main.c`, `model.h`, `transport.h`, `world.h`, `arena.h`, `economy.h`, and the
MIT-licensed `jsmn.h`.
Build with a Windows Zig C toolchain:

```powershell
.\build.ps1 -Compiler 'C:\path\to\zig.exe' -Test
```

The compiler is needed only to build. Windows system DLLs provide the UI,
networking, cryptography, and C runtime. Local compiler downloads are excluded from source control.
Run `./package.ps1` after building to produce clean release archives in `dist/release`;
the executable has no additional app runtime dependencies.

The adventurer icon is embedded in the executable for File Explorer, the window,
and the taskbar. `adventurer.png` is the transparent 256-pixel image, and
`adventurer.ico` contains seven crisp sizes from 16 to 256 pixels. The checked-in
assets build without Python; `generate_icon.py` is an optional development helper
using Pillow to regenerate them from the game's existing pixel character style.

`AgentGuild.exe --self-test` exits with zero when JSON/Unicode/status and simulation
tests pass. The world tests cover travel, attacks, monster KOs, waiting/idle agents
never attacking, stopping combat when status changes, connection loss, and removal.
Economy tests verify affordable purchases, preventing overspending, maximum tiers,
bounded save data, and atomic save/reload using an isolated temporary fixture.
`AgentGuild.exe --probe` checks the live connection and writes `probe.json` with
counts only, plus `preview.bmp` rendering the actual current UI. This screenshot may
contain private task titles and project names; review it before sharing. A failed connection
returns exit code 20. Diagnostic files are written beside the executable.

Development verification also exercises a separate local socket fixture with
fragmented messages, interleaved ping/pong, Unicode names, waiting flags, masked
client frames, a read-only method whitelist, and an unavailable host. Run
`test_transport.py` with Python only when developing; it is not an app dependency.

jsmn copyright and MIT license are retained in its header. It comes from
[zserge/jsmn](https://github.com/zserge/jsmn).

## License

MIT. See [LICENSE](LICENSE) and [THIRD-PARTY-NOTICES.txt](THIRD-PARTY-NOTICES.txt).
