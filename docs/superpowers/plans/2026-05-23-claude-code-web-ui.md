# Claude Code Web UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a local-only web UI for Claude Code CLI: browser ↔ FastAPI ↔ `claude-agent-sdk` ↔ `claude` binary, with single auto-resumed session, YOLO permissions, and toggleable tool display modes.

**Architecture:** Python FastAPI backend wraps `claude-agent-sdk` and exposes a WebSocket (`/ws`) for the rich web client and a reserved HTTP POST (`/ask`) for a future ESP32 client. Frontend is a single HTML file with Tailwind CDN and vanilla JS. Latest session_id is persisted to `.claude-web/session.json` for cross-restart continuity.

**Tech Stack:** Python 3.11+, FastAPI, uvicorn, `claude-agent-sdk`, pytest, anyio, Tailwind CDN, vanilla JS.

**Spec:** `docs/superpowers/specs/2026-05-23-claude-code-web-ui-design.md`

---

## File Structure

```
claude-web/
├── pyproject.toml              # deps: fastapi, uvicorn, claude-agent-sdk, anyio, pytest
├── .gitignore                  # __pycache__, .venv, .claude-web/
├── README.md                   # how to install + run + manual test checklist
├── server.py                   # FastAPI app: CLI args, routes, WebSocket handler
├── claude_runner.py            # ClaudeRunner class: wraps SDK, maps events, manages session
├── session_store.py            # load/save/clear session_id with atomic file write
├── static/
│   └── index.html              # full frontend (HTML + Tailwind CDN + vanilla JS)
└── tests/
    ├── __init__.py
    ├── test_session_store.py
    └── test_claude_runner.py
```

Runtime-only (gitignored):
```
.claude-web/session.json        # {"session_id": "...", "updated_at": "..."}
```

Each file has one job:
- `session_store.py`: filesystem persistence only, no SDK awareness
- `claude_runner.py`: SDK integration + event normalization; owns session lifecycle
- `server.py`: HTTP/WebSocket transport and CLI; delegates everything else
- `static/index.html`: rendering + UI state; transport-agnostic message shape

---

## Task 1: Project scaffold

**Files:**
- Create: `claude-web/pyproject.toml`
- Create: `claude-web/.gitignore`
- Create: `claude-web/static/` (empty dir)
- Create: `claude-web/tests/__init__.py` (empty file)

- [ ] **Step 1: Create directory structure**

Run:
```bash
mkdir -p claude-web/static claude-web/tests
touch claude-web/tests/__init__.py
```

- [ ] **Step 2: Write `claude-web/pyproject.toml`**

```toml
[project]
name = "claude-web"
version = "0.1.0"
description = "Local web UI for Claude Code CLI"
requires-python = ">=3.11"
dependencies = [
    "fastapi>=0.115",
    "uvicorn[standard]>=0.32",
    "claude-agent-sdk>=0.1.0",
    "anyio>=4.0",
]

[project.optional-dependencies]
dev = [
    "pytest>=8.0",
    "pytest-asyncio>=0.24",
]

[tool.pytest.ini_options]
asyncio_mode = "auto"
testpaths = ["tests"]
```

- [ ] **Step 3: Write `claude-web/.gitignore`**

```
__pycache__/
*.pyc
.venv/
.pytest_cache/
*.egg-info/
.claude-web/
```

- [ ] **Step 4: Create venv and install deps**

Run:
```bash
cd claude-web
python3 -m venv .venv
source .venv/bin/activate
pip install -e ".[dev]"
```

Expected: install succeeds. If `claude-agent-sdk` version pin fails, drop the `>=0.1.0` constraint and let pip resolve to whatever's published.

- [ ] **Step 5: Verify SDK import works**

Run from `claude-web/` with venv active:
```bash
python -c "from claude_agent_sdk import query, ClaudeAgentOptions; print('OK')"
```

Expected: prints `OK`. If import fails, check the SDK's actual public symbols (`pip show claude-agent-sdk` + `python -c "import claude_agent_sdk; print(dir(claude_agent_sdk))"`) and note any name differences for use in later tasks.

- [ ] **Step 6: Commit**

```bash
git add claude-web/pyproject.toml claude-web/.gitignore claude-web/tests/__init__.py
git commit -m "chore(claude-web): project scaffold"
```

---

## Task 2: `session_store.py` — atomic file persistence (TDD)

**Files:**
- Create: `claude-web/session_store.py`
- Create: `claude-web/tests/test_session_store.py`

- [ ] **Step 1: Write failing tests**

