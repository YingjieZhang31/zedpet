from dataclasses import dataclass, field
from typing import Any

from claude_runner import map_sdk_message


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
