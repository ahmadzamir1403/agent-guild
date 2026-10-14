# Security and privacy

Agent Guild is an experimental Windows desktop monitor. Source review and
tests reduce risk; they are not a security certification or a guarantee that
all vulnerabilities have been found.

## Runtime access

- Connects only to the local AF_UNIX socket selected by CODEX_HOME (or the
  current user's .codex directory). It opens no TCP listener and makes no
  external network requests.
- Sends initialize/initialized, thread/loaded/list, and thread/read with
  includeTurns:false; protocol ping replies are the only other output.
- Never starts turns, approves actions, resumes threads, edits Codex settings,
  installs hooks, reads API keys, or starts the Codex daemon.
- Thread summaries can include private metadata such as titles, project paths,
  and preview text. The app displays titles and folder names, discards preview
  text, and does not request turn history. Metadata lives in process memory.
- Saves thread IDs and game statistics to
  %LOCALAPPDATA%\AgentGuild\progress.json using a temporary file and atomic
  replacement. These IDs can be identifying; treat the save as private.

The local daemon and the Windows user account are trust boundaries. This app
does not sandbox or protect against another program running as the same user.
A custom CODEX_HOME must refer to a location you trust. No elevation is needed.

## Defensive handling

WebSocket responses are limited to 2 MiB, JSON to 65,536 tokens, loaded agents
to 512, and persisted heroes to 1,024. Socket operations have deadlines, client
frames are masked, the handshake accept value is checked, and failed snapshots
show disconnected status. Save input is bounded and upgrade tiers are clamped.
The optional dwmapi.dll is loaded from the Windows system directory.

## Diagnostics and downloads

--probe writes counts to probe.json AND a screenshot to preview.bmp beside the
executable. The screenshot can expose task titles and project names. Review it
before sharing. Diagnostics, saves, and local research scripts are excluded
from source control and release packages.

Download releases only from this repository, compare SHA256SUMS.txt, or build
from source. A matching checksum verifies consistency with the published
artifact; it is not a malware scan or a digital signature. Releases are unsigned.
Do not disable Windows Defender or SmartScreen; build from source if Windows
blocks the downloaded executable.

## Report a vulnerability

Use the repository's Security tab / private vulnerability reporting when
available. Public issues should contain only non-sensitive reproduction steps.
Never upload Codex credentials, transcripts, real task screenshots, or saves.
