#pragma once
#include <Arduino.h>
#include "claude_client.h"

class ClaudeUi {
public:
    void begin();         // called once from setup()
    void enter();         // entering Claude mode: clear screen, draw UI
    void exit();          // leaving Claude mode: clear screen
    void update();        // call every loop iter while active
    bool isActive() const { return active_; }

private:
    enum class State { Idle, Typing, Waiting, Streaming, Error };

    bool active_ = false;
    State state_ = State::Idle;
    String inputBuf_;
    String replyBuf_;
    String errorMsg_;
    int scrollLines_ = 0;          // 0 = follow bottom; >0 = lines scrolled up
    bool inputDirty_ = true;
    bool replyDirty_ = true;
    bool headerDirty_ = true;
    uint32_t lastSpinnerAt_ = 0;
    uint8_t spinnerIdx_ = 0;
    uint32_t lastCursorBlinkAt_ = 0;
    bool cursorOn_ = true;

    ClaudeClient client_;
    std::vector<char> prevKeys_;

    void handleKeys();
    void sendCurrentInput();
    void appendReplyChunk(const String& chunk);
    void onStreamDone(bool ok, const String& err);

    void drawAll();
    void drawHeader();
    void drawReply();
    void drawInput();
    void clearStatusArea();
};
