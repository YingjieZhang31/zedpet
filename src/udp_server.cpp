#include "udp_server.h"
#include <sys/time.h>
#include <time.h>

// ===== Time Sync =====

void UdpServer::syncTime() {
    configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov");
    Serial.println("[UDP] Syncing time via NTP...");
    struct tm ti;
    if (getLocalTime(&ti, 5000)) {
        timeSynced = true;
        Serial.printf("[UDP] Time synced: %02d:%02d\n", ti.tm_hour, ti.tm_min);
    } else {
        Serial.println("[UDP] NTP sync failed, starting from 00:00");
        struct timeval tv = {};
        tv.tv_sec = 8 * 3600;
        settimeofday(&tv, nullptr);
        timeSynced = true;
    }
}

// ===== Public API =====

void UdpServer::begin() {
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
        syncTime();
    } else {
        Serial.println("\n[UDP] WiFi connection failed!");
    }
}

bool UdpServer::isWiFiConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

const char* UdpServer::getLocalIP() const {
    static char ipStr[16];
    if (WiFi.status() == WL_CONNECTED) {
        strcpy(ipStr, WiFi.localIP().toString().c_str());
    } else {
        ipStr[0] = '\0';
    }
    return ipStr;
}

const char* UdpServer::getCurrentTime() const {
    static char timeStr[6];
    struct tm ti;
    if (getLocalTime(&ti, 0)) {
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d", ti.tm_hour, ti.tm_min);
    } else {
        timeStr[0] = '\0';
    }
    return timeStr;
}

// ===== JSON Command Parsing =====

const char* UdpServer::parseCommand() {
    // Find "cmd" key: skip optional whitespace, look for "cmd"
    const char* p = packetBuffer;
    while (*p == ' ' || *p == '\t' || *p == '{') p++;

    const char* key = strstr(p, "\"cmd\"");
    if (!key) return nullptr;

    // Skip past "cmd" and optional whitespace to find ':'
    key += 5;
    while (*key == ' ' || *key == '\t') key++;
    if (*key != ':') return nullptr;
    key++;
    while (*key == ' ' || *key == '\t') key++;
    if (*key != '"') return nullptr;
    key++;

    static char cmd[16];
    int i = 0;
    while (*key && *key != '"' && i < 15) {
        cmd[i++] = *key++;
    }
    cmd[i] = '\0';
    return cmd;
}

const char* UdpServer::checkCommand() {
    if (WiFi.status() != WL_CONNECTED) return nullptr;

    int packetSize = udp.parsePacket();
    if (packetSize <= 0) return nullptr;

    int len = udp.read(packetBuffer, sizeof(packetBuffer) - 1);
    if (len <= 0) return nullptr;
    packetBuffer[len] = '\0';

    Serial.printf("[UDP] Received: %s\n", packetBuffer);

    return parseCommand();
}

void UdpServer::sendAck(const char* cmd) {
    if (WiFi.status() != WL_CONNECTED) return;

    char ack[64];
    snprintf(ack, sizeof(ack), "{\"ack\":\"%s\",\"status\":\"ok\"}", cmd);

    udp.beginPacket(udp.remoteIP(), udp.remotePort());
    udp.print(ack);
    udp.endPacket();

    Serial.printf("[UDP] Sent ACK: %s\n", ack);
}

// ===== Global instance =====
UdpServer udpServer;
