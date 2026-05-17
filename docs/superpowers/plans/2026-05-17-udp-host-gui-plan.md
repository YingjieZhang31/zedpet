# UDP 上位机 GUI 通信 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 zedpet 添加基于 UDP 的上下位机通信：Python tkinter GUI 上位机发送动作命令，ESP32 下位机 ACK 后执行动画。

**Architecture:** ESP32 新增 udp_server 模块处理 WiFi+UDP 收发包；Pet 类新增 receiveCommand() 接口，停止自动循环改为等待命令；Python 上位机用 tkinter GUI + socket UDP 通信。

**Tech Stack:** ESP32 Arduino WiFiUDP, Python 3 tkinter + socket + threading + queue

---

### Task 1: Pet 类改动 — 移除自动循环 + 添加命令接口

**Files:**
- Modify: `src/pet.h` — 添加 `receiveCommand()` 声明
- Modify: `src/pet.cpp` — 移除 `update()` 中的自动循环，实现 `receiveCommand()`

- [ ] **Step 1: 修改 pet.h，添加 receiveCommand 声明**

在 `/Users/zhangyingjie/Documents/PlatformIO/Projects/zedpet/src/pet.h` 的 Pet 类 public 部分，`begin()` 和 `update()` 之间添加：

```cpp
void receiveCommand(const char* cmd);  // handle UDP command, maps string to state
```

最终 pet.h：

```cpp
#pragma once
#include <cstdint>

enum class PetState : uint8_t {
    IDLE,
    HAPPY,
    SLEEP,
    TALK,
    STRETCH,
    LOOK,
    STATE_COUNT  // must be last — used for iteration bounds
};

class Pet {
public:
    void begin();
    void receiveCommand(const char* cmd);  // handle UDP command, maps string to state
    void update();  // call every frame from loop()

private:
    PetState state = PetState::IDLE;
    int frameIndex = 0;
    unsigned long stateStartTime = 0;
    unsigned long lastFrameTime = 0;
    int frameInterval = 500;  // ms per frame
    int stateDuration = 3000; // ms per state before switching

    void setState(PetState newState);
    void nextState();
    void drawSprite16(int x, int y, const uint16_t* data);
    void drawCharacter();
    const char* stateName(PetState s) const;
};
```

- [ ] **Step 2: 修改 pet.cpp update()，移除自动循环**

修改 `/Users/zhangyingjie/Documents/PlatformIO/Projects/zedpet/src/pet.cpp` 的 `update()` 函数，删除自动 `nextState()` 调用：

```cpp
void Pet::update() {
    unsigned long now = millis();

    // Advance animation frame
    if (now - lastFrameTime >= frameInterval) {
        lastFrameTime = now;
        frameIndex++;

        switch (state) {
            case PetState::IDLE:
            case PetState::LOOK:
                frameIndex %= IDLE_FRAME_COUNT;
                break;
            case PetState::HAPPY:
            case PetState::STRETCH:
                frameIndex %= HAPPY_FRAME_COUNT;
                break;
            case PetState::SLEEP:
                frameIndex %= SLEEP_FRAME_COUNT;
                break;
            case PetState::TALK:
                frameIndex %= TALK_FRAME_COUNT;
                break;
            default:
                break;
        }
    }

    canvas.fillScreen(TFT_BLACK);
    canvas.setTextColor(TFT_WHITE);
    canvas.setTextSize(2);
    canvas.setCursor(4, 2);
    canvas.print(stateName(state));
    drawCharacter();
    canvas.pushSprite(0, 0);
}
```

- [ ] **Step 3: 在 pet.cpp 中添加 receiveCommand() 实现**

在 `pet.cpp` 的 `begin()` 函数之后添加：

```cpp
void Pet::receiveCommand(const char* cmd) {
    PetState newState = PetState::IDLE;

    if (strcmp(cmd, "idle") == 0)      newState = PetState::IDLE;
    else if (strcmp(cmd, "happy") == 0)  newState = PetState::HAPPY;
    else if (strcmp(cmd, "sleep") == 0)  newState = PetState::SLEEP;
    else if (strcmp(cmd, "talk") == 0)   newState = PetState::TALK;
    else if (strcmp(cmd, "stretch") == 0) newState = PetState::STRETCH;
    else if (strcmp(cmd, "look") == 0)  newState = PetState::LOOK;
    else return;  // unknown command, ignore

    setState(newState);

    Serial.printf("[UDP] cmd=%s -> %s\n", cmd, stateName(newState));
}
```

需要在 `pet.cpp` 顶部添加 `#include <cstring>`，因为使用了 `strcmp`。

- [ ] **Step 4: 编译验证下位机改动**

```bash
cd /Users/zhangyingjie/Documents/PlatformIO/Projects/zedpet && pio run
```

Expected: BUILD SUCCESS

- [ ] **Step 5: Commit**

```bash
git -C /Users/zhangyingjie/Documents/PlatformIO/Projects/zedpet add src/pet.h src/pet.cpp
git -C /Users/zhangyingjie/Documents/PlatformIO/Projects/zedpet commit -m "feat: add receiveCommand, remove auto-cycle from pet"
```

