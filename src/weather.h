#pragma once
#include <cstdint>
#include <M5Cardputer.h>

// ===== Weather Types =====

enum class WeatherType : uint8_t {
    CLEAR = 0,
    PARTLY_CLOUDY,
    OVERCAST,
    FOG,
    DRIZZLE,
    RAIN,
    SNOW,
    THUNDER,
    WEATHER_COUNT
};

enum class AccessoryType : uint8_t {
    NONE,
    SUNGLASSES,
    UMBRELLA,
    SNOW_HAT,
    MASK
};

// ===== API =====

void weatherBegin();
void weatherNext();
void weatherDraw(M5Canvas& canvas);
AccessoryType weatherGetAccessory();
const char* weatherTypeName();
int weatherCurrentHour();
