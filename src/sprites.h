#pragma once
#include <cstdint>

// Sprite dimensions
constexpr int CHAR_W = 16;
constexpr int CHAR_H = 16;
constexpr int CHAR_SCALE = 3;
constexpr int CHAR_DRAW_W = CHAR_W * CHAR_SCALE;  // 48
constexpr int CHAR_DRAW_H = CHAR_H * CHAR_SCALE;  // 48

// RGB565 color helper
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// Color palette — OpenClaw lobster
constexpr uint16_t _ = rgb565(255, 0, 255);       // Magenta = transparent
constexpr uint16_t K = 0x0000;                    // Black outline
constexpr uint16_t W = 0xFFFF;                    // White eyes
constexpr uint16_t R = rgb565(210, 50, 40);       // Red main body
constexpr uint16_t D = rgb565(160, 30, 25);       // Dark red shadow
constexpr uint16_t H = rgb565(240, 100, 80);      // Highlight red
constexpr uint16_t O = rgb565(230, 140, 60);      // Orange claws inner
constexpr uint16_t E = rgb565(20, 20, 20);        // Eye pupil
constexpr uint16_t C = rgb565(190, 40, 35);       // Claw red
constexpr uint16_t T = rgb565(180, 60, 50);       // Tail/legs

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