Write `claude-web/tests/test_session_store.py`:
```python
import json
from pathlib import Path

from session_store import SessionStore


def test_load_missing_file_returns_none(tmp_path: Path):
    store = SessionStore(tmp_path / "session.json")
    assert store.load() is None


def test_save_then_load_roundtrip(tmp_path: Path):
    store = SessionStore(tmp_path / "session.json")
    store.save("abc123")
    assert store.load() == "abc123"


def test_save_overwrites_previous(tmp_path: Path):
    store = SessionStore(tmp_path / "session.json")
    store.save("first")
    store.save("second")
    assert store.load() == "second"


def test_save_writes_updated_at(tmp_path: Path):
    path = tmp_path / "session.json"
    store = SessionStore(path)
    store.save("xyz")
    data = json.loads(path.read_text())
    assert data["session_id"] == "xyz"
    assert "updated_at" in data


def test_clear_removes_file(tmp_path: Path):
    path = tmp_path / "session.json"
    store = SessionStore(path)
    store.save("xyz")
    store.clear()
    assert not path.exists()
    assert store.load() is None


def test_clear_when_missing_is_noop(tmp_path: Path):
    store = SessionStore(tmp_path / "session.json")
    store.clear()  # must not raise


def test_atomic_write_preserves_old_on_crash(tmp_path: Path, monkeypatch):
    """If rename fails mid-save, the old file must remain intact."""
    path = tmp_path / "session.json"
    store = SessionStore(path)
    store.save("original")

    real_replace = Path.replace
    def boom(self, target):
        raise OSError("simulated crash")
    monkeypatch.setattr(Path, "replace", boom)

    try:
        store.save("new")
    except OSError:
        pass

    monkeypatch.setattr(Path, "replace", real_replace)
    assert store.load() == "original"


def test_load_corrupt_file_returns_none(tmp_path: Path):
    path = tmp_path / "session.json"
    path.write_text("not json")
    store = SessionStore(path)
    assert store.load() is None
```

- [ ] **Step 2: Run tests to verify they fail**

Run from `claude-web/`:
```bash
PYTHONPATH=. pytest tests/test_session_store.py -v
```
Expected: all 8 tests fail with `ModuleNotFoundError: No module named 'session_store'`.

- [ ] **Step 3: Write `claude-web/session_store.py`**

```python
"""Persistence of the latest Claude Code session_id."""
from __future__ import annotations

import json
import os
import tempfile
from datetime import datetime, timezone
from pathlib import Path


class SessionStore:
    def __init__(self, path: Path):
        self._path = Path(path)

    def load(self) -> str | None:
        if not self._path.exists():
            return None
        try:
            data = json.loads(self._path.read_text())
        except (json.JSONDecodeError, OSError):
            return None
        sid = data.get("session_id")
        return sid if isinstance(sid, str) and sid else None

    def save(self, session_id: str) -> None:
        self._path.parent.mkdir(parents=True, exist_ok=True)
        payload = {
            "session_id": session_id,
            "updated_at": datetime.now(timezone.utc).isoformat(),
        }
        fd, tmp_name = tempfile.mkstemp(
            prefix=".session-", suffix=".json.tmp", dir=str(self._path.parent)
        )
        tmp = Path(tmp_name)
        try:
            with os.fdopen(fd, "w") as f:
                json.dump(payload, f)
            tmp.replace(self._path)
        except Exception:
            if tmp.exists():
                tmp.unlink(missing_ok=True)
            raise

    def clear(self) -> None:
        try:
            self._path.unlink()
        except FileNotFoundError:
            pass
```

- [ ] **Step 4: Run tests to verify they pass**

Run:
```bash
PYTHONPATH=. pytest tests/test_session_store.py -v
```
Expected: all 8 pass.

- [ ] **Step 5: Commit**

```bash
git add claude-web/session_store.py claude-web/tests/test_session_store.py
git commit -m "feat(claude-web): session_store with atomic write"
```

---

## Task 3: `claude_runner.py` — event mapping (TDD, no SDK calls yet)

**Background:** `claude-agent-sdk` yields message objects (`AssistantMessage`, `UserMessage`, `ResultMessage`, etc.) whose `content` is a list of blocks (`TextBlock`, `ToolUseBlock`, `ToolResultBlock`, `ThinkingBlock`). We need a pure function that turns one SDK message into zero or more JSON dicts matching the wire protocol from the spec (`assistant_text`, `tool_use`, `tool_result`, `turn_end`).

**Files:**
- Create: `claude-web/claude_runner.py` (event mapping portion only)
- Create: `claude-web/tests/test_claude_runner.py`

- [ ] **Step 1: Write failing tests with fake message classes**

