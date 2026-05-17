# zedpet 项目实施踩坑记录

## 1. RGB565 颜色值计算错误

**现象：** 从 ClawPuter 移植 sprites.h 时，手动将 `rgb565(r,g,b)` 宏展开为 hex 值，结果全部算错。

**原因：** RGB565 编码公式是 `((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)`，手动心算容易出错。

**错误 vs 正确：**

| 颜色 | 错误值 | 正确值 | 差值 |
|------|--------|--------|------|
| R (210,50,40) | 0xC128 | 0xD185 | +0x105D |
| D (160,30,25) | 0xA0C3 | 0xA0E3 | +0x0020 |
| H (240,100,80) | 0xFA2A | 0xF32A | -0x0700 |
| O (230,140,60) | 0xE46C | 0xEA67 | +0x05FB |
| E (20,20,20) | 0x1082 | 0x10A2 | +0x0020 |
| C (190,40,35) | 0xC123 | 0xB944 | -0x07DF |
| T (180,60,50) | 0xC30A | 0xB1E6 | -0x1124 |

**教训：** 不要手动展开 RGB565 宏。直接复用原始的 `rgb565()` 函数，让编译器算。

```cpp
// 正确做法：
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
constexpr uint16_t R = rgb565(210, 50, 40);
```

---

## 2. 透明色与黑色冲突

**现象：** 精灵的黑色描边（眼睛、钳子轮廓、触角）全部消失不渲染。

**原因：** 移植时将透明色 `_` 和黑色 `K` 都定义成了 `0x0000`。

```cpp
// 错误：
constexpr uint16_t _ = 0x0000;  // 透明
constexpr uint16_t K = 0x0000;  // 黑色 —— 和透明一样！
```

而 ClawPuter 原始代码用的是品红透明（经典 chroma-key 做法）：

```cpp
// ClawPuter 正确做法：
constexpr uint16_t TRANSPARENT = rgb565(255, 0, 255);  // 品红
constexpr uint16_t BLACK      = 0x0000;                // 纯黑
```

渲染时的跳过检查 `if (color != 0x0000)` 会把 `_` 和 `K` 一起跳过。

**修复：**
```cpp
constexpr uint16_t _ = rgb565(255, 0, 255);  // 品红 = 透明
constexpr uint16_t K = 0x0000;               // 黑色描边
// 渲染检查改为: if (color != _)
```

**教训：** 移植像素精灵时，透明色必须是一个精灵数据中不会出现的颜色值。品红 (255,0,255) 是业界标准的 chroma-key 选择。

---

## 3. 状态切换时第 0 帧被跳过

**现象：** 每次切换到新状态，动画从第 1 帧开始播放而不是第 0 帧。比如 HAPPY 状态先显示"钳子张开"再显示"钳子举起"（顺序反了）。

**原因：** `setState()` 中 `lastFrameTime = 0` 的意图是"立即触发第一帧"，但在 `update()` 中：

```cpp
// setState: lastFrameTime = 0, frameIndex = 0
// 第一次 update():
if (now - lastFrameTime >= frameInterval) {  // millis() - 0 >= 500 → true!
    frameIndex++;  // 0 → 1，第 0 帧被跳过了！
}
```

因为 `millis()` 返回值远大于 0，条件立即为真，frameIndex 直接跳到 1。

**修复：**
```cpp
lastFrameTime = millis();  // 让第一次 update 的差值接近 0，不触发帧递增
```

**教训：** `millis() - 0 >= interval` 这种模式有陷阱。用 `0` 做"立即触发"的哨兵值时，要考虑到 `millis()` 在系统启动后已经跑了几百毫秒。

---

## 4. 精灵居中计算偏差

**现象：** 精灵垂直位置偏上 1px。

**原因：** 整数除法截断。

```cpp
constexpr int SPRITE_Y = (135 - 48) / 2;  // = 87 / 2 = 43，不是 44
```

