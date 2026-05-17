# Pet 渲染与动作移植 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 ClawPuter 的 pet 精灵渲染和 6 个基础动作状态移植到 zedpet，循环播放并显示状态名。

**Architecture:** 单文件 Pet 类封装状态机+渲染，main.cpp 驱动状态循环。精灵数据从 ClawPuter 完整复制到 sprites.h。

**Tech Stack:** PlatformIO, M5Cardputer, Arduino framework, ESP32-S3

---

### Task 1: 项目基础设施

**Files:**
- Create: `platformio.ini`
- Create: `lib/README` (占位，不需要额外库)
- Create: `src/` 目录已存在

- [ ] **Step 1: 创建 platformio.ini**

```ini
[platformio]
default_envs = m5stack-cardputer

[env:m5stack-cardputer]
platform = espressif32
board = m5stack-stamps3
framework = arduino
lib_deps =
    m5stack/M5Cardputer@^1.0.2
monitor_speed = 115200
upload_port = /dev/cu.usbmodem3101
monitor_port = /dev/cu.usbmodem3101
```

- [ ] **Step 2: 验证项目结构**

```bash
cd /Users/zhangyingjie/Documents/PlatformIO/Projects/zedpet
pio run --target compiledb  # 应该能解析配置，即使没源文件
```

Expected: PlatformIO 成功初始化项目，无报错。

- [ ] **Step 3: Commit**

```bash
git add platformio.ini lib/
git commit -m "chore: add PlatformIO project config for M5Cardputer"
```

---

### Task 2: 移植精灵数据

**Files:**
- Create: `src/sprites.h`

从 ClawPuter 的 `src/sprites.h` 复制精灵数据，只保留核心部分（去掉配件/天气相关的颜色常量）。

- [ ] **Step 1: 创建 src/sprites.h**

