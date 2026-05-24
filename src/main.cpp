#include <algorithm>
#include <M5Cardputer.h>
#include "claude_ui.h"
#include "pet.h"
#include "udp_server.h"
#include "weather.h"

Pet pet;
ClaudeUi claudeUi;

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.fillScreen(TFT_BLACK);

    pet.begin();
    claudeUi.begin();

    udpServerBegin();
    weather.begin();
}

static bool qWasDown = false;
static bool wWasDown = false;
static bool tabWasDown = false;
static bool key1WasDown = false;
static bool key2WasDown = false;
static bool key3WasDown = false;

void loop() {
    M5Cardputer.update();
    auto ks = M5Cardputer.Keyboard.keysState();

    // Mode toggle: Tab key enters/exits Claude mode.
    if (ks.tab && !tabWasDown) {
        if (claudeUi.isActive()) claudeUi.exit();
        else                     claudeUi.enter();
    }
    tabWasDown = ks.tab;

    if (claudeUi.isActive()) {
        claudeUi.update();
        delay(16);
        return;
    }

    bool qDown = std::find(ks.word.begin(), ks.word.end(), 'q') != ks.word.end();
    if (qDown && !qWasDown) pet.nextState();
    qWasDown = qDown;

    bool wDown = std::find(ks.word.begin(), ks.word.end(), 'w') != ks.word.end();
    if (wDown && !wWasDown) weather.next();
    wWasDown = wDown;

    // IMU param tuning: 1=cycle, 2=decrease, 3=increase
    bool k1 = std::find(ks.word.begin(), ks.word.end(), '1') != ks.word.end();
    if (k1 && !key1WasDown) pet.nextParam();
    key1WasDown = k1;

    bool k2 = std::find(ks.word.begin(), ks.word.end(), '2') != ks.word.end();
    if (k2 && !key2WasDown) pet.adjustParam(-1);
    key2WasDown = k2;

    bool k3 = std::find(ks.word.begin(), ks.word.end(), '3') != ks.word.end();
    if (k3 && !key3WasDown) pet.adjustParam(+1);
    key3WasDown = k3;

    const char* cmd = udpCheckCommand();
    if (cmd && cmd[0] != '\0') {
        udpSendAck(cmd);
        pet.receiveCommand(cmd);
    }

    pet.update();
    delay(16);
}
