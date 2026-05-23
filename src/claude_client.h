#pragma once
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <functional>

class ClaudeClient {
public:
    using OnChunkCb = std::function<void(const String& chunk)>;
    using OnDoneCb  = std::function<void(bool ok, const String& err)>;

    // Begin a request. Returns false if WiFi is down or another request is in
    // progress. Callbacks fire from update().
    bool send(const String& serverUrl, const String& prompt,
              OnChunkCb onChunk, OnDoneCb onDone);

    // Poll once per loop iteration; advances the read state machine and may
    // invoke onChunk / onDone.
    void update();

    bool isBusy() const { return state_ != State::Idle; }

    // Close the connection and return to Idle without firing onDone.
    void cancel();

private:
    enum class State { Idle, Reading, Done, Error };

    void finishOk();
    void finishErr(const String& msg);

    State state_ = State::Idle;
    HTTPClient http_;
    WiFiClient* stream_ = nullptr;     // borrowed from http_; do not delete
    OnChunkCb onChunk_;
    OnDoneCb  onDone_;
    uint32_t lastByteAt_ = 0;
    static constexpr uint32_t IDLE_TIMEOUT_MS = 60000;
};
