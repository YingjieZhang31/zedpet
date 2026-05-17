#include "pet.h"
#include "sprites.h"
#include <M5Cardputer.h>

// Screen dimensions
constexpr int SCREEN_W = 240;
constexpr int SCREEN_H = 135;

// Sprite centered on screen
constexpr int SPRITE_X = (SCREEN_W - CHAR_DRAW_W) / 2;  // 96
constexpr int SPRITE_Y = ((SCREEN_H - CHAR_DRAW_H) + 1) / 2;  // 44

// Helper to get display reference
static decltype(M5Cardputer.Display)& display() { return M5Cardputer.Display; }

void Pet::begin() {
    setState(PetState::IDLE);
}

void Pet::setState(PetState newState) {
    state = newState;
    frameIndex = 0;
    stateStartTime = millis();
    lastFrameTime = millis();  // draw frame 0 first

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
}

void Pet::drawSprite16(int x, int y, const uint16_t* data) {
    for (int py = 0; py < CHAR_H; py++) {
        for (int px = 0; px < CHAR_W; px++) {
            uint16_t color = data[py * CHAR_W + px];
            if (color != _) {  // skip transparent (magenta) pixels
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
