"""Tests for /ask streaming endpoint."""
from __future__ import annotations

from pathlib import Path
from typing import AsyncIterator

import pytest
from fastapi.testclient import TestClient

from server import build_app


class FakeRunner:
    """In-memory replacement for ClaudeRunner used by /ask tests."""

    def __init__(self, events: list[dict]):
        self._events = events

    async def query(self, prompt: str) -> AsyncIterator[dict]:
        for ev in self._events:
            yield ev

    def current_session_id(self):
        return None

    def reset_session(self):
        pass

    @property
    def cwd(self):
        return Path("/tmp")


@pytest.fixture
def app_with_fake(monkeypatch, tmp_path):
    # Avoid SDK side-effects: stub ClaudeRunner so build_app() succeeds without
    # the real claude binary. Tests will overwrite app.state.ask_runner.
    import claude_runner
    monkeypatch.setattr(claude_runner, "ClaudeRunner",
                        lambda cwd, session_store: FakeRunner([]))
    app = build_app(tmp_path)
    return app


def test_ask_streams_assistant_text_concatenated(app_with_fake):
    app_with_fake.state.ask_runner = FakeRunner([
        {"type": "assistant_text", "message_id": "m1", "text": "hello "},
        {"type": "assistant_text", "message_id": "m1", "text": "world"},
        {"type": "turn_end", "session_id": "s1", "usage": {}},
    ])
    with TestClient(app_with_fake) as c:
        with c.stream("POST", "/ask", json={"text": "hi"}) as r:
            assert r.status_code == 200
            assert r.headers["content-type"].startswith("text/plain")
            body = b"".join(r.iter_bytes()).decode()
    assert body == "hello world"


def test_ask_drops_tool_events(app_with_fake):
    app_with_fake.state.ask_runner = FakeRunner([
        {"type": "tool_use", "tool_use_id": "t1", "name": "Bash", "input": {}},
        {"type": "assistant_text", "message_id": "m1", "text": "done"},
        {"type": "tool_result", "tool_use_id": "t1", "content": "ok", "is_error": False},
        {"type": "turn_end", "session_id": "s1", "usage": {}},
    ])
    with TestClient(app_with_fake) as c:
        body = c.post("/ask", json={"text": "hi"}).content.decode()
    assert body == "done"


def test_ask_inlines_error_event(app_with_fake):
    app_with_fake.state.ask_runner = FakeRunner([
        {"type": "assistant_text", "message_id": "m1", "text": "partial"},
        {"type": "error", "message": "boom", "recoverable": False},
    ])
    with TestClient(app_with_fake) as c:
        body = c.post("/ask", json={"text": "hi"}).content.decode()
    assert body == "partial\n[error: boom]\n"


def test_ask_400_on_empty_text(app_with_fake):
    with TestClient(app_with_fake) as c:
        r = c.post("/ask", json={"text": ""})
    assert r.status_code == 400
    assert "missing 'text'" in r.json()["detail"]


def test_ask_400_on_missing_text(app_with_fake):
    with TestClient(app_with_fake) as c:
        r = c.post("/ask", json={})
    assert r.status_code == 400


def test_ask_400_on_non_string_text(app_with_fake):
    with TestClient(app_with_fake) as c:
        r = c.post("/ask", json={"text": 42})
    assert r.status_code == 400


def test_ask_runner_distinct_from_web_runner(app_with_fake):
    assert app_with_fake.state.runner is not app_with_fake.state.ask_runner