Write `claude-web/tests/test_claude_runner.py`:
```python
from dataclasses import dataclass, field
from typing import Any

from claude_runner import map_sdk_message


# Fakes that mimic the SDK's duck-typed shapes. The mapper looks at attributes,
# not isinstance checks, so these stand in for the real SDK classes.
@dataclass
class FakeTextBlock:
    text: str
    type: str = "text"


@dataclass
class FakeToolUseBlock:
    id: str
    name: str
    input: dict
    type: str = "tool_use"


@dataclass
class FakeToolResultBlock:
    tool_use_id: str
    content: Any
    is_error: bool = False
    type: str = "tool_result"


@dataclass
class FakeAssistantMessage:
    content: list = field(default_factory=list)
    type: str = "assistant"


@dataclass
class FakeUserMessage:
    content: list = field(default_factory=list)
    type: str = "user"


@dataclass
class FakeResultMessage:
    session_id: str
    usage: dict = field(default_factory=dict)
    type: str = "result"


def test_assistant_text_block_becomes_assistant_text_event():
    msg = FakeAssistantMessage(content=[FakeTextBlock(text="hello")])
    events = list(map_sdk_message(msg, message_id="m1"))
    assert events == [
        {"type": "assistant_text", "message_id": "m1", "text": "hello"}
    ]


def test_assistant_tool_use_block_becomes_tool_use_event():
    msg = FakeAssistantMessage(content=[
        FakeToolUseBlock(id="tu1", name="Bash", input={"cmd": "ls"}),
    ])
    events = list(map_sdk_message(msg, message_id="m1"))
    assert events == [
        {"type": "tool_use", "tool_use_id": "tu1", "name": "Bash", "input": {"cmd": "ls"}}
    ]


def test_assistant_mixed_blocks_produce_ordered_events():
    msg = FakeAssistantMessage(content=[
        FakeTextBlock(text="let me check"),
        FakeToolUseBlock(id="tu1", name="Read", input={"file": "a.txt"}),
        FakeTextBlock(text="done"),
    ])
    events = list(map_sdk_message(msg, message_id="m1"))
    assert [e["type"] for e in events] == ["assistant_text", "tool_use", "assistant_text"]


def test_user_message_with_tool_result_becomes_tool_result_event():
    msg = FakeUserMessage(content=[
        FakeToolResultBlock(tool_use_id="tu1", content="file contents"),
    ])
    events = list(map_sdk_message(msg, message_id="m2"))
    assert events == [
        {"type": "tool_result", "tool_use_id": "tu1", "content": "file contents", "is_error": False}
    ]


def test_tool_result_with_list_content_is_stringified():
    msg = FakeUserMessage(content=[
        FakeToolResultBlock(
            tool_use_id="tu1",
            content=[{"type": "text", "text": "part one"}, {"type": "text", "text": "part two"}],
        ),
    ])
    events = list(map_sdk_message(msg, message_id="m2"))
    assert events[0]["content"] == "part one\npart two"


def test_tool_result_error_flag_preserved():
    msg = FakeUserMessage(content=[
        FakeToolResultBlock(tool_use_id="tu1", content="boom", is_error=True),
    ])
    events = list(map_sdk_message(msg, message_id="m2"))
    assert events[0]["is_error"] is True


def test_user_message_with_only_text_is_ignored():
    """User text blocks are echoes of the prompt; the frontend already rendered them."""
    msg = FakeUserMessage(content=[FakeTextBlock(text="my question")])
    events = list(map_sdk_message(msg, message_id="m2"))
    assert events == []


def test_result_message_becomes_turn_end():
    msg = FakeResultMessage(session_id="sess-xyz", usage={"input_tokens": 10, "output_tokens": 5})
    events = list(map_sdk_message(msg, message_id="m3"))
    assert events == [
        {"type": "turn_end", "session_id": "sess-xyz", "usage": {"input_tokens": 10, "output_tokens": 5}}
    ]


def test_unknown_message_type_is_ignored():
    @dataclass
    class FakeSystem:
        type: str = "system"
    events = list(map_sdk_message(FakeSystem(), message_id="m4"))
    assert events == []
```

- [ ] **Step 2: Run tests to verify they fail**

Run:
```bash
PYTHONPATH=. pytest tests/test_claude_runner.py -v
```
Expected: all 9 fail with `ModuleNotFoundError`.

- [ ] **Step 3: Write `claude-web/claude_runner.py` (mapping only)**

```python
"""Wraps claude-agent-sdk and normalizes its events to the wire protocol."""
from __future__ import annotations

from typing import Any, Iterator


def _stringify_tool_result_content(content: Any) -> str:
    if isinstance(content, str):
        return content
    if isinstance(content, list):
        parts = []
        for block in content:
            if isinstance(block, dict) and block.get("type") == "text":
                parts.append(str(block.get("text", "")))
            elif hasattr(block, "text"):
                parts.append(str(block.text))
            else:
                parts.append(str(block))
        return "\n".join(parts)
    return str(content)


def map_sdk_message(msg: Any, message_id: str) -> Iterator[dict]:
    """Convert one SDK message into zero or more wire-protocol events.

    Uses attribute duck-typing so it stays decoupled from exact SDK class names.
    """
    msg_type = getattr(msg, "type", None)

    if msg_type == "assistant":
        for block in getattr(msg, "content", []) or []:
            block_type = getattr(block, "type", None)
            if block_type == "text":
                yield {
                    "type": "assistant_text",
                    "message_id": message_id,
                    "text": getattr(block, "text", ""),
                }
            elif block_type == "tool_use":
                yield {
                    "type": "tool_use",
                    "tool_use_id": getattr(block, "id", ""),
                    "name": getattr(block, "name", ""),
                    "input": getattr(block, "input", {}) or {},
                }
            # thinking blocks and others are intentionally dropped
        return

    if msg_type == "user":
        for block in getattr(msg, "content", []) or []:
            if getattr(block, "type", None) == "tool_result":
                yield {
                    "type": "tool_result",
                    "tool_use_id": getattr(block, "tool_use_id", ""),
                    "content": _stringify_tool_result_content(getattr(block, "content", "")),
                    "is_error": bool(getattr(block, "is_error", False)),
                }
        return

    if msg_type == "result":
        yield {
            "type": "turn_end",
            "session_id": getattr(msg, "session_id", ""),
            "usage": getattr(msg, "usage", {}) or {},
        }
```

- [ ] **Step 4: Run tests to verify they pass**

Run:
```bash
PYTHONPATH=. pytest tests/test_claude_runner.py -v
```
Expected: all 9 pass.

- [ ] **Step 5: Commit**

```bash
git add claude-web/claude_runner.py claude-web/tests/test_claude_runner.py
git commit -m "feat(claude-web): SDK event mapping with tests"
```

---

## Task 4: `claude_runner.py` — `ClaudeRunner` class with SDK integration

**Background:** Wrap the mapping in a class that owns the `SessionStore`, builds `ClaudeAgentOptions` with the resumed `session_id` and YOLO permissions, calls `query(...)`, and yields normalized events. Implements the resume-failure fallback from the spec.

This task involves the real SDK. The SDK's public API (the exact name of `permission_mode` values, `resume` parameter, and message class names) should be confirmed once at the start by inspecting the installed package; adjust the calls below if the names differ.

