# 天气系统迁移 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 ClawPuter 天气系统移植到 zedpet，支持 w 键循环切换 8 种天气，包含天空背景、天体（太阳/月亮/星星）、天气粒子（雨/雪/雾/闪电）、地面和宠物配件。

**Architecture:** 新增 weather.h/cpp 模块独立管理天气状态和渲染；pet.cpp 重构渲染顺序为先天气层再 UI/角色层；main.cpp 加 w 键处理。

**Tech Stack:** ESP32 Arduino, M5Cardputer M5Canvas

---

### Task 1: weather.h — 天气模块头文件

**Files:**
- Create: `src/weather.h`

- [ ] **Step 1: 创建 src/weather.h**

```cpp
#pragma once
#include <cstdint>
#include <M5Cardputer.h>

// ===== Weather Types =====

enum class WeatherType : uint8_t {
    CLEAR = 0,
    PARTLY_CLOUDY,
    OVERCAST,
    FOG,
    DRIZZLE,
    RAIN,
    SNOW,
    THUNDER,
    WEATHER_COUNT
};

enum class AccessoryType : uint8_t {
    NONE,
    SUNGLASSES,
    UMBRELLA,
    SNOW_HAT,
    MASK
};

// ===== API =====

void weatherBegin();
void weatherNext();
void weatherDraw(M5Canvas& canvas);
AccessoryType weatherGetAccessory();
const char* weatherTypeName();
int weatherCurrentHour();
```

---

### Task 2: weather.cpp — 天气模块实现（上）：数据结构 + 背景 + 天体

**Files:**
- Create: `src/weather.cpp`

- [ ] **Step 1: 创建文件基础结构和常量**

```cpp
#include "weather.h"
#include "sprites.h"
#include <M5Cardputer.h>
#include <time.h>

// ===== Screen Layout =====
constexpr int SCREEN_W = 240;
constexpr int SCREEN_H = 135;
constexpr int GROUND_Y = SCREEN_H - 28;

// ===== Sky Colors =====
static uint16_t _rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
#define C(r,g,b) _rgb565(r,g,b)

static const char* WEATHER_NAMES[] = {
    "Clear", "Cloudy", "Overcast", "Fog",
    "Drizzle", "Rain", "Snow", "Thunder"
};
```

- [ ] **Step 2: 添加天气状态和工具函数**

```cpp
// ===== State =====
static WeatherType currentWeather = WeatherType::CLEAR;
static bool particlesInit = false;

// ===== Blend Utility =====
static uint16_t blendRGB565(uint16_t a, uint16_t b, uint8_t t) {
    uint8_t r1 = (a >> 11) & 0x1F, g1 = (a >> 5) & 0x3F, b1 = a & 0x1F;
    uint8_t r2 = (b >> 11) & 0x1F, g2 = (b >> 5) & 0x3F, b2 = b & 0x1F;
    uint8_t r = r1 + ((int)(r2 - r1) * t / 255);
    uint8_t g = g1 + ((int)(g2 - g1) * t / 255);
    uint8_t bl = b1 + ((int)(b2 - b1) * t / 255);
    return (r << 11) | (g << 5) | bl;
}

// ===== API =====

void weatherBegin() {
    currentWeather = WeatherType::CLEAR;
    particlesInit = false;
}

void weatherNext() {
    int n = (static_cast<int>(currentWeather) + 1) % static_cast<int>(WeatherType::WEATHER_COUNT);
    currentWeather = static_cast<WeatherType>(n);
    particlesInit = false;
    Serial.printf("[WEATHER] -> %s\n", WEATHER_NAMES[n]);
}

const char* weatherTypeName() {
    return WEATHER_NAMES[static_cast<int>(currentWeather)];
}

int weatherCurrentHour() {
    struct tm ti;
    if (getLocalTime(&ti, 0)) return ti.tm_hour;
    return 12;  // default noon
}

AccessoryType weatherGetAccessory() {
    switch (currentWeather) {
        case WeatherType::CLEAR:
        case WeatherType::PARTLY_CLOUDY: return AccessoryType::SUNGLASSES;
        case WeatherType::RAIN:
        case WeatherType::DRIZZLE:
        case WeatherType::THUNDER:      return AccessoryType::UMBRELLA;
        case WeatherType::SNOW:         return AccessoryType::SNOW_HAT;
        case WeatherType::FOG:
        case WeatherType::OVERCAST:     return AccessoryType::MASK;
        default:                        return AccessoryType::NONE;
    }
}
```

