# Cardputer Claude Client Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a third mode to M5Cardputer that talks to the existing `claude-web` server via a new `/ask` chunked-streaming HTTP endpoint, letting users type prompts on the device keyboard and watch Claude's reply stream onto the 240×135 screen.

**Architecture:** Server side: implement the previously stubbed `POST /ask` as a `StreamingResponse` that yields plain-text chunks from a second `ClaudeRunner` instance (independent session). Device side: a new `c`-key Claude mode owns the screen, with `ClaudeClient` handling the HTTP+chunked-read state machine and `ClaudeUi` handling the input row, scrollable reply area, state indicators, and keyboard events. Pet/weather modules stay untouched.

**Tech Stack:** Python 3.11+ FastAPI (server), Arduino/PlatformIO with `m5stack/M5Cardputer` library (device), ESP32-S3 (M5StampS3), `WiFiClient` + `HTTPClient` (device HTTP), `g++ -std=c++17` for host-side unit tests.

**Spec:** `docs/superpowers/specs/2026-05-23-cardputer-claude-client-design.md`

**Branch:** `feat/cardputer-claude-client` (already created from latest `main`)

---

## File Structure

```
zedpet/
├── claude-web/
│   ├── server.py                                    # MODIFY (Task 1)
│   └── tests/test_ask_endpoint.py                   # CREATE (Task 1)
├── src/
│   ├── main.cpp                                     # MODIFY (Task 6)
│   ├── config.h                                     # MODIFY (Task 2)
│   ├── claude_client_parsing.h                      # CREATE (Task 3)
│   ├── claude_client_parsing.cpp                    # CREATE (Task 3)
│   ├── claude_client.h                              # CREATE (Task 4)
│   ├── claude_client.cpp                            # CREATE (Task 4)
│   ├── claude_ui.h                                  # CREATE (Task 5)
│   └── claude_ui.cpp                                # CREATE (Task 5)
├── tests/host/                                      # CREATE (Task 3)
│   ├── Makefile
│   └── test_claude_client_parsing.cpp
├── platformio.ini                                   # MODIFY (Task 2)
└── claude-web/README.md                             # MODIFY (Task 7)
```

### Per-file responsibility

- **`server.py`**: adds a second `ClaudeRunner` (`ask_runner`) and a real `/ask` route that streams text. Nothing else changes.
- **`claude_client_parsing.h/.cpp`**: pure C++ string helpers (uses `std::string`/`std::vector`, no Arduino headers) — wrap text to N columns, detect `[error: ...]` line prefixes, truncate buffer keeping the tail. Host-testable with plain g++.
- **`claude_client.h/.cpp`**: Arduino-only HTTP+chunked-read client. Owns a `WiFiClient` and `HTTPClient`; calls `update()` each frame to advance the read; invokes `onChunk` / `onDone` callbacks. Uses `String` (Arduino) at the boundary; converts to/from `std::string` when calling parsing helpers.
- **`claude_ui.h/.cpp`**: state machine + screen rendering + keyboard event handling. Owns one `ClaudeClient` instance. Uses `M5Cardputer` API.
- **`main.cpp`**: adds a single `'c'` toggle, dispatches to `claudeUi.update()` when active and short-circuits the rest of the loop.
- **`config.h`**: adds layout constants for the Claude UI; no behavior change for existing modules.
- **`tests/host/Makefile`**: compiles the parsing module with bare g++ and runs the test binary.

### Key implementation decision (cancel key)

The Cardputer keyboard has no `Esc` key. The plan maps the spec's "Esc" semantics to the most natural Cardputer-available shortcut:
- **Cancel current operation** (WAITING / STREAMING / clear ERROR): `Ctrl + C` (detected via `keysState.ctrl && contains(ks.word, 'c')`)
- **Exit Claude mode**: press `c` again from IDLE/TYPING (already in the spec)

---

## Task 1: Server `/ask` endpoint (real implementation) with TDD

**Files:**
- Create: `claude-web/tests/test_ask_endpoint.py`
- Modify: `claude-web/server.py`

- [ ] **Step 1: Write the failing tests**

Write `claude-web/tests/test_ask_endpoint.py`:
```python
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
    # The web runner (used by /ws) and ask runner must be separate instances.
    assert app_with_fake.state.runner is not app_with_fake.state.ask_runner
```

- [ ] **Step 2: Run tests to confirm they fail**

```bash
cd /Users/xd/java_proj/zedpet/claude-web && source .venv/bin/activate
PYTHONPATH=. pytest tests/test_ask_endpoint.py -v
```
Expected: tests fail. Most will fail because (a) `app.state.ask_runner` doesn't exist yet, and (b) `/ask` currently returns 501.

- [ ] **Step 3: Modify `claude-web/server.py`**

In `build_app(cwd)`, **right after** the existing `runner = ClaudeRunner(...)` line and `app.state.runner = runner` assignment, add:
```python
    ask_runner = ClaudeRunner(
        cwd=cwd,
        session_store=SessionStore(cwd / ".claude-web" / "ask-session.json"),
    )
    app.state.ask_runner = ask_runner
```

Add this import near the other FastAPI imports at the top:
```python
from fastapi.responses import StreamingResponse
```

