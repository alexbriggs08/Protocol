#include "GameState.h"
#include "ItemDefs.h"
#include "Player.h"
#include "raylib.h"
#include "version.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <format>
#include <string>
#include <vector>

// ============================================================
// CONSTANTS
// ============================================================
static constexpr int DEFAULT_W = 1024;
static constexpr int DEFAULT_H = 768;
static constexpr int HUD_H = 60;
static constexpr int INPUT_H = 25;
static constexpr int FONT_SZ = 16;
static constexpr int LINE_H = FONT_SZ + 4;
static constexpr int MAX_LINES = 200;
static constexpr int MAX_HISTORY = 20;
static constexpr float CURSOR_BLINK = 0.5f;
static constexpr float DECAY_TICK = 1.0f;

static constexpr Color C_LIME = {57, 255, 20, 255};     // #39FF14
static constexpr Color C_DIM = {42, 201, 16, 255};      // #2AC910
static constexpr Color C_HUD_BG = {10, 26, 0, 255};     // #0A1A00
static constexpr Color C_AMBER = {255, 165, 0, 255};    // #FFA500
static constexpr Color C_RED = {255, 34, 34, 255};      // #FF2222
static constexpr Color C_DIM_TRACK = {42, 201, 16, 40}; // scrollbar track (dim)
static constexpr Color C_DIM_LINE = {42, 201, 16, 100}; // separator line (mid)

// ============================================================
// ENUMS
// ============================================================
enum class AppState { MainMenu, Playing, Dead };
enum class InputMode { Command, LimbSelect, DamageLevel };

// ============================================================
// TERMINAL BUFFER
// ============================================================
struct ColoredLine {
  std::string text;
  Color color = C_LIME;
};

struct TerminalBuffer {
  std::deque<ColoredLine> lines;
  int scrollOffset = 0; // 0 = bottom, positive = scrolled up N lines

  void push(const std::string &text, Color color = C_LIME) {
    lines.push_back({text, color});
    if ((int)lines.size() > MAX_LINES)
      lines.pop_front();
    scrollOffset = 0; // auto-scroll to bottom on new output
  }

  void scrollUp(int n = 3) {
    if (lines.empty())
      return;
    scrollOffset = std::min(scrollOffset + n, (int)lines.size() - 1);
  }
  void scrollDown(int n = 3) { scrollOffset = std::max(scrollOffset - n, 0); }
};

// ============================================================
// ANIMATION QUEUE
// replaces blocking typeText / stutterText sleep loops
// ============================================================
struct AnimEvent {
  std::string fullText;
  int charsRevealed = 0;
  float timer = 0.f;
  float charDelay = 0.04f; // seconds per character
  bool isStutter = false;
  Color color = C_LIME;
};

struct AnimationQueue {
  std::deque<AnimEvent> events;
  TerminalBuffer *buf = nullptr; // set before use

