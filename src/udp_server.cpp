#include "udp_server.h"

static WiFiUDP udp;
static char packetBuffer[256];

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
    } else {
        Serial.println("\n[UDP] WiFi connection failed!");
    }
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