**Files:**
- Modify: `claude-web/claude_runner.py`

- [ ] **Step 1: Inspect the installed SDK to confirm public API**

Run from `claude-web/` with venv active:
```bash
python -c "
import claude_agent_sdk as s
print('symbols:', [n for n in dir(s) if not n.startswith('_')])
from claude_agent_sdk import ClaudeAgentOptions
import inspect
print('ClaudeAgentOptions fields:', inspect.signature(ClaudeAgentOptions))
"
```

Expected output names the actual constructor parameters. If `resume`, `permission_mode`, or `cwd` are spelled differently, use the actual names in the code below. Common alternatives to watch for: `session_id=` instead of `resume=`, `permissions=` instead of `permission_mode=`.

- [ ] **Step 2: Append `ClaudeRunner` class to `claude-web/claude_runner.py`**

Add this at the bottom of the existing file:
```python
import uuid
from pathlib import Path
from typing import AsyncIterator

from claude_agent_sdk import ClaudeAgentOptions, query as sdk_query

from session_store import SessionStore


class ClaudeRunner:
    """Owns the current session and exposes a single async query() iterator."""

    def __init__(self, cwd: Path, session_store: SessionStore):
        self._cwd = Path(cwd).resolve()
        self._store = session_store

    @property
    def cwd(self) -> Path:
        return self._cwd

    def current_session_id(self) -> str | None:
        return self._store.load()

    def reset_session(self) -> None:
        self._store.clear()

    async def query(self, prompt: str) -> AsyncIterator[dict]:
        """Yield wire-protocol events for one user turn.

        If a saved session_id exists, resumes it. If resume fails, clears the
        saved id and retries once as a fresh session, emitting an `error`
        event with `recoverable=True` so the frontend can show a hint.
        """
        async for ev in self._run_once(prompt, resume=self._store.load()):
            yield ev

    async def _run_once(self, prompt: str, resume: str | None) -> AsyncIterator[dict]:
        options = ClaudeAgentOptions(
            cwd=str(self._cwd),
            permission_mode="bypassPermissions",
        )
        if resume:
            # See Step 1 — adjust attribute name if SDK uses a different one.
            options.resume = resume

        message_id = uuid.uuid4().hex
        try:
            async for sdk_msg in sdk_query(prompt=prompt, options=options):
                for ev in map_sdk_message(sdk_msg, message_id=message_id):
                    if ev["type"] == "turn_end" and ev.get("session_id"):
                        self._store.save(ev["session_id"])
                    yield ev
        except Exception as e:  # noqa: BLE001
            if resume:
                # Resume failed: drop the bad id and retry once fresh.
                self._store.clear()
                yield {
                    "type": "error",
                    "message": f"resume failed ({e!r}); started a new session",
                    "recoverable": True,
                }
                async for ev in self._run_once(prompt, resume=None):
                    yield ev
            else:
                yield {"type": "error", "message": repr(e), "recoverable": False}
```

- [ ] **Step 3: Smoke-test the runner against a real `claude` binary**

Create `claude-web/scripts/smoke_runner.py` (temporary, will not be committed):
```python
import anyio
from pathlib import Path
from session_store import SessionStore
from claude_runner import ClaudeRunner

async def main():
    runner = ClaudeRunner(
        cwd=Path.cwd(),
        session_store=SessionStore(Path(".claude-web/session.json")),
    )
    async for ev in runner.query("Say hi in 5 words."):
        print(ev)

anyio.run(main)
```

Run from the **repo root** (so `cwd` points to `zedpet/`):
```bash
cd /Users/xd/java_proj/zedpet
PYTHONPATH=claude-web python claude-web/scripts/smoke_runner.py
```

Expected: a stream of dicts ending with one `{"type":"turn_end", "session_id": "..."}`. Verify `.claude-web/session.json` was created and contains that id. Then run the same command a second time and confirm Claude's reply acknowledges context from the first turn (e.g., `"as I said earlier..."`).

- [ ] **Step 4: Delete the smoke script**

```bash
rm -rf claude-web/scripts
```

- [ ] **Step 5: Commit**

```bash
git add claude-web/claude_runner.py
git commit -m "feat(claude-web): ClaudeRunner with session resume + fallback"
```

---

## Task 5: `server.py` — CLI, static serving, binary check

**Files:**
- Create: `claude-web/server.py`

- [ ] **Step 1: Write `claude-web/server.py`**

