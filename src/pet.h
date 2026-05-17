#pragma once
#include <cstdint>

enum class PetState : uint8_t {
    IDLE,
    HAPPY,
    SLEEP,
    TALK,
    STRETCH,
    LOOK,
    STATE_COUNT  // must be last — used for iteration bounds
};

class Pet {
public:
    void begin();
    void receiveCommand(const char* cmd);  // map string to PetState
    void update();  // call every frame from loop()

private:
    PetState state = PetState::IDLE;
    int frameIndex = 0;
    unsigned long lastFrameTime = 0;
    int frameInterval = 500;  // ms per frame

    void setState(PetState newState);
    void drawSprite16(int x, int y, const uint16_t* data);
    void drawCharacter();
    const char* stateName(PetState s) const;
};