```cpp
#pragma once
#include <cstdint>

// Sprite dimensions
constexpr int CHAR_W = 16;
constexpr int CHAR_H = 16;
constexpr int CHAR_SCALE = 3;
constexpr int CHAR_DRAW_W = CHAR_W * CHAR_SCALE;  // 48
constexpr int CHAR_DRAW_H = CHAR_H * CHAR_SCALE;  // 48

// Color palette — OpenClaw lobster
constexpr uint16_t _ = 0x0000;                    // Transparent (never drawn)
constexpr uint16_t K = 0x0000;                    // Black outline
constexpr uint16_t W = 0xFFFF;                    // White eyes
constexpr uint16_t R = 0xC128;                    // Red main body (210,50,40)
constexpr uint16_t D = 0xA0C3;                    // Dark red shadow (160,30,25)
constexpr uint16_t H = 0xFA2A;                    // Highlight red (240,100,80)
constexpr uint16_t O = 0xE46C;                    // Orange claws inner (230,140,60)
constexpr uint16_t E = 0x1082;                    // Eye pupil (20,20,20)
constexpr uint16_t C = 0xC123;                    // Claw red (190,40,35)
constexpr uint16_t T = 0xC30A;                    // Tail/legs (180,60,50)

// ── Idle frame 1: eyes open, claws down ──
const uint16_t sprite_idle1[CHAR_W * CHAR_H] = {
    _, _, K, _, _, _, _, _, _, _, _, _, _, K, _, _,
    _, _, _, K, _, _, _, _, _, _, _, _, K, _, _, _,
    _, _, _, _, K, K, K, K, K, K, K, K, _, _, _, _,
    _, _, _, K, R, R, R, R, R, R, R, R, K, _, _, _,
    _, _, K, R, R, W, W, R, R, W, W, R, R, K, _, _,
    _, _, K, R, R, W, E, R, R, W, E, R, R, K, _, _,
    _, _, K, R, R, R, R, R, R, R, R, R, R, K, _, _,
    _, _, _, K, R, R, R, O, O, R, R, R, K, _, _, _,
    _, _, _, K, R, R, R, R, R, R, R, R, K, _, _, _,
    _, K, K, _, K, R, R, R, R, R, R, K, _, K, K, _,
    K, C, C, K, _, K, D, D, D, D, K, _, K, C, C, K,
    K, C, C, K, _, K, D, D, D, D, K, _, K, C, C, K,
    _, K, K, _, _, K, D, D, D, D, K, _, _, K, K, _,
    _, _, _, _, K, T, K, D, D, K, T, K, _, _, _, _,
    _, _, _, _, K, T, K, T, T, K, T, K, _, _, _, _,
    _, _, _, _, _, K, _, K, K, _, K, _, _, _, _, _,
};

// ── Idle frame 2: blink ──
const uint16_t sprite_idle2[CHAR_W * CHAR_H] = {
    _, _, K, _, _, _, _, _, _, _, _, _, _, K, _, _,
    _, _, _, K, _, _, _, _, _, _, _, _, K, _, _, _,
    _, _, _, _, K, K, K, K, K, K, K, K, _, _, _, _,
    _, _, _, K, R, R, R, R, R, R, R, R, K, _, _, _,
    _, _, K, R, R, R, R, R, R, R, R, R, R, K, _, _,
    _, _, K, R, R, K, K, R, R, K, K, R, R, K, _, _,
    _, _, K, R, R, R, R, R, R, R, R, R, R, K, _, _,
    _, _, _, K, R, R, R, O, O, R, R, R, K, _, _, _,
    _, _, _, K, R, R, R, R, R, R, R, R, K, _, _, _,
    _, K, K, _, K, R, R, R, R, R, R, K, _, K, K, _,
    K, C, C, K, _, K, D, D, D, D, K, _, K, C, C, K,
    K, C, C, K, _, K, D, D, D, D, K, _, K, C, C, K,
    _, K, K, _, _, K, D, D, D, D, K, _, _, K, K, _,
    _, _, _, _, K, T, K, D, D, K, T, K, _, _, _, _,
    _, _, _, _, K, T, K, T, T, K, T, K, _, _, _, _,
    _, _, _, _, _, K, _, K, K, _, K, _, _, _, _, _,
};

// ── Idle frame 3: body bob (1px down) ──
const uint16_t sprite_idle3[CHAR_W * CHAR_H] = {
    _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
    _, _, K, _, _, _, _, _, _, _, _, _, _, K, _, _,
    _, _, _, K, _, _, _, _, _, _, _, _, K, _, _, _,
    _, _, _, _, K, K, K, K, K, K, K, K, _, _, _, _,
    _, _, _, K, R, R, R, R, R, R, R, R, K, _, _, _,
    _, _, K, R, R, W, W, R, R, W, W, R, R, K, _, _,
    _, _, K, R, R, W, E, R, R, W, E, R, R, K, _, _,
    _, _, K, R, R, R, R, R, R, R, R, R, R, K, _, _,
    _, _, _, K, R, R, R, O, O, R, R, R, K, _, _, _,
    _, _, _, K, R, R, R, R, R, R, R, R, K, _, _, _,
    _, K, K, _, K, R, R, R, R, R, R, K, _, K, K, _,
    K, C, C, K, _, K, D, D, D, D, K, _, K, C, C, K,
    K, C, C, K, _, K, D, D, D, D, K, _, K, C, C, K,
    _, K, K, _, _, K, D, D, D, D, K, _, _, K, K, _,
    _, _, _, _, K, T, K, D, D, K, T, K, _, _, _, _,
    _, _, _, _, _, K, _, K, K, _, K, _, _, _, _, _,
};

// ── Happy frame 1: claws up! ──
const uint16_t sprite_happy1[CHAR_W * CHAR_H] = {
    K, C, C, K, _, _, _, _, _, _, _, _, K, C, C, K,
    K, C, C, K, _, _, _, _, _, _, _, _, K, C, C, K,
    _, K, K, _, K, K, K, K, K, K, K, K, _, K, K, _,
    _, _, _, K, R, R, R, R, R, R, R, R, K, _, _, _,
    _, _, K, R, R, H, H, R, R, H, H, R, R, K, _, _,
    _, _, K, R, R, H, E, R, R, H, E, R, R, K, _, _,
    _, _, K, R, R, R, R, R, R, R, R, R, R, K, _, _,
    _, _, _, K, R, R, O, K, K, O, R, R, K, _, _, _,
    _, _, _, K, R, R, R, R, R, R, R, R, K, _, _, _,
    _, _, _, _, K, R, R, R, R, R, R, K, _, _, _, _,
    _, _, _, _, _, K, D, D, D, D, K, _, _, _, _, _,
    _, _, _, _, _, K, D, D, D, D, K, _, _, _, _, _,
    _, _, _, _, _, K, D, D, D, D, K, _, _, _, _, _,
    _, _, _, _, _, _, K, K, K, K, _, _, _, _, _, _,
    _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
    _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
};

// ── Happy frame 2: claws spread wide ──
const uint16_t sprite_happy2[CHAR_W * CHAR_H] = {
    _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
    K, C, K, _, K, K, K, K, K, K, K, K, _, K, C, K,
    K, C, C, K, R, R, R, R, R, R, R, R, K, C, C, K,
    _, K, K, K, R, R, R, R, R, R, R, R, K, K, K, _,
    _, _, K, R, R, H, H, R, R, H, H, R, R, K, _, _,
    _, _, K, R, R, H, E, R, R, H, E, R, R, K, _, _,
    _, _, K, R, R, R, R, R, R, R, R, R, R, K, _, _,
    _, _, _, K, R, R, O, K, K, O, R, R, K, _, _, _,
    _, _, _, K, R, R, R, R, R, R, R, R, K, _, _, _,
    _, _, _, _, K, R, R, R, R, R, R, K, _, _, _, _,
    _, _, _, _, _, K, D, D, D, D, K, _, _, _, _, _,
    _, _, _, _, _, K, D, D, D, D, K, _, _, _, _, _,
    _, _, _, _, K, T, K, D, D, K, T, K, _, _, _, _,
    _, _, _, _, K, T, K, T, T, K, T, K, _, _, _, _,
    _, _, _, _, _, K, _, K, K, _, K, _, _, _, _, _,
    _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
};

// ── Sleep frame 1: eyes closed, claws tucked ──
const uint16_t sprite_sleep1[CHAR_W * CHAR_H] = {
    _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _,
    _, _, _, _, K, K, K, K, K, K, K, K, _, _, _, _,
    _, _, _, K, R, R, R, R, R, R, R, R, K, _, _, _,
    _, _, _, K, R, R, R, R, R, R, R, R, K, _, _, _,
    _, _, K, R, R, R, R, R, R, R, R, R, R, K, _, _,
    _, _, K, R, R, K, K, R, R, K, K, R, R, K, _, _,
    _, _, K, R, R, R, R, R, R, R, R, R, R, K, _, _,
    _, _, _, K, R, R, R, R, R, R, R, R, K, _, _, _,
    _, _, _, K, R, R, R, R, R, R, R, R, K, _, _, _,
    _, K, K, _, K, R, R, R, R, R, R, K, _, K, K, _,
    K, C, C, K, _, K, D, D, D, D, K, _, K, C, C, K,
    K, C, C, K, _, K, D, D, D, D, K, _, K, C, C, K,
    _, K, K, _, _, K, D, D, D, D, K, _, _, K, K, _,
    _, _, _, _, K, T, K, D, D, K, T, K, _, _, _, _,
    _, _, _, _, K, T, K, T, T, K, T, K, _, _, _, _,
    _, _, _, _, _, K, _, K, K, _, K, _, _, _, _, _,
};

// ── Talk frame 1: mouth open ──
const uint16_t sprite_talk1[CHAR_W * CHAR_H] = {
    _, _, K, _, _, _, _, _, _, _, _, _, _, K, _, _,
    _, _, _, K, _, _, _, _, _, _, _, _, K, _, _, _,
    _, _, _, _, K, K, K, K, K, K, K, K, _, _, _, _,
    _, _, _, K, R, R, R, R, R, R, R, R, K, _, _, _,
    _, _, K, R, R, W, W, R, R, W, W, R, R, K, _, _,
    _, _, K, R, R, W, E, R, R, W, E, R, R, K, _, _,
    _, _, K, R, R, R, R, R, R, R, R, R, R, K, _, _,
    _, _, _, K, R, R, K, O, O, K, R, R, K, _, _, _,
    _, _, _, K, R, R, K, K, K, K, R, R, K, _, _, _,
    _, K, K, _, K, R, R, R, R, R, R, K, _, K, K, _,
    K, C, C, K, _, K, D, D, D, D, K, _, K, C, C, K,
    K, C, C, K, _, K, D, D, D, D, K, _, K, C, C, K,
    _, K, K, _, _, K, D, D, D, D, K, _, _, K, K, _,
    _, _, _, _, K, T, K, D, D, K, T, K, _, _, _, _,
    _, _, _, _, K, T, K, T, T, K, T, K, _, _, _, _,
    _, _, _, _, _, K, _, K, K, _, K, _, _, _, _, _,
};

// ── Talk frame 2: mouth closed ──
const uint16_t sprite_talk2[CHAR_W * CHAR_H] = {
    _, _, K, _, _, _, _, _, _, _, _, _, _, K, _, _,
    _, _, _, K, _, _, _, _, _, _, _, _, K, _, _, _,
    _, _, _, _, K, K, K, K, K, K, K, K, _, _, _, _,
    _, _, _, K, R, R, R, R, R, R, R, R, K, _, _, _,
    _, _, K, R, R, W, W, R, R, W, W, R, R, K, _, _,
    _, _, K, R, R, W, E, R, R, W, E, R, R, K, _, _,
    _, _, K, R, R, R, R, R, R, R, R, R, R, K, _, _,
    _, _, _, K, R, R, K, K, K, K, R, R, K, _, _, _,
    _, _, _, K, R, R, R, R, R, R, R, R, K, _, _, _,
    _, K, K, _, K, R, R, R, R, R, R, K, _, K, K, _,
    K, C, C, K, _, K, D, D, D, D, K, _, K, C, C, K,
    K, C, C, K, _, K, D, D, D, D, K, _, K, C, C, K,
    _, K, K, _, _, K, D, D, D, D, K, _, _, K, K, _,
    _, _, _, _, K, T, K, D, D, K, T, K, _, _, _, _,
    _, _, _, _, K, T, K, T, T, K, T, K, _, _, _, _,
    _, _, _, _, _, K, _, K, K, _, K, _, _, _, _, _,
};

// Sprite lookup arrays
const uint16_t* const idle_frames[] = { sprite_idle1, sprite_idle2, sprite_idle1, sprite_idle3 };
constexpr int IDLE_FRAME_COUNT = 4;

const uint16_t* const happy_frames[] = { sprite_happy1, sprite_happy2 };
constexpr int HAPPY_FRAME_COUNT = 2;

const uint16_t* const sleep_frames[] = { sprite_sleep1 };
constexpr int SLEEP_FRAME_COUNT = 1;

const uint16_t* const talk_frames[] = { sprite_talk1, sprite_talk2 };
constexpr int TALK_FRAME_COUNT = 2;
```

