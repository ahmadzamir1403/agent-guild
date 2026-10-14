# Codex live integration verified on Windows

Research and verification date: 2026-10-13. Installed CLI and existing app-server daemon: 0.162.1.

## Recommended implementation

Use one native Winsock AF_UNIX connection to the already running local Codex daemon. Perform its WebSocket handshake, then poll `thread/loaded/list` and read each returned ID with `thread/read` and `includeTurns:false`. This was verified against the current root task and this live research subagent. It needs no Python runtime, helper process, API key, settings edits, hook installation, or agent turn creation in the shipped C application. Python was only a development verification tool.

The local development experiments used `verify-websocket-native.py` and
`verify-websocket-proxy.py` (not distributed). Both succeeded during the original
verification. The published `test_transport.py` provides a reproducible isolated
transport fixture. Neither starts/stops the daemon or issues a turn, resume, approval decision, or settings write.

## Existing host and transport

Local CLI evidence:

- Binary: `%LOCALAPPDATA%\Programs\OpenAI\Codex\bin\codex.exe`.
- `codex --version`: `codex-cli 0.162.1`.
- `codex app-server daemon version`: `status:running`, `appServerVersion:0.162.1`, and `socketPath:%USERPROFILE%\.codex\app-server-control\app-server-control.sock`.
- `codex app-server proxy --help`: forwards stdio bytes to the existing socket, with optional `--sock` path. It is a raw tunnel, not a JSONL-to-WebSocket converter.

