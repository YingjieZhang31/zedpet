#pragma once
#include <WiFi.h>
#include <WiFiUdp.h>

constexpr const char* WIFI_SSID = "your-ssid";
constexpr const char* WIFI_PASS = "your-password";
constexpr int UDP_PORT = 19820;

void udpServerBegin();
const char* udpCheckCommand();  // returns cmd string or nullptr
void udpSendAck(const char* cmd);
