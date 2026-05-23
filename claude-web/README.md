# claude-web

Local web UI for Claude Code CLI. Browser ↔ FastAPI ↔ `claude-agent-sdk` ↔ `claude`.

## Install

Requires Python 3.11+ and the `claude` CLI on your `PATH`.

```bash
cd claude-web
python3 -m venv .venv
source .venv/bin/activate
pip install -e ".[dev]"
```

## Run

```bash
python server.py --cwd /path/to/your/project --port 8000
# open http://localhost:8000
```

Args:
- `--cwd` (required): the directory Claude Code operates in
- `--host` (default `127.0.0.1`)
- `--port` (default `8000`)

The latest session id is saved to `<cwd>/.claude-web/session.json` so the
conversation continues across page reloads and server restarts.

## Permissions

YOLO mode: every tool call (Read, Edit, Bash, …) is auto-approved. Only point
this at directories you'd trust Claude Code to act on freely.

## Display modes

Toggle "Card mode" in the header:
- Compact (default): each tool call is a one-line summary
- Card: each tool call is a collapsible card with full input/output

## Tests

```bash
PYTHONPATH=. pytest -v
```

## Manual acceptance checklist

1. Start server, open browser, status pill turns green
2. Ask "ls the src directory" → see Bash tool event + assistant summary
3. Reload the page → ask "what files did you just see?" → reply references prior turn (session resume working)
4. Toggle Card mode → existing and new tool events re-render correctly
5. Stop server while page is open → status pill turns red → restart server → pill goes green (auto-reconnect)
6. Click "New session" → confirm → next message starts fresh context
7. Ask Claude to run `false` in bash → tool event shows red error

---

## Cardputer client (`feat/cardputer-claude-client`)

M5Cardputer can act as a client to this server via the `POST /ask` endpoint.
Source: `src/claude_client*.{h,cpp}`, `src/claude_ui.{h,cpp}`, `src/main.cpp`.

### Build flags

The device firmware needs three env vars at build time:

```bash
export WIFI_SSID="your-wifi"
export WIFI_PASS="your-password"
export CLAUDE_SERVER="http://<laptop-LAN-IP>:8000"    # e.g. http://192.168.1.42:8000
make build      # or: pio run -e m5stack-cardputer
make upload     # flash
```

### Wire protocol

```
POST /ask  Content-Type: application/json
Body: {"text": "your prompt"}

Response: 200 text/plain; Transfer-Encoding: chunked
- Streams raw text chunks (no framing)
- Errors inlined as `\n[error: ...]\n`
- 400 if `text` is empty/missing/non-string
```

The Cardputer uses an independent session — stored at
`<cwd>/.claude-web/ask-session.json` — separate from the browser's
`session.json`. Two distinct conversations.

### Manual acceptance checklist

Run the server (`python server.py --cwd /path/to/project --port 8000`), then:

1. Power the Cardputer; press `Tab` → screen clears to Claude mode (input row `>` at bottom, WiFi dot bottom-right)
2. Type "say hi in 5 words" + Enter → spinner appears top-right → reply streams onto the screen
3. Ask "what did I just ask?" → reply references the prior turn (independent ask-session resume works)
4. Ask "tell me a 200 word story" → text scrolls automatically as it overflows
5. Mid-stream, press `Ctrl+C` → stream stops, what's already shown stays
6. Press `Tab` → returns to pet/weather; press `q` and `w` → original modes still work
7. With server stopped, press `Tab` and send a prompt → `[error] HTTP ...` red footer; press `Ctrl+C` to clear

### Host-side parsing tests

Pure-C++ helpers in `src/claude_client_parsing.{h,cpp}` are unit-tested on the
build host (macOS / Linux, plain g++):

```bash
cd tests/host && make
# expect: "OK: all parsing tests passed"
```

### Known limitations

- The Cardputer default font is ASCII-only; non-ASCII characters in replies render as `?`
- No persistent scrollback / history UI on the device
- Server address is baked at build time (no runtime config)
