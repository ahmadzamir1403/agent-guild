"""Development-only integration test: fragmented WS, ping, passive RPC, disconnect.
Uses an isolated local socket; never touches the real Codex daemon.
"""
import base64
import ctypes
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import threading

ROOT = Path(__file__).resolve().parent
TEST_HOME = ROOT / "test-home"
CONTROL = TEST_HOME / "app-server-control"
CONTROL.mkdir(parents=True, exist_ok=True)
SOCKET_PATH = CONTROL / "app-server-control.sock"
assert len(str(SOCKET_PATH).encode()) < 108

ws = ctypes.WinDLL("ws2_32")
for name in ("socket", "accept"):
    getattr(ws, name).restype = ctypes.c_size_t
ws.socket.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int]
ws.accept.argtypes = [ctypes.c_size_t, ctypes.c_void_p, ctypes.c_void_p]
for name in ("bind", "connect"):
    getattr(ws, name).argtypes = [ctypes.c_size_t, ctypes.c_void_p, ctypes.c_int]
ws.listen.argtypes = [ctypes.c_size_t, ctypes.c_int]
ws.recv.argtypes = [ctypes.c_size_t, ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
ws.send.argtypes = [ctypes.c_size_t, ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
ws.closesocket.argtypes = [ctypes.c_size_t]
state = ctypes.create_string_buffer(512)
assert ws.WSAStartup(0x202, state) == 0
errors = []
methods = []

def read_exact(sock, n):
    out = bytearray()
    while len(out) < n:
        buf = ctypes.create_string_buffer(n - len(out))
        count = ws.recv(sock, buf, len(buf), 0)
        if count <= 0:
            raise EOFError("client closed")
        out += buf.raw[:count]
    return bytes(out)

def write(sock, raw):
    while raw:
        count = ws.send(sock, raw, len(raw), 0)
        assert count > 0
        raw = raw[count:]

def frame(sock, op, payload, fin=True):
    if isinstance(payload, str):
        payload = payload.encode()
    n = len(payload)
    header = bytes([(128 if fin else 0) | op])
    header += bytes([n]) if n < 126 else b"\x7e" + struct.pack("!H", n)
    write(sock, header + payload)

def client_frame(sock):
    a, b = read_exact(sock, 2)
    assert a & 128 and b & 128, "client frames must be final and masked"
    n = b & 127
    if n == 126:
        n = struct.unpack("!H", read_exact(sock, 2))[0]
    elif n == 127:
        n = struct.unpack("!Q", read_exact(sock, 8))[0]
    assert n < 65536
    mask = read_exact(sock, 4)
    raw = read_exact(sock, n)
    return a & 15, bytes(c ^ mask[i % 4] for i, c in enumerate(raw))

def server(listener):
    sock = ws.accept(listener, None, None)
    try:
        header = b""
        while not header.endswith(b"\r\n\r\n"):
            header += read_exact(sock, 1)
        key = next(line.split(b":", 1)[1].strip() for line in header.split(b"\r\n") if line.lower().startswith(b"sec-websocket-key:"))
        accept = base64.b64encode(hashlib.sha1(key + b"258EAFA5-E914-47DA-95CA-C5AB0DC85B11").digest())
        write(sock, b"HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " + accept + b"\r\n\r\n")
        while True:
            op, raw = client_frame(sock)
            assert op == 1
            request = json.loads(raw)
            method = request["method"]
            methods.append(method)
            assert method in {"initialize", "initialized", "thread/loaded/list", "thread/read"}, method
            if method == "initialized":
                continue
            if method == "initialize":
                result = {"userAgent": "test"}
            elif method == "thread/loaded/list":
                assert request["params"] == {}
                result = {"data": ["root", "child"]}
            else:
                assert request["params"]["includeTurns"] is False
                child = request["params"]["threadId"] == "child"
                result = {"thread": {"id": "child" if child else "root", "name": "Unicode \u4f60\u597d \U0001f680", "cwd": str(ROOT), "status": {"type": "active", "activeFlags": ["waitingOnUserInput"] if child else []}, "parentThreadId": "root" if child else None}}
            raw = json.dumps({"id": request["id"], "result": result}).encode()
            half = len(raw) // 2
            frame(sock, 1, raw[:half], False)
            frame(sock, 9, b"guild-ping")
            op, pong = client_frame(sock)
            assert op == 10 and pong == b"guild-ping"
            frame(sock, 0, raw[half:])
            if method == "thread/read" and request["params"]["threadId"] == "child":
                break
    except BaseException as error:
        errors.append(error)
    finally:
        ws.closesocket(sock)

if SOCKET_PATH.exists():
    SOCKET_PATH.unlink()
listener = ws.socket(1, 1, 0)
address = ctypes.create_string_buffer(struct.pack("<H", 1) + str(SOCKET_PATH).encode().ljust(108, b"\0"))
assert ws.bind(listener, address, 110) == 0, ws.WSAGetLastError()
assert ws.listen(listener, 1) == 0
thread = threading.Thread(target=server, args=(listener,), daemon=True)
thread.start()
env = dict(os.environ, CODEX_HOME=str(TEST_HOME))
try:
    result = subprocess.run([str(ROOT / "AgentGuild.exe"), "--probe"], env=env, timeout=20)
    thread.join(2)
    assert not errors, errors
    assert result.returncode == 0, result.returncode
    probe = json.loads((ROOT / "probe.json").read_text())
    assert probe["connected"] and probe["working"] == 1 and probe["waiting"] == 1 and probe["subagents"] == 1, probe
    (ROOT / "transport-test-results.json").write_text(json.dumps({"passed": True, "fragmentation": True, "ping_pong": True, "unicode": True, "read_only_methods": methods}, indent=2))
    print("PASS: fragmented messages, interleaved ping/pong, masked client frames, Unicode, waiting flags, read-only RPC.")
finally:
    ws.closesocket(listener)
    ws.WSACleanup()
    if SOCKET_PATH.exists():
        SOCKET_PATH.unlink()

# No listener at the isolated path now: a failure must never appear as idle.
result = subprocess.run([str(ROOT / "AgentGuild.exe"), "--probe"], env=env, timeout=20)
probe = json.loads((ROOT / "probe.json").read_text())
assert result.returncode == 20 and probe["connected"] is False, (result.returncode, probe)
print("PASS: unavailable host produces disconnected state and exit code 20.")
