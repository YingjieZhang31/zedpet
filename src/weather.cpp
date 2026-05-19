#include "weather.h"
#include "sprites.h"
#include <M5Cardputer.h>
#include <time.h>

// ===== Screen Layout =====
constexpr int SCREEN_W = 240;
constexpr int SCREEN_H = 135;
constexpr int GROUND_Y = SCREEN_H - 28;

// ===== RGB565 Helper =====
static uint16_t _rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
#define C(r,g,b) _rgb565(r,g,b)

static const char* WEATHER_NAMES[] = {
    "Clear", "Cloudy", "Overcast", "Fog",
    "Drizzle", "Rain", "Snow", "Thunder"
};

// ===== State =====
static WeatherType currentWeather = WeatherType::CLEAR;
static bool particlesInit = false;

// ===== Blend Utility =====
static uint16_t blendRGB565(uint16_t a, uint16_t b, uint8_t t) {
    uint8_t r1 = (a >> 11) & 0x1F, g1 = (a >> 5) & 0x3F, b1 = a & 0x1F;
    uint8_t r2 = (b >> 11) & 0x1F, g2 = (b >> 5) & 0x3F, b2 = b & 0x1F;
    uint8_t r = r1 + ((int)(r2 - r1) * t / 255);
    uint8_t g = g1 + ((int)(g2 - g1) * t / 255);
    uint8_t bl = b1 + ((int)(b2 - b1) * t / 255);
    return (r << 11) | (g << 5) | bl;
}

// ===== Forward Declarations =====
static void initStars();
static void initParticles();

// ===== API =====

void weatherBegin() {
    currentWeather = WeatherType::CLEAR;
    particlesInit = false;
    initStars();
}

void weatherNext() {
    int n = (static_cast<int>(currentWeather) + 1) % static_cast<int>(WeatherType::WEATHER_COUNT);
    currentWeather = static_cast<WeatherType>(n);
    particlesInit = false;
    Serial.printf("[WEATHER] -> %s\n", WEATHER_NAMES[n]);
}

const char* weatherTypeName() {
    return WEATHER_NAMES[static_cast<int>(currentWeather)];
}

int weatherCurrentHour() {
    struct tm ti;
    if (getLocalTime(&ti, 0)) return ti.tm_hour;
    return 12;
}

AccessoryType weatherGetAccessory() {
    switch (currentWeather) {
        case WeatherType::CLEAR:
        case WeatherType::PARTLY_CLOUDY: return AccessoryType::SUNGLASSES;
        case WeatherType::RAIN:
        case WeatherType::DRIZZLE:
        case WeatherType::THUNDER:      return AccessoryType::UMBRELLA;
        case WeatherType::SNOW:         return AccessoryType::SNOW_HAT;
        case WeatherType::FOG:
        case WeatherType::OVERCAST:     return AccessoryType::MASK;
        default:                        return AccessoryType::NONE;
    }
}

// ===== Stars =====
constexpr int MAX_STARS = 12;
struct Star { int x, y; bool visible; };
static Star stars[MAX_STARS];
static unsigned long lastStarTwinkle = 0;

static void initStars() {
    for (int i = 0; i < MAX_STARS; i++) {
        stars[i].x = random(10, SCREEN_W - 10);
        stars[i].y = random(5, GROUND_Y - 60);
        stars[i].visible = random(2) == 0;
    }
}

// ===== Particles =====
constexpr int MAX_RAIN = 15;
constexpr int MAX_SNOW = 15;
struct RainDrop { int16_t x, y; };
struct Snowflake { int16_t x, y; int8_t drift; };
static RainDrop rainDrops[MAX_RAIN];
static Snowflake snowflakes[MAX_SNOW];
static unsigned long lastThunderFlash = 0;
static bool thunderFlashing = false;

static void initParticles() {
    for (int i = 0; i < MAX_RAIN; i++) {
        rainDrops[i].x = random(SCREEN_W);
        rainDrops[i].y = random(GROUND_Y);
    }
    for (int i = 0; i < MAX_SNOW; i++) {
        snowflakes[i].x = random(SCREEN_W);
        snowflakes[i].y = random(GROUND_Y);
        snowflakes[i].drift = random(3) - 1;
    }
    particlesInit = true;
}

// ===== Sky Elements =====

