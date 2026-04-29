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
// BARREL DISTORTION SHADER
// ============================================================
static constexpr const char *BARREL_FRAG = R"(
#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform float barrelStrength;   // positive = barrel (CRT bulge)

out vec4 finalColor;

void main() {
    // Map UV to [-1, 1] centred space
    vec2 p = fragTexCoord * 2.0 - 1.0;

    // Barrel distortion: push pixels outward proportional to r^2
    float r2 = dot(p, p);
    vec2 distorted = p * (1.0 + barrelStrength * r2);

    // Map back to [0, 1] UV space
    vec2 uv = distorted * 0.5 + 0.5;

    // Outside the distorted frame → black (the curved screen border)
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        finalColor = vec4(0.0, 0.0, 0.0, 1.0);
    } else {
        finalColor = texture(texture0, uv) * fragColor;
    }
}
)";

// ============================================================
// CONSTANTS
// ============================================================
static constexpr int DEFAULT_W = 1024;
static constexpr int DEFAULT_H = 768;
static constexpr int TOPBAR_H = 26;
static constexpr int HUD_H = 60;
static constexpr int INPUT_H = 26;
static constexpr int SIDEBAR_W = 190;
static constexpr int FONT_SZ = 18;
static constexpr int LINE_H = FONT_SZ + 5;
static constexpr int MAX_LINES = 200;
static constexpr int MAX_HISTORY = 20;
static constexpr float CURSOR_BLINK = 0.5f;
static constexpr float DECAY_TICK = 5.0f;

// ── Amber CRT phosphor palette ────────────────────────────
static constexpr Color C_AMBER = {255, 176, 0, 255};      // #FFB000 primary
static constexpr Color C_AMBER_DIM = {160, 112, 0, 255};  // #A07000 dim
static constexpr Color C_AMBER_BRT = {255, 208, 96, 255}; // #FFD060 bright
static constexpr Color C_BG = {10, 8, 0, 255};           // #0A0800
static constexpr Color C_RED = {255, 68, 34, 255};      // #FF4422
static constexpr Color C_GREEN = {68, 255, 136, 255};   // #44FF88 heal
static constexpr Color C_CYAN = {68, 204, 255, 255};    // #44CCFF drink
static constexpr Color C_ORANGE = {255, 136, 0, 255};   // #FF8800 low hp
static constexpr Color C_YELLOW = {255, 204, 68, 255};  // #FFCC44 eat
static constexpr Color C_DIM_TRACK = {255, 176, 0, 40};
static constexpr Color C_DIM_LINE = {255, 176, 0, 100};

// ============================================================
// ENUMS
// ============================================================
enum class AppState { Boot, MainMenu, Playing, Dead };
enum class InputMode { Command, LimbSelect, DamageLevel };

// ============================================================
// TERMINAL BUFFER
// ============================================================
struct ColoredLine {
  std::string text;
  Color color = C_AMBER;
};

struct TerminalBuffer {
  std::deque<ColoredLine> lines;
  int scrollOffset = 0;

