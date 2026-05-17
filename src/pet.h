#pragma once
#include <M5Cardputer.h>

enum class PetState : uint8_t {
    IDLE,
    HAPPY,
    SLEEP,
    TALK,
    STRETCH,
    LOOK,
    STATE_COUNT
};

class Pet {
public:
    void begin();
    void update();  // call every frame from loop()

private:
    PetState state = PetState::IDLE;
    int frameIndex = 0;
    unsigned long stateStartTime = 0;
    unsigned long lastFrameTime = 0;
    int frameInterval = 500;  // ms per frame
    int stateDuration = 3000; // ms per state before switching

    void setState(PetState newState);
    void nextState();
    void drawSprite16(int x, int y, const uint16_t* data);
    void drawCharacter();
    const char* stateName(PetState s) const;
};
