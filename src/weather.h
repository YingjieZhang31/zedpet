#pragma once
#include <cstdint>

#include <M5Cardputer.h>

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

class Weather {
public:
    void begin();
    void next();
    void draw(M5Canvas& canvas);
    AccessoryType getAccessory() const;
    const char* typeName() const;
    static int currentHour();

private:
    WeatherType current = WeatherType::CLEAR;
    bool particlesInit = false;

    static constexpr int MAX_STARS = 12;
    struct Star { int x, y; bool visible; };
    Star stars[MAX_STARS] = {};
    unsigned long lastStarTwinkle = 0;

    static constexpr int MAX_RAIN = 15;
    static constexpr int MAX_SNOW = 15;
    struct RainDrop { int16_t x, y; };
    struct Snowflake { int16_t x, y; int8_t drift; };
    RainDrop rainDrops[MAX_RAIN] = {};
    Snowflake snowflakes[MAX_SNOW] = {};
    unsigned long lastThunderFlash = 0;
    bool thunderFlashing = false;

    void initStars();
    void initParticles();
    void drawSkyElements(M5Canvas& canvas, int hour, bool hideSun, uint16_t skyColor);
    void drawParticles(M5Canvas& canvas);
    static void drawSun(M5Canvas& canvas);
    static void drawClouds(M5Canvas& canvas, uint16_t color);
    void drawStars(M5Canvas& canvas);
    static void drawMoon(M5Canvas& canvas, uint16_t skyColor);
    static void drawDarkClouds(M5Canvas& canvas);
    void drawRainParticles(M5Canvas& canvas, int count, int speed, int len);
    void drawSnowParticles(M5Canvas& canvas);
    static void drawFogParticles(M5Canvas& canvas);
    static uint16_t blendRGB565(uint16_t a, uint16_t b, uint8_t t);
};

extern Weather weather;
