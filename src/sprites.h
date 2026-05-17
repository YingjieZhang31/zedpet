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
