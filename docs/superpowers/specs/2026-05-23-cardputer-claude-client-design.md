# M5Cardputer Claude Client 设计文档

**日期**：2026-05-23
**分支**：`feat/cardputer-claude-client`
**位置**：`src/`（设备端，新增模块）+ `claude-web/server.py`（服务端补 `/ask`）

---

## 1. 目标与范围

让 M5Cardputer（ESP32-S3 + 240×135 TFT + QWERTY 微键盘）作为 `claude-web` 服务端的一个客户端：
通过 WiFi 把用户输入的问题 POST 给已有的 server，把 Claude 的回复流式显示在屏幕上。Cardputer 替代浏览器，担任"独立终端"角色。

### 在范围内
- 在 Cardputer 上新增"Claude 模式"（按 `c` 进入/退出），与现有 pet（`q`）、weather（`w`）平级
- 完整 QWERTY 输入：字母数字、Enter 发送、Backspace 删除、Esc 取消/退出
- 流式渲染：边收边打字机效果显示
- 长回复自动滚动到底，按方向键可手动回看
- 服务端 `/ask` 端点完成实现（之前是 501 占位）
- Cardputer 与浏览器**独立 session**（两份 session 文件）
- Server 地址通过 build flag 写死

### 不在范围内（YAGNI）
- 多项目 / 多 server 切换
- mDNS / 服务发现
- 鉴权
- 中文 / Unicode 字体（M5 字体限制）
- ANSI / Markdown 渲染
- 工具调用细节展示（tool_use / tool_result 在 `/ask` 流里被丢弃）
- 持久化对话历史 UI 回看（设备上没列表 UI）
- Cardputer 自动化测试（host 部分纯函数除外）

---

## 2. 整体架构

```
M5Cardputer (ESP32-S3, WiFi)              Laptop (claude-web server)
┌────────────────────────────┐           ┌─────────────────────────────┐
│ main.cpp loop:             │           │ FastAPI                     │
│   按 'c' 进 Claude mode    │           │  /          → 浏览器 html   │
│                            │           │  /ws        → 浏览器走这个  │
│ ┌──────────────────┐       │  HTTP     │  /ask       → Cardputer走这 │
│ │ ClaudeClient     │ POST  │  chunked  │ ┌─────────────────────────┐ │
│ │  - 流式 reader   │──────►│           │ │ ask_runner: ClaudeRunner│ │
│ │  - cancel/超时   │       │  /ask     │ │  独立 SessionStore       │ │
│ │  - 回调 onChunk  │◄──────│  text/    │ │  → .claude-web/         │ │
│ └──────────────────┘       │  plain    │ │     ask-session.json    │ │
│                            │  chunked  │ └────────┬────────────────┘ │
│ ┌──────────────────┐       │           │          │                  │
│ │ ClaudeUi         │       │           │          ▼                  │
│ │  - state machine │       │           │   spawn claude 子进程       │
│ │  - 输入行 / 输出 │       │           │                             │
│ │  - 滚动 / 状态指示│       │           └─────────────────────────────┘
│ └──────────────────┘       │
└────────────────────────────┘
```

### 职责
- **服务端 `/ask`**：接收 POST、调 `ask_runner.query()`、把 `assistant_text` 事件的文本块以 chunked plain text 流式返回；丢掉 `tool_use` / `tool_result`，错误事件以 `\n[error: ...]\n` 内联
- **服务端 `ask_runner`**：第二个 `ClaudeRunner` 实例，独立 `SessionStore` 路径 `<cwd>/.claude-web/ask-session.json`
- **设备端 `ClaudeClient`**：单连接 HTTP 客户端，`update()` 在 loop 里被 poll，按段读流并调用 `onChunk` 回调
- **设备端 `ClaudeUi`**：状态机 + 键盘事件 + 屏幕渲染；持有 `ClaudeClient`
- **`main.cpp`**：加 `'c'` 键切换；Claude 模式激活时占管屏幕，其它模块（pet/weather）跳过绘制

### 启动配置
`platformio.ini` 增加一个 build flag：
```ini
build_flags =
    -DWIFI_SSID=\"${sysenv.WIFI_SSID}\"
    -DWIFI_PASS=\"${sysenv.WIFI_PASS}\"
    -DCLAUDE_SERVER=\"http://192.168.1.42:8000\"   ; 新增
```