static void drawSun(M5Canvas& canvas) {
    canvas.fillCircle(200, 18, 10, C(255, 220, 60));
    for (int i = 0; i < 8; i++) {
        float angle = i * 0.785f;
        int x1 = 200 + cos(angle) * 13;
        int y1 = 18 + sin(angle) * 13;
        int x2 = 200 + cos(angle) * 16;
        int y2 = 18 + sin(angle) * 16;
        canvas.drawLine(x1, y1, x2, y2, C(255, 220, 60));
    }
}

static void drawClouds(M5Canvas& canvas, uint16_t color) {
    canvas.fillRoundRect(40, 12, 24, 8, 4, color);
    canvas.fillRoundRect(48, 8, 16, 8, 4, color);
    canvas.fillRoundRect(130, 18, 20, 6, 3, color);
    canvas.fillRoundRect(136, 14, 14, 6, 3, color);
}

static void drawStars(M5Canvas& canvas) {
    if (millis() - lastStarTwinkle > 800) {
        lastStarTwinkle = millis();
        int idx = random(MAX_STARS);
        stars[idx].visible = !stars[idx].visible;
    }
    for (int i = 0; i < MAX_STARS; i++) {
        if (stars[i].visible) {
            canvas.drawPixel(stars[i].x, stars[i].y, C(200, 200, 140));
            if (i % 3 == 0) {
                canvas.drawPixel(stars[i].x - 1, stars[i].y, C(200, 200, 140));
                canvas.drawPixel(stars[i].x + 1, stars[i].y, C(200, 200, 140));
                canvas.drawPixel(stars[i].x, stars[i].y - 1, C(200, 200, 140));
                canvas.drawPixel(stars[i].x, stars[i].y + 1, C(200, 200, 140));
            }
        }
    }
}

static void drawMoon(M5Canvas& canvas, uint16_t skyColor) {
    canvas.fillCircle(30, 20, 10, C(220, 220, 200));
    canvas.fillCircle(34, 17, 9, skyColor);
}

static void drawDarkClouds(M5Canvas& canvas) {
    uint16_t c = C(150, 150, 160);
    canvas.fillRoundRect(30, 10, 30, 10, 5, c);
    canvas.fillRoundRect(40, 5, 20, 10, 5, c);
    canvas.fillRoundRect(120, 15, 28, 9, 4, c);
    canvas.fillRoundRect(128, 9, 18, 9, 4, c);
    canvas.fillRoundRect(180, 12, 26, 8, 4, c);
}

// ===== Particle Drawing =====

static void drawRainParticles(M5Canvas& canvas, int count, int speed, int len) {
    uint16_t rainColor = C(140, 160, 200);
    for (int i = 0; i < count; i++) {
        rainDrops[i].y += speed;
        if (rainDrops[i].y >= GROUND_Y) {
            rainDrops[i].y = random(-10, 0);
            rainDrops[i].x = random(SCREEN_W);
        }
        if (rainDrops[i].y >= 0) {
            int endY = rainDrops[i].y + len;
            if (endY > GROUND_Y) endY = GROUND_Y;
            canvas.drawFastVLine(rainDrops[i].x, rainDrops[i].y, endY - rainDrops[i].y, rainColor);
        }
    }
}

static void drawSnowParticles(M5Canvas& canvas) {
    uint16_t snowColor = C(220, 220, 230);
    for (int i = 0; i < MAX_SNOW; i++) {
        snowflakes[i].y += 1;
        snowflakes[i].x += snowflakes[i].drift;
        if (random(20) == 0) snowflakes[i].drift = random(3) - 1;
        if (snowflakes[i].y >= GROUND_Y) {
            snowflakes[i].y = random(-5, 0);
            snowflakes[i].x = random(SCREEN_W);
        }
        if (snowflakes[i].x < 0) snowflakes[i].x = SCREEN_W - 1;
        if (snowflakes[i].x >= SCREEN_W) snowflakes[i].x = 0;
        if (snowflakes[i].y >= 0) {
            canvas.drawPixel(snowflakes[i].x, snowflakes[i].y, snowColor);
            if (i % 3 == 0) {
                canvas.drawPixel(snowflakes[i].x + 1, snowflakes[i].y, snowColor);
                canvas.drawPixel(snowflakes[i].x, snowflakes[i].y + 1, snowColor);
            }
        }
    }
}

