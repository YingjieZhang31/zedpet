# Python UDP 上位机 + 下位机通信设计

## 目标

为 zedpet 添加基于 UDP 的上下位机通信。上位机是 Python GUI 应用（tkinter），下位机是 ESP32 M5Cardputer（zedpet 固件）。上位机发送动作命令，下位机 ACK 后执行对应动画。

## 架构

```
Python GUI (host/)              UDP                ESP32 (src/)
┌──────────────────┐      port 19820            ┌──────────────────┐
│  6 个动作按钮     │ ── {"cmd":"happy"} ──▶     │  udp_server 收包  │
│  状态反馈栏       │ ◀─ {"ack":"happy"} ──     │  pet 执行动画     │
└──────────────────┘                             └──────────────────┘
```

## 通信协议

传输层：UDP，端口 19820

### 上位机 → 下位机（命令）

```json
{"cmd": "<action>"}
```

action 取值：`idle`, `happy`, `sleep`, `talk`, `stretch`, `look`

### 下位机 → 上位机（确认）

收到命令立即回复：
```json
{"ack": "<action>", "status": "ok"}
```

下位机 ACK 后执行对应动画。

### 超时与重试

- 上位机发命令后等待 2 秒
- 2 秒内未收到 ACK 则显示 "no response"

## 文件结构

```
zedpet/
├── src/                         # 下位机（ESP32 固件）
│   ├── main.cpp                 # 加 WiFi + UDP 初始化
│   ├── pet.h / pet.cpp          # 加 receiveCommand() 方法，现有代码不变
│   ├── sprites.h                # 不变
│   └── udp_server.h / udp_server.cpp  # 新增：UDP 监听的独立模块
├── host/                        # 新增：上位机（Python）
│   ├── main.py                  # tkinter GUI 入口
│   └── udp_client.py            # UDP 发送 + ACK 接收
├── platformio.ini               # 不变
```

## 下位机设计

### udp_server 模块

独立模块，负责：
- 连接 WiFi（SSID/PASS 写死在代码中，后续可改为配置文件）
- 初始化 WiFiUDP，监听 19820 端口
- `checkCommand()` — 非阻塞检查是否有新 JSON 包，有则解析 cmd 字段返回
- `sendAck(const char* cmd)` — 向上位机发送 ACK JSON
- 用 `millis()` 计时，每 500ms 检查一次收包

### Pet 类改动

在 `pet.h` 中新增一个公共方法：
```cpp
void receiveCommand(const char* cmd);
```

实现：将 6 个 cmd 字符串映射到对应 `PetState`，调用 `setState()`，停止自动循环。

`main.cpp` 的 `loop()` 中，如果 udp 收到新命令，先调 `pet.receiveCommand()` 改变状态，然后正常调 `pet.update()` 渲染。

### 自动循环保留

初始状态为 IDLE。没有 UDP 命令时，pet 保持 IDLE（不再自动循环切换，改为等待外部命令）。

## 上位机设计

### 技术选型

- Python 3 + tkinter（Python 标准库，无需额外安装）
- socket（Python 标准库，UDP 通信）

### GUI 布局

```
ZEDPET                           ← 窗口标题
┌──────────────────────┐
│     ZEDPET Control    │         ← 标题栏
├──────────────────────┤
│  [    🦞 IDLE      ]  │         ← 按钮，点击发送 {"cmd":"idle"}
│  [    😆 HAPPY     ]  │
│  [    😴 SLEEP     ]  │
│  [    🗣️ TALK     ]  │
│  [    🙆 STRETCH   ]  │
│  [    👀 LOOK      ]  │
├──────────────────────┤
│  Status: ACK happy ✓ │         ← 底部状态栏
└──────────────────────┘
```

### 交互流程

1. 启动时提示输入 ESP32 IP 地址
2. 点击按钮 → 发送 UDP JSON 命令到 ESP32:19820
3. 开启后台线程监听 UDP ACK
4. 收到 ACK → 状态栏显示 "ACK <action> ✓"
5. 2 秒未收到 → 状态栏显示 "no response from <ip>"

### 实现要点

- tkinter 主线程处理 GUI，`threading.Thread` 后台收 UDP ACK
- 用 `queue.Queue` 把收到的 ACK 从后台线程传给主线程更新 UI
- 按钮点击用 `socket.sendto()` 发 JSON
- 窗口大小固定 280×400