---

## 3. 文件结构与模块

```
zedpet/
├── claude-web/
│   ├── server.py              # 改：补完 /ask，加 ask_runner
│   └── tests/test_ask_endpoint.py   # 新：/ask 单元测试
├── src/
│   ├── main.cpp               # 改：'c' 键切换 + ClaudeUi update 调用
│   ├── claude_client.{h,cpp}  # 新：HTTP + chunked 流式客户端
│   ├── claude_client_parsing.{h,cpp}  # 新：chunk 拼装 / 错误前缀检测（纯函数，host 可测）
│   ├── claude_ui.{h,cpp}      # 新：状态机 + 屏幕渲染
│   └── config.h               # 改：Claude UI 布局常量
├── tests/host/                # 新：可在 macOS 上 g++ 编译跑的纯函数测试
│   ├── Makefile
│   └── test_claude_client_parsing.cpp
└── platformio.ini             # 改：CLAUDE_SERVER build flag
```

### 服务端改动 `claude-web/server.py`

**`build_app(cwd)` 内新建第二个 runner：**
```python
ask_runner = ClaudeRunner(
    cwd=cwd,
    session_store=SessionStore(cwd / ".claude-web" / "ask-session.json"),
)
app.state.ask_runner = ask_runner
```

**`/ask` 端点（替换 501 占位）：**
```python
from fastapi.responses import StreamingResponse

@app.post("/ask")
async def ask(payload: dict):
    text = (payload or {}).get("text", "")
    if not isinstance(text, str) or not text.strip():
        raise HTTPException(400, "missing 'text'")

    async def stream():
        async for ev in app.state.ask_runner.query(text):
            if ev["type"] == "assistant_text":
                yield ev["text"]
            elif ev["type"] == "error":
                yield f"\n[error: {ev['message']}]\n"
    return StreamingResponse(stream(), media_type="text/plain; charset=utf-8")
```

### 设备端 `claude_client.h`
```cpp
class ClaudeClient {
public:
    using OnChunkCb = std::function<void(const String& chunk)>;
    using OnDoneCb  = std::function<void(bool ok, const String& err)>;

    void send(const String& prompt, OnChunkCb onChunk, OnDoneCb onDone);
    void update();                  // 每帧调一次，推进流式读
    bool isBusy() const;
    void cancel();                  // 关 socket、回 Idle

private:
    enum class State { Idle, Connecting, Reading, Done, Error };
    State state = State::Idle;
    WiFiClient client;
    HTTPClient http;
    OnChunkCb onChunk;
    OnDoneCb  onDone;
    uint32_t lastByteAt = 0;        // 用于 60s 超时检测
};
```

### 设备端 `claude_ui.h`
```cpp
class ClaudeUi {
public:
    void begin();
    void enter();       // 进入 mode：清屏、画 UI
    void exit();        // 退出：清屏，main 接管
    void update();      // 每帧：键盘 + client.update() + 脏区刷新
    bool isActive() const;

private:
    enum class State { Idle, Typing, Waiting, Streaming, Error };
    State state = State::Idle;
    String inputBuf;
    String replyBuf;
    int scrollOffset = 0;
    bool inputDirty = true, replyDirty = true, headerDirty = true;
    ClaudeClient client;
    String errorMsg;

    void handleKey(...);
    void drawInput();
    void drawReply();
    void drawHeader();         // spinner / status / WiFi 指示
    void appendChunk(const String& s);
};
```

### `main.cpp` 改动
```cpp
ClaudeUi claudeUi;
static bool cWasDown = false;

void loop() {
    M5Cardputer.update();
    auto ks = M5Cardputer.Keyboard.keysState();

    bool cDown = std::find(ks.word.begin(), ks.word.end(), 'c') != ks.word.end();
    if (cDown && !cWasDown) {
        if (claudeUi.isActive()) claudeUi.exit();
        else                     claudeUi.enter();
    }
    cWasDown = cDown;

    if (claudeUi.isActive()) {
        claudeUi.update();
        return;                     // pet / weather / udp 都跳过
    }

    // 原 pet / weather / udp 逻辑保持不变
    ...
}
```

