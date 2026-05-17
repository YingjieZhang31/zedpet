# Pet 渲染与动作移植设计

## 目标

将 ClawPuter 的 pet 渲染和 6 个基础动作状态移植到 zedpet（独立 M5Cardputer 固件），并在屏幕上循环播放所有动作动画，顶部显示当前状态名。

## 平台

- 硬件：M5Stack Cardputer (ESP32-S3)
- 框架：Arduino (M5Cardputer 库)
- 构建：PlatformIO

## 文件结构

```
zedpet/
├── platformio.ini          # M5Cardputer 平台配置
├── src/
│   ├── main.cpp            # 入口 + 状态循环控制
│   ├── sprites.h           # 从 ClawPuter 移植的 16×16 RGB565 精灵数据
│   ├── pet.h               # Pet 类声明
│   └── pet.cpp             # Pet 类实现
```

## 移植内容（从 ClawPuter）

### 完整移植

1. `sprites.h` — 8 帧精灵数据 + 帧查找数组（idle_frames/happy_frames/sleep_frames/talk_frames）
2. `drawSprite16()` — 3× 最近邻放大 + 透明色跳过 + 水平翻转
3. `CompanionState` 枚举 — IDLE, HAPPY, SLEEP, TALK, STRETCH, LOOK
4. `setState()` — 状态切换时重置帧索引并设置帧间隔
5. 帧动画驱动 — Timer tick 驱动帧索引递增取模
6. 位移叠加 — HAPPY 弹跳 (y-6)、LOOK 摇摆 (x±3)、STRETCH 上伸 (y-3)

### 不移植

- 天气系统、湿度系统、配件渲染、粒子效果
- 背景绘制（纯黑背景）
- 自动行为、睡眠 Z 动画
- UDP 广播、macOS 桌面同步
- 键盘交互、通知系统

## 状态循环

自动循环播放，每个状态定时切换：

```
IDLE(3s) → HAPPY(1.5s) → SLEEP(2s) → TALK(2s) → STRETCH(2s) → LOOK(3s) → 循环
```

## 帧动画参数

| 状态 | 帧数组 | 帧间隔 | 位移效果 |
|------|--------|--------|----------|
| IDLE | idle_frames[4] | 500ms | 无 |
| HAPPY | happy_frames[2] | 200ms | y-6 (偶数帧) |
| SLEEP | sleep_frames[1] | 1000ms | 无 |
| TALK | talk_frames[2] | 250ms | 无 |
| STRETCH | happy_frames[2] (复用) | 400ms | y-3 (偶数帧) |
| LOOK | idle_frames[4] (复用) | 300ms | x±3 (偶数帧) |

## UI 布局

```
┌─────────────────────────┐
│  IDLE                   │  ← 顶部 (0, 0)，textSize=2，白色
│                         │
│                         │
│       ┌──────┐          │
│       │ 48×48│          │  ← 精灵居中，3×缩放 (16×16 → 48×48)
│       │ 精灵 │          │
│       └──────┘          │
│                         │
└─────────────────────────┘
```

屏幕 240×135，精灵绘制区域 48×48，居中位置 x=96, y=(135-48)/2 ≈ 44。

## 实现要点

1. `Pet` 类封装状态机 + 渲染逻辑
2. `main.cpp` 中 `loop()` 调用 `pet.update()` 驱动帧动画，定时切换状态
3. 精灵始终居中，不移动，不翻转（始终面向右）
4. `drawSprite16` 使用 `M5Canvas.fillRect` 实现 3× 像素放大
5. 每帧先清屏 → 画状态名 → 画精灵 → push 到屏幕