**Replace** the existing `@app.post("/ask")` handler (the 501 placeholder) with:
```python
    @app.post("/ask")
    async def ask(payload: dict):
        text = (payload or {}).get("text", "")
        if not isinstance(text, str) or not text.strip():
            from fastapi import HTTPException
            raise HTTPException(status_code=400, detail="missing 'text'")

        async def stream():
            async for ev in app.state.ask_runner.query(text):
                if ev["type"] == "assistant_text":
                    yield ev["text"]
                elif ev["type"] == "error":
                    yield f"\n[error: {ev['message']}]\n"
                # tool_use / tool_result / turn_end / ready intentionally dropped

        return StreamingResponse(stream(), media_type="text/plain; charset=utf-8")
```

- [ ] **Step 4: Run tests to confirm they pass**

```bash
PYTHONPATH=. pytest tests/test_ask_endpoint.py -v
```
Expected: all 7 tests pass. Also re-run the full suite to confirm no regression:
```bash
PYTHONPATH=. pytest -v
```
Expected: 24 passed (17 original + 7 new).

- [ ] **Step 5: Commit**

```bash
cd /Users/xd/java_proj/zedpet
git add claude-web/server.py claude-web/tests/test_ask_endpoint.py
git commit -m "feat(claude-web): implement /ask streaming endpoint for Cardputer client"
```

---

## Task 2: PlatformIO build flag + device-side layout constants

**Files:**
- Modify: `platformio.ini`
- Modify: `src/config.h`

- [ ] **Step 1: Modify `platformio.ini` to add the server-address build flag**

In `[env:m5stack-cardputer]`, **replace** the `build_flags = ...` block with:
```ini
build_flags =
    -DWIFI_SSID=\"${sysenv.WIFI_SSID}\"
    -DWIFI_PASS=\"${sysenv.WIFI_PASS}\"
    -DCLAUDE_SERVER=\"${sysenv.CLAUDE_SERVER}\"
```