### `config.h` 增加
```cpp
// Claude UI layout
constexpr int CLAUDE_INPUT_H   = 18;
constexpr int CLAUDE_REPLY_Y   = 0;
constexpr int CLAUDE_REPLY_H   = SCREEN_H - CLAUDE_INPUT_H;
constexpr int CLAUDE_LINE_H    = 9;            // 默认字体 6×8 + 1 行间距
constexpr int CLAUDE_COLS      = 40;           // 240 / 6
constexpr int CLAUDE_MAX_INPUT = 200;
constexpr int CLAUDE_REPLY_CAP = 4096;
```

---

## 4. 通信协议

### 请求
```http
POST /ask HTTP/1.1
Host: 192.168.1.42:8000
Content-Type: application/json

{"text": "what's in src/?"}
```

### 响应（chunked plain text）
```http
HTTP/1.1 200 OK
Content-Type: text/plain; charset=utf-8
Transfer-Encoding: chunked

<chunk>Looking at the
<chunk> src/ directory I see
<chunk> main.cpp, pet.cpp...
[EOF]
```

### 约定
- 每个 chunk 是 UTF-8 文本片段，无 framing（不是 SSE/JSON-lines），Cardputer 直接 append 到回复 buffer
- 错误用 `\n[error: ...]\n` 内联在流里，HTTP 状态码只表示建连成功与否
- 长度未知 → chunked transfer encoding（FastAPI `StreamingResponse` 默认行为）
- 不返回 JSON，省 parse 成本

### 边界
| 场景 | server | Cardputer |
|---|---|---|
| 正常完成 | 流自然结束 | EOF → IDLE，焦点回输入框 |
| Esc 取消 | — | `client.cancel()` 关 socket；server `StreamingResponse` 检测到客户端断开会停止消费 |
| Claude 报错 | 推 `\n[error: ...]\n` 后结束流 | 行首识别红色高亮 |
| WiFi 断 / server 不可达 | — | 设备 Error 状态，红条提示 |
| 60s 无新字节 | — | 当断流处理（不报 fatal） |

---

## 5. 设备端 UI 状态机与渲染

### 状态机

```
                    ┌──────────┐
            enter() │          │ exit() (按 c)
        ┌──────────►│   IDLE   │◄──────────┐
        │           └─────┬────┘           │
        │                 │ 字母键          │
        │                 ▼                │
        │           ┌──────────┐           │
   流结束/Esc  ┌────│  TYPING  │           │
        │     │    └─────┬────┘           │
        │     │          │ Enter           │
        │     │          ▼                 │
        │     │    ┌──────────┐ socket OK  │
        │     │    │ WAITING  │──────────┐ │
        │     │    └──────────┘          ▼ │
        │     │                    ┌──────────┐
        │     └───────────────────►│ STREAMING│
        │                          └─────┬────┘
        │     stream 出错 / HTTP 失败    │ 流结束
        │           ┌──────────┐         │
        └───────────│  ERROR   │◄────────┘
                    └──────────┘
```

### 键盘行为
| 状态 | 字母/数字 | Enter | Backspace | Esc | ↑↓ |
|---|---|---|---|---|---|
| IDLE | → TYPING + append | — | — | exit() | 滚动 |
| TYPING | append inputBuf | 发送 → WAITING | 删一字 | 清 input 回 IDLE | — |
| WAITING | 忽略 | — | — | cancel → IDLE | — |
| STREAMING | 忽略 | — | — | cancel → IDLE | 滚动 |
| ERROR | 任意键 → IDLE | — | — | exit() | — |

### 屏幕布局（240×135）
```
y=0    ┌────────────────────────────────────────┐
       │ 回复区 (滚动)                            │
       │ ...                                    │
y=117  ├────────────────────────────────────────┤
       │ > prompt here_                         │
y=135  └────────────────────────────────────────┘
```
- 默认字体（6×8）→ **40 列 × ~14 行可视**
- 输入行底色淡灰，光标 `_` 每 500ms 反色
- 自动卷到底；按 ↑/↓ 进入"手动滚动"模式，再 Enter 发新问题时跳回底
- 状态指示（右上角）：`WAITING` spinner / `STREAMING` `▼` / `ERROR` 红 `!`；右下角永久 WiFi 点（绿/红）