The native experiment used `WSAStartup(0x202)`, `socket(AF_UNIX=1, SOCK_STREAM=1, 0)`, and a 110-byte address containing a 2-byte family followed by a NUL-padded 108-byte UTF-8 path. It connected successfully. This Python installation lacks `socket.AF_UNIX`; direct `ws2_32` calls still work. Microsoft documents Windows AF_UNIX support through Winsock and UTF-8 pathname addressing: [Microsoft AF_UNIX announcement](https://devblogs.microsoft.com/commandline/af_unix-comes-to-windows/).

Send this HTTP request, with actual CRLF line endings and a fresh base64 encoding of 16 random bytes for the key:

```http
GET / HTTP/1.1
Host: localhost
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Key: <base64-random-16-bytes>
Sec-WebSocket-Version: 13

```

Observed response: `HTTP/1.1 101 Switching Protocols`. No credential was needed for this existing local socket. Validate the Upgrade response and `Sec-WebSocket-Accept` in the application. Implement partial reads/writes, bounded frame lengths, ping/pong, close, fragmentation, and client masking.

Official OpenAI documentation confirms Unix sockets use the standard HTTP WebSocket Upgrade; stdio is JSONL, and WebSocket messages use text frames. It also documents initialization, read-only thread reads, runtime status changes, loaded-thread listing, and source filters. WebSocket/app-server integration remains experimental: [Codex app-server](https://learn.chatgpt.com/docs/app-server), [developer commands](https://learn.chatgpt.com/docs/developer-commands).

## Verified request sequence

Each JSON object is a masked WebSocket text frame, not a newline-delimited packet on this transport. Do not include a `jsonrpc` field.

```json
{"id":1,"method":"initialize","params":{"clientInfo":{"name":"agent_guild","version":"0.1.0"}}}
```

Wait for the matching response, then:

```json
{"method":"initialized"}
{"id":2,"method":"thread/loaded/list","params":{}}
```

For each `result.data` ID:

```json
{"id":3,"method":"thread/read","params":{"threadId":"<returned-id>","includeTurns":false}}
```

Important installed-server behavior: omitting `params:{}` on `thread/loaded/list` returns code `-32600`, `Invalid request: missing field params`. Include it even though a documentation example omits it.

Anonymized response shape, verified locally:

```json
{"id":2,"result":{"data":["<root-id>","<subagent-id>"]}}
{"id":3,"result":{"thread":{"id":"<root-id>","source":"vscode","status":{"type":"active","activeFlags":[]}}}}
{"id":4,"result":{"thread":{"id":"<subagent-id>","parentThreadId":"<root-id>","source":{"subAgent":{"thread_spawn":{"parent_thread_id":"<root-id>","depth":1,"agent_path":"/root/integration_research","agent_nickname":"<nickname>","agent_role":null}}},"status":{"type":"active","activeFlags":[]}}}}
```

Responses contain more fields than this example. Keep only metadata needed by the UI. No conversation turn history was requested or printed during verification.

## Exact installed status and source schemas

The CLI generated version-matched schemas locally into `protocol-schema`:
`v2/ThreadListResponse.json`, `v2/ThreadListParams.json`, and `ClientRequest.json`.
These generated development artifacts are not distributed; the relevant shapes
are recorded below.

`ThreadStatus` variants:

```json
{"type":"notLoaded"}
{"type":"idle"}
{"type":"systemError"}
{"type":"active","activeFlags":[]}
```

`ThreadActiveFlag` enum: `waitingOnApproval`, `waitingOnUserInput`.

`SessionSource` accepts simple strings `cli`, `vscode`, `exec`, `appServer`, `unknown`; an object with `custom`; or an object with `subAgent`. Subagent sources may be string variants `review`, `compact`, `memory_consolidation`, an `other` object, or `thread_spawn` metadata as above. The latter requires `depth` and `parent_thread_id`; nickname, path, and role may be null. Thread summaries also expose `parentThreadId`, `agentNickname`, `agentRole`, `name`, `model`, `cwd`, `createdAt`, `updatedAt`, and `sessionId` in this installed schema. Prefer the direct summary fields, with source metadata as a fallback.

## Why loaded/list is essential

Observed on this task: loaded/list returned 2 IDs, and separate read calls showed both root and research subagent active. In the same connection, thread/list with all ten explicit source kinds plus `useStateDbOnly:true` returned 59 persisted records, only the root active; it omitted the live subagent. Therefore persisted listing alone misses some current agents. The successful loaded/list + read approach is the recommendation.

If implementing history later, explicit sourceKinds values are `cli`, `vscode`, `exec`, `appServer`, `subAgent`, `subAgentReview`, `subAgentCompact`, `subAgentThreadSpawn`, `subAgentOther`, `unknown`. Omitted or empty sourceKinds defaults to interactive sources in the installed schema. `useStateDbOnly:true` avoids rollout scan-and-repair behavior.

## Read-only observation and coverage limits

The installed stable ClientRequest schema has `thread/unsubscribe`, but no `thread/subscribe`. Official documentation says thread/read does not subscribe or load the thread. Do not resume agents merely to obtain subscriptions. Poll loaded/list approximately once per 1-2 seconds, read the current IDs, and replace the visible set only after a complete successful snapshot. Notifications received incidentally during verification were `remoteControl/status/changed` and `account/updated`; their bodies were not printed. Global delivery of thread/status/changed to a passive reader has not been verified, so do not rely on it without a separate check.

Runtime working status is confirmed from the same daemon's status fields. A successful snapshot can show idle or zero loaded agents; a failed socket or request means disconnected/unknown. A finished thread can remain loaded and idle; vanished loaded IDs do not prove successful completion. Elapsed working time may be measured locally from observed transitions; it is not a turn timestamp supplied by these summary reads. Show other Codex hosts as outside current coverage.

Hooks are a possible future cross-host adapter. They require configuration and exact-definition trust review; they cannot provide immediate observation without installation. Their lifecycle payload distinguishes parent session_id from agent_id, while transcript format is not stable. Keep hooks outside this first read-only version: [Codex hooks](https://learn.chatgpt.com/docs/hooks).

## Development artifact note

Generated schema bundles and Python verification scripts are development evidence only. Exclude them from the small executable distribution. The shipped program can link Windows system libraries and connect directly; the already installed Codex daemon is the observed service, not a monitor helper process.
