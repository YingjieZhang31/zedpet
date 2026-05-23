#include "claude_ui.h"

#include <M5Cardputer.h>
#include <WiFi.h>
#include <algorithm>
#include <vector>

#include "claude_client_parsing.h"
#include "config.h"

namespace {
constexpr int FONT_W = 6;
constexpr int FONT_H = 8;

const char* SPINNER_FRAMES[] = {"|", "/", "-", "\\"};
constexpr uint8_t SPINNER_COUNT = 4;
}  // namespace

void ClaudeUi::begin() {
    // Nothing yet; placeholder for future setup-time work.
}

void ClaudeUi::enter() {
    active_ = true;
    state_ = State::Idle;
    inputBuf_ = "";
    errorMsg_ = "";
    scrollLines_ = 0;
    inputDirty_ = replyDirty_ = headerDirty_ = true;
    M5Cardputer.Display.fillScreen(CLAUDE_BG);
    drawAll();
}

void ClaudeUi::exit() {
    if (client_.isBusy()) client_.cancel();
    active_ = false;
    M5Cardputer.Display.fillScreen(TFT_BLACK);
}

void ClaudeUi::update() {
    if (!active_) return;

    handleKeys();
    client_.update();

    // Animate spinner / cursor
    uint32_t now = millis();
    if ((state_ == State::Waiting || state_ == State::Streaming)
            && now - lastSpinnerAt_ > 100) {
        spinnerIdx_ = (spinnerIdx_ + 1) % SPINNER_COUNT;
        lastSpinnerAt_ = now;
        headerDirty_ = true;
    }
    if (state_ == State::Typing && now - lastCursorBlinkAt_ > 500) {
        cursorOn_ = !cursorOn_;
        lastCursorBlinkAt_ = now;
        inputDirty_ = true;
    }

    if (headerDirty_) drawHeader();
    if (replyDirty_)  drawReply();
    if (inputDirty_)  drawInput();
}

void ClaudeUi::handleKeys() {
    M5Cardputer.update();
    auto ks = M5Cardputer.Keyboard.keysState();

    // Ctrl+C → cancel mid-operation / clear error
    bool ctrlC = ks.ctrl && std::find(ks.word.begin(), ks.word.end(), 'c') != ks.word.end();
    if (ctrlC) {
        if (state_ == State::Waiting || state_ == State::Streaming) {
            client_.cancel();
            state_ = State::Idle;
            headerDirty_ = true;
            return;
        }
        if (state_ == State::Error) {
            state_ = State::Idle;
            errorMsg_ = "";
            headerDirty_ = true;
            replyDirty_ = true;
            return;
        }
    }

    // Enter: send current input
    if (ks.enter && state_ == State::Typing && inputBuf_.length() > 0) {
        sendCurrentInput();
        return;
    }

    // Backspace: delete last char while typing
    if (ks.del && state_ == State::Typing && inputBuf_.length() > 0) {
        inputBuf_.remove(inputBuf_.length() - 1);
        inputDirty_ = true;
        return;
    }

    // Character input: only when Idle or Typing
    if (state_ != State::Idle && state_ != State::Typing) return;
    for (auto k : ks.word) {
        if (k == 'c' && ks.ctrl) continue;     // already handled above
        if (inputBuf_.length() >= CLAUDE_MAX_INPUT) break;
        if (k >= 32 && k < 127) {
            inputBuf_ += (char)k;
            state_ = State::Typing;
            inputDirty_ = true;
        }
    }
}

void ClaudeUi::sendCurrentInput() {
    String prompt = inputBuf_;
    inputBuf_ = "";
    inputDirty_ = true;
    state_ = State::Waiting;
    headerDirty_ = true;

    // Echo the user prompt into the reply log so user can see what they asked.
    replyBuf_ += "> ";
    replyBuf_ += prompt;
    replyBuf_ += "\n";
    replyDirty_ = true;

    client_.send(
        String(CLAUDE_SERVER), prompt,
        [this](const String& chunk) { this->appendReplyChunk(chunk); },
        [this](bool ok, const String& err) { this->onStreamDone(ok, err); });
}

