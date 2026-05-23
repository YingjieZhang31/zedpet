# Claude Code Web UI 设计文档

**日期**：2026-05-23
**分支**：`feat/claude-web-ui`
**位置**：`claude-web/` 子目录

---

## 1. 目标与范围

构建一个**本地个人工具**：浏览器作为 Claude Code CLI 的前端，复用 Claude Code 的完整能力（读文件、改文件、跑 Bash、WebFetch 等），把终端体验换成富文本网页体验。

### 在范围内
- 单一固定项目目录（cwd 在启动时通过命令行参数指定）
- 完整工具集（YOLO 模式，所有工具调用自动放行）
- 单一会话，跨刷新自动续接（持久化最近 session_id）
- 流式渲染 assistant 文本与工具调用
- 工具调用展示支持两种模式（极简单行 / 折叠卡片），运行时可切换
- 预留 ESP32 客户端的轻量 HTTP 端点（`/ask`，POST 进同步文本出）

### 不在范围内（YAGNI）
- 多项目 / 多 cwd 切换
- 多会话列表 UI / 对话历史回看
- 用户鉴权（仅 localhost）
- 权限确认对话框（YOLO 模式）
- 并发多 tab 防御
- 自动化前端测试 / e2e

---

## 2. 整体架构

```
┌──────────────┐    WebSocket /ws    ┌──────────────────┐    subprocess     ┌─────────────┐
│  浏览器       │ ◄────JSON 事件────► │  FastAPI server  │ ◄──stream-json──► │ claude CLI  │
│ index.html   │                     │   (Python)       │                   │ (本地二进制) │
└──────────────┘                     │                  │                   └─────────────┘
                                     │  claude-agent-   │
                  HTTP POST /ask     │  sdk wrapper     │
ESP32 (将来) ──────────────────────► │                  │
                                     │  ┌────────────┐  │
                                     │  │session.json│  │  ← 持久化 session_id
                                     │  └────────────┘  │
                                     └──────────────────┘
```

### 职责
- **浏览器**：渲染对话、收发 WebSocket 事件、本地控制 UI 状态（展开/折叠、显示模式切换）
- **FastAPI 后端**：持有 `claude-agent-sdk` 实例；把 SDK 的 async iterator 事件序列化成 JSON 推到 WebSocket；接收用户消息喂给 SDK
- **session 持久化**：单文件 `.claude-web/session.json` 存最近 session_id；启动时读出来，下次 query 用 `resume=<id>`
- **ESP32 端点（预留，可后置实现）**：`POST /ask` 同步返回最终文本，复用同一个 SDK 实例

### 启动方式
```bash
cd claude-web
python server.py --cwd ~/java_proj/zedpet --port 8000
# 浏览器打开 http://localhost:8000
```

### 协议选型理由
浏览器走 **WebSocket**：未来需要"中途打断""只读模式切换""工具实时进度"等双向能力。
ESP32 走 **HTTP POST 或 SSE**（待 ESP32 接入时再定）：小屏不需要富事件，避免在 MCU 上引入 WebSocket 依赖。
两端用不同协议，互不妥协。

---

## 3. 项目结构与组件

```
zedpet/
├── claude-web/                  ← 新增子目录
│   ├── server.py                # FastAPI 入口
│   ├── claude_runner.py         # 封装 claude-agent-sdk
│   ├── session_store.py         # session_id 文件读写
│   ├── static/
│   │   └── index.html           # 单文件前端（Tailwind CDN + 原生 JS）
│   ├── pyproject.toml           # 依赖：fastapi, uvicorn, claude-agent-sdk
│   ├── .gitignore               # 忽略 .claude-web/、__pycache__
│   └── README.md                # 启动方式、命令行参数说明
└── .claude-web/                 # 运行时目录（git 忽略）
    └── session.json             # {"session_id": "abc123", "updated_at": "..."}
```

### `server.py`
- CLI 参数解析：`--cwd`（必填）、`--port`（默认 8000）、`--host`（默认 127.0.0.1）
- 启动时检测 `claude` 二进制存在；找不到打印安装指引并退出
- 路由：
  - `GET /` → `static/index.html`
  - `WS /ws` → 主交互通道
  - `POST /ask` → ESP32 用，MVP 可只占位

### `claude_runner.py`
- 对外暴露 `async def query(prompt: str) -> AsyncIterator[dict]`
- 内部构造 `ClaudeAgentOptions(cwd=..., permission_mode="bypassPermissions", resume=last_session_id)`
- SDK 事件 → 协议 JSON dict 的映射表见 §4
- 每轮结束从 `ResultMessage` 提取新的 session_id，调用 `session_store.save(...)`
- Resume 失败时清空 session 并以新会话重试一次（见 §5）

### `session_store.py`
- `load() -> str | None`：读 `.claude-web/session.json`，文件不存在返回 None
- `save(session_id: str)`：原子写（先写临时文件再 rename）
- `clear()`：删除文件，用于"新会话"按钮和 resume 失败兜底

### `static/index.html`
- 整页单文件：HTML 骨架 + Tailwind CDN + `<script>` 内含 WebSocket 客户端、消息渲染、UI 状态
- 不引入 React/Vue，原生 DOM 操作
- 顶部工具栏：显示模式开关（极简/卡片）、"新会话"按钮、连接状态指示灯
- 主区域：消息列表，按类型渲染（user 消息 / assistant 文本气泡 / tool_use 项）
- 底部：textarea + 发送按钮，Enter 发送、Shift+Enter 换行

