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
    """Convert one SDK message into zero or more wire-protocol events."""
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