135 减 48 得 87，除以 2 整数截断为 43。但真正的视觉中心需要上舍入到 44（上面 43px，下面 44px，精灵才能视觉居中）。

**修复：**
```cpp
constexpr int SPRITE_Y = ((135 - 48) + 1) / 2;  // = 88 / 2 = 44
```

**教训：** 奇数差除以 2 时要注意舍入方向。`(a + 1) / 2` 实现向上取整。

---

## 5. M5GFX vs M5Canvas 类型混淆

**现象：** 代码中写 `M5Canvas& display()`，编译报错类型不匹配。

**原因：** `M5Cardputer.Display` 的实际类型是 `M5GFX`（物理屏幕驱动），不是 `M5Canvas`（离屏画布）。它们有相似的绘图 API 但类型不同。

**修复：**
```cpp
// 错误：
static M5Canvas& display() { return M5Cardputer.Display; }

// 正确：
static decltype(M5Cardputer.Display)& display() { return M5Cardputer.Display; }
```

**教训：** 嵌入式硬件库的类型层次可能和直觉不同。`decltype` 是处理这类复杂类型的好工具。不过最终我们改用了 M5Canvas 双缓冲方案，就不需要这个 helper 了。

---

## 6. 直接绘制导致屏幕闪烁（最影响体验的坑）

**现象：** 烧录后屏幕剧烈闪烁，精灵和文字忽隐忽现。

**原因：** 直接在物理屏幕上逐像素绘制，没有双缓冲：

```
每帧流程：fillScreen(黑) → 屏显示黑 → 画文字 → 画150个fillRect
                                                      ↓
                                            LCD控制器在绘制中途
                                            就把半成品扫出去了
```

ESP32 SPI 传输和 LCD 刷新是异步的，`fillScreen` 清屏后到精灵画完之间有几十毫秒的时间窗口，LCD 控制器在这个窗口内刷新就会显示黑屏（或者半幅画面）。

**修复：** 用 `M5Canvas` 做离屏渲染，最后一次性 `pushSprite`：

```cpp
// 初始化
static M5Canvas canvas(&M5Cardputer.Display);
canvas.createSprite(SCREEN_W, SCREEN_H);

// 每帧：画到离屏 canvas，再一次性推送
canvas.fillScreen(TFT_BLACK);
canvas.drawString(...);
drawCharacter();  // fillRect 都画在 canvas 上
canvas.pushSprite(0, 0);  // 一次性推到屏幕 ← 关键
```

**教训：** 嵌入式 LCD 开发，除非画面极简（单色填充），否则一定要双缓冲。`M5Canvas` + `pushSprite` 是 M5Stack 生态的标准做法。

---

## 7. clangd 误报诊断

**现象：** IDE 中大量红色波浪线报错，包括：
- `Unknown argument '-mlongcalls'`
- `'_ansi.h' file not found`
- `No member named 'fillScreen' in 'M5GFX'`

**原因：** 项目的 clangd 使用宿主 macOS 的 Clang 去解析 ESP32-S3 的代码。ESP-IDF 的编译选项（`-mlongcalls`、`-fstrict-volatile-bitfields`）和头文件路径（`_ansi.h`）只有 ESP32 工具链才有。

**处理：** 忽略即可。以 `pio run` 的编译结果为准。这些诊断不影响实际构建和烧录。

**教训：** 嵌入式项目的 IDE 诊断仅供参考，最终以交叉编译工具链的结果为准。

---

## 8. 头文件不必要的硬件耦合

**现象：** `pet.h` 包含了 `<M5Cardputer.h>`，导致任何引用 pet.h 的文件都依赖完整的 M5 硬件库。

**原因：** Pet 类的接口只用了 `uint16_t`、`uint8_t` 这些标准类型，并不需要 M5Cardputer 的任何类型。

**修复：** 将 `#include <M5Cardputer.h>` 改为 `#include <cstdint>`，把硬件依赖限制在 `pet.cpp` 中。

**教训：** 头文件中只包含实际需要的依赖。这降低了编译耦合，也让类更容易测试和移植。