```python
"""FastAPI app: serves the web UI and bridges WebSocket to ClaudeRunner."""
from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path

import uvicorn
from fastapi import FastAPI
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles

from claude_runner import ClaudeRunner
from session_store import SessionStore


def build_app(cwd: Path) -> FastAPI:
    app = FastAPI(title="claude-web")
    static_dir = Path(__file__).parent / "static"

    runner = ClaudeRunner(
        cwd=cwd,
        session_store=SessionStore(cwd / ".claude-web" / "session.json"),
    )
    app.state.runner = runner

    app.mount("/static", StaticFiles(directory=static_dir), name="static")

    @app.get("/")
    def index():
        return FileResponse(static_dir / "index.html")

    return app


def parse_args(argv: list[str]) -> argparse.Namespace:
    p = argparse.ArgumentParser(prog="claude-web")
    p.add_argument("--cwd", required=True, type=Path,
                   help="Working directory Claude Code operates in")
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", default=8000, type=int)
    return p.parse_args(argv)


def check_claude_binary() -> None:
    if shutil.which("claude") is None:
        sys.stderr.write(
            "Error: `claude` CLI not found in PATH.\n"
            "Install Claude Code: https://docs.claude.com/claude-code\n"
        )
        sys.exit(1)


def main(argv: list[str] | None = None) -> None:
    args = parse_args(argv if argv is not None else sys.argv[1:])
    check_claude_binary()

    cwd = args.cwd.expanduser().resolve()
    if not cwd.is_dir():
        sys.stderr.write(f"Error: --cwd {cwd} is not a directory\n")
        sys.exit(1)

    print(f"claude-web: cwd={cwd}  http://{args.host}:{args.port}")
    uvicorn.run(build_app(cwd), host=args.host, port=args.port, log_level="info")


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: Create a stub `static/index.html` so `/` returns something**

Write `claude-web/static/index.html`:
```html
<!doctype html>
<html><body>placeholder — to be replaced in Task 9</body></html>
```

- [ ] **Step 3: Smoke-test the server**

In one terminal, from repo root:
```bash
cd /Users/xd/java_proj/zedpet/claude-web
source .venv/bin/activate
python server.py --cwd /Users/xd/java_proj/zedpet --port 8765
```
Expected: prints `claude-web: cwd=... http://127.0.0.1:8765` and uvicorn startup logs.

In another terminal:
```bash
curl -s http://127.0.0.1:8765/ | head -3
```
Expected: the placeholder HTML.

Stop the server with Ctrl+C.

- [ ] **Step 4: Verify CLI argument errors are clean**

Run:
```bash
python server.py --cwd /nonexistent/path
```
Expected: prints `Error: --cwd /nonexistent/path is not a directory` and exits non-zero.

- [ ] **Step 5: Commit**

```bash
git add claude-web/server.py claude-web/static/index.html
git commit -m "feat(claude-web): FastAPI server scaffold + CLI"
```

---

## Task 6: `server.py` — WebSocket `/ws` handler

**Files:**
- Modify: `claude-web/server.py`

- [ ] **Step 1: Add WebSocket handler to `build_app`**

In `claude-web/server.py`, add these imports at the top with the others:
```python
import json
from fastapi import WebSocket, WebSocketDisconnect
```

Then inside `build_app`, after the `index()` route, add:
```python
    @app.websocket("/ws")
    async def ws_endpoint(ws: WebSocket):
        await ws.accept()
        runner: ClaudeRunner = app.state.runner
        await ws.send_json({
            "type": "ready",
            "session_id": runner.current_session_id(),
            "cwd": str(runner.cwd),
        })
        try:
            while True:
                raw = await ws.receive_text()
                try:
                    msg = json.loads(raw)
                except json.JSONDecodeError:
                    await ws.send_json({"type": "error", "message": "invalid JSON"})
                    continue

                mtype = msg.get("type")
                if mtype == "user_message":
                    text = msg.get("text", "")
                    if not isinstance(text, str) or not text.strip():
                        await ws.send_json({"type": "error", "message": "empty prompt"})
                        continue
                    async for ev in runner.query(text):
                        await ws.send_json(ev)
                elif mtype == "new_session":
                    runner.reset_session()
                    await ws.send_json({
                        "type": "ready",
                        "session_id": None,
                        "cwd": str(runner.cwd),
                    })
                else:
                    await ws.send_json({
                        "type": "error",
                        "message": f"unknown message type: {mtype!r}",
                    })
        except WebSocketDisconnect:
            return
```

- [ ] **Step 2: Smoke-test the WebSocket with `websocat`**

If `websocat` not installed: `brew install websocat`.

Start the server:
```bash
cd /Users/xd/java_proj/zedpet/claude-web && source .venv/bin/activate
python server.py --cwd /Users/xd/java_proj/zedpet --port 8765
```

In another terminal:
```bash
echo '{"type":"user_message","text":"say hi in 5 words"}' | websocat ws://127.0.0.1:8765/ws
```

Expected: first a `{"type":"ready",...}` event, then several `assistant_text` events, possibly `tool_use`/`tool_result` events, ending with a `{"type":"turn_end","session_id":"..."}`.

Test bad input:
```bash
echo 'not json' | websocat ws://127.0.0.1:8765/ws
```
Expected: `ready` then `{"type":"error","message":"invalid JSON"}`, connection stays open.

- [ ] **Step 3: Commit**

```bash
git add claude-web/server.py
git commit -m "feat(claude-web): WebSocket /ws bridges runner events"
```

---

## Task 7: `server.py` — `/ask` placeholder endpoint for future ESP32 client

**Files:**
- Modify: `claude-web/server.py`

- [ ] **Step 1: Add the endpoint**

Add after the WebSocket handler in `build_app`:
```python
    @app.post("/ask")
    async def ask(payload: dict):
        """Synchronous text-in / text-out endpoint reserved for ESP32 client.

        MVP returns 501; the ESP32 integration will implement the body when needed.
        """
        from fastapi import HTTPException
        raise HTTPException(
            status_code=501,
            detail="Not implemented — reserved for ESP32 client integration",
        )
```

- [ ] **Step 2: Verify the endpoint responds 501**

With server running:
```bash
curl -i -X POST http://127.0.0.1:8765/ask -H 'content-type: application/json' -d '{"q":"hi"}'
```
Expected: `HTTP/1.1 501 Not Implemented` with the detail message.

- [ ] **Step 3: Commit**

```bash
git add claude-web/server.py
git commit -m "feat(claude-web): reserve /ask endpoint for ESP32 client"
```