- [ ] **Step 3: 添加星空和粒子结构体**

```cpp
// ===== Stars =====
constexpr int MAX_STARS = 12;
struct Star { int x, y; bool visible; };
static Star stars[MAX_STARS];
static unsigned long lastStarTwinkle = 0;

static void initStars() {
    for (int i = 0; i < MAX_STARS; i++) {
        stars[i].x = random(10, SCREEN_W - 10);
        stars[i].y = random(5, GROUND_Y - 60);
        stars[i].visible = random(2) == 0;
    }
}

// ===== Particles =====
constexpr int MAX_RAIN = 15;
constexpr int MAX_SNOW = 15;
struct RainDrop { int16_t x, y; };
struct Snowflake { int16_t x, y; int8_t drift; };
static RainDrop rainDrops[MAX_RAIN];
static Snowflake snowflakes[MAX_SNOW];
static unsigned long lastThunderFlash = 0;
static bool thunderFlashing = false;

static void initParticles() {
    for (int i = 0; i < MAX_RAIN; i++) {
        rainDrops[i].x = random(SCREEN_W);
        rainDrops[i].y = random(GROUND_Y);
    }
    for (int i = 0; i < MAX_SNOW; i++) {
        snowflakes[i].x = random(SCREEN_W);
        snowflakes[i].y = random(GROUND_Y);
        snowflakes[i].drift = random(3) - 1;
    }
    particlesInit = true;
}
```

- [ ] **Step 4: 添加天体绘制函数**

```cpp
// ===== Sky Elements =====

static void drawSun(M5Canvas& canvas) {
    // Sun at top-right
    canvas.fillCircle(200, 18, 10, C(255, 220, 60));
    for (int i = 0; i < 8; i++) {
        float angle = i * 0.785f;
        int x1 = 200 + cos(angle) * 13;
        int y1 = 18 + sin(angle) * 13;
        int x2 = 200 + cos(angle) * 16;
        int y2 = 18 + sin(angle) * 16;
        canvas.drawLine(x1, y1, x2, y2, C(255, 220, 60));
    }
}

static void drawClouds(M5Canvas& canvas, uint16_t color) {
    canvas.fillRoundRect(40, 12, 24, 8, 4, color);
    canvas.fillRoundRect(48, 8, 16, 8, 4, color);
    canvas.fillRoundRect(130, 18, 20, 6, 3, color);
    canvas.fillRoundRect(136, 14, 14, 6, 3, color);
}

static void drawStars(M5Canvas& canvas) {
    if (millis() - lastStarTwinkle > 800) {
        lastStarTwinkle = millis();
        stars[random(MAX_STARS)].visible = !stars[random(MAX_STARS)].visible;
    }
    for (int i = 0; i < MAX_STARS; i++) {
        if (stars[i].visible) {
            canvas.drawPixel(stars[i].x, stars[i].y, C(200, 200, 140));
            if (i % 3 == 0) {
                canvas.drawPixel(stars[i].x - 1, stars[i].y, C(200, 200, 140));
                canvas.drawPixel(stars[i].x + 1, stars[i].y, C(200, 200, 140));
                canvas.drawPixel(stars[i].x, stars[i].y - 1, C(200, 200, 140));
                canvas.drawPixel(stars[i].x, stars[i].y + 1, C(200, 200, 140));
            }
        }
    }
}

static void drawMoon(M5Canvas& canvas, uint16_t skyColor) {
    canvas.fillCircle(30, 20, 10, C(220, 220, 200));
    canvas.fillCircle(34, 17, 9, skyColor);
}

static void drawDarkClouds(M5Canvas& canvas) {
    uint16_t c = C(150, 150, 160);
    canvas.fillRoundRect(30, 10, 30, 10, 5, c);
    canvas.fillRoundRect(40, 5, 20, 10, 5, c);
    canvas.fillRoundRect(120, 15, 28, 9, 4, c);
    canvas.fillRoundRect(128, 9, 18, 9, 4, c);
    canvas.fillRoundRect(180, 12, 26, 8, 4, c);
}
```