- [ ] **Step 2: 验证 sprites.h 编译通过**

```bash
echo '#include "sprites.h"' > /tmp/test_sprites.cpp
# 我们将在此计划中最后才编译整个项目
```

- [ ] **Step 3: Commit**

```bash
git add src/sprites.h
git commit -m "feat: port ClawPuter sprite data (8 frames, 16x16 RGB565)"
```

---

### Task 3: Pet 类声明

**Files:**
- Create: `src/pet.h`

- [ ] **Step 1: 创建 src/pet.h**

```cpp
#pragma once
#include <M5Cardputer.h>

enum class PetState : uint8_t {
    IDLE,
    HAPPY,
    SLEEP,
    TALK,
    STRETCH,
    LOOK,
    STATE_COUNT
};

class Pet {
public:
    void begin();
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

- [ ] **Step 2: Commit**

```bash
git add src/pet.h
git commit -m "feat: add Pet class declaration with 6-state enum"
```

---

### Task 4: Pet 类实现

**Files:**
- Create: `src/pet.cpp`

- [ ] **Step 1: 创建 src/pet.cpp**

```cpp
#include "pet.h"
#include "sprites.h"

// Screen dimensions
constexpr int SCREEN_W = 240;
constexpr int SCREEN_H = 135;

// Sprite centered on screen
constexpr int SPRITE_X = (SCREEN_W - CHAR_DRAW_W) / 2;  // 96
constexpr int SPRITE_Y = (SCREEN_H - CHAR_DRAW_H) / 2;  // 44