---

## Task 8: Frontend — HTML skeleton + WebSocket connection + status

**Files:**
- Modify: `claude-web/static/index.html`

- [ ] **Step 1: Replace `static/index.html` with full skeleton**

Write `claude-web/static/index.html`:
```html
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>claude-web</title>
  <script src="https://cdn.tailwindcss.com"></script>
</head>
<body class="bg-gray-50 text-gray-900 h-screen flex flex-col">
  <header class="flex items-center gap-3 px-4 py-2 border-b bg-white">
    <div class="font-semibold">claude-web</div>
    <div id="cwd" class="text-xs text-gray-500 truncate"></div>
    <div class="flex-1"></div>
    <label class="text-xs flex items-center gap-1">
      <input id="mode-toggle" type="checkbox" class="align-middle" />
      <span>Card mode</span>
    </label>
    <button id="new-session"
      class="text-xs px-2 py-1 border rounded hover:bg-gray-100">New session</button>
    <span id="conn" class="text-xs px-2 py-0.5 rounded bg-gray-200">…</span>
  </header>

  <main id="messages" class="flex-1 overflow-y-auto p-4 space-y-3"></main>

  <footer class="border-t bg-white p-3">
    <form id="form" class="flex gap-2">
      <textarea id="input" rows="2"
        class="flex-1 border rounded px-2 py-1 text-sm resize-none focus:outline-none focus:ring"
        placeholder="Ask Claude Code…  (Enter to send, Shift+Enter for newline)"></textarea>
      <button id="send" type="submit"
        class="px-3 py-1 bg-black text-white rounded text-sm disabled:opacity-40">Send</button>
    </form>
    <div id="footer-info" class="text-xs text-gray-400 mt-1 h-4"></div>
  </footer>

  <script src="/static/app.js"></script>
</body>
</html>
```

- [ ] **Step 2: Create `claude-web/static/app.js` with the connection layer**

```javascript
// claude-web frontend — vanilla JS, no build step.
const el = (id) => document.getElementById(id);
const messagesEl = el("messages");
const inputEl = el("input");
const sendBtn = el("send");
const formEl = el("form");
const connEl = el("conn");
const cwdEl = el("cwd");
const newSessionBtn = el("new-session");
const modeToggle = el("mode-toggle");
const footerInfo = el("footer-info");

let ws = null;
let reconnectDelay = 500;
let cardMode = false;

function setConn(state) {
  const styles = {
    connecting: ["bg-yellow-100 text-yellow-800", "connecting…"],
    open:       ["bg-green-100 text-green-800",  "connected"],
    closed:     ["bg-red-100 text-red-800",      "disconnected"],
  };
  const [cls, label] = styles[state];
  connEl.className = `text-xs px-2 py-0.5 rounded ${cls}`;
  connEl.textContent = label;
  sendBtn.disabled = state !== "open";
}

function connect() {
  setConn("connecting");
  const proto = location.protocol === "https:" ? "wss" : "ws";
  ws = new WebSocket(`${proto}://${location.host}/ws`);

  ws.onopen = () => {
    setConn("open");
    reconnectDelay = 500;
  };
  ws.onclose = () => {
    setConn("closed");
    setTimeout(connect, reconnectDelay);
    reconnectDelay = Math.min(reconnectDelay * 2, 10_000);
  };
  ws.onerror = () => { /* onclose will fire after */ };
  ws.onmessage = (e) => {
    let ev; try { ev = JSON.parse(e.data); } catch { return; }
    handleEvent(ev);
  };
}

function send(obj) {
  if (ws && ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify(obj));
}

formEl.addEventListener("submit", (e) => {
  e.preventDefault();
  const text = inputEl.value.trim();
  if (!text) return;
  appendUserBubble(text);
  send({ type: "user_message", text });
  inputEl.value = "";
  sendBtn.disabled = true;
});

inputEl.addEventListener("keydown", (e) => {
  if (e.key === "Enter" && !e.shiftKey) {
    e.preventDefault();
    formEl.requestSubmit();
  }
});

newSessionBtn.addEventListener("click", () => {
  if (!confirm("Start a new session and discard the current conversation?")) return;
  send({ type: "new_session" });
  messagesEl.innerHTML = "";
});

modeToggle.addEventListener("change", () => {
  cardMode = modeToggle.checked;
  document.body.dataset.toolMode = cardMode ? "card" : "compact";
});
document.body.dataset.toolMode = "compact";

// Placeholders to be implemented in later tasks.
function handleEvent(ev) {
  console.log("[event]", ev);
  if (ev.type === "ready") {
    cwdEl.textContent = `cwd: ${ev.cwd}` + (ev.session_id ? ` · session ${ev.session_id.slice(0, 8)}` : " · new session");
  }
}
function appendUserBubble(text) { /* Task 10 */ }

connect();
```

- [ ] **Step 3: Smoke-test in the browser**

Restart server, open `http://127.0.0.1:8765/` in browser:
- Status pill should turn green and say "connected"
- Header should show `cwd: /Users/xd/java_proj/zedpet ...`
- Open devtools console and confirm `[event] {type:"ready", ...}` is logged
- Stop the server; pill should flip to red "disconnected"; restart server and confirm it auto-reconnects to green

- [ ] **Step 4: Commit**

```bash
git add claude-web/static/index.html claude-web/static/app.js
git commit -m "feat(claude-web): frontend skeleton with WS connection"
```

---

## Task 9: Frontend — render user + `assistant_text` events

