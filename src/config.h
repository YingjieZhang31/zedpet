#pragma once
#include <cstdint>

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
