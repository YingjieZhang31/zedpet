#include "claude_client.h"
#include <WiFi.h>

bool ClaudeClient::send(const String& serverUrl, const String& prompt,
                        OnChunkCb onChunk, OnDoneCb onDone) {
    if (state_ != State::Idle) return false;
    if (WiFi.status() != WL_CONNECTED) {
        onDone(false, "No WiFi");
        return false;
    }

    onChunk_ = std::move(onChunk);
    onDone_  = std::move(onDone);

    String url = serverUrl + "/ask";
    if (!http_.begin(url)) {
        onDone_(false, "begin() failed");
        return false;
    }
    http_.addHeader("Content-Type", "application/json");
    http_.setTimeout(IDLE_TIMEOUT_MS);

    // Build JSON manually (no ArduinoJson dep). Escape only quotes and
    // backslashes — Cardputer input is ASCII so no Unicode worries.
    String body;
    body.reserve(prompt.length() + 16);
    body = "{\"text\":\"";
    for (size_t i = 0; i < prompt.length(); ++i) {
        char c = prompt[i];
        if (c == '\\' || c == '"') body += '\\';
        body += c;
    }
    body += "\"}";

    int code = http_.POST(body);
    if (code != 200) {
        String err = "HTTP " + String(code);
        http_.end();
        onDone_(false, err);
        return false;
    }
    stream_ = http_.getStreamPtr();
    state_ = State::Reading;
    lastByteAt_ = millis();
    return true;
}

void ClaudeClient::update() {
    if (state_ != State::Reading) return;

    // Idle-byte timeout
    if (millis() - lastByteAt_ > IDLE_TIMEOUT_MS) {
        finishOk();  // treat as natural end
        return;
    }

    if (!stream_) { finishErr("stream lost"); return; }

    size_t avail = stream_->available();
    if (avail == 0) {
        // Connection closed?
        if (!http_.connected()) { finishOk(); }
        return;
    }

    // Read up to 256 bytes per tick to avoid blocking the loop too long
    char buf[257];
    size_t want = avail > 256 ? 256 : avail;
    int n = stream_->readBytes(buf, want);
    if (n <= 0) return;
    buf[n] = '\0';
    lastByteAt_ = millis();
    onChunk_(String(buf));
}

void ClaudeClient::cancel() {
    if (state_ == State::Idle) return;
    http_.end();
    stream_ = nullptr;
    state_ = State::Idle;
    onChunk_ = nullptr;
    onDone_  = nullptr;
    // Intentionally no onDone callback — caller initiated.
}

void ClaudeClient::finishOk() {
    http_.end();
    stream_ = nullptr;
    state_ = State::Idle;
    auto cb = onDone_;
    onDone_ = nullptr; onChunk_ = nullptr;
    if (cb) cb(true, "");
}

void ClaudeClient::finishErr(const String& msg) {
    http_.end();
    stream_ = nullptr;
    state_ = State::Idle;
    auto cb = onDone_;
    onDone_ = nullptr; onChunk_ = nullptr;
    if (cb) cb(false, msg);
}