  // Splits text on '\n' and enqueues each line as a separate event.
  void enqueue(const std::string &text, float charDelayMs, bool stutter,
               Color color) {
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

  void typeText(const std::string &t, Color c = C_LIME) {
    enqueue(t, 40.f, false, c);
  }
  void stutterText(const std::string &t) { enqueue(t, 30.f, true, C_RED); }

  bool isActive() const { return !events.empty(); }
  std::string currentPartial() const {
    if (events.empty())
      return "";
    return events.front().fullText.substr(0, events.front().charsRevealed);
  }
  Color currentColor() const {
    return events.empty() ? C_LIME : events.front().color;
  }

  // Call once per frame with delta time in seconds.
  void advance(float dt) {
    if (events.empty() || !buf)
      return;
    AnimEvent &ev = events.front();

    float delay =
        ev.isStutter ? (rand() % 10 == 0 ? 0.2f : 0.03f) : ev.charDelay;

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
  std::string current;
  std::vector<std::string> history;
  int historyIdx = -1;
  float cursorTimer = 0.f;
  bool cursorVis = true;
  InputMode mode = InputMode::Command;
  std::string pendingCmd;  // "SYS_HEAL" or "DAMAGE"
  std::string pendingLimb; // stored between DAMAGE prompts
};

// ============================================================
// GLOBAL RENDERER CONTEXT
// playHealingAnimation is declared in Player.h and defined here
// so that Player::healLimb can output into the terminal buffer.
// ============================================================
static TerminalBuffer *g_tbuf = nullptr;
static AnimationQueue *g_anim = nullptr;

void playHealingAnimation(const std::string &statusMessage) {
  if (!g_tbuf || !g_anim)
    return;
  g_tbuf->push("[!] INITIATING BIO-RECONSTRUCTION...");
  g_tbuf->push("PROGRESS: [████████████████████] 100%");
  g_anim->typeText("SYSTEM OS: " + statusMessage + "\n\n");
}

// ============================================================
// FORWARD DECLARATIONS
// ============================================================
static void pushMainMenu(TerminalBuffer &buf);
static void pushCommandMenu(TerminalBuffer &buf);
static void pushIntro(AnimationQueue &anim);
static void handleCommand(const std::string &input, GameState &state,
                          TerminalBuffer &buf, AnimationQueue &anim,
                          InputState &inputSt, AppState &appState);
static void handleSubPrompt(const std::string &input, GameState &state,
                            TerminalBuffer &buf, AnimationQueue &anim,
                            InputState &inputSt);
static void drawTerminal(const TerminalBuffer &buf, const AnimationQueue &anim,
                         Font font, int screenW, int screenH);
static void drawInputLine(const InputState &st, Font font, int screenW,
                          int screenH);
static void drawHUD(const Player &player, Font font, int screenW, int screenH);

// ============================================================
// RENDERING — TERMINAL AREA
// ============================================================
static void drawTerminal(const TerminalBuffer &buf, const AnimationQueue &anim,
                         Font font, int screenW, int screenH) {
  int termH = screenH - HUD_H - INPUT_H;
  int maxVisible = termH / LINE_H;
  int totalLines = (int)buf.lines.size() + (anim.isActive() ? 1 : 0);
  int startIdx =
      std::max(0, (int)buf.lines.size() - maxVisible - buf.scrollOffset);

  // Scroll indicator (right edge strip)
  if (buf.scrollOffset > 0 && totalLines > maxVisible) {
    int indicH = std::max(20, termH * maxVisible / totalLines);
    int indicY = termH - indicH -
                 (buf.scrollOffset * (termH - indicH) /
                  std::max(totalLines - maxVisible, 1));
    indicY = std::max(0, indicY);
    DrawRectangle(screenW - 4, 0, 4, termH, C_DIM_TRACK);
    DrawRectangle(screenW - 4, indicY, 4, indicH, C_LIME);
  }

  int y = 4;
  for (int i = startIdx; i < (int)buf.lines.size() && y + LINE_H <= termH;
       ++i) {
    DrawTextEx(font, buf.lines[i].text.c_str(), {4.f, (float)y}, FONT_SZ, 1,
               buf.lines[i].color);
    y += LINE_H;
  }

  // Partial animation line rendered below committed lines
  if (anim.isActive() && buf.scrollOffset == 0 && y + LINE_H <= termH) {
    DrawTextEx(font, anim.currentPartial().c_str(), {4.f, (float)y}, FONT_SZ, 1,
               anim.currentColor());
  }
}

// ============================================================
// RENDERING — INPUT LINE
// ============================================================
static void drawInputLine(const InputState &st, Font font, int screenW,
                          int screenH) {
  int y = screenH - HUD_H - INPUT_H;
  DrawLine(0, y, screenW, y, C_DIM_LINE);

  const char *promptStr = st.mode == InputMode::Command ? "[PROTOCOL]> "
                          : st.mode == InputMode::LimbSelect
                              ? "TARGET LIMB: "
                              : "DAMAGE LEVEL [1-3]: ";

  std::string display = promptStr + st.current;
  if (st.cursorVis)
    display += '_';
  DrawTextEx(font, display.c_str(), {4.f, (float)(y + 4)}, FONT_SZ, 1, C_DIM);
}

// ============================================================
// RENDERING — HUD BAR
// ============================================================
// HUD_H must be >= 4 + 2 * LINE_H (currently 44); kept at 60 for breathing
// room.
static void drawHUD(const Player &player, Font font, int screenW, int screenH) {
  int y = screenH - HUD_H;
  DrawRectangle(0, y, screenW, HUD_H, C_HUD_BG);
  DrawLine(0, y, screenW, y, C_LIME);

  std::string row1 = std::format(
      "OVERALL:{:>4}  HEAD:{:>3}  TORSO:{:>3}  "
      "L.ARM:{:>3}  R.ARM:{:>3}  L.LEG:{:>3}  R.LEG:{:>3}",
      player.m_overallHealth, player.m_head, player.m_torso, player.m_leftArm,
      player.m_rightArm, player.m_leftLeg, player.m_rightLeg);

  std::string row2 =
      std::format("ENERGY:{:>5.1f}  HYDRATION:{:>5.1f}  RADIATION:{:>3}",
                  player.m_energy, player.m_hydration, player.m_radiation);

  DrawTextEx(font, row1.c_str(), {4.f, (float)(y + 4)}, FONT_SZ, 1, C_LIME);
  DrawTextEx(font, row2.c_str(), {4.f, (float)(y + 4 + LINE_H)}, FONT_SZ, 1,
             C_LIME);
}

// ============================================================
// TERMINAL CONTENT HELPERS
// ============================================================
static void pushMainMenu(TerminalBuffer &buf) {
  int padCount = std::max(0, 26 - (int)std::string(PROJECT_VERSION).size());
  std::string vpad(padCount, ' ');
  buf.push("");
  buf.push("╔══════════════════════════════════════════════╗");
  buf.push("║  PROTOCOL OS // v" + std::string(PROJECT_VERSION) + vpad + "║");
  buf.push("╠══════════════════════════════════════════════╣");
  buf.push("║  [1] NEW GAME                                ║");
  buf.push("║  [2] LOAD GAME                               ║");
  buf.push("║  [3] QUIT                                    ║");
  buf.push("╚══════════════════════════════════════════════╝");
  buf.push("");
}

static void pushCommandMenu(TerminalBuffer &buf) {
  buf.push("");
  buf.push("╔══════════════════════════════════════════════╗");
  buf.push("║  PROTOCOL OS // COMMAND INTERFACE v0.1       ║");
  buf.push("╠══════════════════════════════════════════════╣");
  buf.push("║  SYS_HEAL    » Initiate bio-reconstruction   ║");
  buf.push("║  SYS_DECON   » Suppress radiation            ║");
  buf.push("║  INT_CHARGE  » Restore energy reserves       ║");
  buf.push("║  INT_COOLANT » Restore hydration             ║");
  buf.push("║  DAMAGE      » Apply damage to limb          ║");
  buf.push("║  CURRENT     » Display biometric feed        ║");
  buf.push("║  SHW_STATS   » Display survival metrics      ║");
  buf.push("║  SHW_INV     » Display inventory manifest    ║");
  buf.push("║  SYS_SAVE    » Save current session          ║");
  buf.push("║  SYS_LOAD    » Restore saved session         ║");
  buf.push("║  SYS_HELP    » Reprint this menu             ║");
  buf.push("║  EXIT        » Terminate session             ║");
  buf.push("╚══════════════════════════════════════════════╝");
  buf.push("");
}

static void pushIntro(AnimationQueue &anim) {
  anim.typeText(std::string("PROTOCOL OS [Version ") + PROJECT_VERSION +
                "]...\n");
  anim.typeText("Initializing biometric link...\n");
  anim.typeText("Limb status: NOMINAL.\n");
  anim.typeText("Ready for input.\n\n");
}

// ============================================================
// COMMAND DISPATCHER
// ============================================================
static void handleCommand(const std::string& input, GameState& state,
                          TerminalBuffer& buf, AnimationQueue& anim,
                          InputState& inputSt, AppState& appState) {
    Player& player = state.player;

    // ── Main menu ──────────────────────────────────────────
    if (appState == AppState::MainMenu) {
        if (input == "1") {
            for (const char* name : {"Med-Kit", "Diazine (Grade B)",
                                     "Adreno-Spike 0.5mg",
                                     "Recycled Biometric Coolant"}) {
                if (auto def = findItemDef(name))
                    player.m_inventory.addItem(*def);
            }
            appState = AppState::Playing;
            pushIntro(anim);
            pushCommandMenu(buf);
        } else if (input == "2") {
            if (state.load("save.txt")) {
                appState = AppState::Playing;
                anim.typeText("SAVE RESTORED. RESUMING SESSION...\n\n");
                pushCommandMenu(buf);
            } else {
                buf.push("NO SAVE FILE FOUND.");
            }
        } else if (input == "3") {
            CloseWindow();
        } else {
            buf.push("ENTER 1, 2, OR 3.");
        }
        return;
    }

    // ── Playing commands ───────────────────────────────────
    if (input == "SYS_HEAL") {
        if (!player.m_inventory.findItem(ItemEffect::healLimb)) {
            buf.push("NO HEALING ITEMS IN INVENTORY.");
            return;
        }
        inputSt.mode       = InputMode::LimbSelect;
        inputSt.pendingCmd = "SYS_HEAL";

    } else if (input == "SYS_DECON") {
        const Item* item = player.m_inventory.findItem(ItemEffect::reduceRadiation);
        if (!item) { buf.push("NO DECONTAMINATION ITEMS."); return; }
        std::string msg  = item->useMessage;
        std::string name = item->name;
        player.applyItem(*item);
        player.m_inventory.removeItem(name);
        anim.typeText(msg + "\n");

    } else if (input == "INT_CHARGE") {
        const Item* item = player.m_inventory.findItem(ItemEffect::restoreEnergy);
        if (!item) { buf.push("NO ENERGY ITEMS."); return; }
        std::string msg  = item->useMessage;
        std::string name = item->name;
        player.applyItem(*item);
        player.m_inventory.removeItem(name);
        anim.typeText(msg + "\n");

    } else if (input == "INT_COOLANT") {
        const Item* item = player.m_inventory.findItem(ItemEffect::restoreHydration);
        if (!item) { buf.push("NO HYDRATION ITEMS."); return; }
        std::string msg  = item->useMessage;
        std::string name = item->name;
        player.applyItem(*item);
        player.m_inventory.removeItem(name);
        anim.typeText(msg + "\n");

    } else if (input == "SHW_INV") {
        if (player.m_inventory.isEmpty()) {
            buf.push("INVENTORY: EMPTY.");
        } else {
            for (int i = 0; i < player.m_inventory.size(); ++i) {
                Item it    = player.m_inventory.getItem(i);
                int  count = player.m_inventory.getCount(i);
                buf.push(std::format("[{}] {} x{} — {}", i + 1,
                                     it.name, count, it.description));
            }
        }

    } else if (input == "DAMAGE") {
        inputSt.mode       = InputMode::LimbSelect;
        inputSt.pendingCmd = "DAMAGE";

    } else if (input == "CURRENT") {
        buf.push("╔══════ BIOMETRIC FEED ══════╗");
        buf.push(std::format("║  {:<9}: {:<15}║", "OVERALL", player.getTotalHealth()));
        buf.push("╠════════════════════════════╣");
        buf.push(std::format("║  {:<9}: {:<15}║", "HEAD",      player.m_head));
        buf.push(std::format("║  {:<9}: {:<15}║", "TORSO",     player.m_torso));
        buf.push(std::format("║  {:<9}: {:<15}║", "LEFT ARM",  player.m_leftArm));
        buf.push(std::format("║  {:<9}: {:<15}║", "RIGHT ARM", player.m_rightArm));
        buf.push(std::format("║  {:<9}: {:<15}║", "LEFT LEG",  player.m_leftLeg));
        buf.push(std::format("║  {:<9}: {:<15}║", "RIGHT LEG", player.m_rightLeg));
        buf.push("╚════════════════════════════╝");

    } else if (input == "SHW_STATS") {
        buf.push(std::format("Radiation: {}", player.m_radiation));
        buf.push(std::format("Energy: {:.1f}", player.m_energy));
        buf.push(std::format("Hydration: {:.1f}", player.m_hydration));

    } else if (input == "SYS_SAVE") {
        state.save("save.txt");
        anim.typeText("SAVE COMPLETE.\n");

    } else if (input == "SYS_LOAD") {
        if (state.load("save.txt")) anim.typeText("SAVE RESTORED.\n");
        else buf.push("NO SAVE FILE FOUND.");

    } else if (input == "SYS_HELP") {
        pushCommandMenu(buf);

    } else if (input == "KILL") {
        player.m_head = 0;

    } else if (input == "EXIT") {
        buf.push("TERMINATING SESSION...");
        CloseWindow();

    } else {
        buf.push("UNRECOGNIZED COMMAND. TYPE SYS_HELP FOR COMMAND LIST.");
    }
}

// ============================================================
// SUB-PROMPT HANDLER  (limb select + damage level)
// ============================================================
static void handleSubPrompt(const std::string& input, GameState& state,
                            TerminalBuffer& buf, AnimationQueue& anim,
                            InputState& inputSt) {
    Player& player = state.player;

    if (inputSt.mode == InputMode::LimbSelect) {
        BodyPart part = player.stringToBodyPart(input);

        if (inputSt.pendingCmd == "SYS_HEAL") {
            if (part == BodyPart::None) {
                buf.push("INVALID LIMB.");
            } else {
                const Item* item = player.m_inventory.findItem(ItemEffect::healLimb);
                if (item) {
                    std::string name = item->name;
                    player.applyItem(*item, part);
                    player.m_inventory.removeItem(name);
                }
            }
            inputSt.mode = InputMode::Command;
            inputSt.pendingCmd.clear();

        } else if (inputSt.pendingCmd == "DAMAGE") {
            if (part == BodyPart::None) {
                buf.push("INVALID LIMB.");
                inputSt.mode = InputMode::Command;
                inputSt.pendingCmd.clear();
            } else {
                inputSt.pendingLimb = input;
                inputSt.mode = InputMode::DamageLevel;
            }
        }

    } else if (inputSt.mode == InputMode::DamageLevel) {
        int level = 0;
        try { level = std::stoi(input); } catch (...) {}

        if (level < 1 || level > 3) {
            buf.push("INVALID DAMAGE LEVEL.");
        } else {
            BodyPart part = player.stringToBodyPart(inputSt.pendingLimb);
            player.damagePlayer(part, level);
            buf.push(std::format("DAMAGE APPLIED TO {}.", inputSt.pendingLimb));
        }
        inputSt.mode = InputMode::Command;
        inputSt.pendingCmd.clear();
        inputSt.pendingLimb.clear();
    }
}

// ============================================================
// MAIN
// ============================================================
int main() {
    srand(static_cast<unsigned>(time(nullptr)));

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(DEFAULT_W, DEFAULT_H, "PROTOCOL OS");
    SetTargetFPS(60);

    // Load font — falls back to raylib default (size 20) if file missing
    Font font = LoadFontEx("assets/fonts/ShareTechMono-Regular.ttf",
                           FONT_SZ, nullptr, 0);
    if (font.texture.id == 0)
        font = GetFontDefault();

    GameState      gameState;
    AppState       appState   = AppState::MainMenu;
    TerminalBuffer tbuf;
    AnimationQueue anim;
    anim.buf = &tbuf;
    InputState     inputSt;
    float          decayTimer    = 0.f;
    int            lastWarnLevel = 3;  // 3=fine 2=≤200 1=≤100 0=≤40

    // Set global renderer context for playHealingAnimation
    g_tbuf = &tbuf;
    g_anim = &anim;

    pushMainMenu(tbuf);

    while (!WindowShouldClose()) {
        const float dt = GetFrameTime();
        const int   W  = GetScreenWidth();
        const int   H  = GetScreenHeight();

        // ── Mouse wheel scroll ─────────────────────────
        float wheel = GetMouseWheelMove();
        if (wheel > 0.f) tbuf.scrollUp(3);
        if (wheel < 0.f) tbuf.scrollDown(3);

        // ── Cursor blink ───────────────────────────────
        inputSt.cursorTimer += dt;
        if (inputSt.cursorTimer >= CURSOR_BLINK) {
            inputSt.cursorTimer = 0.f;
            inputSt.cursorVis   = !inputSt.cursorVis;
        }

        // ── Keyboard input (blocked during animations) ─
        if (!anim.isActive()) {
            int ch;
            while ((ch = GetCharPressed()) > 0)
                if (ch >= 32 && ch < 127)
                    inputSt.current += static_cast<char>(ch);

            if (IsKeyPressed(KEY_BACKSPACE) && !inputSt.current.empty())
                inputSt.current.pop_back();

            if (IsKeyPressed(KEY_UP) && !inputSt.history.empty()) {
                inputSt.historyIdx = std::min(inputSt.historyIdx + 1,
                                              (int)inputSt.history.size() - 1);
                inputSt.current = inputSt.history[inputSt.historyIdx];
            }
            if (IsKeyPressed(KEY_DOWN)) {
                if (inputSt.historyIdx > 0) {
                    --inputSt.historyIdx;
                    inputSt.current = inputSt.history[inputSt.historyIdx];
                } else {
                    inputSt.historyIdx = -1;
                    inputSt.current.clear();
                }
            }

            if (IsKeyPressed(KEY_ENTER) && !inputSt.current.empty()) {
                std::string submitted = inputSt.current;
                inputSt.current.clear();
                inputSt.historyIdx = -1;

                // Add to history
                inputSt.history.insert(inputSt.history.begin(), submitted);
                if ((int)inputSt.history.size() > MAX_HISTORY)
                    inputSt.history.pop_back();

                // Upper-case for command matching
                std::string upper = submitted;
                std::transform(upper.begin(), upper.end(), upper.begin(),
                               [](unsigned char c) { return std::toupper(c); });

                // Echo input in dim colour
                const char* echo_prompt =
                    inputSt.mode == InputMode::Command     ? "[PROTOCOL]> "        :
                    inputSt.mode == InputMode::LimbSelect  ? "TARGET LIMB: "       :
                                                             "DAMAGE LEVEL [1-3]: ";
                tbuf.push(std::string(echo_prompt) + upper, C_DIM);

                if (inputSt.mode == InputMode::Command)
                    handleCommand(upper, gameState, tbuf, anim, inputSt, appState);
                else
                    handleSubPrompt(upper, gameState, tbuf, anim, inputSt);
            }
        }

        // ── Advance animations ─────────────────────────
        anim.advance(dt);

        // ── Game ticks (only while Playing) ───────────
        if (appState == AppState::Playing) {
            // Stat decay (1 point per second)
            decayTimer += dt;
            if (decayTimer >= DECAY_TICK) {
                decayTimer = 0.f;
                if (gameState.player.m_energy    > 0.f)
                    gameState.player.m_energy    -= 1.f;
                if (gameState.player.m_hydration > 0.f)
                    gameState.player.m_hydration -= 1.f;
            }

            gameState.player.getTotalHealth();
            int hp = gameState.player.m_overallHealth;

            // Health warnings — only push when level changes to avoid spam
            int newWarnLevel =
                hp <= 40  ? 0 :
                hp <= 100 ? 1 :
                hp <= 200 ? 2 : 3;

            if (newWarnLevel < lastWarnLevel && !anim.isActive()) {
                lastWarnLevel = newWarnLevel;
                if (newWarnLevel == 0) {
                    anim.stutterText("CRITICAL_FAILURE: BIOMETRIC_RESERVES_EXHAUSTED...\n");
                    anim.stutterText("EMERGENCY_DUMP: PHYSICAL_CONTAINMENT_BREACHED.\n");
                } else if (newWarnLevel == 1) {
                    tbuf.push("[***] COGNITION RUNTIME ERROR: 0x8004210F [***]", C_RED);
                    tbuf.push("CRITICAL: SYNAPTIC LINK DESYNC DETECTED.",          C_RED);
                } else if (newWarnLevel == 2) {
                    tbuf.push("[!] WARNING: BIOMETRIC FEED UNSTABLE. SEEK RECONSTRUCTION.",
                              C_AMBER);
                }
            }

            // Death check
            if (appState != AppState::Dead && gameState.player.isDead()) {
                appState = AppState::Dead;
                for (int i = 0; i < 5; ++i) tbuf.push("");
                tbuf.push("                                [ CONNECTION LOST ]");
                tbuf.push("                                  SIGNAL NULL");
                for (int i = 0; i < 5; ++i) tbuf.push("");
            }
        }

        // ── Draw ──────────────────────────────────────
        BeginDrawing();
            ClearBackground(BLACK);
            drawTerminal(tbuf, anim, font, W, H);
            drawInputLine(inputSt, font, W, H);
            drawHUD(gameState.player, font, W, H);
        EndDrawing();
    }

    UnloadFont(font);
    CloseWindow();
    return 0;
}
