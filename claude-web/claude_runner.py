"""Wraps claude-agent-sdk and normalizes its events to the wire protocol."""
from __future__ import annotations

from typing import Any, Iterator

# Real SDK block classes — imported lazily to stay test-friendly when SDK absent.
try:
    from claude_agent_sdk import TextBlock as _TextBlock
    from claude_agent_sdk import ToolResultBlock as _ToolResultBlock
    from claude_agent_sdk import ToolUseBlock as _ToolUseBlock
except ImportError:  # pragma: no cover – SDK not installed in test env
    _TextBlock = None  # type: ignore[assignment,misc]
    _ToolUseBlock = None  # type: ignore[assignment,misc]
    _ToolResultBlock = None  # type: ignore[assignment,misc]


def _block_type(block: Any) -> str | None:
    """Return the logical block type string regardless of how it is represented.

    Supports both fake test objects (which carry a `.type` string attribute) and
    real SDK dataclass instances (which are identified by class membership)."""
    t = getattr(block, "type", None)
    if t is not None:
        return t
    if _TextBlock is not None and isinstance(block, _TextBlock):
        return "text"
    if _ToolUseBlock is not None and isinstance(block, _ToolUseBlock):
        return "tool_use"
    if _ToolResultBlock is not None and isinstance(block, _ToolResultBlock):
        return "tool_result"
    return None


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
    """Convert one SDK message into zero or more wire-protocol events."""
    msg_type = getattr(msg, "type", None)

    if msg_type == "assistant":
        for block in getattr(msg, "content", []) or []:
            block_type = _block_type(block)
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
        return

    if msg_type == "user":
        for block in getattr(msg, "content", []) or []:
            if _block_type(block) == "tool_result":
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


import uuid
from pathlib import Path
from typing import AsyncIterator

from claude_agent_sdk import (
    AssistantMessage,
    ClaudeAgentOptions,
    ResultMessage,
    UserMessage,
    query as sdk_query,
)

from session_store import SessionStore


class _TypedWrapper:
    """Thin adapter that adds a `.type` attribute to a real SDK message object
    so that `map_sdk_message` (which uses `getattr(msg, "type", None)`) works
    against live SDK dataclasses that don't expose a `.type` field."""

    __slots__ = ("_inner", "type")

    def __init__(self, inner: object, type_str: str) -> None:
        object.__setattr__(self, "_inner", inner)
        object.__setattr__(self, "type", type_str)

    def __getattr__(self, name: str):  # noqa: ANN204
        return getattr(object.__getattribute__(self, "_inner"), name)


def _adapt(msg: object) -> object:
    """Return the message with a `.type` attribute set if it lacks one."""
    if getattr(msg, "type", None) is not None:
        return msg  # already has type (e.g. fake test objects)
    if isinstance(msg, AssistantMessage):
        return _TypedWrapper(msg, "assistant")
    if isinstance(msg, UserMessage):
        return _TypedWrapper(msg, "user")
    if isinstance(msg, ResultMessage):
        return _TypedWrapper(msg, "result")
    return msg  # HookEventMessage, SystemMessage etc. — will be ignored by map_sdk_message


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
        """Yield wire-protocol events for one user turn."""
        async for ev in self._run_once(prompt, resume=self._store.load()):
            yield ev

    async def _run_once(self, prompt: str, resume: str | None) -> AsyncIterator[dict]:
        options = ClaudeAgentOptions(
            cwd=str(self._cwd),
            permission_mode="bypassPermissions",
            resume=resume,
        )

        message_id = uuid.uuid4().hex
        try:
            async for sdk_msg in sdk_query(prompt=prompt, options=options):
                for ev in map_sdk_message(_adapt(sdk_msg), message_id=message_id):
                    if ev["type"] == "turn_end" and ev.get("session_id"):
                        self._store.save(ev["session_id"])
                    yield ev
        except Exception as e:  # noqa: BLE001
            if resume:
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