---

### Task 2: UDP Server 模块（下位机）

**Files:**
- Create: `src/udp_server.h`
- Create: `src/udp_server.cpp`

- [ ] **Step 1: 创建 src/udp_server.h**

```cpp
#pragma once
#include <WiFi.h>
#include <WiFiUdp.h>

// ===== Configuration =====
// Change these to match your network
constexpr const char* WIFI_SSID = "your-ssid";
constexpr const char* WIFI_PASS = "your-password";
constexpr int UDP_PORT = 19820;

void udpServerBegin();
const char* udpCheckCommand();  // returns cmd string or nullptr if no command
void udpSendAck(const char* cmd);
```

- [ ] **Step 2: 创建 src/udp_server.cpp**

```cpp
#include "udp_server.h"

static WiFiUDP udp;
static char packetBuffer[256];

void udpServerBegin() {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.printf("[UDP] Connecting to WiFi %s", WIFI_SSID);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[UDP] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        udp.begin(UDP_PORT);
        Serial.printf("[UDP] Listening on port %d\n", UDP_PORT);
    } else {
        Serial.println("\n[UDP] WiFi connection failed!");
    }
}

const char* udpCheckCommand() {
    if (WiFi.status() != WL_CONNECTED) return nullptr;

    int packetSize = udp.parsePacket();
    if (packetSize <= 0) return nullptr;

    int len = udp.read(packetBuffer, sizeof(packetBuffer) - 1);
    if (len <= 0) return nullptr;
    packetBuffer[len] = '\0';

    Serial.printf("[UDP] Received: %s\n", packetBuffer);

    // Parse JSON: {"cmd":"happy"} -> "happy"
    // Simple string extraction — no JSON library needed
    const char* cmdStart = strstr(packetBuffer, "\"cmd\":\"");
    if (!cmdStart) return nullptr;
    cmdStart += 7;  // skip "cmd":"

    static char cmd[16];
    int i = 0;
    while (*cmdStart && *cmdStart != '\"' && i < 15) {
        cmd[i++] = *cmdStart++;
    }
    cmd[i] = '\0';

    return cmd;
}

void udpSendAck(const char* cmd) {
    if (WiFi.status() != WL_CONNECTED) return;

    char ack[64];
    snprintf(ack, sizeof(ack), "{\"ack\":\"%s\",\"status\":\"ok\"}", cmd);

    udp.beginPacket(udp.remoteIP(), udp.remotePort());
    udp.print(ack);
    udp.endPacket();

    Serial.printf("[UDP] Sent ACK: %s\n", ack);
}
```

- [ ] **Step 3: Commit**

```bash
git -C /Users/zhangyingjie/Documents/PlatformIO/Projects/zedpet add src/udp_server.h src/udp_server.cpp
git -C /Users/zhangyingjie/Documents/PlatformIO/Projects/zedpet commit -m "feat: add udp_server module for WiFi + JSON command handling"
```

---

### Task 3: main.cpp 集成 UDP

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: 修改 main.cpp，集成 UDP server 和 pet 命令**

将 `/Users/zhangyingjie/Documents/PlatformIO/Projects/zedpet/src/main.cpp` 的内容替换为：

```cpp
#include <M5Cardputer.h>
#include "pet.h"
#include "udp_server.h"

Pet pet;

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);  // true = init display
    M5Cardputer.Display.setRotation(1);        // landscape
    M5Cardputer.Display.fillScreen(TFT_BLACK);

    pet.begin();

    udpServerBegin();  // connect WiFi, start UDP
}

void loop() {
    M5Cardputer.update();

    const char* cmd = udpCheckCommand();
    if (cmd && cmd[0] != '\0') {
        udpSendAck(cmd);
        pet.receiveCommand(cmd);
    }

    pet.update();
    delay(16);  // ~60fps
}
```

- [ ] **Step 2: 编译验证**

```bash
cd /Users/zhangyingjie/Documents/PlatformIO/Projects/zedpet && pio run
```

Expected: BUILD SUCCESS, RAM < 15%, Flash < 20%

- [ ] **Step 3: Commit**

```bash
git -C /Users/zhangyingjie/Documents/PlatformIO/Projects/zedpet add src/main.cpp
git -C /Users/zhangyingjie/Documents/PlatformIO/Projects/zedpet commit -m "feat: integrate UDP server into main loop"
```

---

### Task 4: Python 上位机 — UDP Client

**Files:**
- Create: `host/udp_client.py`

- [ ] **Step 1: 创建 host/ 目录和 udp_client.py**

```bash
mkdir -p /Users/zhangyingjie/Documents/PlatformIO/Projects/zedpet/host
```

创建 `/Users/zhangyingjie/Documents/PlatformIO/Projects/zedpet/host/udp_client.py`：