**Files:**
- Modify: `claude-web/static/app.js`

- [ ] **Step 1: Implement bubble rendering**

In `claude-web/static/app.js`, replace the placeholder `appendUserBubble` and extend `handleEvent`:

Replace:
```javascript
function appendUserBubble(text) { /* Task 10 */ }
```
with:
```javascript
function appendUserBubble(text) {
  const div = document.createElement("div");
  div.className = "flex justify-end";
  div.innerHTML = `<div class="max-w-[80%] bg-blue-600 text-white rounded-lg px-3 py-2 text-sm whitespace-pre-wrap"></div>`;
  div.firstChild.textContent = text;
  messagesEl.appendChild(div);
  scrollToBottom();
}

function scrollToBottom() {
  messagesEl.scrollTop = messagesEl.scrollHeight;
}

const assistantBubbles = new Map();  // message_id -> <div> with the streaming text

function appendAssistantChunk(messageId, text) {
  let bubble = assistantBubbles.get(messageId);
  if (!bubble) {
    const wrap = document.createElement("div");
    wrap.className = "flex justify-start";
    wrap.innerHTML = `<div class="max-w-[80%] bg-white border rounded-lg px-3 py-2 text-sm whitespace-pre-wrap shadow-sm"></div>`;
    bubble = wrap.firstChild;
    assistantBubbles.set(messageId, bubble);
    messagesEl.appendChild(wrap);
  }
  bubble.textContent += text;
  scrollToBottom();
}

function appendErrorLine(text) {
  const div = document.createElement("div");
  div.className = "text-xs text-red-700 bg-red-50 border border-red-200 rounded px-2 py-1";
  div.textContent = text;
  messagesEl.appendChild(div);
  scrollToBottom();
}

function onTurnEnd(ev) {
  assistantBubbles.clear();
  sendBtn.disabled = false;
  inputEl.focus();
  if (ev.usage) {
    const { input_tokens = 0, output_tokens = 0 } = ev.usage;
    footerInfo.textContent = `last turn: ${input_tokens} in / ${output_tokens} out tokens`;
  }
}
```

Replace the existing `handleEvent` with:
```javascript
function handleEvent(ev) {
  switch (ev.type) {
    case "ready":
      cwdEl.textContent = `cwd: ${ev.cwd}` +
        (ev.session_id ? ` · session ${ev.session_id.slice(0, 8)}` : " · new session");
      break;
    case "assistant_text":
      appendAssistantChunk(ev.message_id, ev.text);
      break;
    case "tool_use":
      appendToolUse(ev);   // Task 11
      break;
    case "tool_result":
      attachToolResult(ev); // Task 11
      break;
    case "turn_end":
      onTurnEnd(ev);
      break;
    case "error":
      appendErrorLine(ev.message || "error");
      sendBtn.disabled = false;
      break;
  }
}

// Placeholders for Task 11.
function appendToolUse(ev) { console.log("tool_use", ev); }
function attachToolResult(ev) { console.log("tool_result", ev); }
```

- [ ] **Step 2: Browser smoke test**

Restart server, open the page, ask "say hi in 5 words":
- Your message appears as a right-aligned blue bubble
- A left-aligned white bubble appears and streams text
- Footer shows token counts
- Input box re-enables and focuses

- [ ] **Step 3: Commit**

```bash
git add claude-web/static/app.js
git commit -m "feat(claude-web): render user prompts and streaming replies"
```

---

## Task 10: Frontend — tool_use / tool_result rendering with mode toggle

**Files:**
- Modify: `claude-web/static/app.js`
- Modify: `claude-web/static/index.html`

- [ ] **Step 1: Add CSS for the two display modes**

In `static/index.html`, add inside `<head>` after the Tailwind script:
```html
<style>
  /* Compact mode: single-line summary, details hidden */
  body[data-tool-mode="compact"] .tool-card { display: none; }
  body[data-tool-mode="compact"] .tool-line { display: flex; }
  /* Card mode: collapsible card with details, hide the one-liner */
  body[data-tool-mode="card"] .tool-card { display: block; }
  body[data-tool-mode="card"] .tool-line { display: none; }
</style>
```

- [ ] **Step 2: Replace the tool placeholders in `static/app.js`**