### 重绘策略
- 三个 dirty flag：`inputDirty` / `replyDirty` / `headerDirty`
- 流式新文本只 append-draw 到回复区底部
- 滚动用 M5GFX 的 `scroll` 或 `pushImage` 局部刷新

---

## 6. 错误处理矩阵

### 服务端
| 场景 | 行为 |
|---|---|
| `payload.text` 空 / 非字符串 | `400 missing 'text'`，不开流 |
| `ask_runner.query` 异常 | 流里推 `\n[error: <repr>]\n` 后结束 |
| 客户端断开 | `StreamingResponse` 自动取消 generator；SDK 收尾 `claude` 子进程 |
| 浏览器同时在 `/ws` 跑 | 用不同 runner，互不阻塞 |

### 设备端
| 场景 | 行为 |
|---|---|
| WiFi 没连上 | 进 Claude mode 立刻 ERROR "No WiFi" |
| TCP connect 失败 | 1 次 500ms 退避重试，仍失败 → ERROR "Server unreachable" |
| HTTP 非 200 | ERROR `"HTTP <code>"` + body 摘要 |
| 流中途断开 | 不当 fatal；replyBuf 空才报 ERROR |
| 60s 无字节 | 当断流处理 |
| Esc 取消 | `cancel()` 关 socket、回 IDLE，已收到部分保留 |
| `inputBuf > CLAUDE_MAX_INPUT` | 拒收新输入，输入行右下角闪红 `!` |
| `replyBuf > CLAUDE_REPLY_CAP` | 截掉最早 1KB，保留尾部最新 |
| 行首 `[error:` | 该行红色渲染 |
| 发送时 WiFi 已断 | 立刻 ERROR，不发请求 |

### 不防御（YAGNI）
- 设备多用户并发
- 鉴权
- ANSI / Markdown
- Unicode（屏幕字体限制）
- 设备端历史回看 UI

---

## 7. 测试策略

### 服务端 (`claude-web/`)
新增 `tests/test_ask_endpoint.py`：
- FastAPI `TestClient`
- mock `ask_runner.query` 返回预设事件序列
- 断言：chunked 响应 concat 后等于 `assistant_text` 拼接结果
- 测 400：空 text、非 str text
- 测 `error` 事件被转成 `[error: ...]` 内联

### 设备端 (`src/`)
- 纯函数（chunk 拼装、错误前缀检测）抽到 `claude_client_parsing.{h,cpp}`，在 `tests/host/` 下用 macOS g++ 编译运行（无需烧板）
- `claude_ui` 状态转换函数也抽到可 host 测的纯函数
- 渲染、键盘、HTTP 实际工作 → 手动验收

### 手动验收清单
1. 烧固件 + 启动 server，按 `c` 进 Claude 模式
2. 屏幕显示空回复区 + 输入行 `>`，右下角 WiFi 绿点
3. 输入 `say hi in 5 words` Enter → spinner → 几秒内开始流式打字
4. 接着问 `what did I just ask?` → 回复体现上下文（独立 ask session 续接生效）
5. 长回答 `tell me a 200 word story` → 文字超过 14 行后自动滚到底
6. 流中按 Esc → 立刻停笔，已显示部分保留
7. ↑/↓ 滚动回看
8. 关 server → 再发问 → ERROR "Server unreachable"，按任意键回 IDLE
9. 按 `c` 退出 → pet/weather 正常；按 `q` `w` 验证原功能未受影响

### 跳过
- 浏览器 e2e（本次不动 `/ws`）
- 性能 / 长时间稳定性
- ESP32 OTA、多设备

---

## 8. 决策摘要（用户已确认）

| 维度 | 决策 |
|---|---|
| Cardputer 角色 | 独立 HTTP 客户端（A） |
| 设备 UX | 第三个模式，`c` 键切换（A） |
| 输入 | 自由打字 QWERTY（A） |
| 响应传输 | chunked plain text 流（C） |
| Server 地址 | build flag `CLAUDE_SERVER` 写死（A） |
| Session | Cardputer 独立 session 文件（B） |
| 工具事件 | `/ask` 流里丢弃，只发 `assistant_text` |
| 错误传达 | `\n[error: ...]\n` 内联 |
| 分支 | `feat/cardputer-claude-client`（基于最新 main） |
