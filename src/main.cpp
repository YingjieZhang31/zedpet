#include <M5Cardputer.h>
#include "pet.h"

Pet pet;

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);  // true = init display
    M5Cardputer.Display.setRotation(1);        // landscape
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    pet.begin();
}

void loop() {
    M5Cardputer.update();
    pet.update();
    delay(16);  // ~60fps
}
