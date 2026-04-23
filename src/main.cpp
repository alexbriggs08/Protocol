#include "GameState.h"
#include "ItemDefs.h"
#include "Player.h"
#include "version.h"
#include "raylib.h"
#include <chrono>
#include <format>
#include <iostream>
#include <string>
#include <thread>

// Forward Declarations
static void handlePlayerInput(GameState &state, bool &isNotRunning);
static void showMenu();
static void showMainMenu();
static void playIntro();
static void playDeathSequence();
void typeText(const std::string &text, int speedMs = 40);
void playHealingAnimation(const std::string &statusMessage);
static void stutterText(const std::string &text);
void decayPlayerStats(float &decay, float decayTime);

// MAIN
int main() {
  srand(static_cast<unsigned>(time(nullptr)));
  bool windowClosed{false};
  GameState mainState;

  // Main menu loop — runs until the player starts or quits
  while (true) {
    showMainMenu();
    std::string choice;
    std::cout << "[PROTOCOL]> ";
    std::getline(std::cin >> std::ws, choice);

    if (choice == "1") {
      // New game — populate starting inventory from ItemDefs
      for (const char *name : {"Med-Kit", "Diazine (Grade B)",
                               "Adreno-Spike 0.5mg",
                               "Recycled Biometric Coolant"}) {
        auto def = findItemDef(name);
        if (def) mainState.player.m_inventory.addItem(*def);
      }
      break;
    } else if (choice == "2") {
      if (mainState.load("save.txt")) {
        typeText("SAVE RESTORED. RESUMING SESSION...\n\n");
        break;
      } else {
        typeText("NO SAVE FILE FOUND.\n\n");
      }
    } else if (choice == "3") {
      typeText("TERMINATING PROTOCOL OS...\n");
      return 0;
    }
  }

  playIntro();
  mainState.player.getTotalHealth();

  while (!windowClosed) {
    handlePlayerInput(mainState, windowClosed);
    mainState.player.getTotalHealth();

    decayPlayerStats(mainState.player.m_energy, 1.0f);
    decayPlayerStats(mainState.player.m_hydration, 1.0f);

    // Health warnings (max total HP is 400)
    if (mainState.player.m_overallHealth <= 200) {
      std::cout
          << "[!] WARNING: BIOMETRIC FEED UNSTABLE. SEEK RECONSTRUCTION. \n";
    } else if (mainState.player.m_overallHealth <= 100) {
      std::cout << "\n\033[1;31m[***] COGNITION RUNTIME ERROR: 0x8004210F "
                   "[***]\033[0m\n";
      std::cout << "CRITICAL: SYNAPTIC LINK DESYNC DETECTED.\n";
    } else if (mainState.player.m_overallHealth <= 40) {
      stutterText("CRITICAL_FAILURE: BIOMETRIC_RESERVES_EXHAUSTED...\n");
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      stutterText("EMERGENCY_DUMP: PHYSICAL_CONTAINMENT_BREACHED.\n");
    }
    bool isPlayerDead = mainState.player.isDead();
    if (isPlayerDead && !windowClosed) {
      playDeathSequence();
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } // TODO: expand this to restart the game later.
  }
  std::cout << '\n'
            << std::string(32, ' ') << "[ COGNITION RUN TIME ERROR! ]"
            << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(1));
  return 0;
}

// Functions
static void showMainMenu() {
  std::cout << "\n";
  std::cout << "╔══════════════════════════════════════════════╗\n";
  std::cout << "║  PROTOCOL OS // v" << PROJECT_VERSION;
  // Pad to fill column width (version field is 26 chars wide)
  int padLen = 26 - static_cast<int>(std::string(PROJECT_VERSION).size());
  std::cout << std::string(padLen, ' ') << "║\n";
  std::cout << "╠══════════════════════════════════════════════╣\n";
  std::cout << "║  [1] NEW GAME                                ║\n";
  std::cout << "║  [2] LOAD GAME                               ║\n";
  std::cout << "║  [3] QUIT                                    ║\n";
  std::cout << "╚══════════════════════════════════════════════╝\n";
  std::cout << "\n";
}

