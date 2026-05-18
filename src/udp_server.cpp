#include "udp_server.h"
#include <time.h>

static WiFiUDP udp;
static char packetBuffer[256];
static bool timeSynced = false;

void udpServerBegin() {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.printf("[UDP] Connecting to WiFi %s", WIFI_SSID);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[UDP] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        udp.begin(UDP_PORT);
        Serial.printf("[UDP] Listening on port %d\n", UDP_PORT);

        // NTP time sync (UTC+8)
        configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov");
        Serial.println("[UDP] Syncing time via NTP...");
        struct tm ti;
        if (getLocalTime(&ti, 5000)) {
            timeSynced = true;
            Serial.printf("[UDP] Time synced: %02d:%02d\n", ti.tm_hour, ti.tm_min);
        } else {
            Serial.println("[UDP] NTP sync failed");
        }
    } else {
        Serial.println("\n[UDP] WiFi connection failed!");
    }
}

bool udpIsWiFiConnected() {
    return WiFi.status() == WL_CONNECTED;
}

const char* udpGetLocalIP() {
    static char ipStr[16];
    if (WiFi.status() == WL_CONNECTED) {
        strcpy(ipStr, WiFi.localIP().toString().c_str());
    } else {
        ipStr[0] = '\0';
    }
    return ipStr;
}

const char* udpCheckCommand() {
    if (WiFi.status() != WL_CONNECTED) return nullptr;

    int packetSize = udp.parsePacket();
    if (packetSize <= 0) return nullptr;

    int len = udp.read(packetBuffer, sizeof(packetBuffer) - 1);
    if (len <= 0) return nullptr;
    packetBuffer[len] = '\0';

    Serial.printf("[UDP] Received: %s\n", packetBuffer);

    // Parse JSON: {"cmd":"happy"} -> "happy"
    const char* cmdStart = strstr(packetBuffer, "\"cmd\":\"");
    if (!cmdStart) return nullptr;
    cmdStart += 7;  // skip "cmd":"

    static char cmd[16];
    int i = 0;
    while (*cmdStart && *cmdStart != '\"' && i < 15) {
        cmd[i++] = *cmdStart++;
    }
    cmd[i] = '\0';

    return cmd;
}

void udpSendAck(const char* cmd) {
    if (WiFi.status() != WL_CONNECTED) return;

    char ack[64];
    snprintf(ack, sizeof(ack), "{\"ack\":\"%s\",\"status\":\"ok\"}", cmd);

    udp.beginPacket(udp.remoteIP(), udp.remotePort());
    udp.print(ack);
    udp.endPacket();

    Serial.printf("[UDP] Sent ACK: %s\n", ack);
}

const char* udpGetCurrentTime() {
    static char timeStr[6];
    struct tm ti;
    if (timeSynced && getLocalTime(&ti, 0)) {
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d", ti.tm_hour, ti.tm_min);
    } else {
        timeStr[0] = '\0';
    }
    return timeStr;
}