This mirrors the existing WiFi env-var pattern. The user will export `CLAUDE_SERVER="http://192.168.1.42:8000"` (substitute their laptop's LAN IP) before building.

- [ ] **Step 2: Update the Makefile so it validates `CLAUDE_SERVER`**

In `Makefile`, **add** a CLAUDE_SERVER guard to both targets so missing env vars fail early. Replace the entire file with:
```makefile
.PHONY: build upload

build:
	@test -n "$$WIFI_SSID" || { echo "ERROR: WIFI_SSID not set"; exit 1; }
	@test -n "$$WIFI_PASS" || { echo "ERROR: WIFI_PASS not set"; exit 1; }
	@test -n "$$CLAUDE_SERVER" || { echo "ERROR: CLAUDE_SERVER not set (e.g. http://192.168.1.42:8000)"; exit 1; }
	pio run

upload:
	@test -n "$$WIFI_SSID" || { echo "ERROR: WIFI_SSID not set"; exit 1; }
	@test -n "$$WIFI_PASS" || { echo "ERROR: WIFI_PASS not set"; exit 1; }
	@test -n "$$CLAUDE_SERVER" || { echo "ERROR: CLAUDE_SERVER not set (e.g. http://192.168.1.42:8000)"; exit 1; }
	pio run --target upload
```

- [ ] **Step 3: Modify `src/config.h` to add Claude UI constants**

Append at the bottom of `src/config.h` (after the existing `rgb565` / `C(...)` block):
```cpp
// Claude UI layout
constexpr int CLAUDE_INPUT_H   = 18;
constexpr int CLAUDE_REPLY_Y   = 0;
constexpr int CLAUDE_REPLY_H   = SCREEN_H - CLAUDE_INPUT_H;
constexpr int CLAUDE_LINE_H    = 9;            // 6×8 default font + 1px line spacing
constexpr int CLAUDE_COLS      = 40;           // 240 / 6
constexpr int CLAUDE_VISIBLE_LINES = CLAUDE_REPLY_H / CLAUDE_LINE_H;  // ~13
constexpr size_t CLAUDE_MAX_INPUT = 200;
constexpr size_t CLAUDE_REPLY_CAP = 4096;
constexpr size_t CLAUDE_REPLY_TRIM_TO = 3000;  // when over cap, keep last N bytes

// Colors
constexpr uint16_t CLAUDE_BG          = TFT_BLACK;
constexpr uint16_t CLAUDE_FG          = TFT_WHITE;
constexpr uint16_t CLAUDE_INPUT_BG    = C(32, 32, 40);
constexpr uint16_t CLAUDE_PROMPT_FG   = C(120, 180, 255);
constexpr uint16_t CLAUDE_ERROR_FG    = C(255, 80, 80);
constexpr uint16_t CLAUDE_STATUS_FG   = C(160, 160, 160);
constexpr uint16_t CLAUDE_WIFI_OK     = C(80, 200, 80);
constexpr uint16_t CLAUDE_WIFI_BAD    = C(200, 80, 80);
```

- [ ] **Step 4: Verify the build still succeeds (no functional change yet)**

Run from the repo root with the env vars set:
```bash
cd /Users/xd/java_proj/zedpet
WIFI_SSID=test WIFI_PASS=test CLAUDE_SERVER="http://192.168.1.42:8000" pio run -e m5stack-cardputer
```
Expected: build succeeds (no new code is referenced yet; this just confirms the build flag and config additions don't break compilation). If PlatformIO isn't installed, skip the build and report it — later tasks will surface any issue.

- [ ] **Step 5: Commit**

```bash
git add platformio.ini Makefile src/config.h
git commit -m "feat(cardputer): add CLAUDE_SERVER build flag and Claude UI constants"
```

---

## Task 3: Pure parsing helpers with host-side TDD

**Files:**
- Create: `tests/host/Makefile`
- Create: `tests/host/test_claude_client_parsing.cpp`
- Create: `src/claude_client_parsing.h`
- Create: `src/claude_client_parsing.cpp`

- [ ] **Step 1: Create the host test directory**

```bash
mkdir -p /Users/xd/java_proj/zedpet/tests/host
```

- [ ] **Step 2: Write the host Makefile**

Create `tests/host/Makefile`:
```makefile
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O0 -g -I../../src

BIN = test_claude_client_parsing
SRCS = test_claude_client_parsing.cpp ../../src/claude_client_parsing.cpp

all: $(BIN)
	./$(BIN)

$(BIN): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -f $(BIN)
```

- [ ] **Step 3: Write the failing tests**

Create `tests/host/test_claude_client_parsing.cpp`:
```cpp
#include "claude_client_parsing.h"

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

using namespace claude_parsing;

static int failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { ++failures; std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

#define EQ(a, b) do { \
    auto _av = (a); auto _bv = (b); \
    if (!(_av == _bv)) { ++failures; std::fprintf(stderr, "FAIL %s:%d expected %s == %s\n", __FILE__, __LINE__, #a, #b); } \
} while (0)

static void test_is_error_line() {
    CHECK(is_error_line("[error: boom]"));
    CHECK(is_error_line("[error: anything goes"));
    CHECK(!is_error_line("hello world"));
    CHECK(!is_error_line(""));
    CHECK(!is_error_line("error: not bracketed"));
    CHECK(!is_error_line(" [error: leading space not allowed"));
}

static void test_wrap_text_short() {
    auto lines = wrap_text("hello", 40);
    EQ(lines.size(), (size_t)1);
    EQ(lines[0], std::string("hello"));
}

static void test_wrap_text_break_on_whitespace() {
    auto lines = wrap_text("hello world this is a test", 10);
    EQ(lines.size(), (size_t)3);
    EQ(lines[0], std::string("hello"));
    EQ(lines[1], std::string("world this"));
    EQ(lines[2], std::string("is a test"));
}

static void test_wrap_text_hard_break_long_word() {
    auto lines = wrap_text("abcdefghijklmnop", 5);
    EQ(lines.size(), (size_t)4);
    EQ(lines[0], std::string("abcde"));
    EQ(lines[1], std::string("fghij"));
    EQ(lines[2], std::string("klmno"));
    EQ(lines[3], std::string("p"));
}

static void test_wrap_text_preserves_explicit_newlines() {
    auto lines = wrap_text("line one\nline two\nline three", 40);
    EQ(lines.size(), (size_t)3);
    EQ(lines[0], std::string("line one"));
    EQ(lines[1], std::string("line two"));
    EQ(lines[2], std::string("line three"));
}

static void test_wrap_text_empty_returns_empty() {
    auto lines = wrap_text("", 40);
    EQ(lines.size(), (size_t)0);
}

static void test_truncate_keep_tail_no_op_when_under_cap() {
    std::string s = "short";
    EQ(truncate_keep_tail(s, 100, 50), std::string("short"));
}

static void test_truncate_keep_tail_keeps_last_n() {
    std::string s(5000, 'x');
    s += "TAIL";
    std::string out = truncate_keep_tail(s, 4096, 3000);
    EQ(out.size(), (size_t)3000);
    // last 4 chars should still be TAIL
    EQ(out.substr(out.size() - 4), std::string("TAIL"));
}

static void test_truncate_keep_tail_exactly_at_cap_is_no_op() {
    std::string s(4096, 'y');
    EQ(truncate_keep_tail(s, 4096, 3000).size(), (size_t)4096);
}

int main() {
    test_is_error_line();
    test_wrap_text_short();
    test_wrap_text_break_on_whitespace();
    test_wrap_text_hard_break_long_word();
    test_wrap_text_preserves_explicit_newlines();
    test_wrap_text_empty_returns_empty();
    test_truncate_keep_tail_no_op_when_under_cap();
    test_truncate_keep_tail_keeps_last_n();
    test_truncate_keep_tail_exactly_at_cap_is_no_op();

    if (failures == 0) {
        std::printf("OK: all parsing tests passed\n");
        return 0;
    }
    std::fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
}
```

- [ ] **Step 4: Verify tests fail**

```bash
cd /Users/xd/java_proj/zedpet/tests/host && make
```
Expected: compilation fails because `claude_client_parsing.h` doesn't exist yet.

- [ ] **Step 5: Write the header**

Create `src/claude_client_parsing.h`:
```cpp
#pragma once
#include <string>
#include <vector>

// Pure-C++ string helpers for the Cardputer Claude client. No Arduino headers.
// Host-testable with plain g++.
namespace claude_parsing {

// True if the line starts (with no leading whitespace) with the literal "[error:".
bool is_error_line(const std::string& line);

// Word-wrap `text` to lines no longer than `cols` characters.
// Treats '\n' as a hard line break. Breaks on the last whitespace before the
// limit when possible; otherwise hard-breaks at `cols`.
// Empty input returns an empty vector.
std::vector<std::string> wrap_text(const std::string& text, std::size_t cols);

// If buf.size() > cap, return the last `keep` characters of buf; otherwise
// return buf unchanged. `keep` should be <= cap; behaviour for keep > buf size
// is "return buf".
std::string truncate_keep_tail(const std::string& buf, std::size_t cap, std::size_t keep);

}  // namespace claude_parsing
```

- [ ] **Step 6: Write the implementation**

Create `src/claude_client_parsing.cpp`:
```cpp
#include "claude_client_parsing.h"

namespace claude_parsing {

bool is_error_line(const std::string& line) {
    static const std::string prefix = "[error:";
    return line.compare(0, prefix.size(), prefix) == 0;
}

static void push_wrapped(std::vector<std::string>& out, const std::string& s, std::size_t cols) {
    if (s.empty()) {
        out.emplace_back("");
        return;
    }
    std::size_t i = 0;
    while (i < s.size()) {
        std::size_t remaining = s.size() - i;
        if (remaining <= cols) {
            out.push_back(s.substr(i));
            return;
        }
        // Try to break at last whitespace within [i, i+cols)
        std::size_t break_at = std::string::npos;
        for (std::size_t j = i + cols; j > i; --j) {
            if (s[j - 1] == ' ' || s[j - 1] == '\t') {
                break_at = j - 1;
                break;
            }
        }
        if (break_at == std::string::npos) {
            out.push_back(s.substr(i, cols));
            i += cols;
        } else {
            out.push_back(s.substr(i, break_at - i));
            i = break_at + 1;  // skip the whitespace itself
        }
    }
}

std::vector<std::string> wrap_text(const std::string& text, std::size_t cols) {
    std::vector<std::string> out;
    if (text.empty() || cols == 0) return out;

    std::size_t start = 0;
    while (start <= text.size()) {
        std::size_t nl = text.find('\n', start);
        std::string segment = (nl == std::string::npos)
            ? text.substr(start)
            : text.substr(start, nl - start);
        push_wrapped(out, segment, cols);
        if (nl == std::string::npos) break;
        start = nl + 1;
    }
    return out;
}

std::string truncate_keep_tail(const std::string& buf, std::size_t cap, std::size_t keep) {
    if (buf.size() <= cap) return buf;
    if (keep >= buf.size()) return buf;
    return buf.substr(buf.size() - keep);
}

}  // namespace claude_parsing
```

- [ ] **Step 7: Run tests and confirm they pass**

```bash
cd /Users/xd/java_proj/zedpet/tests/host && make clean && make
```
Expected: prints `OK: all parsing tests passed` and exits 0.

- [ ] **Step 8: Commit**

```bash
cd /Users/xd/java_proj/zedpet
git add tests/host/Makefile tests/host/test_claude_client_parsing.cpp \
        src/claude_client_parsing.h src/claude_client_parsing.cpp
git commit -m "feat(cardputer): pure C++ parsing helpers with host tests"
```

---

## Task 4: `ClaudeClient` — HTTP + chunked-read state machine

**Files:**
- Create: `src/claude_client.h`
- Create: `src/claude_client.cpp`

No host test for this — it depends on `WiFiClient` / `HTTPClient` from the Arduino core. Manual verification happens in Task 8.

- [ ] **Step 1: Write `src/claude_client.h`**

```cpp
#pragma once
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <functional>

class ClaudeClient {
public:
    using OnChunkCb = std::function<void(const String& chunk)>;
    using OnDoneCb  = std::function<void(bool ok, const String& err)>;

    // Begin a request. Returns false if WiFi is down or another request is in
    // progress. Callbacks fire from update().
    bool send(const String& serverUrl, const String& prompt,
              OnChunkCb onChunk, OnDoneCb onDone);

    // Poll once per loop iteration; advances the read state machine and may
    // invoke onChunk / onDone.
    void update();

    bool isBusy() const { return state_ != State::Idle; }

    // Close the connection and return to Idle without firing onDone.
    void cancel();

private:
    enum class State { Idle, Reading, Done, Error };

    void finishOk();
    void finishErr(const String& msg);

    State state_ = State::Idle;
    HTTPClient http_;
    WiFiClient* stream_ = nullptr;     // borrowed from http_; do not delete
    OnChunkCb onChunk_;
    OnDoneCb  onDone_;
    uint32_t lastByteAt_ = 0;
    static constexpr uint32_t IDLE_TIMEOUT_MS = 60000;
};
```

- [ ] **Step 2: Write `src/claude_client.cpp`**

```cpp
#include "claude_client.h"
#include <WiFi.h>

bool ClaudeClient::send(const String& serverUrl, const String& prompt,
                        OnChunkCb onChunk, OnDoneCb onDone) {
    if (state_ != State::Idle) return false;
    if (WiFi.status() != WL_CONNECTED) {
        onDone(false, "No WiFi");
        return false;
    }

    onChunk_ = std::move(onChunk);
    onDone_  = std::move(onDone);

    String url = serverUrl + "/ask";
    if (!http_.begin(url)) {
        onDone_(false, "begin() failed");
        return false;
    }
    http_.addHeader("Content-Type", "application/json");
    http_.setTimeout(IDLE_TIMEOUT_MS);

    // Build JSON manually (no ArduinoJson dep). Escape only quotes and
    // backslashes — Cardputer input is ASCII so no Unicode worries.
    String body;
    body.reserve(prompt.length() + 16);
    body = "{\"text\":\"";
    for (size_t i = 0; i < prompt.length(); ++i) {
        char c = prompt[i];
        if (c == '\\' || c == '"') body += '\\';
        body += c;
    }
    body += "\"}";

    int code = http_.POST(body);
    if (code != 200) {
        String err = "HTTP " + String(code);
        http_.end();
        onDone_(false, err);
        return false;
    }
    stream_ = http_.getStreamPtr();
    state_ = State::Reading;
    lastByteAt_ = millis();
    return true;
}

void ClaudeClient::update() {
    if (state_ != State::Reading) return;

    // Idle-byte timeout
    if (millis() - lastByteAt_ > IDLE_TIMEOUT_MS) {
        finishOk();  // treat as natural end
        return;
    }

    if (!stream_) { finishErr("stream lost"); return; }

    size_t avail = stream_->available();
    if (avail == 0) {
        // Connection closed?
        if (!http_.connected()) { finishOk(); }
        return;
    }

    // Read up to 256 bytes per tick to avoid blocking the loop too long
    char buf[257];
    size_t want = avail > 256 ? 256 : avail;
    int n = stream_->readBytes(buf, want);
    if (n <= 0) return;
    buf[n] = '\0';
    lastByteAt_ = millis();
    onChunk_(String(buf));
}

void ClaudeClient::cancel() {
    if (state_ == State::Idle) return;
    http_.end();
    stream_ = nullptr;
    state_ = State::Idle;
    // Intentionally no onDone callback — caller initiated.
}

void ClaudeClient::finishOk() {
    http_.end();
    stream_ = nullptr;
    state_ = State::Done;
    auto cb = onDone_;
    onDone_ = nullptr; onChunk_ = nullptr;
    state_ = State::Idle;
    if (cb) cb(true, "");
}

void ClaudeClient::finishErr(const String& msg) {
    http_.end();
    stream_ = nullptr;
    auto cb = onDone_;
    onDone_ = nullptr; onChunk_ = nullptr;
    state_ = State::Idle;
    if (cb) cb(false, msg);
}
```

- [ ] **Step 3: Confirm the project still builds**

```bash
cd /Users/xd/java_proj/zedpet
WIFI_SSID=test WIFI_PASS=test CLAUDE_SERVER="http://192.168.1.42:8000" pio run -e m5stack-cardputer
```
Expected: build succeeds. The class is not yet referenced anywhere; this just verifies the new translation units compile.

If `pio` isn't installed, the implementer should still flag this as untested rather than skipping silently — Task 8 will catch it during the manual acceptance step.

- [ ] **Step 4: Commit**

```bash
git add src/claude_client.h src/claude_client.cpp
git commit -m "feat(cardputer): ClaudeClient HTTP chunked-read client"
```

---

## Task 5: `ClaudeUi` — state machine, keyboard, rendering

**Files:**
- Create: `src/claude_ui.h`
- Create: `src/claude_ui.cpp`

- [ ] **Step 1: Write `src/claude_ui.h`**

```cpp
#pragma once
#include <Arduino.h>
#include "claude_client.h"

class ClaudeUi {
public:
    void begin();         // called once from setup()
    void enter();         // entering Claude mode: clear screen, draw UI
    void exit();          // leaving Claude mode: clear screen
    void update();        // call every loop iter while active
    bool isActive() const { return active_; }

private:
    enum class State { Idle, Typing, Waiting, Streaming, Error };

    bool active_ = false;
    State state_ = State::Idle;
    String inputBuf_;
    String replyBuf_;
    String errorMsg_;
    int scrollLines_ = 0;          // 0 = follow bottom; >0 = lines scrolled up
    bool inputDirty_ = true;
    bool replyDirty_ = true;
    bool headerDirty_ = true;
    uint32_t lastSpinnerAt_ = 0;
    uint8_t spinnerIdx_ = 0;
    uint32_t lastCursorBlinkAt_ = 0;
    bool cursorOn_ = true;

    ClaudeClient client_;

    void handleKeys();
    void sendCurrentInput();
    void appendReplyChunk(const String& chunk);
    void onStreamDone(bool ok, const String& err);

    void drawAll();
    void drawHeader();
    void drawReply();
    void drawInput();
    void clearStatusArea();
};
```

- [ ] **Step 2: Write `src/claude_ui.cpp`**

```cpp
#include "claude_ui.h"

#include <M5Cardputer.h>
#include <WiFi.h>
#include <algorithm>
#include <vector>

#include "claude_client_parsing.h"
#include "config.h"

namespace {
constexpr int FONT_W = 6;
constexpr int FONT_H = 8;

const char* SPINNER_FRAMES[] = {"|", "/", "-", "\\"};
constexpr uint8_t SPINNER_COUNT = 4;
}  // namespace

void ClaudeUi::begin() {
    // Nothing yet; placeholder for future setup-time work.
}

void ClaudeUi::enter() {
    active_ = true;
    state_ = State::Idle;
    inputBuf_ = "";
    errorMsg_ = "";
    scrollLines_ = 0;
    inputDirty_ = replyDirty_ = headerDirty_ = true;
    M5Cardputer.Display.fillScreen(CLAUDE_BG);
    drawAll();
}

void ClaudeUi::exit() {
    if (client_.isBusy()) client_.cancel();
    active_ = false;
    M5Cardputer.Display.fillScreen(TFT_BLACK);
}

void ClaudeUi::update() {
    if (!active_) return;

    handleKeys();
    client_.update();

    // Animate spinner / cursor
    uint32_t now = millis();
    if ((state_ == State::Waiting || state_ == State::Streaming)
            && now - lastSpinnerAt_ > 100) {
        spinnerIdx_ = (spinnerIdx_ + 1) % SPINNER_COUNT;
        lastSpinnerAt_ = now;
        headerDirty_ = true;
    }
    if (state_ == State::Typing && now - lastCursorBlinkAt_ > 500) {
        cursorOn_ = !cursorOn_;
        lastCursorBlinkAt_ = now;
        inputDirty_ = true;
    }

    if (headerDirty_) drawHeader();
    if (replyDirty_)  drawReply();
    if (inputDirty_)  drawInput();
}

void ClaudeUi::handleKeys() {
    M5Cardputer.update();
    auto ks = M5Cardputer.Keyboard.keysState();

    // Ctrl+C → cancel mid-operation / clear error
    bool ctrlC = ks.ctrl && std::find(ks.word.begin(), ks.word.end(), 'c') != ks.word.end();
    if (ctrlC) {
        if (state_ == State::Waiting || state_ == State::Streaming) {
            client_.cancel();
            state_ = State::Idle;
            headerDirty_ = true;
            return;
        }
        if (state_ == State::Error) {
            state_ = State::Idle;
            errorMsg_ = "";
            headerDirty_ = true;
            return;
        }
    }

    // Arrow keys scroll the reply area (only when not actively typing)
    if (state_ != State::Typing) {
        for (auto k : ks.word) {
            if (k == ';') { /* ESP32 arrow mapping varies; no-op for now */ }
        }
    }

    // Enter: send current input
    if (ks.enter && state_ == State::Typing && inputBuf_.length() > 0) {
        sendCurrentInput();
        return;
    }

    // Backspace: delete last char while typing
    if (ks.del && state_ == State::Typing && inputBuf_.length() > 0) {
        inputBuf_.remove(inputBuf_.length() - 1);
        inputDirty_ = true;
        return;
    }

    // Character input: only when Idle or Typing
    if (state_ != State::Idle && state_ != State::Typing) return;
    for (auto k : ks.word) {
        if (k == 'c' && ks.ctrl) continue;     // already handled
        if (inputBuf_.length() >= CLAUDE_MAX_INPUT) break;
        if (k >= 32 && k < 127) {
            inputBuf_ += (char)k;
            state_ = State::Typing;
            inputDirty_ = true;
        }
    }
}

void ClaudeUi::sendCurrentInput() {
    String prompt = inputBuf_;
    inputBuf_ = "";
    inputDirty_ = true;
    state_ = State::Waiting;
    headerDirty_ = true;

    // Echo the user prompt into the reply log so user can see what they asked.
    replyBuf_ += "> ";
    replyBuf_ += prompt;
    replyBuf_ += "\n";
    replyDirty_ = true;

    bool ok = client_.send(
        String(CLAUDE_SERVER), prompt,
        [this](const String& chunk) { this->appendReplyChunk(chunk); },
        [this](bool ok, const String& err) { this->onStreamDone(ok, err); });
    if (!ok) {
        // send() already invoked onDone synchronously for the failure case.
    }
}

void ClaudeUi::appendReplyChunk(const String& chunk) {
    if (state_ == State::Waiting) {
        state_ = State::Streaming;
        headerDirty_ = true;
    }
    replyBuf_ += chunk;

    // Cap memory growth
    std::string s(replyBuf_.c_str());
    std::string trimmed = claude_parsing::truncate_keep_tail(s, CLAUDE_REPLY_CAP, CLAUDE_REPLY_TRIM_TO);
    if (trimmed.size() != s.size()) {
        replyBuf_ = trimmed.c_str();
    }
    replyDirty_ = true;
}

void ClaudeUi::onStreamDone(bool ok, const String& err) {
    if (!ok) {
        state_ = State::Error;
        errorMsg_ = err;
    } else {
        state_ = State::Idle;
        replyBuf_ += "\n";
    }
    headerDirty_ = true;
    replyDirty_ = true;
}

void ClaudeUi::drawAll() {
    drawHeader();
    drawReply();
    drawInput();
}

void ClaudeUi::drawHeader() {
    // Status indicator: top-right corner (16x10 area)
    auto& d = M5Cardputer.Display;
    int x = SCREEN_W - 16;
    int y = 0;
    d.fillRect(x, y, 16, 10, CLAUDE_BG);
    d.setTextSize(1);
    d.setTextColor(CLAUDE_STATUS_FG, CLAUDE_BG);

    const char* glyph = "";
    uint16_t color = CLAUDE_STATUS_FG;
    if (state_ == State::Waiting || state_ == State::Streaming) {
        glyph = SPINNER_FRAMES[spinnerIdx_];
    } else if (state_ == State::Error) {
        glyph = "!";
        color = CLAUDE_ERROR_FG;
    }
    d.setTextColor(color, CLAUDE_BG);
    d.setCursor(x + 2, y + 1);
    d.print(glyph);

    // WiFi dot: bottom-right of reply area
    int wx = SCREEN_W - 6;
    int wy = CLAUDE_REPLY_H - 6;
    bool wifiUp = WiFi.status() == WL_CONNECTED;
    d.fillRect(wx, wy, 4, 4, wifiUp ? CLAUDE_WIFI_OK : CLAUDE_WIFI_BAD);

    headerDirty_ = false;
}

void ClaudeUi::drawReply() {
    auto& d = M5Cardputer.Display;
    d.fillRect(0, CLAUDE_REPLY_Y, SCREEN_W, CLAUDE_REPLY_H, CLAUDE_BG);
    d.setTextSize(1);

    std::vector<std::string> lines = claude_parsing::wrap_text(
        std::string(replyBuf_.c_str()), CLAUDE_COLS);

    // Always show the tail (scroll-to-bottom). Honour `scrollLines_` later if added.
    int maxLines = CLAUDE_VISIBLE_LINES;
    int total = (int)lines.size();
    int first = total > maxLines ? total - maxLines : 0;

    int y = CLAUDE_REPLY_Y;
    for (int i = first; i < total; ++i) {
        const std::string& l = lines[i];
        uint16_t color = claude_parsing::is_error_line(l) ? CLAUDE_ERROR_FG : CLAUDE_FG;
        d.setTextColor(color, CLAUDE_BG);
        d.setCursor(0, y);
        d.print(l.c_str());
        y += CLAUDE_LINE_H;
    }

    if (state_ == State::Error) {
        d.setTextColor(CLAUDE_ERROR_FG, CLAUDE_BG);
        d.setCursor(0, CLAUDE_REPLY_H - CLAUDE_LINE_H - 1);
        String prefix = "[error] ";
        d.print((prefix + errorMsg_).c_str());
    }

    replyDirty_ = false;
}

void ClaudeUi::drawInput() {
    auto& d = M5Cardputer.Display;
    int y = SCREEN_H - CLAUDE_INPUT_H;
    d.fillRect(0, y, SCREEN_W, CLAUDE_INPUT_H, CLAUDE_INPUT_BG);
    d.setTextSize(1);

    d.setTextColor(CLAUDE_PROMPT_FG, CLAUDE_INPUT_BG);
    d.setCursor(2, y + 5);
    d.print("> ");

    d.setTextColor(CLAUDE_FG, CLAUDE_INPUT_BG);
    // Show last (CLAUDE_COLS - 2) characters of inputBuf_
    int maxShow = CLAUDE_COLS - 2;
    String show = inputBuf_;
    if ((int)show.length() > maxShow) {
        show = show.substring(show.length() - maxShow);
    }
    d.print(show.c_str());

    if (state_ == State::Typing && cursorOn_) {
        d.print('_');
    }

    inputDirty_ = false;
}

void ClaudeUi::clearStatusArea() {
    // Reserved for future use (e.g. token/status footer).
}
```

- [ ] **Step 3: Confirm the project builds**

```bash
cd /Users/xd/java_proj/zedpet
WIFI_SSID=test WIFI_PASS=test CLAUDE_SERVER="http://192.168.1.42:8000" pio run -e m5stack-cardputer
```
Expected: build succeeds. The class is not yet referenced from `main.cpp`; this is just a compilation check.

- [ ] **Step 4: Commit**

```bash
git add src/claude_ui.h src/claude_ui.cpp
git commit -m "feat(cardputer): ClaudeUi state machine + rendering"
```

---

## Task 6: Wire `ClaudeUi` into `main.cpp`

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Replace `src/main.cpp` with the integrated version**

The full new file:
```cpp
#include <algorithm>
#include <M5Cardputer.h>
#include "claude_ui.h"
#include "pet.h"
#include "udp_server.h"
#include "weather.h"

Pet pet;
ClaudeUi claudeUi;

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.fillScreen(TFT_BLACK);

    pet.begin();
    claudeUi.begin();

    udpServerBegin();
    weather.begin();
}

static bool qWasDown = false;
static bool wWasDown = false;
static bool cWasDown = false;

void loop() {
    M5Cardputer.update();
    auto ks = M5Cardputer.Keyboard.keysState();

    // Mode toggle: 'c' (without ctrl) enters/exits Claude mode.
    bool cDown = !ks.ctrl &&
                 std::find(ks.word.begin(), ks.word.end(), 'c') != ks.word.end();
    if (cDown && !cWasDown) {
        if (claudeUi.isActive()) claudeUi.exit();
        else                     claudeUi.enter();
    }
    cWasDown = cDown;

    if (claudeUi.isActive()) {
        claudeUi.update();
        delay(16);
        return;
    }

    bool qDown = std::find(ks.word.begin(), ks.word.end(), 'q') != ks.word.end();
    if (qDown && !qWasDown) pet.nextState();
    qWasDown = qDown;

    bool wDown = std::find(ks.word.begin(), ks.word.end(), 'w') != ks.word.end();
    if (wDown && !wWasDown) weather.next();
    wWasDown = wDown;

    const char* cmd = udpCheckCommand();
    if (cmd && cmd[0] != '\0') {
        udpSendAck(cmd);
        pet.receiveCommand(cmd);
    }

    pet.update();
    weather.update();
    delay(16);
}
```

Note the change vs. the original:
- Added `#include "claude_ui.h"`
- Added `ClaudeUi claudeUi;` and call to `claudeUi.begin()` in `setup()`
- Added the `'c'` edge-detect block at the top of `loop()`
- When `claudeUi.isActive()`, run only `claudeUi.update()` and `return` early so pet/weather/udp logic is skipped
- The `'c'` check explicitly excludes the `ctrl` modifier so `Ctrl+C` (cancel-in-Claude-mode) doesn't trigger a mode toggle

- [ ] **Step 2: Build the full project**

```bash
cd /Users/xd/java_proj/zedpet
WIFI_SSID=test WIFI_PASS=test CLAUDE_SERVER="http://192.168.1.42:8000" pio run -e m5stack-cardputer
```
Expected: build succeeds. If the implementer can flash a real device, also run `make upload` (with real WIFI_SSID/WIFI_PASS/CLAUDE_SERVER) to validate runtime — but the build is the gate for this task.

- [ ] **Step 3: Commit**

```bash
git add src/main.cpp
git commit -m "feat(cardputer): wire ClaudeUi into main loop (c-key toggle)"
```

---

## Task 7: README + manual acceptance documentation

**Files:**
- Modify: `claude-web/README.md`

- [ ] **Step 1: Append a Cardputer section to `claude-web/README.md`**

Append this block at the bottom of `claude-web/README.md`:

```markdown

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

1. Power the Cardputer; press `c` → screen clears to Claude mode (input row `>` at bottom, WiFi dot bottom-right)
2. Type "say hi in 5 words" + Enter → spinner appears top-right → reply streams onto the screen
3. Ask "what did I just ask?" → reply references the prior turn (independent ask-session resume works)
4. Ask "tell me a 200 word story" → text scrolls automatically as it overflows
5. Mid-stream, press `Ctrl+C` → stream stops, what's already shown stays
6. Press `c` → returns to pet/weather; press `q` and `w` → original modes still work
7. With server stopped, press `c` and send a prompt → `[error] HTTP ...` red footer; press `Ctrl+C` to clear

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
```

- [ ] **Step 2: Commit**

```bash
git add claude-web/README.md
git commit -m "docs(cardputer): document /ask client + acceptance checklist"
```

---

## Task 8: End-to-end verification

This task has no code changes — it runs the full set of tests and the manual checklist.

- [ ] **Step 1: Run server tests**

```bash
cd /Users/xd/java_proj/zedpet/claude-web && source .venv/bin/activate
PYTHONPATH=. pytest -v
```
Expected: 24 passed (17 original + 7 from Task 1).

- [ ] **Step 2: Run host parsing tests**

```bash
cd /Users/xd/java_proj/zedpet/tests/host && make clean && make
```
Expected: `OK: all parsing tests passed`.

- [ ] **Step 3: Build the device firmware**

```bash
cd /Users/xd/java_proj/zedpet
WIFI_SSID=test WIFI_PASS=test CLAUDE_SERVER="http://192.168.1.42:8000" pio run -e m5stack-cardputer
```
Expected: build succeeds.

- [ ] **Step 4: Walk the README's manual acceptance checklist on real hardware (if available)**

This step requires a physical Cardputer flashed with the actual WiFi credentials and the laptop's real LAN IP. If hardware is unavailable, mark this step as deferred and leave a note in the final report rather than skipping silently.

- [ ] **Step 5: Final cleanup commit (if any whitespace/lint adjustments came up)**

```bash
git status
# only commit if you actually adjusted something during verification
```

---

## Self-Review Notes

**Spec coverage:**
- Spec §1 (target/scope): covered by Tasks 1 (server `/ask`), 2 (build flag), 5 (UI mode), 6 (main integration)
- Spec §2 (architecture): Task 1 (server runner + endpoint), Tasks 3–6 (device pieces)
- Spec §3 (file structure): explicit File Structure section above, plus per-task Files lists
- Spec §4 (protocol): Task 1 enforces the request/response shape; Task 4 implements the device-side reader
- Spec §5 (UI state machine + rendering): Task 5
- Spec §6 (error matrix): server-side cases in Task 1 (400 + inline errors); device-side cases in Tasks 4 (WiFi/HTTP failure, idle timeout) and 5 (cancel, error display, input cap, reply cap via parsing helper)
- Spec §7 (tests): Task 1 server tests, Task 3 host tests, Task 7 manual checklist, Task 8 e2e

**Placeholder scan:** No "TBD" / "implement later" / "appropriate handling" — every step shows the actual code or command.

**Type consistency:**
- `ClaudeClient::send(serverUrl, prompt, onChunk, onDone)` signature matches in `.h` and `.cpp` and the call from `ClaudeUi::sendCurrentInput`
- `wrap_text(text, cols)` and `truncate_keep_tail(buf, cap, keep)` signatures match between `.h`, `.cpp`, tests, and the call in `claude_ui.cpp`
- `is_error_line(line)` signature consistent
- `OnChunkCb` / `OnDoneCb` aliases used consistently
- `ClaudeUi::State` values referenced only inside `claude_ui.cpp`; no leakage

**Adjustments from spec:** The spec said "Esc" for cancel — the Cardputer has no Esc key, so the plan maps it to `Ctrl+C`. This is documented in both the File Structure section above and the README appended in Task 7.

**Deferred from spec:** Arrow-key scrolling for manual reply-area scrollback (`scrollLines_` field exists but no key handler) — the spec mentions it but no precise key mapping was given for Cardputer; for MVP the screen always auto-scrolls to the latest output, and users can re-ask if they need to see earlier content. If needed later, add an arrow-key handler in `ClaudeUi::handleKeys` that adjusts `scrollLines_` and the `first`/`y` computation in `drawReply`.
