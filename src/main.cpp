#include <algorithm>
#include <M5Cardputer.h>
#include "pet.h"
#include "udp_server.h"

Pet pet;

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);  // true = init display
    M5Cardputer.Display.setRotation(1);        // landscape
    M5Cardputer.Display.fillScreen(TFT_BLACK);

    pet.begin();

    udpServerBegin();  // connect WiFi, start UDP
}

static bool qWasDown = false;

void loop() {
    M5Cardputer.update();

    // 'q' key: cycle to next state (edge-triggered)
    auto ks = M5Cardputer.Keyboard.keysState();
    bool qDown = std::find(ks.word.begin(), ks.word.end(), 'q') != ks.word.end();
    if (qDown && !qWasDown) {
        pet.nextState();
    }
    qWasDown = qDown;

    const char* cmd = udpCheckCommand();
    if (cmd && cmd[0] != '\0') {
        udpSendAck(cmd);
        pet.receiveCommand(cmd);
    }

    pet.update();
    delay(16);  // ~60fps
}