Replace:
```javascript
function appendToolUse(ev) { console.log("tool_use", ev); }
function attachToolResult(ev) { console.log("tool_result", ev); }
```
with:
```javascript
const toolNodes = new Map();  // tool_use_id -> { line, card, resultLineEl, resultDetailEl }

function summarizeInput(name, input) {
  if (!input || typeof input !== "object") return "";
  if (name === "Bash") return input.command || "";
  if (name === "Read" || name === "Edit" || name === "Write") return input.file_path || input.path || "";
  if (name === "Grep") return input.pattern || "";
  if (name === "Glob") return input.pattern || "";
  // Generic fallback: first 80 chars of JSON
  const s = JSON.stringify(input);
  return s.length > 80 ? s.slice(0, 80) + "…" : s;
}

function summarizeResult(content, isError) {
  if (!content) return isError ? "error" : "ok";
  const oneLine = content.replace(/\s+/g, " ").trim();
  return oneLine.length > 60 ? oneLine.slice(0, 60) + "…" : oneLine;
}

function appendToolUse(ev) {
  const wrap = document.createElement("div");
  wrap.className = "tool-event";

  // Compact one-liner
  const line = document.createElement("div");
  line.className = "tool-line items-center text-xs text-gray-500 gap-2";
  const resultLineEl = document.createElement("span");
  resultLineEl.className = "text-gray-400";
  resultLineEl.textContent = "…";
  line.innerHTML = `<span>🔧</span><span class="font-medium text-gray-700"></span><span class="truncate"></span><span>·</span>`;
  line.children[1].textContent = ev.name;
  line.children[2].textContent = summarizeInput(ev.name, ev.input);
  line.appendChild(resultLineEl);

  // Expanded card
  const card = document.createElement("details");
  card.className = "tool-card border rounded bg-gray-50";
  const summary = document.createElement("summary");
  summary.className = "cursor-pointer px-2 py-1 text-xs text-gray-700 flex items-center gap-2";
  summary.innerHTML = `<span>🔧</span><span class="font-medium"></span><span class="text-gray-500 truncate"></span>`;
  summary.children[1].textContent = ev.name;
  summary.children[2].textContent = summarizeInput(ev.name, ev.input);
  card.appendChild(summary);

  const body = document.createElement("div");
  body.className = "px-2 pb-2 text-xs space-y-2";
  const inputPre = document.createElement("pre");
  inputPre.className = "bg-white border rounded p-2 overflow-x-auto";
  inputPre.textContent = JSON.stringify(ev.input, null, 2);
  body.appendChild(labelled("input", inputPre));

  const resultDetailEl = document.createElement("pre");
  resultDetailEl.className = "bg-white border rounded p-2 overflow-x-auto text-gray-400";
  resultDetailEl.textContent = "(waiting…)";
  body.appendChild(labelled("output", resultDetailEl));
  card.appendChild(body);

  wrap.appendChild(line);
  wrap.appendChild(card);
  messagesEl.appendChild(wrap);
  scrollToBottom();

  toolNodes.set(ev.tool_use_id, { line, card, resultLineEl, resultDetailEl });
}

function attachToolResult(ev) {
  const node = toolNodes.get(ev.tool_use_id);
  if (!node) return;
  node.resultLineEl.textContent = summarizeResult(ev.content, ev.is_error);
  node.resultLineEl.className = ev.is_error ? "text-red-600" : "text-gray-500";
  node.resultDetailEl.textContent = ev.content || "";
  node.resultDetailEl.className = "bg-white border rounded p-2 overflow-x-auto " +
    (ev.is_error ? "text-red-700" : "text-gray-800");
}

function labelled(label, child) {
  const wrap = document.createElement("div");
  const lbl = document.createElement("div");
  lbl.className = "text-gray-500 mb-1";
  lbl.textContent = label;
  wrap.appendChild(lbl);
  wrap.appendChild(child);
  return wrap;
}
```

- [ ] **Step 3: Browser smoke test**

Restart server. Ask "list the files in the src directory":
- Compact mode (default): a single grey line `🔧 Bash · ls src/ · main.cpp pet.cpp …` followed by the assistant's summary text
- Toggle "Card mode": the compact lines disappear and folded `<details>` cards take their place; click to expand each and see full input + output
- Toggle back: cards collapse to one-liners; existing messages re-render correctly (only CSS changed)

- [ ] **Step 4: Test an error case**

Ask "run `false` in bash and tell me what happened":
- The tool result one-liner should be red
- Expanded card output should be red text

- [ ] **Step 5: Commit**

```bash
git add claude-web/static/app.js claude-web/static/index.html
git commit -m "feat(claude-web): tool event rendering with compact/card toggle"
```

---

## Task 11: README with manual integration checklist

**Files:**
- Create: `claude-web/README.md`

- [ ] **Step 1: Write the README**

```markdown
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
```

- [ ] **Step 2: Commit**

```bash
git add claude-web/README.md
git commit -m "docs(claude-web): README with run + acceptance checklist"
```

---

## Task 12: Run the full acceptance checklist

- [ ] **Step 1: Run all unit tests**

From `claude-web/` with venv active:
```bash
PYTHONPATH=. pytest -v
```
Expected: all tests pass.

- [ ] **Step 2: Walk through every item in the README "Manual acceptance checklist"**

Tick each step in the README mentally; if any fails, file the bug as a follow-up task rather than patching ad-hoc.

- [ ] **Step 3: Final tidy commit if anything needed adjusting**

```bash
git status
# only commit if there are intentional fixes
```

---

## Self-Review Notes

- Spec §1 (target/scope): covered by Tasks 1, 5 (`--cwd` CLI, localhost only).
- Spec §2 (architecture): server (Tasks 5–7), runner (Tasks 3–4), session store (Task 2), frontend (Tasks 8–10).
- Spec §3 (project structure): laid down by Tasks 1, 2, 3, 5, 8.
- Spec §4 (wire protocol): every event type implemented — `ready` (Task 6), `assistant_text` (Task 9), `tool_use`/`tool_result` (Task 10), `turn_end` (Task 9), `error` (Tasks 6, 9), `user_message`/`new_session`/`interrupt` (Task 6, except `interrupt` which the spec marked "将来" / future).
- Spec §5 (session + error handling): session lifecycle in Task 4 (resume + fallback); error matrix in Tasks 5 (binary check), 6 (bad JSON, unknown type), 4 (SDK exceptions), 8 (WS reconnect).
- Spec §6 (testing): unit tests in Tasks 2 & 3; manual checklist in Tasks 11 & 12.
- `interrupt` is intentionally deferred — spec marks it future and no concrete behavior was specified.
- `/ask` endpoint is intentionally a 501 placeholder — spec says "MVP 可只占位".
```
