#include "pet.h"
#include "sprites.h"
#include "udp_server.h"
#include <M5Cardputer.h>
#include <cstring>

// Screen dimensions
constexpr int SCREEN_W = 240;
constexpr int SCREEN_H = 135;

// Sprite centered on screen
constexpr int SPRITE_X = (SCREEN_W - CHAR_DRAW_W) / 2;  // 96
constexpr int SPRITE_Y = ((SCREEN_H - CHAR_DRAW_H) + 1) / 2;  // 44

// Offscreen canvas for double-buffered rendering
static M5Canvas canvas(&M5Cardputer.Display);

void Pet::begin() {
    canvas.setColorDepth(16);
    canvas.createSprite(SCREEN_W, SCREEN_H);
    setState(PetState::IDLE);
}

void Pet::receiveCommand(const char* cmd) {
    PetState newState = PetState::IDLE;

    if (strcmp(cmd, "idle") == 0)      newState = PetState::IDLE;
    else if (strcmp(cmd, "happy") == 0)  newState = PetState::HAPPY;
    else if (strcmp(cmd, "sleep") == 0)  newState = PetState::SLEEP;
    else if (strcmp(cmd, "talk") == 0)   newState = PetState::TALK;
    else if (strcmp(cmd, "stretch") == 0) newState = PetState::STRETCH;
    else if (strcmp(cmd, "look") == 0)  newState = PetState::LOOK;
    else return;  // unknown command

    setState(newState);

    Serial.printf("[PET] cmd=%s -> %s\n", cmd, stateName(newState));
}

void Pet::nextState() {
    int next = (static_cast<int>(state) + 1) % static_cast<int>(PetState::STATE_COUNT);
    setState(static_cast<PetState>(next));
}

void Pet::setState(PetState newState) {
    state = newState;
    frameIndex = 0;
    lastFrameTime = millis();  // draw frame 0 first

    switch (state) {
        case PetState::IDLE:    frameInterval = 500; break;
        case PetState::HAPPY:   frameInterval = 200; break;
        case PetState::SLEEP:   frameInterval = 1000; break;
        case PetState::TALK:    frameInterval = 250; break;
        case PetState::STRETCH: frameInterval = 400; break;
        case PetState::LOOK:    frameInterval = 300; break;
        default: break;
    }
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
    // Advance animation frame
    unsigned long now = millis();
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
    canvas.fillScreen(TFT_BLACK);

    // State name at top-left
    canvas.setTextColor(TFT_WHITE);
    canvas.setTextSize(2);
    canvas.setCursor(4, 2);
    canvas.print(stateName(state));

    // Current time at top center
    const char* t = udpGetCurrentTime();
    if (t[0] != '\0') {
        canvas.setTextColor(rgb565(180, 180, 200));
        canvas.setTextSize(2);
        int tw = canvas.textWidth(t);
        canvas.setCursor((SCREEN_W - tw) / 2, 2);
        canvas.print(t);
    }

    // WiFi status at top-right
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

    // Character centered
    drawCharacter();

    canvas.pushSprite(0, 0);
}

void Pet::drawSprite16(int x, int y, const uint16_t* data) {
    for (int py = 0; py < CHAR_H; py++) {
        for (int px = 0; px < CHAR_W; px++) {
            uint16_t color = data[py * CHAR_W + px];
            if (color != _) {  // skip transparent (magenta) pixels
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