// Helper to get display reference
static M5Canvas& display() { return M5Cardputer.Display; }

void Pet::begin() {
    setState(PetState::IDLE);
}

void Pet::setState(PetState newState) {
    state = newState;
    frameIndex = 0;
    stateStartTime = millis();
    lastFrameTime = 0;  // force immediate first frame

    switch (state) {
        case PetState::IDLE:
            frameInterval = 500;
            stateDuration = 3000;
            break;
        case PetState::HAPPY:
            frameInterval = 200;
            stateDuration = 1500;
            break;
        case PetState::SLEEP:
            frameInterval = 1000;
            stateDuration = 2000;
            break;
        case PetState::TALK:
            frameInterval = 250;
            stateDuration = 2000;
            break;
        case PetState::STRETCH:
            frameInterval = 400;
            stateDuration = 2000;
            break;
        case PetState::LOOK:
            frameInterval = 300;
            stateDuration = 3000;
            break;
        default:
            break;
    }
}

void Pet::nextState() {
    int next = (static_cast<int>(state) + 1) % static_cast<int>(PetState::STATE_COUNT);
    setState(static_cast<PetState>(next));
}

const char* Pet::stateName(PetState s) const {
    switch (s) {
        case PetState::IDLE:     return "IDLE";
        case PetState::HAPPY:    return "HAPPY";
        case PetState::SLEEP:    return "SLEEP";
        case PetState::TALK:     return "TALK";
        case PetState::STRETCH:  return "STRETCH";
        case PetState::LOOK:     return "LOOK";
        default:                 return "???";
    }
}