```python
"""UDP client for communicating with zedpet ESP32 firmware."""
import socket

PORT = 19820
TIMEOUT = 2.0  # seconds to wait for ACK


def send_command(ip: str, cmd: str) -> str | None:
    """Send a command to the ESP32 and wait for ACK.

    Args:
        ip: ESP32 IP address
        cmd: one of idle/happy/sleep/talk/stretch/look

    Returns:
        The ACK action string, or None if timeout.
    """
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(TIMEOUT)

    import json
    payload = json.dumps({"cmd": cmd}).encode()
    sock.sendto(payload, (ip, PORT))

    try:
        data, addr = sock.recvfrom(256)
        response = json.loads(data.decode())
        return response.get("ack")
    except socket.timeout:
        return None
    finally:
        sock.close()
```

- [ ] **Step 2: Commit**

```bash
git -C /Users/zhangyingjie/Documents/PlatformIO/Projects/zedpet add host/udp_client.py
git -C /Users/zhangyingjie/Documents/PlatformIO/Projects/zedpet commit -m "feat: add Python UDP client"
```

---

### Task 5: Python 上位机 — tkinter GUI

**Files:**
- Create: `host/main.py`

- [ ] **Step 1: 创建 host/main.py**

创建 `/Users/zhangyingjie/Documents/PlatformIO/Projects/zedpet/host/main.py`：

```python
#!/usr/bin/env python3
"""zedpet Control — GUI host for M5Cardputer pet."""
import tkinter as tk
from tkinter import messagebox, simpledialog
import threading
import queue
import udp_client


ACTIONS = [
    ("IDLE",     "idle"),
    ("HAPPY",    "happy"),
    ("SLEEP",    "sleep"),
    ("TALK",     "talk"),
    ("STRETCH",  "stretch"),
    ("LOOK",     "look"),
]


class App:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("ZEDPET Control")
        self.root.resizable(False, False)

        self.esp_ip: str | None = None
        self.ack_queue = queue.Queue()

        self._build_ui()
        self._ask_ip()

    def _build_ui(self):
        # Title
        title = tk.Label(self.root, text="ZEDPET Control",
                         font=("Helvetica", 16, "bold"), pady=10)
        title.pack()

        # Action buttons
        frame = tk.Frame(self.root)
        frame.pack(padx=20, pady=10)

        labels = ["IDLE", "HAPPY", "SLEEP", "TALK", "STRETCH", "LOOK"]
        cmds = ["idle", "happy", "sleep", "talk", "stretch", "look"]

        for label, cmd in zip(labels, cmds):
            btn = tk.Button(frame, text=label, width=20, height=2,
                            font=("Helvetica", 12),
                            command=lambda c=cmd: self._send(c))
            btn.pack(pady=4)

        # Status bar
        self.status = tk.Label(self.root, text="Ready",
                               font=("Helvetica", 10), fg="gray",
                               pady=10)
        self.status.pack()

    def _ask_ip(self):
        ip = simpledialog.askstring(
            "ESP32 IP", "Enter ESP32 IP address:",
            parent=self.root)
        if ip:
            self.esp_ip = ip.strip()
            self.status.config(text=f"Target: {self.esp_ip}")
        else:
            self.status.config(text="No IP set — edit and restart")

    def _send(self, cmd: str):
        if not self.esp_ip:
            self.status.config(text="No IP set!", fg="red")
            return

        self.status.config(text=f"Sending {cmd}...", fg="gray")

        def task():
            ack = udp_client.send_command(self.esp_ip, cmd)
            self.ack_queue.put((cmd, ack))

        t = threading.Thread(target=task, daemon=True)
        t.start()
        self.root.after(100, self._check_ack)

    def _check_ack(self):
        try:
            cmd, ack = self.ack_queue.get_nowait()
            if ack:
                self.status.config(text=f"ACK {ack} OK", fg="green")
            else:
                self.status.config(text="No response", fg="red")
        except queue.Empty:
            self.root.after(100, self._check_ack)


def main():
    root = tk.Tk()
    app = App(root)
    root.mainloop()


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: 验证 Python 语法**

```bash
cd /Users/zhangyingjie/Documents/PlatformIO/Projects/zedpet/host && python3 -c "import py_compile; py_compile.compile('main.py', doraise=True); py_compile.compile('udp_client.py', doraise=True)"
```

Expected: 无输出（即无语法错误）

- [ ] **Step 3: Commit**

```bash
git -C /Users/zhangyingjie/Documents/PlatformIO/Projects/zedpet add host/main.py
git -C /Users/zhangyingjie/Documents/PlatformIO/Projects/zedpet commit -m "feat: add Python tkinter GUI host application"
```

---

### Task 6: 最终编译验证

- [ ] **Step 1: 编译下位机**

```bash
cd /Users/zhangyingjie/Documents/PlatformIO/Projects/zedpet && pio run
```

Expected: BUILD SUCCESS

- [ ] **Step 2: 验证项目结构**

```bash
cd /Users/zhangyingjie/Documents/PlatformIO/Projects/zedpet && git status && git log --oneline -5
```

Expected: 工作区干净，6 个新提交

- [ ] **Step 3: 提交（如有遗漏）**

如有未提交文件，add + commit。