void ClaudeUi::appendReplyChunk(const String& chunk) {
    if (state_ == State::Waiting) {
        state_ = State::Streaming;
        headerDirty_ = true;
    }
    replyBuf_ += chunk;

    // Cap memory growth
    std::string s(replyBuf_.c_str());
    std::string trimmed = claude_parsing::truncate_keep_tail(s, CLAUDE_REPLY_CAP, CLAUDE_REPLY_TRIM_TO);
    if (trimmed.size() != s.size()) {
        replyBuf_ = trimmed.c_str();
    }
    replyDirty_ = true;
}

void ClaudeUi::onStreamDone(bool ok, const String& err) {
    if (!ok) {
        state_ = State::Error;
        errorMsg_ = err;
    } else {
        state_ = State::Idle;
        replyBuf_ += "\n";
    }
    headerDirty_ = true;
    replyDirty_ = true;
}

void ClaudeUi::drawAll() {
    drawHeader();
    drawReply();
    drawInput();
}

void ClaudeUi::drawHeader() {
    auto& d = M5Cardputer.Display;
    int x = SCREEN_W - 16;
    int y = 0;
    d.fillRect(x, y, 16, 10, CLAUDE_BG);
    d.setTextSize(1);

    const char* glyph = "";
    uint16_t color = CLAUDE_STATUS_FG;
    if (state_ == State::Waiting || state_ == State::Streaming) {
        glyph = SPINNER_FRAMES[spinnerIdx_];
    } else if (state_ == State::Error) {
        glyph = "!";
        color = CLAUDE_ERROR_FG;
    }
    d.setTextColor(color, CLAUDE_BG);
    d.setCursor(x + 2, y + 1);
    d.print(glyph);

    // WiFi dot: bottom-right of reply area
    int wx = SCREEN_W - 6;
    int wy = CLAUDE_REPLY_H - 6;
    bool wifiUp = WiFi.status() == WL_CONNECTED;
    d.fillRect(wx, wy, 4, 4, wifiUp ? CLAUDE_WIFI_OK : CLAUDE_WIFI_BAD);

    headerDirty_ = false;
}

void ClaudeUi::drawReply() {
    auto& d = M5Cardputer.Display;
    d.fillRect(0, CLAUDE_REPLY_Y, SCREEN_W, CLAUDE_REPLY_H, CLAUDE_BG);
    d.setTextSize(1);

    std::vector<std::string> lines = claude_parsing::wrap_text(
        std::string(replyBuf_.c_str()), CLAUDE_COLS);

    int maxLines = CLAUDE_VISIBLE_LINES;
    int total = (int)lines.size();
    int first = total > maxLines ? total - maxLines : 0;

    int y = CLAUDE_REPLY_Y;
    for (int i = first; i < total; ++i) {
        const std::string& l = lines[i];
        uint16_t color = claude_parsing::is_error_line(l) ? CLAUDE_ERROR_FG : CLAUDE_FG;
        d.setTextColor(color, CLAUDE_BG);
        d.setCursor(0, y);
        d.print(l.c_str());
        y += CLAUDE_LINE_H;
    }

    if (state_ == State::Error) {
        d.setTextColor(CLAUDE_ERROR_FG, CLAUDE_BG);
        d.setCursor(0, CLAUDE_REPLY_H - CLAUDE_LINE_H - 1);
        String prefix = "[error] ";
        d.print((prefix + errorMsg_).c_str());
    }

    replyDirty_ = false;
}

void ClaudeUi::drawInput() {
    auto& d = M5Cardputer.Display;
    int y = SCREEN_H - CLAUDE_INPUT_H;
    d.fillRect(0, y, SCREEN_W, CLAUDE_INPUT_H, CLAUDE_INPUT_BG);
    d.setTextSize(1);

    d.setTextColor(CLAUDE_PROMPT_FG, CLAUDE_INPUT_BG);
    d.setCursor(2, y + 5);
    d.print("> ");

    d.setTextColor(CLAUDE_FG, CLAUDE_INPUT_BG);
    int maxShow = CLAUDE_COLS - 2;
    String show = inputBuf_;
    if ((int)show.length() > maxShow) {
        show = show.substring(show.length() - maxShow);
    }
    d.print(show.c_str());

    if (state_ == State::Typing && cursorOn_) {
        d.print('_');
    }

    inputDirty_ = false;
}

void ClaudeUi::clearStatusArea() {
    // Reserved for future use.
}