static void showMenu() {
  std::cout << "\n";
  std::cout << "╔══════════════════════════════════════════════╗\n";
  std::cout << "║  PROTOCOL OS // COMMAND INTERFACE v0.1       ║\n";
  std::cout << "╠══════════════════════════════════════════════╣\n";
  std::cout << "║  SYS_HEAL    » Initiate bio-reconstruction   ║\n";
  std::cout << "║  SYS_DECON   » Suppress radiation            ║\n";
  std::cout << "║  INT_CHARGE  » Restore energy reserves       ║\n";
  std::cout << "║  INT_COOLANT » Restore hydration             ║\n";
  std::cout << "║  DAMAGE      » Apply damage to limb          ║\n";
  std::cout << "║  CURRENT     » Display biometric feed        ║\n";
  std::cout << "║  SHW_STATS   » Display survival metrics      ║\n";
  std::cout << "║  SHW_INV     » Display inventory manifest    ║\n";
  std::cout << "║  SYS_SAVE    » Save current session          ║\n";
  std::cout << "║  SYS_LOAD    » Restore saved session         ║\n";
  std::cout << "║  SYS_HELP    » Reprint this menu             ║\n";
  std::cout << "║  EXIT        » Terminate session             ║\n";
  std::cout << "╚══════════════════════════════════════════════╝\n";
  std::cout << "\n";
}

static void handlePlayerInput(GameState &state, bool &isNotRunning) {
  Player &player = state.player;
  static bool firstRun = true;
  if (firstRun) {
    showMenu();
    firstRun = false;
  }

  std::cout << "[PROTOCOL]> ";
  std::string input{};
  std::getline(std::cin >> std::ws, input);
  std::transform(input.begin(), input.end(), input.begin(), ::toupper);

  if (input == "SYS_HEAL") {
    const Item *item = player.m_inventory.findItem(ItemEffect::healLimb);
    if (!item) {
      std::cout << "NO HEALING ITEMS IN INVENTORY.\n";
      return;
    }
    std::cout << "TARGET LIMB: ";
    std::string limbChoice;
    std::getline(std::cin >> std::ws, limbChoice);
    BodyPart part = player.stringToBodyPart(limbChoice);
    if (part == BodyPart::None) {
      std::cout << "INVALID LIMB.\n";
      return;
    }
    std::string itemName = item->name;
    player.applyItem(*item, part);
    player.m_inventory.removeItem(itemName);
  } else if (input == "SYS_DECON") {
    const Item *item = player.m_inventory.findItem(ItemEffect::reduceRadiation);
    if (!item) {
      std::cout << "NO DECONTAMINATION ITEMS.\n";
      return;
    }
    std::string itemName = item->name;
    player.applyItem(*item);
    player.m_inventory.removeItem(itemName);
  } else if (input == "INT_CHARGE") {
    const Item *item = player.m_inventory.findItem(ItemEffect::restoreEnergy);
    if (!item) {
      std::cout << "NO ENERGY ITEMS.\n";
      return;
    }
    std::string itemName = item->name;
    player.applyItem(*item);
    player.m_inventory.removeItem(itemName);
  } else if (input == "INT_COOLANT") {
    const Item *item =
        player.m_inventory.findItem(ItemEffect::restoreHydration);
    if (!item) {
      std::cout << "NO HYDRATION ITEMS.\n";
      return;
    }
    std::string itemName = item->name;
    player.applyItem(*item);
    player.m_inventory.removeItem(itemName);
  } else if (input == "SHW_INV") {
    player.m_inventory.listItems();
  } else if (input == "DAMAGE") {
    std::string limbChoice;
    int damageLevel;

    std::cout << "TARGET LIMB: ";
    std::getline(std::cin >> std::ws, limbChoice);

    std::cout << "DAMAGE LEVEL [1-3]: ";
    std::cin >> damageLevel;
    if (damageLevel >= 4 || damageLevel <= 0) {
      std::cout << "INVALID DAMAGE LEVEL.\n";
      return;
    }

    BodyPart part = player.stringToBodyPart(limbChoice);
    player.damagePlayer(part, damageLevel);

  } else if (input == "CURRENT") {
    std::cout << "\n╔══════ BIOMETRIC FEED ══════╗\n";
    std::cout << std::format("║  {:<9}: {:<15}║\n", "OVERALL",
                             player.getTotalHealth());
    std::cout << "╠════════════════════════════╣\n";
    std::cout << std::format("║  {:<9}: {:<15}║\n", "HEAD", player.m_head);
    std::cout << std::format("║  {:<9}: {:<15}║\n", "TORSO", player.m_torso);
    std::cout << std::format("║  {:<9}: {:<15}║\n", "LEFT ARM",
                             player.m_leftArm);
    std::cout << std::format("║  {:<9}: {:<15}║\n", "RIGHT ARM",
                             player.m_rightArm);
    std::cout << std::format("║  {:<9}: {:<15}║\n", "LEFT LEG",
                             player.m_leftLeg);
    std::cout << std::format("║  {:<9}: {:<15}║\n", "RIGHT LEG",
                             player.m_rightLeg);
    std::cout << "╚════════════════════════════╝\n\n";
  } else if (input == "KILL") {
    player.m_head = 0;
  } else if (input == "SHW_STATS") {
    std::cout << std::format("Radiation: {}\n", player.m_radiation);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << std::format("Energy: {}\n", player.m_energy);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << std::format("Hydration: {}\n", player.m_hydration);
  } else if (input == "SYS_SAVE") {
    state.save("save.txt");
    typeText("SAVE COMPLETE.\n");
  } else if (input == "SYS_LOAD") {
    if (state.load("save.txt"))
      typeText("SAVE RESTORED.\n");
    else
      std::cout << "NO SAVE FILE FOUND.\n";
  } else if (input == "SYS_HELP") {
    showMenu();
  } else if (input == "EXIT") {
    std::cout << "TERMINATING SESSION...\n";
    isNotRunning = true;
  } else {
    std::cout << "UNRECOGNIZED COMMAND. TYPE SYS_HELP FOR COMMAND LIST.\n";
  }
}

