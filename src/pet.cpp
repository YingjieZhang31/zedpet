#include "pet.h"
#include "config.h"
#include "sprites.h"
#include "udp_server.h"
#include "weather.h"
#include <M5Cardputer.h>
#include <cstring>

// ===== State configuration (replaces scattered switch statements) =====
static constexpr StateConfig STATE_CONFIGS[] = {
    {500, 4},   // IDLE
    {200, 2},   // HAPPY
    {1000, 1},  // SLEEP
    {250, 2},   // TALK
    {400, 2},   // STRETCH
    {300, 4},   // LOOK
};
static_assert(static_cast<int>(PetState::STATE_COUNT) == 6, "STATE_CONFIGS out of sync");

// ===== Command-to-state lookup table =====
static constexpr struct { const char* name; PetState state; } CMD_MAP[] = {
    {"idle",    PetState::IDLE},
    {"happy",   PetState::HAPPY},
    {"sleep",   PetState::SLEEP},
    {"talk",    PetState::TALK},
    {"stretch", PetState::STRETCH},
    {"look",    PetState::LOOK},
};

// ===== State name lookup =====
static constexpr const char* STATE_NAMES[] = {
    "IDLE", "HAPPY", "SLEEP", "TALK", "STRETCH", "LOOK"
};

// Offscreen canvas for double-buffered rendering
static M5Canvas canvas(&M5Cardputer.Display);

// ===== Public API =====

void Pet::begin() {
    canvas.setColorDepth(16);
    canvas.createSprite(SCREEN_W, SCREEN_H);
    setState(PetState::IDLE);
}

void Pet::receiveCommand(const char* cmd) {
    for (const auto& entry : CMD_MAP) {
        if (strcmp(cmd, entry.name) == 0) {
            setState(entry.state);
            Serial.printf("[PET] cmd=%s -> %s\n", cmd, stateName(entry.state));
            return;
        }
    }
}

void Pet::nextState() {
    int next = (static_cast<int>(state) + 1) % static_cast<int>(PetState::STATE_COUNT);
    setState(static_cast<PetState>(next));
}

void Pet::setState(PetState newState) {
    state = newState;
    frameIndex = 0;
    lastFrameTime = millis();
}

const char* Pet::stateName(PetState s) {
    int idx = static_cast<int>(s);
    if (idx >= 0 && idx < static_cast<int>(PetState::STATE_COUNT))
        return STATE_NAMES[idx];
    return "???";
}

// ===== Update / Render =====

void Pet::update() {
    const StateConfig& cfg = STATE_CONFIGS[static_cast<int>(state)];

    unsigned long now = millis();
    if (now - lastFrameTime >= static_cast<unsigned long>(cfg.frameInterval)) {
        lastFrameTime = now;
        frameIndex++;
        frameIndex %= cfg.frameCount;
    }

    weather.draw(canvas);

    canvas.setTextColor(TFT_WHITE);
    canvas.setTextSize(2);
    canvas.setCursor(4, 2);
    canvas.print(stateName(state));

    const char* t = udpGetCurrentTime();
    if (t[0] != '\0') {
        canvas.setTextColor(rgb565(180, 180, 200));
        canvas.setTextSize(2);
        int tw = canvas.textWidth(t);
        canvas.setCursor((SCREEN_W - tw) / 2, 2);
        canvas.print(t);
    }

    if (udpIsWiFiConnected()) {
        const char* ip = udpGetLocalIP();
        canvas.setTextColor(rgb565(0, 255, 0));
        canvas.setTextSize(1);
        int ipW = canvas.textWidth(ip);
        canvas.setCursor(SCREEN_W - ipW - 4, 4);
        canvas.print(ip);
    } else {
        canvas.setTextColor(rgb565(255, 80, 80));
        canvas.setTextSize(1);
        canvas.setCursor(SCREEN_W - 54, 4);
        canvas.print("No WiFi");
    }

    drawCharacter();
    canvas.pushSprite(0, 0);
}

// ===== Drawing Helpers =====

void Pet::drawSprite16(int x, int y, const uint16_t* data) {
    for (int py = 0; py < CHAR_H; py++) {
        for (int px = 0; px < CHAR_W; px++) {
            uint16_t color = data[py * CHAR_W + px];
            if (color != _) {
                canvas.fillRect(x + px * CHAR_SCALE, y + py * CHAR_SCALE,
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
            frame = idle_frames[frameIndex];
            break;
        case PetState::HAPPY:
        case PetState::STRETCH:
            frame = happy_frames[frameIndex];
            break;
        case PetState::SLEEP:
            frame = sleep_frames[frameIndex];
            break;
        case PetState::TALK:
            frame = talk_frames[frameIndex];
            break;
    }

    if (!frame) return;

    int xOffset = 0;
    int yOffset = 0;

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
    drawAccessory(SPRITE_X + xOffset, SPRITE_Y + yOffset);
}

void Pet::drawAccessory(int x, int y) {
    AccessoryType acc = weather.getAccessory();
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