void Pet::update() {
    unsigned long now = millis();

    // Check if state duration expired → switch to next state
    if (now - stateStartTime >= stateDuration) {
        nextState();
        return;  // setState reset our timers, draw next frame
    }

    // Advance animation frame
    if (now - lastFrameTime >= frameInterval) {
        lastFrameTime = now;
        frameIndex++;

        // Wrap frame index based on state
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

    // Draw frame
    display().fillScreen(TFT_BLACK);

    // State name at top-left
    display().setTextColor(TFT_WHITE);
    display().setTextSize(2);
    display().setCursor(4, 2);
    display().print(stateName(state));

    // Character centered
    drawCharacter();

    display().pushSprite(0, 0);
}

void Pet::drawSprite16(int x, int y, const uint16_t* data) {
    for (int py = 0; py < CHAR_H; py++) {
        for (int px = 0; px < CHAR_W; px++) {
            uint16_t color = data[py * CHAR_W + px];
            if (color != 0x0000) {  // transparent skip
                display().fillRect(x + px * CHAR_SCALE, y + py * CHAR_SCALE,
                               CHAR_SCALE, CHAR_SCALE, color);
            }
        }
    }
}

void Pet::drawCharacter() {
    const uint16_t* frame = nullptr;

    switch (state) {
        case PetState::IDLE:
        case PetState::LOOK:
            frame = idle_frames[frameIndex % IDLE_FRAME_COUNT];
            break;
        case PetState::HAPPY:
        case PetState::STRETCH:
            frame = happy_frames[frameIndex % HAPPY_FRAME_COUNT];
            break;
        case PetState::SLEEP:
            frame = sleep_frames[frameIndex % SLEEP_FRAME_COUNT];
            break;
        case PetState::TALK:
            frame = talk_frames[frameIndex % TALK_FRAME_COUNT];
            break;
    }

    if (!frame) return;

    int xOffset = 0;
    int yOffset = 0;

    // Displacement effects
    if (state == PetState::HAPPY && frameIndex % 2 == 0) {
        yOffset = -6;
    }
    if (state == PetState::LOOK) {
        xOffset = (frameIndex % 2 == 0) ? -3 : 3;
    }
    if (state == PetState::STRETCH && frameIndex % 2 == 0) {
        yOffset = -3;
    }

    drawSprite16(SPRITE_X + xOffset, SPRITE_Y + yOffset, frame);
}
```

- [ ] **Step 2: Commit**

```bash
git add src/pet.cpp
git commit -m "feat: implement Pet class (state machine + sprite rendering)"
```

---

### Task 5: main.cpp 入口

**Files:**
- Create: `src/main.cpp`

- [ ] **Step 1: 创建 src/main.cpp**

```cpp
#include <M5Cardputer.h>
#include "pet.h"

Pet pet;

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);  // true = init display
    M5Cardputer.Display.setRotation(1);        // landscape
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    M5Cardputer.Display.pushSprite(0, 0);
    pet.begin();
}

void loop() {
    M5Cardputer.update();
    pet.update();
    delay(16);  // ~60fps
}
```

- [ ] **Step 2: 编译项目**

```bash
cd /Users/zhangyingjie/Documents/PlatformIO/Projects/zedpet
pio run
```

Expected: BUILD SUCCESS, 无编译错误。

- [ ] **Step 3: Commit**

```bash
git add src/main.cpp
git commit -m "feat: add main entry point with 6-state pet loop"
```

---

### Task 6: 最终验证

- [ ] **Step 1: 完整编译并检查固件大小**

```bash
cd /Users/zhangyingjie/Documents/PlatformIO/Projects/zedpet
pio run
```

Expected: BUILD SUCCESS, RAM < 80%, Flash < 80%.

- [ ] **Step 2: 验证 git 状态**

```bash
git status
git log --oneline
```

Expected: 工作区干净，所有更改已提交。

