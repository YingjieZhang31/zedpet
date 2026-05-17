#pragma once
#include <WiFi.h>
#include <WiFiUdp.h>

// WIFI_SSID / WIFI_PASS come from PlatformIO build_flags via env vars.
// Set before building:  export WIFI_SSID="..." WIFI_PASS="..."
constexpr int UDP_PORT = 19820;

void udpServerBegin();
const char* udpCheckCommand();  // returns cmd string or nullptr
void udpSendAck(const char* cmd);
bool udpIsWiFiConnected();
const char* udpGetLocalIP();