---

## 4. WebSocket 事件协议

### 前端 → 后端

```jsonc
// 用户发消息
{"type": "user_message", "text": "帮我看看 src/main.cpp 做了什么"}

// (将来) 中途打断
{"type": "interrupt"}

// 开新会话（丢弃现有 session_id）
{"type": "new_session"}
```

### 后端 → 前端

| `type` | 含义 | 关键字段 |
|---|---|---|
| `ready` | 连接建立、宣告当前 session 状态 | `session_id`（可能为 null）、`cwd` |
| `assistant_text` | assistant 文本增量（流式） | `text`、`message_id` |
| `tool_use` | 工具调用开始 | `tool_use_id`、`name`、`input`（dict） |
| `tool_result` | 工具调用结果 | `tool_use_id`、`content`（str）、`is_error`（bool） |
| `turn_end` | 一轮对话结束 | `session_id`、`usage`（tokens） |
| `error` | 服务端异常 | `message` |

### 前端渲染规则
- `assistant_text`：按 `message_id` 定位气泡，追加文本；首次出现新建气泡
- `tool_use`：消息流中插入工具事件项，按当前显示模式（极简/卡片）渲染
- `tool_result`：按 `tool_use_id` 挂到对应工具项的展开区
- `turn_end`：输入框解锁、focus，显示 token 用量
- `error`：消息流中红色提示条

### 显示模式
- 工具栏开关：`极简 ⇄ 卡片`
- 仅切换 CSS class，不重新拉数据
- **极简**：单行 `🔧 Bash · ls src/ · 234B`，灰色小字
- **卡片**：圆角边框 + 浅灰背景，点击展开看完整 input/output；Edit 类做简单 diff 高亮（可选，MVP 可纯文本）

---

## 5. Session 持久化与错误处理

### Session 生命周期

```
服务启动
  ↓
session_store.load() → session_id 或 None
  ↓
首条用户消息到来
  ├─ 有 session_id：ClaudeAgentOptions(resume=session_id, cwd=...)
  └─ 没有：     ClaudeAgentOptions(cwd=...)
  ↓
SDK 跑完一轮，从 ResultMessage 拿到新 session_id
  ↓
session_store.save(new_session_id)
  ↓
下次消息继续 resume 这个 new_session_id
```

**要点**：每次 query 后 SDK 会给出一个（可能与旧的不同的）新 session_id，始终用最新的。

### 错误处理矩阵

| 场景 | 后端行为 | 前端表现 |
|---|---|---|
| `claude` CLI 未安装 | 启动时检测，找不到退出并打印安装指引 | 不会启动 |
| WebSocket 断开 | 正在跑的 query 继续跑完，事件丢弃 | 自动重连（指数退避），重连后收到新 `ready` |
| SDK 抛异常 | 捕获 → 推 `error` 事件 → 不关闭连接 | 红色条，输入框解锁 |
| `claude` 子进程崩溃 | SDK 内部 raise，按上一条处理 | 同上 |
| Resume 失败 | 清空 `session.json` → 以新会话重试一次 → 仍失败才报 `error` | 灰字提示"上次会话已过期，已开新会话" |
| 前端发非法 JSON | 推 `error` 事件，忽略消息 | 红色条 |

### 不防御的场景（YAGNI）
- 多 tab 并发
- 持久化对话回看
- localhost 之外的鉴权

---

## 6. 测试策略

### 单元测试（pytest）

| 模块 | 测什么 |
|---|---|
| `session_store.py` | load 不存在返回 None；save 后 load 一致；原子写不会损坏旧文件 |
| `claude_runner.py` 事件转换 | mock SDK 的 AsyncIterator，输入各种 block 类型 → 验证输出 dict 符合协议 |

### 集成测试（手动验收，写在 README）
1. 启动服务，浏览器打开，问"ls 当前目录"，验证 Bash 工具卡片、结果、assistant 总结都正常
2. 刷新页面再问一句，验证 session 续接生效
3. 杀掉 `claude` 进程，验证前端看到错误提示且能继续用
4. 切换"极简/卡片"模式，验证已有和新消息都按新模式渲染
5. 点"新会话"，验证下一条消息从零开始

### 跳过
- 前端自动化测试 / e2e
- 性能 / 压力测试

---

## 7. 决策摘要（用户已确认）

| 维度 | 决策 |
|---|---|
| 目标用户 | 纯本地个人工具（仅 localhost） |
| 能力范围 | 完整 Claude Code 工具集（C） |
| 后端 | Python + FastAPI |
| 前端 | 单 HTML 文件 + Tailwind CDN + 原生 JS |
| 权限模型 | YOLO（bypass permissions） |
| 会话 | 单会话、永远续接（A） |
| cwd | 启动时命令行参数（a） |
| 工具显示 | 极简 / 卡片 双模式可切换（a + b） |
| 协议 | 浏览器 WebSocket；ESP32 走独立 HTTP/SSE 端点（方案 1） |
| 子目录 | `claude-web/` |
| 分支 | `feat/claude-web-ui` |
