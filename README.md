# zedpet

A multi-mode toy for the M5Cardputer (ESP32-S3 + 240×135 TFT + QWERTY) that
also doubles as a Claude Code front-end:

- **Pet mode** — animated sprite, cycles state with `q`
- **Weather mode** — cycles forecast view with `w`
- **Claude mode** — full Claude Code chat over WiFi via the bundled `claude-web` server (`c` to toggle)

Plus a browser UI to the same Claude Code session, for when you'd rather use a
laptop screen than a 240×135 panel.

---

## Components

```
zedpet/
├── claude-web/        Python (FastAPI) server: wraps `claude` CLI, serves
│                      browser UI on /ws and Cardputer endpoint on /ask
├── src/               Cardputer firmware (Arduino/PlatformIO, ESP32-S3)
├── host/              Misc Python helpers
├── tests/host/        g++ unit tests for pure-C++ helpers
└── docs/superpowers/  Specs and implementation plans
```

---

## Prerequisites

| For | You need |
|---|---|
| Server (always) | Python ≥ 3.11, [`claude` CLI](https://docs.claude.com/claude-code) on PATH |
| Browser UI | a modern browser, that's it |
| Cardputer client | M5Stack Cardputer, [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html), USB-C cable, WiFi |

---

## 1. Start the server

```bash
cd claude-web
python3 -m venv .venv
source .venv/bin/activate
pip install -e ".[dev]"

python server.py --cwd /path/to/your/project --port 8000
```

Arguments:
- `--cwd` (required): the directory `claude` operates in (read/edit/bash all happen here)
- `--port` (default 8000), `--host` (default 127.0.0.1)

The server runs `claude` as a child process per turn. Two **independent**
sessions are persisted side-by-side inside `<cwd>/.claude-web/`:
- `session.json` — used by the browser (`/ws`)
- `ask-session.json` — used by the Cardputer (`/ask`)

Each conversation carries on across page reloads, server restarts, and reboots
until you delete the corresponding `*.json` (or click "New session" in the browser).

---

## 2. Use the browser UI

Open `http://localhost:8000/`.

| Action | Key / Click |
|---|---|
| Send | Enter (Shift+Enter for newline) |
| Toggle "Card mode" (rich tool view) | top-right checkbox |
| Start a fresh session | "New session" button |
| Disconnect handling | auto-reconnect with exponential backoff |

What you see streaming:
- Blue right-aligned bubbles = your prompts
- White left-aligned bubbles = Claude's reply
- Grey one-liners or expandable cards = each tool call (Read / Edit / Bash / …)
- Red lines = errors

---

## 3. Use the Cardputer

### 3.1 First-time setup

In a shell, export three env vars (or put them in your shell profile):

```bash
export WIFI_SSID="your-wifi"
export WIFI_PASS="your-password"
export CLAUDE_SERVER="http://192.168.1.42:8000"   # ← your laptop's LAN IP + the port
```

Find your laptop's LAN IP with `ipconfig getifaddr en0` (macOS) or
`ip addr show` (Linux). Use that, not `127.0.0.1` — the Cardputer needs to
reach you over WiFi.

### 3.2 Build + flash

```bash
make build     # or: pio run -e m5stack-cardputer
make upload    # plug in the Cardputer via USB-C first
```

`make upload` runs `pio run --target upload`; PlatformIO autodetects the
serial port. If it can't find it, uncomment the `upload_port` / `monitor_port`
lines in `platformio.ini` with your `/dev/cu.usbmodem*` path.

### 3.3 Daily use

| Key | Action |
|---|---|
| `c` (from any mode) | Enter / exit Claude mode |
| `q` (in pet/weather mode) | Cycle pet state |
| `w` (in pet/weather mode) | Cycle weather view |
| letters / digits (in Claude mode) | Type into the prompt row |
| `Enter` (in Claude mode) | Send the prompt |
| `Backspace` (in Claude mode) | Delete one character |
| `Ctrl + C` (in Claude mode) | Cancel current request OR clear an error |

Status indicators while in Claude mode:
- Top-right glyph: `|/-\` spinner while waiting/streaming, red `!` on error, nothing when idle
- Bottom-right 4×4 dot: green = WiFi up, red = WiFi down
- Bottom row: input area with blue `>` prompt

The reply area auto-scrolls to the latest output. Long replies trim from the
front (last ~3 KB kept) so memory stays bounded.

---

## 4. Common operations

**Switch to a different project directory** (server side):
Stop the server (`Ctrl+C`), restart it with a new `--cwd`. Sessions are stored
per cwd, so each project has its own conversation history.

**Wipe the browser conversation**: click "New session" in the header, or
`rm <cwd>/.claude-web/session.json` while the server is stopped.

**Wipe the Cardputer conversation**:
`rm <cwd>/.claude-web/ask-session.json` while the server is stopped.

**Make Cardputer talk to a different server**: change `CLAUDE_SERVER`,
re-run `make upload`. (Server address is baked in at build time.)

**Run all tests**:
```bash
# server-side
cd claude-web && source .venv/bin/activate && PYTHONPATH=. pytest -v

# Cardputer-side pure-C++ helpers (host)
cd tests/host && make
```

---

## 5. Troubleshooting

| Symptom | Likely cause / fix |
|---|---|
| Server: `Error: \`claude\` CLI not found in PATH` | Install Claude Code first; check `which claude` |
| Server: SDK exceptions on each turn | `claude` CLI version too old; update it |
| Browser: status pill stays yellow "connecting…" | Check server log; firewall or wrong port |
| Browser: every message says "session expired" | Stale `session.json` — the server auto-recovers; if persistent, delete the file |
| Cardputer: shows red WiFi dot forever | Wrong `WIFI_SSID/WIFI_PASS` baked in — re-export and `make upload` |
| Cardputer: `[error] HTTP -1` | Server unreachable — wrong `CLAUDE_SERVER` IP, or laptop firewall blocking port 8000 |
| Cardputer: `[error] HTTP 400` | Empty prompt sent (the firmware shouldn't allow this; if you see it, report) |
| Cardputer: reply shows `?` for some characters | Default Cardputer font is ASCII-only; emoji/CJK render as `?` |
| Cardputer: garbled characters at very high stream rate | Slow down the reply with a shorter prompt; this is an ESP32 read-buffer limit |

---

## 6. Architecture & design docs

For the why-behind-this:
- `docs/superpowers/specs/2026-05-23-claude-code-web-ui-design.md` — server + browser design
- `docs/superpowers/specs/2026-05-23-cardputer-claude-client-design.md` — Cardputer client design
- `docs/superpowers/plans/*.md` — implementation plans

For module-by-module read:
- `claude-web/README.md` — server internals + wire protocol
- `claude-web/server.py:build_app` — entry point with the two `ClaudeRunner` instances
- `src/main.cpp` — Cardputer mode dispatch
- `src/claude_ui.cpp` — Cardputer state machine + rendering
