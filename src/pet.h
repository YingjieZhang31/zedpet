#pragma once
#include <cstdint>

struct ImuState {
    float posX = 0, posY = 0;        // current offset from center (px)
    float velX = 0, velY = 0;        // velocity (px/s)
    float calibAx = 0;               // accelerometer baseline X
    float calibAy = 0;               // accelerometer baseline Y
    float calibAz = 0;               // accelerometer baseline Z (gravity reference)
    unsigned long lastPhysicsMs = 0; // timestamp of last physics tick
    bool calibrated = false;
};

enum class PetState : uint8_t {
    IDLE,
    HAPPY,
    SLEEP,
    TALK,
    STRETCH,
    LOOK,
    STATE_COUNT
};

struct StateConfig {
    int frameInterval;
    int frameCount;
};

class Pet {
public:
    void begin();
    void receiveCommand(const char* cmd);
    void nextState();
    void update();

private:
    PetState state = PetState::IDLE;
    int frameIndex = 0;
    unsigned long lastFrameTime = 0;

    void setState(PetState newState);
    void drawSprite16(int x, int y, const uint16_t* data);
    void drawCharacter();
    void drawAccessory(int x, int y);
    void updatePhysics();
    ImuState _imu;
    static const char* stateName(PetState s);
};