  void push(const std::string &text, Color color = C_AMBER) {
    lines.push_back({text, color});
    if ((int)lines.size() > MAX_LINES)
      lines.pop_front();
    scrollOffset = 0;
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
// ============================================================
struct AnimEvent {
  std::string fullText;
  int charsRevealed = 0;
  float timer = 0.f;
  float charDelay = 0.04f;
  bool isStutter = false;
  Color color = C_AMBER;
};

struct AnimationQueue {
  std::deque<AnimEvent> events;
  TerminalBuffer *buf = nullptr;

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

  void typeText(const std::string &t, Color c = C_AMBER) {
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
    return events.empty() ? C_AMBER : events.front().color;
  }

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
  int menuSelection = 0; // 0-2, active item in the main menu
  float cursorTimer = 0.f;
  bool cursorVis = true;
  InputMode mode = InputMode::Command;
  std::string pendingCmd;
  std::string pendingLimb;
};

// ============================================================
// SIDEBAR ACTION FLASH STATE
// ============================================================
struct ActionFlash {
  int slot = -1; // 0-3 for quick actions
  float timer = 0.f;
  static constexpr float DURATION = 0.25f;

  void trigger(int s) {
    slot = s;
    timer = DURATION;
  }
  void advance(float dt) {
    if (slot >= 0) {
      timer -= dt;
      if (timer <= 0.f)
        slot = -1;
    }
  }
  bool isActive(int s) const { return slot == s && timer > 0.f; }
};

// ============================================================
// SCREEN SHAKE
// ============================================================
struct ScreenShake {
  float duration = 0.f;
  float intensity = 0.f;

  void trigger(float dur, float mag) {
    // Allow re-triggering if the new shake is stronger or lasts longer
    if (dur > duration || mag > intensity) {
      duration = dur;
      intensity = mag;
    }
  }

  void advance(float dt) {
    if (duration > 0.f)
      duration -= dt;
  }

  bool isActive() const { return duration > 0.f; }

  // Returns a random pixel offset each frame while active.
  // Intensity decays linearly as the shake winds down.
  Vector2 currentOffset() const {
    if (!isActive())
      return {0.f, 0.f};
    float decay = duration / 0.5f; // normalized 0→1 over 0.5 s
    float mag = intensity * std::min(decay, 1.f);
    auto rnd = [](float range) -> float {
      return ((rand() % 2001) / 1000.f - 1.f) * range;
    };
    return {rnd(mag), rnd(mag)};
  }
};

// ============================================================
// BOOT SEQUENCE STATE
// ============================================================
struct BootState {
  struct Line {
    std::string text;
    Color color;
    float delay;
  };
  std::vector<Line> lines;
  std::vector<bool> visible;
  float elapsed = 0.f;
  bool done = false;

  void init() {
    std::string ver =
        std::string("PROTOCOL OS [") + PROJECT_VERSION + "]  BUILD 20260428";
    lines = {
        {ver, C_AMBER_BRT, 0.00f},
        {"Initializing memory banks...", C_AMBER_DIM, 0.18f},
        {"  > HEAP      [ OK ]", C_GREEN, 0.34f},
        {"  > STACK     [ OK ]", C_GREEN, 0.45f},
        {"  > SUBSYS    [ OK ]", C_GREEN, 0.56f},
        {"Loading asset manifests...", C_AMBER_DIM, 0.72f},
        {"  > ZONES     loaded", C_GREEN, 0.82f},
        {"  > ENTITIES  loaded", C_GREEN, 0.92f},
        {"  > AUDIO     loaded", C_GREEN, 1.02f},
        {"Checking save data...", C_AMBER_DIM, 1.18f},
        {"  > 1 save file found", C_GREEN, 1.28f},
        {"", C_AMBER_DIM, 1.40f},
        {"BIOMETRIC LINK ESTABLISHED.", C_AMBER_BRT, 1.52f},
        {"SYSTEM READY.", C_AMBER_BRT, 1.72f},
    };
    visible.assign(lines.size(), false);
  }

  void advance(float dt) {
    if (done)
      return;
    elapsed += dt;
    bool allVis = true;
    for (int i = 0; i < (int)lines.size(); ++i) {
      if (elapsed >= lines[i].delay)
        visible[i] = true;
      else
        allVis = false;
    }
    if (allVis && elapsed >= 2.2f)
      done = true;
  }
};

// ============================================================
// ENEMY AI
// ============================================================
struct Enemy {
  std::string name;
  std::string type;
  int   hp             = 0;
  int   maxHp          = 0;
  float attackInterval = 5.f;
  float attackTimer    = 5.f;
  int   attackPower    = 1;    // 1-3
  bool  active         = false;
  float playerCooldown = 0.f;  // seconds until player can attack again
  bool  lootReady      = false;
  int   hpThreshold    = 2;    // 2=full, 1=below 50%, 0=below 25%
  std::vector<std::string> lootItems;

  static BodyPart randomLimb() {
    static const BodyPart parts[6] = {
        BodyPart::Head,     BodyPart::Torso,   BodyPart::LeftArm,
        BodyPart::RightArm, BodyPart::LeftLeg, BodyPart::RightLeg,
    };
    return parts[rand() % 6];
  }

  static const char *limbName(BodyPart p) {
    switch (p) {
    case BodyPart::Head:
      return "HEAD";
    case BodyPart::Torso:
      return "TORSO";
    case BodyPart::LeftArm:
      return "L.ARM";
    case BodyPart::RightArm:
      return "R.ARM";
    case BodyPart::LeftLeg:
      return "L.LEG";
    case BodyPart::RightLeg:
      return "R.LEG";
    default:
      return "UNKNOWN";
    }
  }
};

// Returns a randomly-tiered enemy (50% DRONE, 33% SENTINEL, 17% CORRUPTOR).
static Enemy spawnEnemy() {
  static const struct {
    const char *name;
    const char *type;
    int hp;
    float interval;
    int power;
  } defs[3] = {
      {"MK-I PATROL DRONE", "DRONE", 30, 4.f, 1},
      {"SENTINEL-7", "SENTINEL", 60, 6.f, 2},
      {"OMEGA CORRUPTOR", "CORRUPTOR", 90, 9.f, 3},
  };
  int r = rand() % 6;
  int tier = r < 3 ? 0 : r < 5 ? 1 : 2;
  Enemy e;
  e.name = defs[tier].name;
  e.type = defs[tier].type;
  e.hp = e.maxHp = defs[tier].hp;
  e.attackInterval = defs[tier].interval;
  e.attackTimer = defs[tier].interval;
  e.attackPower    = defs[tier].power;
  e.active         = true;
  e.lootReady      = false;
  e.hpThreshold    = 2;
  e.playerCooldown = 0.f;
  e.lootItems.clear();
  if (tier == 0) {  // DRONE: one common item
    e.lootItems.push_back(rand() % 2 == 0 ? "Med-Kit" : "Adreno-Spike 0.5mg");
  } else if (tier == 1) {  // SENTINEL: guaranteed + 50% bonus
    e.lootItems.push_back("Med-Kit");
    if (rand() % 2 == 0) e.lootItems.push_back("Diazine (Grade B)");
  } else {  // CORRUPTOR: two items
    e.lootItems.push_back("Diazine (Grade B)");
    e.lootItems.push_back("Med-Kit");
  }
  return e;
}

// ============================================================
// GLOBAL RENDERER CONTEXT
// ============================================================
static TerminalBuffer *g_tbuf = nullptr;
static AnimationQueue *g_anim = nullptr;
static ScreenShake *g_shake = nullptr;
static Enemy *g_enemy = nullptr;
static float *g_encounterTimer = nullptr;

void playHealingAnimation(const std::string &statusMessage) {
  if (!g_tbuf || !g_anim)
    return;
  g_tbuf->push("[!] INITIATING BIO-RECONSTRUCTION...", C_AMBER);
  g_tbuf->push("PROGRESS: [====================] 100%", C_GREEN);
  g_anim->typeText("SYSTEM OS: " + statusMessage + "\n\n", C_GREEN);
}

// ============================================================
// FORWARD DECLARATIONS
// ============================================================
static void pushCommandMenu(TerminalBuffer &buf);
static void pushIntro(AnimationQueue &anim);
static void handleCommand(const std::string &input, GameState &state,
                          TerminalBuffer &buf, AnimationQueue &anim,
                          InputState &inputSt, AppState &appState);
static void handleSubPrompt(const std::string &input, GameState &state,
                            TerminalBuffer &buf, AnimationQueue &anim,
                            InputState &inputSt);
static void drawScanlines(int screenW, int screenH);
static void drawTopBar(Font font, int screenW, AppState appState);
static void drawSidebar(Font font, int screenW, int screenH,
                        const ActionFlash &flash, const Enemy &enemy);
static void drawTerminal(const TerminalBuffer &buf, const AnimationQueue &anim,
                         Font font, int screenW, int screenH);
static void drawInputLine(const InputState &st, Font font, int screenW,
                          int screenH);
static void drawHUD(const Player &player, Font font, int screenW, int screenH);
static void drawBoot(const BootState &boot, Font font, int screenW,
                     int screenH);
static void drawMainMenu(Font font, int screenW, int screenH,
                         const InputState &inputSt);

// ============================================================
// RENDERING HELPERS
// ============================================================
static Color pulseRed() {
  // Pulse between 60% and 100% opacity for critical indicators
  float t = sinf((float)GetTime() * 8.f) * 0.5f + 0.5f;
  unsigned char a = (unsigned char)(153 + (int)(t * 102.f));
  return {C_RED.r, C_RED.g, C_RED.b, a};
}

// ============================================================
// CRT SCANLINE OVERLAY + VIGNETTE
// ============================================================
static void drawScanlines(int screenW, int screenH) {
  // Static dark bands — fixed grid, never moves
  for (int y = 0; y < screenH; y += 4)
    DrawRectangle(0, y, screenW, 1, {0, 0, 0, 28});

  // Phosphor refresh sweep: a single bright line that drifts top→bottom,
  // looping every ~3 seconds. This is the electron beam scan effect.
  float t = (float)fmod(GetTime() / 3.0, 1.0);
  int beamY = (int)(t * screenH);

  // Bright edge at the beam position
  DrawRectangle(0, beamY, screenW, 2, {255, 200, 80, 18});

  // Decaying amber trail above the beam (phosphor persistence glow)
  static constexpr int TRAIL = 80;
  for (int i = 1; i <= TRAIL; ++i) {
    int ty = beamY - i;
    if (ty < 0)
      break;
    unsigned char a = (unsigned char)(12.f * (1.f - (float)i / TRAIL));
    DrawRectangle(0, ty, screenW, 1, {255, 176, 0, a});
  }
}

// ============================================================
// RENDERING — TOP STATUS BAR
// ============================================================
static void drawTopBar(Font font, int screenW, AppState appState) {
  DrawRectangle(0, 0, screenW, TOPBAR_H, {8, 6, 0, 255});
  DrawLine(0, TOPBAR_H - 1, screenW, TOPBAR_H - 1, C_DIM_LINE);

  // Left: title
  DrawTextEx(font, "PROTOCOL OS", {8.f, 5.f}, FONT_SZ, 1, C_AMBER_BRT);

  // Centre: state info
  const char *centre =
      (appState == AppState::Playing || appState == AppState::Dead)
          ? "SUBLEVEL 2 \x97 MAINTENANCE CORRIDOR"
          : "AUTONOMOUS SURVIVAL INTERFACE";
  Vector2 csz = MeasureTextEx(font, centre, FONT_SZ - 2, 1);
  DrawTextEx(font, centre, {(screenW - csz.x) * 0.5f, 7.f}, FONT_SZ - 2, 1,
             C_AMBER_DIM);

  // Right: session time
  double t = GetTime();
  int hh = (int)(t / 3600);
  int mm = (int)(t / 60) % 60;
  int ss = (int)t % 60;
  std::string timeStr = std::format("{:02d}:{:02d}:{:02d}", hh, mm, ss);
  Vector2 tsz = MeasureTextEx(font, timeStr.c_str(), FONT_SZ - 2, 1);
  DrawTextEx(font, timeStr.c_str(), {screenW - tsz.x - 10.f, 7.f}, FONT_SZ - 2,
             1, C_AMBER_DIM);
}

// ============================================================
// RENDERING — SIDEBAR
// ============================================================
static const char *SIDEBAR_ACTIONS[4] = {
    "[1]  SYS_HEAL",
    "[2]  INT_CHARGE",
    "[3]  INT_COOLANT",
    "[4]  SYS_DECON",
};
static const Color SIDEBAR_ACTION_COLORS[4] = {
    C_GREEN,
    C_YELLOW,
    C_CYAN,
    C_AMBER,
};
static const char *MAP_ROWS[] = {
    "+---+------+", "|.  |  .  .|", "|   @ .  ..|", "|.  |  . ..|",
    "+---+  . --+", "    |  .   |", "    +------+",
};
static constexpr int MAP_FONT = FONT_SZ - 4;

static void drawSidebar(Font font, int screenW, int screenH,
                        const ActionFlash &flash, const Enemy &enemy) {
  int x = screenW - SIDEBAR_W;
  int yTop = TOPBAR_H;
  int yBot = screenH - HUD_H;

  // Background + left border
  DrawRectangle(x, yTop, SIDEBAR_W, yBot - yTop, {8, 6, 0, 220});
  DrawLine(x, yTop, x, yBot, C_DIM_LINE);

  int cy = yTop + 10;

  // ── QUICK ACTIONS ────────────────────────────────────────
  DrawTextEx(font, "QUICK ACTIONS", {(float)(x + 8), (float)cy}, FONT_SZ - 4, 1,
             C_AMBER_DIM);
  cy += LINE_H - 2;
  DrawLine(x + 4, cy, screenW - 4, cy, {255, 176, 0, 25});
  cy += 6;

  for (int i = 0; i < 4; ++i) {
    bool lit = flash.isActive(i);
    Color col = lit ? SIDEBAR_ACTION_COLORS[i] : C_AMBER_DIM;
    if (lit) {
      DrawRectangle(x + 2, cy - 2, SIDEBAR_W - 4, LINE_H,
                    {col.r, col.g, col.b, 35});
    }
    DrawTextEx(font, SIDEBAR_ACTIONS[i], {(float)(x + 10), (float)cy},
               FONT_SZ - 2, 1, col);
    cy += LINE_H;
  }

  cy += 6;
  DrawLine(x + 4, cy, screenW - 4, cy, {255, 176, 0, 25});
  cy += 10;

  // ── ACTIVE THREAT / SALVAGE ───────────────────────────────
  if (enemy.active || enemy.lootReady) {
    DrawLine(x + 4, cy, screenW - 4, cy, {255, 68, 34, 35});
    cy += 10;

    if (enemy.active) {
      float pulse = sinf((float)GetTime() * 6.f) * 0.5f + 0.5f;
      unsigned char pa = (unsigned char)(100 + (int)(pulse * 155.f));
      Color alertCol = {C_RED.r, C_RED.g, C_RED.b, pa};
      DrawTextEx(font, "ACTIVE THREAT", {(float)(x + 8), (float)cy},
                 FONT_SZ - 4, 1, alertCol);
      cy += LINE_H - 2;

      DrawTextEx(font, enemy.name.c_str(), {(float)(x + 10), (float)cy},
                 FONT_SZ - 3, 1, C_RED);
      cy += LINE_H;

      // HP bar
      float frac  = (float)enemy.hp / (float)enemy.maxHp;
      Color hpCol = frac > 0.5f ? C_RED : frac > 0.25f ? C_ORANGE : C_AMBER_BRT;
      int hpBarW  = SIDEBAR_W - 22;
      DrawRectangle(x + 10, cy, hpBarW, 5, {hpCol.r, hpCol.g, hpCol.b, 30});
      int hpFill = (int)(hpBarW * frac);
      if (hpFill > 0) DrawRectangle(x + 10, cy, hpFill, 5, hpCol);
      cy += 11;

      // Cooldown / action hint
      if (enemy.playerCooldown > 0.f) {
        char cdBuf[24];
        snprintf(cdBuf, sizeof(cdBuf), "RECHARGE %.1fs", enemy.playerCooldown);
        DrawTextEx(font, cdBuf, {(float)(x + 10), (float)cy},
                   FONT_SZ - 4, 1, C_AMBER_DIM);
      } else {
        DrawTextEx(font, "ATTACK / SCAN / FLEE", {(float)(x + 10), (float)cy},
                   FONT_SZ - 4, 1, C_AMBER_DIM);
      }
      cy += LINE_H;
    } else {
      // Loot ready
      float pulse = sinf((float)GetTime() * 4.f) * 0.5f + 0.5f;
      unsigned char pa = (unsigned char)(80 + (int)(pulse * 120.f));
      Color lootCol = {C_GREEN.r, C_GREEN.g, C_GREEN.b, pa};
      DrawTextEx(font, "SALVAGE READY", {(float)(x + 8), (float)cy},
                 FONT_SZ - 4, 1, lootCol);
      cy += LINE_H - 2;
      DrawTextEx(font, enemy.name.c_str(), {(float)(x + 10), (float)cy},
                 FONT_SZ - 3, 1, C_AMBER_DIM);
      cy += LINE_H;
      DrawTextEx(font, "TYPE: LOOT", {(float)(x + 10), (float)cy},
                 FONT_SZ - 4, 1, C_GREEN);
      cy += LINE_H;
    }

    DrawLine(x + 4, cy, screenW - 4, cy, {255, 176, 0, 25});
    cy += 10;
  }

  // ── SECTOR MAP ───────────────────────────────────────────
  DrawTextEx(font, "SECTOR MAP", {(float)(x + 8), (float)cy}, FONT_SZ - 4, 1,
             C_AMBER_DIM);
  cy += LINE_H - 2;

  for (const char *row : MAP_ROWS) {
    DrawTextEx(font, row, {(float)(x + 10), (float)cy}, MAP_FONT, 1,
               {160, 112, 0, 170});
    cy += MAP_FONT + 3;
  }
}

// ============================================================
// RENDERING — TERMINAL AREA
// ============================================================
static void drawTerminal(const TerminalBuffer &buf, const AnimationQueue &anim,
                         Font font, int screenW, int screenH) {
  int termW = screenW - SIDEBAR_W;
  int termH = screenH - TOPBAR_H - HUD_H - INPUT_H;
  int maxVisible = termH / LINE_H;
  int totalLines = (int)buf.lines.size() + (anim.isActive() ? 1 : 0);
  int startIdx =
      std::max(0, (int)buf.lines.size() - maxVisible - buf.scrollOffset);

  // Scroll indicator (right edge of terminal, not sidebar)
  if (buf.scrollOffset > 0 && totalLines > maxVisible) {
    int indicH = std::max(20, termH * maxVisible / totalLines);
    int indicY = termH - indicH -
                 (buf.scrollOffset * (termH - indicH) /
                  std::max(totalLines - maxVisible, 1));
    indicY = std::max(0, indicY) + TOPBAR_H;
    DrawRectangle(termW - 4, TOPBAR_H, 4, termH, C_DIM_TRACK);
    DrawRectangle(termW - 4, indicY, 4, indicH, C_AMBER);
  }

  int y = TOPBAR_H + 6;
  for (int i = startIdx;
       i < (int)buf.lines.size() && y + LINE_H <= TOPBAR_H + termH; ++i) {
    DrawTextEx(font, buf.lines[i].text.c_str(), {6.f, (float)y}, FONT_SZ, 1,
               buf.lines[i].color);
    y += LINE_H;
  }

  if (anim.isActive() && buf.scrollOffset == 0 &&
      y + LINE_H <= TOPBAR_H + termH) {
    DrawTextEx(font, anim.currentPartial().c_str(), {6.f, (float)y}, FONT_SZ, 1,
               anim.currentColor());
  }
}

// ============================================================
// RENDERING — INPUT LINE
// ============================================================
static void drawInputLine(const InputState &st, Font font, int screenW,
                          int screenH) {
  int y = screenH - HUD_H - INPUT_H;
  DrawLine(0, y, screenW - SIDEBAR_W, y, C_DIM_LINE);

  const char *promptStr = st.mode == InputMode::Command ? "PROTOCOL:~$ "
                          : st.mode == InputMode::LimbSelect
                              ? "TARGET LIMB: "
                              : "DAMAGE LEVEL [1-3]: ";

  std::string display = promptStr + st.current;
  if (st.cursorVis)
    display += '_';
  DrawTextEx(font, display.c_str(), {6.f, (float)(y + 4)}, FONT_SZ, 1,
             C_AMBER_DIM);
}

// ============================================================
// RENDERING — HUD BAR
// ============================================================
static void drawHUD(const Player &player, Font font, int screenW, int screenH) {
  int y = screenH - HUD_H;
  DrawRectangle(0, y, screenW, HUD_H, C_BG);
  DrawLine(0, y, screenW, y, C_AMBER);

  const int BAR_H = 8;
  const int ROW_H = 19;
  const int PAD = 5;
  const int LBLW = 52;

  int leftW = (int)(screenW * 0.72f);

  auto barColor = [](float frac) -> Color {
    if (frac > 0.7f)
      return C_GREEN;
    if (frac > 0.4f)
      return C_AMBER;
    if (frac > 0.2f)
      return C_ORANGE;
    return pulseRed();
  };

  // ── Row 1: HEAD TORSO L.ARM R.ARM ────────────────────────
  struct LB {
    const char *lbl;
    float val;
    float maxV;
  };
  LB row1[4] = {
      {"HEAD", (float)player.m_head, 40.f},
      {"TORSO", (float)player.m_torso, 60.f},
      {"L.ARM", (float)player.m_leftArm, 75.f},
      {"R.ARM", (float)player.m_rightArm, 75.f},
  };
  int r1y = y + PAD;
  int cellW = leftW / 4;
  int barW = cellW - LBLW - PAD;
  for (int i = 0; i < 4; ++i) {
    int cx = i * cellW + PAD;
    float frac = std::clamp(row1[i].val / row1[i].maxV, 0.f, 1.f);
    Color col = barColor(frac);
    DrawTextEx(font, row1[i].lbl, {(float)cx, (float)r1y}, FONT_SZ - 2, 1,
               C_AMBER_DIM);
    DrawRectangle(cx + LBLW, r1y + 2, barW, BAR_H, {col.r, col.g, col.b, 28});
    int fw = (int)(barW * frac);
    if (fw > 0) {
      DrawRectangle(cx + LBLW, r1y + 2, fw, BAR_H, col);
      DrawRectangle(cx + LBLW, r1y + 2, fw, 1, {255, 255, 255, 50});
    }
  }

  // ── Row 2: L.LEG R.LEG OVERALL ───────────────────────────
  LB row2[3] = {
      {"L.LEG", (float)player.m_leftLeg, 75.f},
      {"R.LEG", (float)player.m_rightLeg, 75.f},
      {"OVERALL", (float)player.m_overallHealth, 400.f},
  };
  int r2y = r1y + ROW_H;
  int cellW2 = leftW / 3;
  int barW2 = cellW2 - LBLW - PAD;
  for (int i = 0; i < 3; ++i) {
    int cx = i * cellW2 + PAD;
    float frac = std::clamp(row2[i].val / row2[i].maxV, 0.f, 1.f);
    Color col = barColor(frac);
    DrawTextEx(font, row2[i].lbl, {(float)cx, (float)r2y}, FONT_SZ - 2, 1,
               C_AMBER_DIM);
    DrawRectangle(cx + LBLW, r2y + 2, barW2, BAR_H, {col.r, col.g, col.b, 28});
    int fw = (int)(barW2 * frac);
    if (fw > 0) {
      DrawRectangle(cx + LBLW, r2y + 2, fw, BAR_H, col);
      DrawRectangle(cx + LBLW, r2y + 2, fw, 1, {255, 255, 255, 50});
    }
  }

  // ── Right panel: NRG / HYD / RAD ─────────────────────────
  int rightX = leftW + 10;
  int statW = screenW - rightX - PAD;
  DrawLine(leftW + 4, y + 4, leftW + 4, y + HUD_H - 4,
           {C_AMBER.r, C_AMBER.g, C_AMBER.b, 60});

  struct SB {
    const char *lbl;
    float val;
    float maxV;
    bool inv;
  };
  SB stats[3] = {
      {"NRG", player.m_energy, 100.f, false},
      {"HYD", player.m_hydration, 100.f, false},
      {"RAD", (float)player.m_radiation, 100.f, true},
  };
  const int SLBLW = 28;
  int statRowH = (HUD_H - PAD * 2) / 3;
  for (int i = 0; i < 3; ++i) {
    int sy = y + PAD + i * statRowH;
    float frac = std::clamp(stats[i].val / stats[i].maxV, 0.f, 1.f);
    Color col = stats[i].inv ? (frac > 0.6f   ? C_RED
                                : frac > 0.3f ? C_ORANGE
                                              : C_GREEN)
                             : barColor(frac);
    DrawTextEx(font, stats[i].lbl, {(float)rightX, (float)sy}, FONT_SZ - 2, 1,
               C_AMBER_DIM);
    int bw = statW - SLBLW - 2;
    int bx = rightX + SLBLW + 2;
    DrawRectangle(bx, sy + 2, bw, BAR_H, {col.r, col.g, col.b, 28});
    int fw = (int)(bw * frac);
    if (fw > 0) {
      DrawRectangle(bx, sy + 2, fw, BAR_H, col);
      DrawRectangle(bx, sy + 2, fw, 1, {255, 255, 255, 50});
    }
  }
}

// ============================================================
// RENDERING — BOOT SCREEN
// ============================================================
static void drawBoot(const BootState &boot, Font font, int screenW,
                     int screenH) {
  int x = 40;
  int y = 60;
  for (int i = 0; i < (int)boot.lines.size(); ++i) {
    if (!boot.visible[i])
      break;
    DrawTextEx(font, boot.lines[i].text.c_str(), {(float)x, (float)y}, FONT_SZ,
               1, boot.lines[i].color);
    y += LINE_H + 2;
  }
}

// ============================================================
// TERMINAL CONTENT HELPERS
// ============================================================
static void pushCommandMenu(TerminalBuffer &buf) {
  buf.push("-- COMMANDS ----------------------------------------", C_AMBER_DIM);
  buf.push("  SYS_HEAL    » Initiate bio-reconstruction");
  buf.push("  SYS_DECON   » Suppress radiation");
  buf.push("  INT_CHARGE  » Restore energy reserves");
  buf.push("  INT_COOLANT » Restore hydration");
  buf.push("  DAMAGE      » Apply damage to limb");
  buf.push("  ATTACK      » Strike active threat");
  buf.push("  SCAN        » Analyze active threat");
  buf.push("  FLEE        » Attempt tactical retreat");
  buf.push("  LOOT        » Salvage components from neutralized threat");
  buf.push("  CURRENT     » Display biometric feed");
  buf.push("  SHW_STATS   » Display survival metrics");
  buf.push("  SHW_INV     » Display inventory manifest");
  buf.push("  SYS_SAVE    » Save current session");
  buf.push("  SYS_LOAD    » Restore saved session");
  buf.push("  SYS_HELP    » Reprint this menu");
  buf.push("  EXIT        » Terminate session");
  buf.push("---------------------------------------------------", C_AMBER_DIM);
  buf.push("");
}

static void pushIntro(AnimationQueue &anim) {
  anim.typeText(std::string("PROTOCOL OS [") + PROJECT_VERSION +
                    "] - BIOMETRIC LINK ACTIVE\n",
                C_AMBER_BRT);
  anim.typeText("Limb status: NOMINAL.\n");
  anim.typeText("Inventory loaded. Ready for input.\n\n");
}

// ============================================================
// COMMAND DISPATCHER
// ============================================================
static void handleCommand(const std::string &input, GameState &state,
                          TerminalBuffer &buf, AnimationQueue &anim,
                          InputState &inputSt, AppState &appState) {
  Player &player = state.player;

  if (appState == AppState::Dead) {
    if (input == "RESTART") {
      state.player = Player();
      buf.lines.clear();
      buf.scrollOffset = 0;
      anim.events.clear();
      inputSt.mode = InputMode::Command;
      inputSt.pendingCmd.clear();
      inputSt.pendingLimb.clear();
      if (g_enemy) *g_enemy = Enemy{};
      if (g_encounterTimer) *g_encounterTimer = 45.f;
      appState = AppState::MainMenu;
    } else {
      buf.push("SESSION TERMINATED. TYPE RESTART TO RETURN TO MAIN MENU.",
               C_AMBER_DIM);
    }
    return;
  }

  if (appState == AppState::MainMenu) {
    if (input == "1") {
      state.player = Player();
      for (const char *name :
           {"Med-Kit", "Diazine (Grade B)", "Adreno-Spike 0.5mg",
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
        anim.typeText("SAVE RESTORED. RESUMING SESSION...\n\n", C_AMBER_BRT);
        pushCommandMenu(buf);
      } else {
        buf.push("NO SAVE FILE FOUND.", C_RED);
      }
    } else if (input == "3") {
      CloseWindow();
    } else {
      buf.push("ENTER 1, 2, OR 3.", C_AMBER_DIM);
    }
    return;
  }

  if (input == "SYS_HEAL") {
    if (!player.m_inventory.findItem(ItemEffect::healLimb)) {
      buf.push("NO HEALING ITEMS IN INVENTORY.", C_RED);
      return;
    }
    inputSt.mode = InputMode::LimbSelect;
    inputSt.pendingCmd = "SYS_HEAL";
    buf.push("  TARGETS: HEAD  TORSO  L.ARM  R.ARM  L.LEG  R.LEG", C_AMBER_DIM);

  } else if (input == "SYS_DECON") {
    const Item *item = player.m_inventory.findItem(ItemEffect::reduceRadiation);
    if (!item) {
      buf.push("NO DECONTAMINATION ITEMS.", C_RED);
      return;
    }
    std::string msg = item->useMessage;
    std::string name = item->name;
    player.applyItem(*item);
    player.m_inventory.removeItem(name);
    anim.typeText(msg + "\n", C_GREEN);

  } else if (input == "INT_CHARGE") {
    const Item *item = player.m_inventory.findItem(ItemEffect::restoreEnergy);
    if (!item) {
      buf.push("NO ENERGY ITEMS.", C_RED);
      return;
    }
    std::string msg = item->useMessage;
    std::string name = item->name;
    player.applyItem(*item);
    player.m_inventory.removeItem(name);
    anim.typeText(msg + "\n", C_YELLOW);

  } else if (input == "INT_COOLANT") {
    const Item *item =
        player.m_inventory.findItem(ItemEffect::restoreHydration);
    if (!item) {
      buf.push("NO HYDRATION ITEMS.", C_RED);
      return;
    }
    std::string msg = item->useMessage;
    std::string name = item->name;
    player.applyItem(*item);
    player.m_inventory.removeItem(name);
    anim.typeText(msg + "\n", C_CYAN);

  } else if (input == "SHW_INV") {
    if (player.m_inventory.isEmpty()) {
      buf.push("INVENTORY: EMPTY.", C_AMBER_DIM);
    } else {
      buf.push("-- INVENTORY MANIFEST ------------------------------",
               C_AMBER_DIM);
      for (int i = 0; i < player.m_inventory.size(); ++i) {
        Item it = player.m_inventory.getItem(i);
        int count = player.m_inventory.getCount(i);
        buf.push(std::format("  [{}] {} x{} - {}", i + 1, it.name, count,
                             it.description));
      }
    }

  } else if (input == "DAMAGE") {
    inputSt.mode = InputMode::LimbSelect;
    inputSt.pendingCmd = "DAMAGE";
    buf.push("  TARGETS: HEAD  TORSO  L.ARM  R.ARM  L.LEG  R.LEG", C_AMBER_DIM);

  } else if (input == "CURRENT") {
    auto lc = [](int v, int mx) -> Color {
      float f = (float)v / mx;
      return f > 0.7f   ? C_GREEN
             : f > 0.4f ? C_AMBER
             : f > 0.2f ? C_ORANGE
                        : C_RED;
    };
    buf.push("-- BIOMETRIC FEED ----------------------------------",
             C_AMBER_DIM);
    buf.push(std::format("  OVERALL  {:>5} / 400", player.getTotalHealth()));
    buf.push(std::format("  HEAD     {:>5} / 40", player.m_head),
             lc(player.m_head, 40));
    buf.push(std::format("  TORSO    {:>5} / 60", player.m_torso),
             lc(player.m_torso, 60));
    buf.push(std::format("  L.ARM    {:>5} / 75", player.m_leftArm),
             lc(player.m_leftArm, 75));
    buf.push(std::format("  R.ARM    {:>5} / 75", player.m_rightArm),
             lc(player.m_rightArm, 75));
    buf.push(std::format("  L.LEG    {:>5} / 75", player.m_leftLeg),
             lc(player.m_leftLeg, 75));
    buf.push(std::format("  R.LEG    {:>5} / 75", player.m_rightLeg),
             lc(player.m_rightLeg, 75));

  } else if (input == "SHW_STATS") {
    buf.push("-- SURVIVAL METRICS --------------------------------",
             C_AMBER_DIM);
    buf.push(std::format("  Radiation  : {}", player.m_radiation),
             player.m_radiation > 60   ? C_RED
             : player.m_radiation > 30 ? C_ORANGE
                                       : C_GREEN);
    buf.push(std::format("  Energy     : {:.1f}", player.m_energy),
             player.m_energy < 30.f   ? C_RED
             : player.m_energy < 60.f ? C_AMBER
                                      : C_GREEN);
    buf.push(std::format("  Hydration  : {:.1f}", player.m_hydration),
             player.m_hydration < 30.f   ? C_RED
             : player.m_hydration < 60.f ? C_AMBER
                                         : C_CYAN);

  } else if (input == "SYS_SAVE") {
    state.save("save.txt");
    anim.typeText("SAVE COMPLETE. SESSION PRESERVED.\n", C_AMBER_BRT);

  } else if (input == "SYS_LOAD") {
    if (state.load("save.txt"))
      anim.typeText("SAVE RESTORED.\n", C_AMBER_BRT);
    else
      buf.push("NO SAVE FILE FOUND.", C_RED);

  } else if (input == "SYS_HELP") {
    pushCommandMenu(buf);

  } else if (input == "KILL") {
    player.m_head = 0;

  } else if (input == "ATTACK") {
    if (!g_enemy || !g_enemy->active) {
      buf.push("NO HOSTILE TARGET ACQUIRED.", C_AMBER_DIM);
      return;
    }
    if (g_enemy->playerCooldown > 0.f) {
      buf.push(std::format("ATTACK SYSTEM RECHARGING — {:.1f}s REMAINING.",
                           g_enemy->playerCooldown), C_AMBER_DIM);
      return;
    }
    bool crit = (rand() % 7 == 0);        // ~14% crit chance
    int  dmg  = rand() % 15 + 8;          // 8-22 base
    if (crit) dmg *= 2;
    g_enemy->hp           = std::max(0, g_enemy->hp - dmg);
    g_enemy->playerCooldown = 1.5f;

    // Pick hit message by enemy type
    const char *hitMsg;
    if (crit) {
      hitMsg = "CRITICAL OVERRIDE — DIRECT SYSTEM BREACH";
    } else if (g_enemy->type == "DRONE") {
      static const char *msgs[2] = {"KINETIC IMPACT REGISTERED",
                                    "COMBAT STRIKE CONNECTED"};
      hitMsg = msgs[rand() % 2];
    } else if (g_enemy->type == "SENTINEL") {
      static const char *msgs[2] = {"ARMOR LAYER PENETRATED",
                                    "SUBSYSTEM OVERLOADED"};
      hitMsg = msgs[rand() % 2];
    } else {
      static const char *msgs[2] = {"CONTAINMENT BREACHED",
                                    "RADIATION CORE HIT"};
      hitMsg = msgs[rand() % 2];
    }
    buf.push(std::format("[COMBAT] {} — -{} DMG  |  {} INTEGRITY: {}/{}",
                         hitMsg, dmg, g_enemy->name, g_enemy->hp, g_enemy->maxHp),
             crit ? C_AMBER_BRT : C_CYAN);

    // HP threshold status taunts
    float hpFrac = (float)g_enemy->hp / (float)g_enemy->maxHp;
    if (g_enemy->hpThreshold == 2 && hpFrac < 0.5f) {
      g_enemy->hpThreshold = 1;
      if      (g_enemy->type == "DRONE")
        buf.push("  >> MK-I PATROL DRONE SUSTAINING CRITICAL DAMAGE — SELF-REPAIR OFFLINE.", C_ORANGE);
      else if (g_enemy->type == "SENTINEL")
        buf.push("  >> SENTINEL-7 ARMOR FAILING — EMERGENCY PROTOCOLS ENGAGED.", C_ORANGE);
      else
        buf.push("  >> OMEGA CORRUPTOR LEAKING RADIATION — CONTAINMENT CRITICAL.", C_ORANGE);
    } else if (g_enemy->hpThreshold == 1 && hpFrac < 0.25f) {
      g_enemy->hpThreshold = 0;
      if      (g_enemy->type == "DRONE")
        buf.push("  >> MK-I PATROL DRONE CRITICAL — LOCOMOTION COMPROMISED.", C_RED);
      else if (g_enemy->type == "SENTINEL")
        buf.push("  >> SENTINEL-7 CRITICAL — CORE EXPOSED.", C_RED);
      else
        buf.push("  >> OMEGA CORRUPTOR CRITICAL — DETONATION IMMINENT.", C_RED);
    }

    if (g_enemy->hp <= 0) {
      g_enemy->active    = false;
      g_enemy->lootReady = true;
      if (g_shake) g_shake->trigger(0.35f, 6.f);
      if (g_encounterTimer) *g_encounterTimer = 30.f + (float)(rand() % 31);
      anim.typeText(std::format("ENTITY {} NEUTRALIZED. THREAT SUPPRESSED.\n",
                                g_enemy->name), C_GREEN);
      if (!g_enemy->lootItems.empty())
        buf.push("SALVAGE AVAILABLE — TYPE LOOT TO RETRIEVE COMPONENTS.", C_AMBER);
    }

  } else if (input == "SCAN") {
    if (!g_enemy || !g_enemy->active) {
      buf.push("NO HOSTILE TARGET IN RANGE.", C_AMBER_DIM);
      return;
    }
    float frac = (float)g_enemy->hp / (float)g_enemy->maxHp;
    Color hc = frac > 0.6f ? C_RED : frac > 0.3f ? C_ORANGE : C_GREEN;
    const char *threat = g_enemy->attackPower == 1   ? "LOW"
                         : g_enemy->attackPower == 2 ? "MEDIUM"
                                                     : "HIGH";
    Color tc = g_enemy->attackPower == 1   ? C_GREEN
               : g_enemy->attackPower == 2 ? C_AMBER
                                           : C_RED;
    buf.push("-- THREAT ANALYSIS ---------------------------------",
             C_AMBER_DIM);
    buf.push(std::format("  ENTITY   : {}", g_enemy->name));
    buf.push(std::format("  TYPE     : {}", g_enemy->type));
    buf.push(
        std::format("  INTEGRITY: {:>3} / {:>3}", g_enemy->hp, g_enemy->maxHp),
        hc);
    buf.push(std::format("  THREAT   : {}", threat), tc);

  } else if (input == "FLEE") {
    if (!g_enemy || !g_enemy->active) {
      buf.push("NO ACTIVE ENGAGEMENT TO FLEE FROM.", C_AMBER_DIM);
      return;
    }
    if (rand() % 2 == 0) {
      g_enemy->active = false;
      if (g_encounterTimer)
        *g_encounterTimer = 20.f + (float)(rand() % 21);
      anim.typeText("TACTICAL WITHDRAWAL SUCCESSFUL. THREAT EVADED.\n",
                    C_AMBER);
    } else {
      BodyPart target = Enemy::randomLimb();
      player.damagePlayer(target, g_enemy->attackPower);
      player.getTotalHealth();
      if (g_shake)
        g_shake->trigger(0.3f * g_enemy->attackPower,
                         4.f * g_enemy->attackPower);
      buf.push(std::format("[THREAT] EVASION FAILED — {} STRIKES YOUR {}!",
                           g_enemy->name, Enemy::limbName(target)),
               C_RED);
    }

  } else if (input == "LOOT") {
    if (!g_enemy || !g_enemy->lootReady || g_enemy->lootItems.empty()) {
      buf.push("NO SALVAGEABLE COMPONENTS IN RANGE.", C_AMBER_DIM);
      return;
    }
    buf.push("-- SALVAGE MANIFEST --------------------------------", C_AMBER_DIM);
    for (const auto &itemName : g_enemy->lootItems) {
      if (auto def = findItemDef(itemName)) {
        player.m_inventory.addItem(*def);
        buf.push(std::format("  [+] {} TRANSFERRED TO INVENTORY.", itemName), C_GREEN);
      }
    }
    g_enemy->lootItems.clear();
    g_enemy->lootReady = false;
    buf.push("SALVAGE COMPLETE.", C_AMBER_DIM);

  } else if (input == "EXIT") {
    buf.push("TERMINATING SESSION...", C_AMBER_DIM);
    CloseWindow();

  } else {
    buf.push("UNRECOGNIZED COMMAND. TYPE SYS_HELP FOR COMMAND LIST.",
             C_AMBER_DIM);
  }
}

// ============================================================
// SUB-PROMPT HANDLER
// ============================================================
static void handleSubPrompt(const std::string &input, GameState &state,
                            TerminalBuffer &buf, AnimationQueue &anim,
                            InputState &inputSt) {
  Player &player = state.player;

  if (inputSt.mode == InputMode::LimbSelect) {
    BodyPart part = player.stringToBodyPart(input);

    if (inputSt.pendingCmd == "SYS_HEAL") {
      if (part == BodyPart::None) {
        buf.push("INVALID LIMB.", C_RED);
      } else {
        const Item *item = player.m_inventory.findItem(ItemEffect::healLimb);
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
        buf.push("INVALID LIMB.", C_RED);
        inputSt.mode = InputMode::Command;
        inputSt.pendingCmd.clear();
      } else {
        inputSt.pendingLimb = input;
        inputSt.mode = InputMode::DamageLevel;
      }
    }

  } else if (inputSt.mode == InputMode::DamageLevel) {
    int level = 0;
    try {
      level = std::stoi(input);
    } catch (...) {
    }

    if (level < 1 || level > 3) {
      buf.push("INVALID DAMAGE LEVEL.", C_RED);
    } else {
      BodyPart part = player.stringToBodyPart(inputSt.pendingLimb);
      player.damagePlayer(part, level);
      buf.push(std::format("DAMAGE APPLIED TO {}.", inputSt.pendingLimb),
               C_ORANGE);
      if (g_shake)
        g_shake->trigger(0.15f * level, 3.f * level);
    }
    inputSt.mode = InputMode::Command;
    inputSt.pendingCmd.clear();
    inputSt.pendingLimb.clear();
  }
}

// ============================================================
// RENDERING — MAIN MENU OVERLAY
// ============================================================
static void drawMainMenu(Font font, int screenW, int screenH,
                         const InputState &inputSt) {
  const int panelW = 540;
  const int panelH = 300;
  int px = (screenW - panelW) / 2;
  int py = (screenH - panelH) / 2;

  // Glow halo + panel
  DrawRectangle(px - 3, py - 3, panelW + 6, panelH + 6,
                {C_AMBER.r, C_AMBER.g, C_AMBER.b, 16});
  DrawRectangle(px, py, panelW, panelH, {10, 8, 0, 252});
  DrawRectangleLines(px, py, panelW, panelH, C_AMBER);
  DrawRectangleLines(px + 2, py + 2, panelW - 4, panelH - 4,
                     {C_AMBER.r, C_AMBER.g, C_AMBER.b, 40});

  int cy = py + 16;

  const char *logo3[3] = {
      "|\\  /\\ |¯\\ /¯\\ ¯|¯ /¯\\ /¯  /¯\\ |",
      "|_\\/ | |_/ | |  |  | | |   | | |",
      "|    | |   \\_/  |  \\_/ \\__ \\_/ |__",
  };
  for (int i = 0; i < 3; ++i) {
    Vector2 sz = MeasureTextEx(font, logo3[i], FONT_SZ, 1);
    DrawTextEx(font, logo3[i], {px + (panelW - sz.x) * 0.5f, (float)cy},
               FONT_SZ, 1, C_AMBER_BRT);
    cy += LINE_H;
  }
  cy += 6;

  // Separator + tagline
  DrawLine(px + 14, cy, px + panelW - 14, cy, C_DIM_LINE);
  cy += 8;
  std::string tag =
      std::string("AUTONOMOUS SURVIVAL INTERFACE  //  v") + PROJECT_VERSION;
  Vector2 tsz = MeasureTextEx(font, tag.c_str(), FONT_SZ - 2, 1);
  DrawTextEx(font, tag.c_str(), {px + (panelW - tsz.x) * 0.5f, (float)cy},
             FONT_SZ - 2, 1, C_AMBER_DIM);
  cy += LINE_H + 10;

  // Menu items
  const char *items[3] = {"[1]  NEW SESSION", "[2]  RESTORE SESSION",
                          "[3]  TERMINATE"};
  const int itemH = 32;
  const int itemW = panelW - 60;
  const int itemX = px + 30;

  for (int i = 0; i < 3; ++i) {
    bool selected = (i == inputSt.menuSelection);
    Color rowBg = selected ? Color{C_AMBER.r, C_AMBER.g, C_AMBER.b, 28}
                           : Color{10, 8, 0, 255};
    Color border =
        selected ? C_AMBER
                 : Color{C_AMBER_DIM.r, C_AMBER_DIM.g, C_AMBER_DIM.b, 110};
    Color label = selected ? C_AMBER_BRT : C_AMBER_DIM;

    DrawRectangle(itemX, cy, itemW, itemH, rowBg);
    DrawRectangleLines(itemX, cy, itemW, itemH, border);

    Vector2 isz = MeasureTextEx(font, items[i], FONT_SZ, 1);
    DrawTextEx(
        font, items[i],
        {itemX + (itemW - isz.x) * 0.5f, (float)(cy + (itemH - FONT_SZ) / 2)},
        FONT_SZ, 1, label);

    cy += itemH + 6;
  }

  // Navigation hint
  cy += 4;
  const char *hint = "\x18 \x19  NAVIGATE      ENTER  SELECT";
  Vector2 hsz = MeasureTextEx(font, hint, FONT_SZ - 2, 1);
  DrawTextEx(font, hint, {px + (panelW - hsz.x) * 0.5f, (float)cy}, FONT_SZ - 2,
             1, {C_AMBER_DIM.r, C_AMBER_DIM.g, C_AMBER_DIM.b, 150});

  // Input prompt pinned near bottom of panel
  int inputY = py + panelH - LINE_H - 10;
  DrawLine(px + 14, inputY - 6, px + panelW - 14, inputY - 6, C_DIM_LINE);
  std::string inp = std::string("PROTOCOL:~$ ") + inputSt.current +
                    (inputSt.cursorVis ? "_" : " ");
  Vector2 isz2 = MeasureTextEx(font, inp.c_str(), FONT_SZ, 1);
  DrawTextEx(font, inp.c_str(), {px + (panelW - isz2.x) * 0.5f, (float)inputY},
             FONT_SZ, 1, C_AMBER_DIM);
}

// ============================================================
// MAIN
// ============================================================
int main() {
  srand(static_cast<unsigned>(time(nullptr)));

  SetConfigFlags(FLAG_WINDOW_RESIZABLE);
  InitWindow(DEFAULT_W, DEFAULT_H, "PROTOCOL OS");
  SetTargetFPS(60);

  // Load font with explicit codepoints so extended chars render correctly.
  // Covers standard printable ASCII plus the two non-ASCII chars used in UI
  // strings.
  int codepoints[97];
  int cpCount = 0;
  for (int i = 32; i < 127; ++i)
    codepoints[cpCount++] = i; // ASCII 32-126
  codepoints[cpCount++] =
      0x00BB; // » RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK
  codepoints[cpCount++] = 0x00AF; // ¯ MACRON (used in ASCII logo)

  Font font = LoadFontEx("assets/fonts/ShareTechMono-Regular.ttf", FONT_SZ,
                         codepoints, cpCount);
  if (font.texture.id == 0)
    font = GetFontDefault();

  // Barrel distortion shader — NULL vertex means use raylib's default
  Shader barrelShader = LoadShaderFromMemory(nullptr, BARREL_FRAG);
  int barrelLoc = GetShaderLocation(barrelShader, "barrelStrength");
  float barrelValue = 0.08f; // 0 = flat, higher = more CRT curve
  SetShaderValue(barrelShader, barrelLoc, &barrelValue, SHADER_UNIFORM_FLOAT);

  // Off-screen render target — the whole scene draws here, then gets
  // barrel-distorted when blitted to the actual window
  RenderTexture2D renderTarget = LoadRenderTexture(DEFAULT_W, DEFAULT_H);
  int rtW = DEFAULT_W, rtH = DEFAULT_H;

  GameState gameState;
  AppState appState = AppState::Boot;
  TerminalBuffer tbuf;
  AnimationQueue anim;
  anim.buf = &tbuf;
  InputState inputSt;
  BootState boot;
  boot.init();
  ActionFlash flash;
  ScreenShake shake;
  Camera2D camera = {};
  camera.zoom = 1.0f;
  float decayTimer = 0.f;
  int lastWarnLevel = 3;
  Enemy enemy;
  float encounterTimer = 25.f; // first encounter after 25 s

  g_tbuf = &tbuf;
  g_anim = &anim;
  g_shake = &shake;
  g_enemy = &enemy;
  g_encounterTimer = &encounterTimer;

  while (!WindowShouldClose()) {
    const float dt = GetFrameTime();
    const int W = GetScreenWidth();
    const int H = GetScreenHeight();

    // ── Boot advance ────────────────────────────────────────
    if (appState == AppState::Boot) {
      boot.advance(dt);
      if (boot.done)
        appState = AppState::MainMenu;
    }

    // ── Mouse wheel scroll ───────────────────────────────────
    float wheel = GetMouseWheelMove();
    if (wheel > 0.f)
      tbuf.scrollUp(3);
    if (wheel < 0.f)
      tbuf.scrollDown(3);

    // ── Cursor blink ─────────────────────────────────────────
    inputSt.cursorTimer += dt;
    if (inputSt.cursorTimer >= CURSOR_BLINK) {
      inputSt.cursorTimer = 0.f;
      inputSt.cursorVis = !inputSt.cursorVis;
    }

    // ── Action flash tick ─────────────────────────────────────
    flash.advance(dt);

    // ── Screen shake tick ─────────────────────────────────────
    shake.advance(dt);
    camera.offset = shake.currentOffset();

    // ── Keyboard input ────────────────────────────────────────
    if (appState != AppState::Boot) {
      // ── Main-menu arrow-key navigation ───────────────────────
      if (appState == AppState::MainMenu) {
        if (IsKeyPressed(KEY_UP))
          inputSt.menuSelection = std::max(0, inputSt.menuSelection - 1);
        if (IsKeyPressed(KEY_DOWN))
          inputSt.menuSelection = std::min(2, inputSt.menuSelection + 1);
        if (IsKeyPressed(KEY_ENTER) && inputSt.current.empty()) {
          // Submit the currently highlighted item as if the user typed its
          // number
          inputSt.current = std::to_string(inputSt.menuSelection + 1);
        }
      }

      // ── Terminal scroll (Page Up / Page Down) ─────────────────
      if (IsKeyPressed(KEY_PAGE_UP))
        tbuf.scrollUp(6);
      if (IsKeyPressed(KEY_PAGE_DOWN))
        tbuf.scrollDown(6);

      // Quick-action number shortcuts (Playing mode only)
      if (appState == AppState::Playing && inputSt.mode == InputMode::Command) {
        static const char *quickCmds[4] = {"SYS_HEAL", "INT_CHARGE",
                                           "INT_COOLANT", "SYS_DECON"};
        for (int i = 0; i < 4; ++i) {
          if (IsKeyPressed(KEY_ONE + i)) {
            flash.trigger(i);
            tbuf.push(std::string("PROTOCOL:~$ ") + quickCmds[i], C_AMBER_DIM);
            handleCommand(quickCmds[i], gameState, tbuf, anim, inputSt,
                          appState);
          }
        }
      }

      int ch;
      while ((ch = GetCharPressed()) > 0)
        if (ch >= 32 && ch < 127)
          inputSt.current += static_cast<char>(ch);

      if (IsKeyPressed(KEY_BACKSPACE) && !inputSt.current.empty())
        inputSt.current.pop_back();

      if (IsKeyPressed(KEY_UP) && !inputSt.history.empty() &&
          appState != AppState::MainMenu) {
        inputSt.historyIdx =
            std::min(inputSt.historyIdx + 1, (int)inputSt.history.size() - 1);
        inputSt.current = inputSt.history[inputSt.historyIdx];
      }
      if (IsKeyPressed(KEY_DOWN) && appState != AppState::MainMenu) {
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

        inputSt.history.insert(inputSt.history.begin(), submitted);
        if ((int)inputSt.history.size() > MAX_HISTORY)
          inputSt.history.pop_back();

        std::string upper = submitted;
        std::transform(upper.begin(), upper.end(), upper.begin(),
                       [](unsigned char c) { return std::toupper(c); });

        const char *echo_prompt =
            inputSt.mode == InputMode::Command      ? "PROTOCOL:~$ "
            : inputSt.mode == InputMode::LimbSelect ? "TARGET LIMB: "
                                                    : "DAMAGE LEVEL [1-3]: ";
        tbuf.push(std::string(echo_prompt) + upper, C_AMBER_DIM);

        if (inputSt.mode == InputMode::Command)
          handleCommand(upper, gameState, tbuf, anim, inputSt, appState);
        else
          handleSubPrompt(upper, gameState, tbuf, anim, inputSt);
      }
    }

    // ── Advance animations ────────────────────────────────────
    anim.advance(dt);

    // ── Game ticks ────────────────────────────────────────────
    if (appState == AppState::Playing) {
      decayTimer += dt;
      if (decayTimer >= DECAY_TICK) {
        decayTimer = 0.f;
        if (gameState.player.m_energy > 0.f)
          gameState.player.m_energy -= 1.f;
        if (gameState.player.m_hydration > 0.f)
          gameState.player.m_hydration -= 1.f;
      }

      gameState.player.getTotalHealth();
      int hp = gameState.player.m_overallHealth;

      int newWarnLevel = hp <= 40 ? 0 : hp <= 100 ? 1 : hp <= 200 ? 2 : 3;
      if (newWarnLevel < lastWarnLevel && !anim.isActive()) {
        lastWarnLevel = newWarnLevel;
        if (newWarnLevel == 0) {
          anim.stutterText(
              "CRITICAL_FAILURE: BIOMETRIC_RESERVES_EXHAUSTED...\n");
          anim.stutterText("EMERGENCY_DUMP: PHYSICAL_CONTAINMENT_BREACHED.\n");
          shake.trigger(0.6f, 10.f);
        } else if (newWarnLevel == 1) {
          tbuf.push("[***] COGNITION RUNTIME ERROR: 0x8004210F [***]", C_RED);
          tbuf.push("CRITICAL: SYNAPTIC LINK DESYNC DETECTED.", C_RED);
          shake.trigger(0.4f, 7.f);
        } else if (newWarnLevel == 2) {
          tbuf.push(
              "[!] WARNING: BIOMETRIC FEED UNSTABLE. SEEK RECONSTRUCTION.",
              C_ORANGE);
          shake.trigger(0.25f, 4.f);
        }
      }

      if (appState != AppState::Dead && gameState.player.isDead()) {
        appState = AppState::Dead;
        enemy.active = false;
        shake.trigger(0.8f, 14.f);
        for (int i = 0; i < 5; ++i)
          tbuf.push("");
        tbuf.push("                                [ CONNECTION LOST ]", C_RED);
        tbuf.push("                                  SIGNAL NULL", C_AMBER_DIM);
        for (int i = 0; i < 5; ++i)
          tbuf.push("");
      }

      // ── Enemy AI tick ─────────────────────────────────────
      if (enemy.playerCooldown > 0.f)
        enemy.playerCooldown -= dt;

      if (enemy.active) {
        enemy.attackTimer -= dt;
        if (enemy.attackTimer <= 0.f) {
          enemy.attackTimer = enemy.attackInterval;
          BodyPart target = Enemy::randomLimb();
          gameState.player.damagePlayer(target, enemy.attackPower);
          gameState.player.getTotalHealth();
          shake.trigger(0.2f * enemy.attackPower, 3.f * enemy.attackPower);
          if (enemy.type == "CORRUPTOR") {
            gameState.player.m_radiation =
                std::min(100, gameState.player.m_radiation + 10);
            tbuf.push(std::format("[THREAT] {} RELEASES RADIATION BURST — "
                                  "CONTAMINATION +10, {} HIT!",
                                  enemy.name, Enemy::limbName(target)),
                      C_RED);
          } else {
            tbuf.push(std::format("[THREAT] {} TARGETS {} — IMPACT REGISTERED.",
                                  enemy.name, Enemy::limbName(target)),
                      C_RED);
          }
        }
      } else {
        encounterTimer -= dt;
        if (encounterTimer <= 0.f) {
          enemy = spawnEnemy();
          tbuf.push("", C_AMBER);
          tbuf.push("[ALERT] *** HOSTILE SIGNATURE DETECTED ***", C_RED);
          tbuf.push(
              std::format("  ENTITY: {}  |  TYPE: {}", enemy.name, enemy.type),
              C_RED);
          tbuf.push("  USE ATTACK, SCAN, OR FLEE TO ENGAGE.", C_AMBER_DIM);
          shake.trigger(0.4f, 6.f);
          encounterTimer = 30.f + (float)(rand() % 31);
        }
      }
    }

    // ── Resize render target when window changes ──────────────
    if (W != rtW || H != rtH) {
      UnloadRenderTexture(renderTarget);
      renderTarget = LoadRenderTexture(W, H);
      rtW = W;
      rtH = H;
    }

    // ── Draw scene into off-screen texture ────────────────────
    BeginTextureMode(renderTarget);
    ClearBackground(C_BG);

    BeginMode2D(camera);
    if (appState == AppState::Boot) {
      drawBoot(boot, font, W, H);
    } else if (appState == AppState::MainMenu) {
      drawMainMenu(font, W, H, inputSt);
    } else {
      drawTopBar(font, W, appState);
      drawTerminal(tbuf, anim, font, W, H);
      drawInputLine(inputSt, font, W, H);
      drawSidebar(font, W, H, flash, enemy);
      drawHUD(gameState.player, font, W, H);
    }
    EndMode2D();

    drawScanlines(W, H);
    EndTextureMode();

    // ── Blit through barrel distortion shader ─────────────────
    BeginDrawing();
    ClearBackground(BLACK);
    BeginShaderMode(barrelShader);
    // RenderTexture is upside-down in raylib — negative height flips it
    DrawTexturePro(renderTarget.texture, {0, 0, (float)W, -(float)H},
                   {0, 0, (float)W, (float)H}, {0, 0}, 0.0f, WHITE);
    EndShaderMode();
    EndDrawing();
  }

  UnloadFont(font);
  UnloadRenderTexture(renderTarget);
  UnloadShader(barrelShader);
  CloseWindow();
  return 0;
}
