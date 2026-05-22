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