---

### Task 3: weather.cpp — 天气模块实现（下）：粒子 + 背景主函数 + 配件

**Files:**
- Modify: `src/weather.cpp` (追加代码)

- [ ] **Step 1: 添加粒子绘制函数**

继续追加到 `src/weather.cpp`：

```cpp
// ===== Particle Drawing =====

static void drawRainParticles(M5Canvas& canvas, int count, int speed, int len) {
    uint16_t rainColor = C(140, 160, 200);
    for (int i = 0; i < count; i++) {
        rainDrops[i].y += speed;
        if (rainDrops[i].y >= GROUND_Y) {
            rainDrops[i].y = random(-10, 0);
            rainDrops[i].x = random(SCREEN_W);
        }
        if (rainDrops[i].y >= 0) {
            int endY = rainDrops[i].y + len;
            if (endY > GROUND_Y) endY = GROUND_Y;
            canvas.drawFastVLine(rainDrops[i].x, rainDrops[i].y, endY - rainDrops[i].y, rainColor);
        }
    }
}

static void drawSnowParticles(M5Canvas& canvas) {
    uint16_t snowColor = C(220, 220, 230);
    for (int i = 0; i < MAX_SNOW; i++) {
        snowflakes[i].y += 1;
        snowflakes[i].x += snowflakes[i].drift;
        if (random(20) == 0) snowflakes[i].drift = random(3) - 1;
        if (snowflakes[i].y >= GROUND_Y) {
            snowflakes[i].y = random(-5, 0);
            snowflakes[i].x = random(SCREEN_W);
        }
        if (snowflakes[i].x < 0) snowflakes[i].x = SCREEN_W - 1;
        if (snowflakes[i].x >= SCREEN_W) snowflakes[i].x = 0;
        if (snowflakes[i].y >= 0) {
            canvas.drawPixel(snowflakes[i].x, snowflakes[i].y, snowColor);
            if (i % 3 == 0) {
                canvas.drawPixel(snowflakes[i].x + 1, snowflakes[i].y, snowColor);
                canvas.drawPixel(snowflakes[i].x, snowflakes[i].y + 1, snowColor);
            }
        }
    }
}

static void drawFogParticles(M5Canvas& canvas) {
    uint16_t fogColor = C(160, 160, 165);
    int offset = (millis() / 200) % 3;
    for (int y = 20 + offset; y < GROUND_Y; y += 4) {
        for (int x = (y % 6); x < SCREEN_W; x += 6) {
            canvas.drawPixel(x, y, fogColor);
        }
    }
}

static void drawParticles(M5Canvas& canvas) {
    if (!particlesInit) initParticles();

    switch (currentWeather) {
        case WeatherType::RAIN:
            drawRainParticles(canvas, MAX_RAIN, 5, 5);
            break;
        case WeatherType::DRIZZLE:
            drawRainParticles(canvas, 8, 3, 3);
            break;
        case WeatherType::THUNDER: {
            drawRainParticles(canvas, MAX_RAIN, 5, 5);
            unsigned long now = millis();
            if (!thunderFlashing && now - lastThunderFlash > 3000 + random(2000)) {
                thunderFlashing = true;
                lastThunderFlash = now;
            }
            if (thunderFlashing) {
                if (now - lastThunderFlash < 50) {
                    canvas.fillScreen(C(200, 200, 220));
                    drawRainParticles(canvas, MAX_RAIN, 5, 5);
                } else {
                    thunderFlashing = false;
                }
            }
            break;
        }
        case WeatherType::SNOW:
            drawSnowParticles(canvas);
            break;
        case WeatherType::FOG:
            drawFogParticles(canvas);
            break;
        default:
            break;
    }
}
```

- [ ] **Step 2: 添加天气主绘制函数 weatherDraw()**

