#include "pet.h"
#include "config.h"
#include "sprites.h"
#include "udp_server.h"
#include "weather.h"
#include <M5Cardputer.h>
#include <M5Unified.h>
#include <cstring>
#include <cmath>
#include <cstdio>

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
    // Reset IMU physics when leaving IDLE
    if (newState != PetState::IDLE) {
        _imu.posX = 0;
        _imu.posY = 0;
        _imu.velX = 0;
        _imu.velY = 0;
        _imu.lastPhysicsMs = 0;
    }
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

    if (state == PetState::IDLE) {
        updatePhysics();
    }

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
    drawParamBar();
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

    int xOffset = static_cast<int>(_imu.posX);
    int yOffset = static_cast<int>(_imu.posY);

    if (state == PetState::HAPPY && frameIndex % 2 == 0) {
        yOffset += -6;
    }
    if (state == PetState::LOOK) {
        xOffset += (frameIndex % 2 == 0) ? -3 : 3;
    }
    if (state == PetState::STRETCH && frameIndex % 2 == 0) {
        yOffset += -3;
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

void Pet::updatePhysics() {
    // Calibrate on first call: capture current IMU reading as zero
    if (!_imu.calibrated) {
        float ax, ay, az;
        M5.Imu.getAccel(&ax, &ay, &az);
        _imu.calibAx = ax;
        _imu.calibAy = ay;
        _imu.calibAz = az;
        _imu.calibrated = true;
        return;
    }

    // Calculate time delta using physics-specific timestamp
    unsigned long now = millis();
    float dt;
    if (_imu.lastPhysicsMs == 0) {
        dt = IMU_MAX_DT;  // first tick after reset
    } else {
        dt = (now - _imu.lastPhysicsMs) / 1000.0f;
    }
    _imu.lastPhysicsMs = now;
    if (dt <= 0 || dt > IMU_MAX_DT) dt = IMU_MAX_DT;

    // Read raw accelerometer (values in g ≈ sin(tilt_angle))
    float ax, ay, az;
    M5.Imu.getAccel(&ax, &ay, &az);

    // Tilt = difference from calibrated baseline
    float tiltX = ax - _imu.calibAx;
    float tiltY = ay - _imu.calibAy;

    // Dead zone: skip acceleration if tilt is negligible
    if (fabsf(tiltX) < IMU_DEAD_ZONE) tiltX = 0;
    if (fabsf(tiltY) < IMU_DEAD_ZONE) tiltY = 0;

    // Acceleration from tilt (raw values already encode sin(angle))
    float ax_acc = -_sens * tiltX;
    float ay_acc = _sens * tiltY;

    // Integrate velocity
    _imu.velX += ax_acc * dt;
    _imu.velY += ay_acc * dt;

    // Apply damping
    float damp = 1.0f - _damp * dt;
    if (damp < 0) damp = 0;
    _imu.velX *= damp;
    _imu.velY *= damp;

    // Cap velocity
    float speed = sqrtf(_imu.velX * _imu.velX + _imu.velY * _imu.velY);
    if (speed > IMU_VELOCITY_CAP) {
        float scale = IMU_VELOCITY_CAP / speed;
        _imu.velX *= scale;
        _imu.velY *= scale;
    }

    // Integrate position
    _imu.posX += _imu.velX * dt;
    _imu.posY += _imu.velY * dt;

    // Boundary collision with elastic bounce
    const int minX = -SPRITE_X;
    const int maxX = SCREEN_W - SPRITE_X - CHAR_DRAW_W;
    const int minY = -SPRITE_Y;
    const int maxY = SCREEN_H - SPRITE_Y - CHAR_DRAW_H;

    if (_imu.posX < minX) {
        _imu.posX = minX;
        _imu.velX *= -_bounce;
    } else if (_imu.posX > maxX) {
        _imu.posX = maxX;
        _imu.velX *= -_bounce;
    }

    if (_imu.posY < minY) {
        _imu.posY = minY;
        _imu.velY *= -_bounce;
    } else if (_imu.posY > maxY) {
        _imu.posY = maxY;
        _imu.velY *= -_bounce;
    }
}

// ===== IMU Param Tuning =====

void Pet::nextParam() {
    _paramSel = (_paramSel + 1) % 3;
}

void Pet::adjustParam(int delta) {
    switch (_paramSel) {
        case 0: _sens  += delta * 5.0f;  if (_sens  < 5.0f)  _sens  = 5.0f;  break;
        case 1: _damp  += delta * 0.5f;  if (_damp  < 0.0f)  _damp  = 0.0f;  break;
        case 2: _bounce += delta * 0.1f; if (_bounce < 0.0f) _bounce = 0.0f;
                 if (_bounce > 1.0f) _bounce = 1.0f; break;
    }
}

void Pet::drawParamBar() {
    const int barY = SCREEN_H - 10;
    const uint16_t bg   = rgb565(20, 20, 30);
    const uint16_t fg   = rgb565(140, 140, 160);
    const uint16_t sel  = rgb565(255, 220, 80);
    const uint16_t valC = rgb565(200, 200, 220);

    canvas.fillRect(0, barY, SCREEN_W, 10, bg);

    canvas.setTextSize(1);
    canvas.setCursor(2, barY + 1);

    struct { const char* label; float val; int dec; } const params[] = {
        {"SENS", _sens,  0},
        {"DAMP", _damp,  1},
        {"BNC",  _bounce, 1},
    };

    for (int i = 0; i < 3; i++) {
        bool active = (i == _paramSel);
        canvas.setTextColor(active ? sel : fg);

        char buf[16];
        if (params[i].dec == 0) {
            snprintf(buf, sizeof(buf), "%s:%.0f", params[i].label, params[i].val);
        } else {
            snprintf(buf, sizeof(buf), "%s:%.1f", params[i].label, params[i].val);
        }
        canvas.print(buf);
        canvas.print("  ");
    }
}