static void drawFogParticles(M5Canvas& canvas) {
    uint16_t fogColor = C(160, 160, 165);
    int offset = (millis() / 200) % 3;
    for (int y = 20 + offset; y < GROUND_Y; y += 4) {
        for (int x = (y % 6); x < SCREEN_W; x += 6) {
            canvas.drawPixel(x, y, fogColor);
        }
    }
}

static void drawParticles(M5Canvas& canvas) {
    if (!particlesInit) initParticles();

    switch (currentWeather) {
        case WeatherType::RAIN:
            drawRainParticles(canvas, MAX_RAIN, 5, 5);
            break;
        case WeatherType::DRIZZLE:
            drawRainParticles(canvas, 8, 3, 3);
            break;
        case WeatherType::THUNDER: {
            drawRainParticles(canvas, MAX_RAIN, 5, 5);
            unsigned long now = millis();
            if (!thunderFlashing && now - lastThunderFlash > 3000 + random(2000)) {
                thunderFlashing = true;
                lastThunderFlash = now;
            }
            if (thunderFlashing) {
                if (now - lastThunderFlash < 50) {
                    canvas.fillScreen(C(200, 200, 220));
                    drawRainParticles(canvas, MAX_RAIN, 5, 5);
                } else {
                    thunderFlashing = false;
                }
            }
            break;
        }
        case WeatherType::SNOW:
            drawSnowParticles(canvas);
            break;
        case WeatherType::FOG:
            drawFogParticles(canvas);
            break;
        default:
            break;
    }
}

// ===== Main Weather Draw =====

void weatherDraw(M5Canvas& canvas) {
    int h = weatherCurrentHour();
    uint16_t skyColor, groundColor, groundTopColor;

    if (h >= 6 && h < 17) {
        skyColor = C(60, 120, 200);
        groundColor = C(80, 140, 60);
        groundTopColor = C(100, 170, 70);
    } else if (h >= 17 && h < 19) {
        skyColor = C(180, 80, 60);
        groundColor = C(46, 34, 47);
        groundTopColor = C(62, 53, 70);
    } else {
        skyColor = C(10, 10, 30);
        groundColor = C(46, 34, 47);
        groundTopColor = C(62, 53, 70);
    }

    bool hideSun = false;
    switch (currentWeather) {
        case WeatherType::OVERCAST:
            skyColor = blendRGB565(skyColor, C(130, 140, 160), 80);
            hideSun = true;
            break;
        case WeatherType::RAIN:
        case WeatherType::THUNDER:
            skyColor = blendRGB565(skyColor, C(60, 60, 75), 180);
            hideSun = true;
            break;
        case WeatherType::DRIZZLE:
            skyColor = blendRGB565(skyColor, C(90, 90, 105), 140);
            hideSun = true;
            break;
        case WeatherType::SNOW:
            skyColor = blendRGB565(skyColor, C(120, 120, 135), 150);
            hideSun = true;
            break;
        case WeatherType::FOG:
            skyColor = blendRGB565(skyColor, C(140, 140, 145), 170);
            hideSun = true;
            break;
        default:
            break;
    }

    // 1. Sky
    canvas.fillScreen(skyColor);

    // 2. Sky elements
    if (h >= 6 && h < 17) {
        if (!hideSun) {
            drawSun(canvas);
            drawClouds(canvas, C(220, 230, 240));
        } else {
            drawDarkClouds(canvas);
        }
    } else if (h >= 17 && h < 19) {
        if (!hideSun) {
            canvas.fillCircle(200, GROUND_Y - 10, 12, C(255, 220, 60));
            canvas.fillCircle(200, GROUND_Y - 10, 10, C(255, 160, 40));
        }
    } else {
        if (!hideSun) {
            drawStars(canvas);
            drawMoon(canvas, skyColor);
        }
    }

    // 3. Particles
    drawParticles(canvas);

    // 4. Ground
    canvas.fillRect(0, GROUND_Y, SCREEN_W, SCREEN_H - GROUND_Y, groundColor);
    canvas.drawFastHLine(0, GROUND_Y, SCREEN_W, groundTopColor);
    for (int i = 0; i < 8; i++) {
        int gx = (i * 31 + 10) % SCREEN_W;
        canvas.drawPixel(gx, GROUND_Y + 4, groundTopColor);
        canvas.drawPixel(gx + 15, GROUND_Y + 8, groundTopColor);
    }
}
