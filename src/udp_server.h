#pragma once
#include <WiFi.h>
#include <WiFiUdp.h>

constexpr int UDP_PORT = 19820;

class UdpServer {
public:
    void begin();
    const char* checkCommand();
    void sendAck(const char* cmd);
    bool isWiFiConnected() const;
    const char* getLocalIP() const;
    const char* getCurrentTime() const;

private:
    WiFiUDP udp;
    char packetBuffer[256] = {};
    bool timeSynced = false;

    void syncTime();
    const char* parseCommand();
};

extern UdpServer udpServer;

// Backward-compatible wrappers
inline void udpServerBegin()           { udpServer.begin(); }
inline const char* udpCheckCommand()   { return udpServer.checkCommand(); }
inline void udpSendAck(const char* c)  { udpServer.sendAck(c); }
inline bool udpIsWiFiConnected()       { return udpServer.isWiFiConnected(); }
inline const char* udpGetLocalIP()     { return udpServer.getLocalIP(); }
inline const char* udpGetCurrentTime() { return udpServer.getCurrentTime(); }