static void playIntro() {
  typeText("PROTOCOL OS [Version 0.1.0]...\n");
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  typeText("Initializing biometric link...\n");
  typeText("Limb status: NOMINAL.\n");
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  typeText("Ready for input.\n\n");
}

static void playDeathSequence() {
  for (int n = 0; n < 10; ++n)
    std::cout << "\n";

  std::cout << std::string(32, ' ') << "[ CONNECTION LOST ]" << std::endl;
  std::cout << std::string(34, ' ') << "SIGNAL NULL" << std::endl;

  for (int n = 0; n < 10; ++n)
    std::cout << "\n";
}

void typeText(const std::string &text, int speedMs) {
  for (char c : text) {
    std::cout << c << std::flush;
    std::this_thread::sleep_for(std::chrono::milliseconds(speedMs));
  }
}

void playHealingAnimation(const std::string &statusMessage) {
  std::cout << "\n[!] INITIATING BIO-RECONSTRUCTION...\n";

  // Progress Bar
  std::cout << "PROGRESS: [";
  for (int i = 0; i < 20; ++i) {
    std::cout << "■" << std::flush;
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
  }
  std::cout << "] 100%\n";

  // Display the status
  typeText("SYSTEM OS: " + statusMessage + "\n\n");
}

static void stutterText(const std::string &text) {
  for (char c : text) {
    std::cout << "\033[1;31m" << c << "\033[0m" << std::flush;
    // Randomly pause longer to simulate "lag"
    int delay = (rand() % 10 == 0) ? 200 : 30;
    std::this_thread::sleep_for(std::chrono::milliseconds(delay));
  }
}

void decayPlayerStats(float &decay, float decayTime) {
  if (decay > 0) {
    decay -= decayTime;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}
