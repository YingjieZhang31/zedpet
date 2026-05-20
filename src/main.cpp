#include <algorithm>
#include <M5Cardputer.h>
#include "pet.h"
#include "udp_server.h"
#include "weather.h"

Pet pet;

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.fillScreen(TFT_BLACK);

    pet.begin();

    udpServerBegin();   // backward-compatible inline wrapper
    weather.begin();    // Weather class method
}

static bool qWasDown = false;
static bool wWasDown = false;

void loop() {
    M5Cardputer.update();

    auto ks = M5Cardputer.Keyboard.keysState();

    bool qDown = std::find(ks.word.begin(), ks.word.end(), 'q') != ks.word.end();
    if (qDown && !qWasDown) pet.nextState();
    qWasDown = qDown;

    bool wDown = std::find(ks.word.begin(), ks.word.end(), 'w') != ks.word.end();
    if (wDown && !wWasDown) weather.next();
    wWasDown = wDown;

    const char* cmd = udpCheckCommand();
    if (cmd && cmd[0] != '\0') {
        udpSendAck(cmd);
        pet.receiveCommand(cmd);
    }

    pet.update();
    delay(16);
}
