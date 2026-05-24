#pragma once
#include <cstdint>
#include <cstddef>

// Screen dimensions
constexpr int SCREEN_W = 240;
constexpr int SCREEN_H = 135;
constexpr int GROUND_Y = SCREEN_H - 28;

// Sprite drawing
constexpr int CHAR_W = 16;
constexpr int CHAR_H = 16;
constexpr int CHAR_SCALE = 3;
constexpr int CHAR_DRAW_W = CHAR_W * CHAR_SCALE;  // 48
constexpr int CHAR_DRAW_H = CHAR_H * CHAR_SCALE;  // 48
constexpr int SPRITE_X = (SCREEN_W - CHAR_DRAW_W) / 2;   // 96
constexpr int SPRITE_Y = ((SCREEN_H - CHAR_DRAW_H) + 1) / 2;  // 44

// RGB565 color helper
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

#define C(r, g, b) rgb565(r, g, b)

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

// Colors (use literal rgb565 values to avoid pulling in M5GFX here — config.h
// is included by translation units that don't otherwise need TFT_* macros)
constexpr uint16_t CLAUDE_BG          = C(0, 0, 0);
constexpr uint16_t CLAUDE_FG          = C(255, 255, 255);
constexpr uint16_t CLAUDE_INPUT_BG    = C(32, 32, 40);
constexpr uint16_t CLAUDE_PROMPT_FG   = C(120, 180, 255);
constexpr uint16_t CLAUDE_ERROR_FG    = C(255, 80, 80);
constexpr uint16_t CLAUDE_STATUS_FG   = C(160, 160, 160);
constexpr uint16_t CLAUDE_WIFI_OK     = C(80, 200, 80);
constexpr uint16_t CLAUDE_WIFI_BAD    = C(200, 80, 80);

// IMU tilt physics parameters (constexpr float -- tune & recompile)
constexpr float IMU_SENSITIVITY   = 3.0f;    // tilt-to-acceleration gain
constexpr float IMU_DAMPING       = 2.5f;    // per-second friction, higher = stops faster
constexpr float IMU_BOUNCE        = 0.5f;    // edge bounce multiplier (0=dead stop, 1=elastic)
constexpr float IMU_DEAD_ZONE     = 0.03f;   // ignore tilt below this to avoid sensor drift
constexpr float IMU_VELOCITY_CAP  = 120.0f;  // max velocity px/s, prevents teleporting
constexpr float IMU_MAX_DT        = 0.1f;    // cap frame delta to avoid huge jumps after lag
