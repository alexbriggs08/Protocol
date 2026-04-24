#include "GameState.h"
#include "ItemDefs.h"
#include "Player.h"
#include "raylib.h"
#include "version.h"
#include <algorithm>
#include <deque>
#include <format>
#include <string>
#include <vector>

// ============================================================
// CONSTANTS
// ============================================================
static constexpr int   DEFAULT_W    = 1024;
static constexpr int   DEFAULT_H    = 768;
static constexpr int   HUD_H        = 60;
static constexpr int   INPUT_H      = 25;
static constexpr int   FONT_SZ      = 16;
static constexpr int   LINE_H       = FONT_SZ + 4;
static constexpr int   MAX_LINES    = 200;
static constexpr int   MAX_HISTORY  = 20;
static constexpr float CURSOR_BLINK = 0.5f;
static constexpr float DECAY_TICK   = 1.0f;

static const Color C_LIME   = {57,  255, 20,  255};  // #39FF14
static const Color C_DIM    = {42,  201, 16,  255};  // #2AC910
static const Color C_HUD_BG = {10,  26,  0,   255};  // #0A1A00
static const Color C_AMBER  = {255, 165, 0,   255};  // #FFA500
static const Color C_RED    = {255, 34,  34,  255};  // #FF2222

// ============================================================
// ENUMS
// ============================================================
enum class AppState  { MainMenu, Playing, Dead };
enum class InputMode { Command, LimbSelect, DamageLevel };

// ============================================================
// TERMINAL BUFFER
// ============================================================
struct ColoredLine {
    std::string text;
    Color       color = C_LIME;
};

struct TerminalBuffer {
    std::deque<ColoredLine> lines;
    int scrollOffset = 0;  // 0 = bottom, positive = scrolled up N lines

    void push(const std::string& text, Color color = C_LIME) {
        lines.push_back({text, color});
        if ((int)lines.size() > MAX_LINES)
            lines.pop_front();
        scrollOffset = 0;  // auto-scroll to bottom on new output
    }

    void scrollUp(int n = 3) {
        scrollOffset = std::min(scrollOffset + n, (int)lines.size() - 1);
    }
    void scrollDown(int n = 3) {
        scrollOffset = std::max(scrollOffset - n, 0);
    }
};

// ============================================================
// ANIMATION QUEUE
// replaces blocking typeText / stutterText sleep loops
// ============================================================
struct AnimEvent {
    std::string fullText;
    int         charsRevealed = 0;
    float       timer         = 0.f;
    float       charDelay     = 0.04f;  // seconds per character
    bool        isStutter     = false;
    Color       color         = C_LIME;
};

struct AnimationQueue {
    std::deque<AnimEvent> events;
    TerminalBuffer*       buf = nullptr;  // set before use

    // Splits text on '\n' and enqueues each line as a separate event.
    void enqueue(const std::string& text, float charDelayMs,
                 bool stutter, Color color) {
        std::string line;
        for (char c : text) {
            if (c == '\n') {
                events.push_back({line, 0, 0.f, charDelayMs / 1000.f, stutter, color});
                line.clear();
            } else {
                line += c;
            }
        }
        if (!line.empty())
            events.push_back({line, 0, 0.f, charDelayMs / 1000.f, stutter, color});
    }

    void typeText(const std::string& t, Color c = C_LIME) {
        enqueue(t, 40.f, false, c);
    }
    void stutterText(const std::string& t) {
        enqueue(t, 30.f, true, C_RED);
    }

    bool        isActive()       const { return !events.empty(); }
    std::string currentPartial() const {
        if (events.empty()) return "";
        return events.front().fullText.substr(0, events.front().charsRevealed);
    }
    Color currentColor() const {
        return events.empty() ? C_LIME : events.front().color;
    }

    // Call once per frame with delta time in seconds.
    void advance(float dt) {
        if (events.empty() || !buf) return;
        AnimEvent& ev = events.front();

        float delay = ev.isStutter
            ? (rand() % 10 == 0 ? 0.2f : 0.03f)
            : ev.charDelay;

        ev.timer += dt;
        while (ev.timer >= delay) {
            ev.timer -= delay;
            if (ev.charsRevealed < (int)ev.fullText.size()) {
                ev.charsRevealed++;
                if (ev.isStutter)
                    delay = (rand() % 10 == 0 ? 0.2f : 0.03f);
            } else {
                buf->push(ev.fullText, ev.color);
                events.pop_front();
                return;
            }
        }
    }
};

// ============================================================
// INPUT STATE
// ============================================================
struct InputState {
    std::string              current;
    std::vector<std::string> history;
    int                      historyIdx  = -1;
    float                    cursorTimer = 0.f;
    bool                     cursorVis   = true;
    InputMode                mode        = InputMode::Command;
    std::string              pendingCmd;   // "SYS_HEAL" or "DAMAGE"
    std::string              pendingLimb;  // stored between DAMAGE prompts
};

// ============================================================
// GLOBAL RENDERER CONTEXT
// playHealingAnimation is declared in Player.h and defined here
// so that Player::healLimb can output into the terminal buffer.
// ============================================================
static TerminalBuffer* g_tbuf = nullptr;
static AnimationQueue* g_anim = nullptr;

void playHealingAnimation(const std::string& statusMessage) {
    if (!g_tbuf || !g_anim) return;
    g_tbuf->push("[!] INITIATING BIO-RECONSTRUCTION...");
    g_tbuf->push("PROGRESS: [████████████████████] 100%");
    g_anim->typeText("SYSTEM OS: " + statusMessage + "\n\n");
}

// ============================================================
// FORWARD DECLARATIONS
// ============================================================
static void pushMainMenu(TerminalBuffer& buf);
static void pushCommandMenu(TerminalBuffer& buf);
static void pushIntro(AnimationQueue& anim);
static void handleCommand(const std::string& input, GameState& state,
                          TerminalBuffer& buf, AnimationQueue& anim,
                          InputState& inputSt, AppState& appState);
static void handleSubPrompt(const std::string& input, GameState& state,
                            TerminalBuffer& buf, AnimationQueue& anim,
                            InputState& inputSt);
static void drawTerminal(const TerminalBuffer& buf, const AnimationQueue& anim,
                         Font font, int screenW, int screenH);
static void drawInputLine(const InputState& st, Font font, int screenW, int screenH);
static void drawHUD(const Player& player, Font font, int screenW, int screenH);

// ============================================================
// STUB — filled in subsequent tasks
// ============================================================
int main() { return 0; }