```cpp
// ===== Main Weather Draw =====

void weatherDraw(M5Canvas& canvas) {
    int h = weatherCurrentHour();
    uint16_t skyColor, groundColor, groundTopColor;

    // Determine base colors by time of day
    if (h >= 6 && h < 17) {
        skyColor = C(60, 120, 200);
        groundColor = C(80, 140, 60);
        groundTopColor = C(100, 170, 70);
    } else if (h >= 17 && h < 19) {
        skyColor = C(180, 80, 60);
        groundColor = C(46, 34, 47);
        groundTopColor = C(62, 53, 70);
    } else {
        skyColor = C(10, 10, 30);
        groundColor = C(46, 34, 47);
        groundTopColor = C(62, 53, 70);
    }

    // Weather sky tinting
    bool hideSun = false;
    switch (currentWeather) {
        case WeatherType::OVERCAST:
            skyColor = blendRGB565(skyColor, C(130, 140, 160), 80);
            hideSun = true;
            break;
        case WeatherType::RAIN:
        case WeatherType::THUNDER:
            skyColor = blendRGB565(skyColor, C(60, 60, 75), 180);
            hideSun = true;
            break;
        case WeatherType::DRIZZLE:
            skyColor = blendRGB565(skyColor, C(90, 90, 105), 140);
            hideSun = true;
            break;
        case WeatherType::SNOW:
            skyColor = blendRGB565(skyColor, C(120, 120, 135), 150);
            hideSun = true;
            break;
        case WeatherType::FOG:
            skyColor = blendRGB565(skyColor, C(140, 140, 145), 170);
            hideSun = true;
            break;
        default:
            break;
    }

    // 1. Sky
    if (thunderFlashing) {
        canvas.fillScreen(C(200, 200, 220));
    } else {
        canvas.fillScreen(skyColor);
    }

    // 2. Sky elements
    if (h >= 6 && h < 17) {
        if (!hideSun) {
            drawSun(canvas);
            drawClouds(canvas, C(220, 230, 240));
        } else {
            drawDarkClouds(canvas);
        }
    } else if (h >= 17 && h < 19) {
        if (!hideSun) {
            canvas.fillCircle(200, GROUND_Y - 10, 12, C(255, 220, 60));
            canvas.fillCircle(200, GROUND_Y - 10, 10, C(255, 160, 40));
        }
    } else {
        if (!hideSun) {
            drawStars(canvas);
            drawMoon(canvas, skyColor);
        }
    }

    // 3. Particles
    drawParticles(canvas);

    // 4. Ground
    canvas.fillRect(0, GROUND_Y, SCREEN_W, SCREEN_H - GROUND_Y, groundColor);
    canvas.drawFastHLine(0, GROUND_Y, SCREEN_W, groundTopColor);
    for (int i = 0; i < 8; i++) {
        int gx = (i * 31 + 10) % SCREEN_W;
        canvas.drawPixel(gx, GROUND_Y + 4, groundTopColor);
        canvas.drawPixel(gx + 15, GROUND_Y + 8, groundTopColor);
    }
}
```

- [ ] **Step 3: 编译验证**

```bash
cd /Users/zhangyingjie/Documents/PlatformIO/Projects/zedpet && pio run
```

Expected: BUILD SUCCESS

---

### Task 4: pet.cpp — 集成天气渲染 + 配件

**Files:**
- Modify: `src/pet.cpp`

- [ ] **Step 1: 添加 weather include**

在 `pet.cpp` 顶部，在 `#include "udp_server.h"` 之后添加：

```cpp
#include "weather.h"
```

- [ ] **Step 2: 去掉 update() 中的 fillScreen(TFT_BLACK)**

将第 102 行的：
```cpp
canvas.fillScreen(TFT_BLACK);
```
改为由天气模块负责填充背景，删除这行。

- [ ] **Step 3: 在 update() 开头调用 weatherDraw**

在删除 fillScreen 的位置，添加天气绘制：

```cpp
    // Draw frame
    weatherDraw(canvas);  // sky + particles + ground
```

- [ ] **Step 4: 修改 drawCharacter() 渲染后添加配件绘制**

