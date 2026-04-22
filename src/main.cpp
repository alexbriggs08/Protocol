#include "Player.h"
#include "raylib.h"
#include <chrono>
#include <iostream>
#include <print>
#include <string>
#include <thread>

// Forward Declarations
static void handlePlayerInput(Player &player, bool &isNotRunning);
static void showMenu();
static void playIntro();
static void playDeathSequence();
void typeText(const std::string &text, int speedMs = 40);
void playHealingAnimation(const std::string &statusMessage);
static void stutterText(const std::string &text);
void decayPlayerStats(float &decay, float decayTime);

// MAIN
int main() {
  srand(static_cast<unsigned>(time(nullptr)));
  bool windowClosed = {false};
  Player mainPlayer;

  // Starting inventory
  mainPlayer.m_inventory.addItem({"Med-Kit", "Heals a limb by 20 HP.", "BIO-RECONSTRUCTION INITIATED.", ItemEffect::healLimb, 20.0f});
  mainPlayer.m_inventory.addItem({"Diazine (Grade B)", "Suppresses radiation.", "RADIATION SUPPRESSED. CELLULAR INTEGRITY RESTORED.", ItemEffect::reduceRadiation, 30.0f});
  mainPlayer.m_inventory.addItem({"Adreno-Spike 0.5mg", "Restores energy.", "ENERGY RESERVES REPLENISHED. PERFORMANCE OPTIMAL.", ItemEffect::restoreEnergy, 25.0f});
  mainPlayer.m_inventory.addItem({"Recycled Biometric Coolant", "Restores hydration.", "HYDRATION NOMINAL. FLUID BALANCE ACHIEVED.", ItemEffect::restoreHydration, 25.0f});

  float decayPerSecond{1};
  playIntro();
  mainPlayer.getTotalHealth();

  while (!windowClosed) {
    handlePlayerInput(mainPlayer, windowClosed);
    mainPlayer.getTotalHealth();

    decayPlayerStats(mainPlayer.m_energy, decayPerSecond);
    decayPlayerStats(mainPlayer.m_hydration, decayPerSecond);

    // Health warnings (max total HP is 400)
    if (mainPlayer.m_overallHealth <= 200) {
      std::cout
          << "[!] WARNING: BIOMETRIC FEED UNSTABLE. SEEK RECONSTRUCTION. \n";
    } else if (mainPlayer.m_overallHealth <= 100) {
      std::cout << "\n\033[1;31m[***] COGNITION RUNTIME ERROR: 0x8004210F "
                   "[***]\033[0m\n";
      std::cout << "CRITICAL: SYNAPTIC LINK DESYNC DETECTED.\n";
    } else if (mainPlayer.m_overallHealth <= 40) {
      stutterText("CRITICAL_FAILURE: BIOMETRIC_RESERVES_EXHAUSTED...\n");
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      stutterText("EMERGENCY_DUMP: PHYSICAL_CONTAINMENT_BREACHED.\n");
    }
    bool isPlayerDead = mainPlayer.isDead();
    if (isPlayerDead && !windowClosed) {
      playDeathSequence();
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } // TODO: expand this to restart the game later.
  }
  std::cout << '\n' << std::string(32, ' ') << "[ COGNITION RUN TIME ERROR! ]" << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(1));
  return 0;
}

// Functions
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
  std::cout << "║  SYS_HELP    » Reprint this menu             ║\n";
  std::cout << "║  EXIT        » Terminate session             ║\n";
  std::cout << "╚══════════════════════════════════════════════╝\n";
  std::cout << "\n";
}

static void handlePlayerInput(Player &player, bool &isNotRunning) {
  static bool firstRun = true;
  if (firstRun) {
    showMenu();
    firstRun = false;
  }

  std::cout << "[PROTOCOL]> ";
  std::string input = {};
  std::getline(std::cin >> std::ws, input);
  std::transform(input.begin(), input.end(), input.begin(), ::toupper);

  if (input == "SYS_HEAL") {
    const Item *item = player.m_inventory.findItem(ItemEffect::healLimb);
    if (!item) { std::cout << "NO HEALING ITEMS IN INVENTORY.\n"; return; }
    std::cout << "TARGET LIMB: ";
    std::string limbChoice;
    std::getline(std::cin >> std::ws, limbChoice);
    BodyPart part = player.stringToBodyPart(limbChoice);
    if (part == BodyPart::None) { std::cout << "INVALID LIMB.\n"; return; }
    std::string itemName = item->name;
    player.applyItem(*item, part);
    player.m_inventory.removeItem(itemName);
  } else if (input == "SYS_DECON") {
    const Item *item = player.m_inventory.findItem(ItemEffect::reduceRadiation);
    if (!item) { std::cout << "NO DECONTAMINATION ITEMS.\n"; return; }
    std::string itemName = item->name;
    player.applyItem(*item);
    player.m_inventory.removeItem(itemName);
  } else if (input == "INT_CHARGE") {
    const Item *item = player.m_inventory.findItem(ItemEffect::restoreEnergy);
    if (!item) { std::cout << "NO ENERGY ITEMS.\n"; return; }
    std::string itemName = item->name;
    player.applyItem(*item);
    player.m_inventory.removeItem(itemName);
  } else if (input == "INT_COOLANT") {
    const Item *item = player.m_inventory.findItem(ItemEffect::restoreHydration);
    if (!item) { std::cout << "NO HYDRATION ITEMS.\n"; return; }
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
    std::print("║  {:<9}: {:<15}║\n", "OVERALL", player.getTotalHealth());
    std::cout << "╠════════════════════════════╣\n";
    std::print("║  {:<9}: {:<15}║\n", "HEAD", player.m_head);
    std::print("║  {:<9}: {:<15}║\n", "TORSO", player.m_torso);
    std::print("║  {:<9}: {:<15}║\n", "LEFT ARM", player.m_leftArm);
    std::print("║  {:<9}: {:<15}║\n", "RIGHT ARM", player.m_rightArm);
    std::print("║  {:<9}: {:<15}║\n", "LEFT LEG", player.m_leftLeg);
    std::print("║  {:<9}: {:<15}║\n", "RIGHT LEG", player.m_rightLeg);
    std::cout << "╚════════════════════════════╝\n\n";
  } else if (input == "KILL") {
    player.m_head = 0;
  } else if (input == "SHW_STATS") {
    std::print("Radiation: {}\n", player.m_radiation);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::print("Energy: {}\n", player.m_energy);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::print("Hydration: {}\n", player.m_hydration);
  } else if (input == "SYS_HELP") {
    showMenu();
  } else if (input == "EXIT") {
    std::cout << "TERMINATING SESSION...\n";
    isNotRunning = {true};
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