在 `pet.cpp` 的 `drawCharacter()` 函数中，在 `drawSprite16(SPRITE_X + xOffset, SPRITE_Y + yOffset, frame);` 这行之后，直接加上配件绘制：

在文件末尾 `drawCharacter()` 内最后一行 `drawSprite16(...)` 之后、函数结束前添加：

```cpp
    // Draw weather accessory
    drawAccessory(SPRITE_X + xOffset, SPRITE_Y + yOffset);
```

- [ ] **Step 5: 添加 drawAccessory 方法**

在 `pet.h` 的 private 部分添加：
```cpp
    void drawAccessory(int x, int y);
```

在 `pet.cpp` 中 `drawCharacter()` 函数之后添加完整配件绘制函数：

```cpp
void Pet::drawAccessory(int x, int y) {
    AccessoryType acc = weatherGetAccessory();
    if (acc == AccessoryType::NONE) return;

    switch (acc) {
        case AccessoryType::SUNGLASSES: {
            uint16_t g = C(20, 20, 40);
            canvas.fillRect(x + 12, y + 15, 9, 3, g);
            canvas.fillRect(x + 27, y + 15, 9, 3, g);
            canvas.drawFastHLine(x + 21, y + 16, 6, g);
            break;
        }
        case AccessoryType::UMBRELLA: {
            canvas.fillRoundRect(x + 6, y - 10, 36, 8, 4, C(60, 60, 200));
            canvas.drawFastVLine(x + 24, y - 2, 8, C(120, 80, 40));
            break;
        }
        case AccessoryType::SNOW_HAT: {
            canvas.fillRoundRect(x + 9, y + 3, 30, 6, 3, C(200, 60, 60));
            canvas.fillCircle(x + 24, y + 2, 3, 0xFFFF);
            break;
        }
        case AccessoryType::MASK: {
            uint16_t mask = C(180, 200, 180);
            uint16_t strap = C(120, 120, 120);
            canvas.fillRect(x + 12, y + 21, 24, 6, mask);
            canvas.drawFastHLine(x + 9, y + 23, 3, strap);
            canvas.drawFastHLine(x + 36, y + 23, 3, strap);
            break;
        }
        default: break;
    }
}
```

宠物颜色宏 `C(r,g,b)` 已在 weather.cpp 中定义，需要 pet.cpp 也能用。简单做法是在 pet.cpp 顶部加一行：

```cpp
#define C(r,g,b) rgb565(r,g,b)
```

- [ ] **Step 6: 编译验证**

```bash
cd /Users/zhangyingjie/Documents/PlatformIO/Projects/zedpet && pio run
```

Expected: BUILD SUCCESS

---

### Task 5: main.cpp — w 键切换天气

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: 添加 weather include**

在 `#include "udp_server.h"` 之后添加：
```cpp
#include "weather.h"
```

- [ ] **Step 2: 在 setup() 中初始化天气**

在 `udpServerBegin()` 之后添加：
```cpp
weatherBegin();
```

- [ ] **Step 3: 在 loop() 中添加 w 键处理**

参考现有的 q 键模式，在 wWasDown 的 q 键处理后面添加 w 键处理：

现有的 wWasDown 行后面加：
```cpp
static bool wWasDown = false;

// ... in loop(), after q-key block ...

    // 'w' key: cycle weather type (edge-triggered)
    bool wDown = std::find(ks.word.begin(), ks.word.end(), 'w') != ks.word.end();
    if (wDown && !wWasDown) {
        weatherNext();
    }
    wWasDown = wDown;
```

注意：q 和 w 复用同一个 `ks` 变量，w 键检测加在 q 键块之后即可。

- [ ] **Step 4: 编译验证**

```bash
cd /Users/zhangyingjie/Documents/PlatformIO/Projects/zedpet && pio run
```

Expected: BUILD SUCCESS

---

### Task 6: 最终验证

- [ ] **Step 1: 完整编译**

```bash
cd /Users/zhangyingjie/Documents/PlatformIO/Projects/zedpet && pio run
```

Expected: BUILD SUCCESS, RAM < 20%, Flash < 30%

- [ ] **Step 2: 验证 git 状态**

```bash
git status
git log --oneline -3
```

所有改动已就绪。
