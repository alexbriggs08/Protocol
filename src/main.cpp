#include "Player.h"
#include "raylib.h"
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

// Forward Declarations
static void handlePlayerInput(Player &player, bool &isNotRunning);
static void playIntro();
static void playDeathSequence();
void typeText(const std::string &text, int speedMs = 40);
void playHealingAnimation(std::string statusMessage);
static void stutterText(const std::string &text);

// MAIN
int main() {
  srand(static_cast<unsigned>(time(nullptr)));
  bool windowClosed = {false};
  Player mainPlayer;
  playIntro();
  mainPlayer.getTotalHealth();

  while (!windowClosed) {
    handlePlayerInput(mainPlayer, windowClosed);
    mainPlayer.getTotalHealth();

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
    if (isPlayerDead) {
      windowClosed = false;
      playDeathSequence();
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } // expand this to restart the game later.
  }
  return 0;
}

// Functions
static void handlePlayerInput(Player &player, bool &isNotRunning) {
  std::cout << "HEAL, DAMAGE, CURRENT, EXIT." << '\n';
  std::string input = {};
  std::getline(std::cin >> std::ws, input);
  std::transform(input.begin(), input.end(), input.begin(), ::toupper);
  if (input == "HEAL") {
    player.healLimb();
  } else if (input == "DAMAGE") {
    std::string limbChoice;
    int damageLevel;

    std::cout << "Choose a limb: \n";
    std::getline(std::cin >> std::ws, limbChoice);

    std::cout << "Choose damage level: \n";
    std::cin >> damageLevel;
    if (damageLevel >= 4 || damageLevel <= 0) {
      std::cout << "Invalid Damage Level \n";
      // TODO: Return early here — damagePlayer() is still called below with
      // the invalid level, silently doing nothing.
    }

    BodyPart part = player.stringToBodyPart(limbChoice);
    player.damagePlayer(part, damageLevel);

  } else if (input == "CURRENT") {
    std::cout << "Overall:" << player.getTotalHealth() << '\n';
    std::cout << "--------------LIMBS---------------- \n";
    std::cout << "Head: " << player.m_head << '\n';
    std::cout << "Torso: " << player.m_torso << '\n';
    std::cout << "Left Arm: " << player.m_leftArm << '\n';
    std::cout << "Right Arm: " << player.m_rightArm << '\n';
    std::cout << "Left Leg: " << player.m_leftLeg << '\n';
    std::cout << "Right Leg: " << player.m_rightLeg << '\n';
    std::cout << "---------------END---------------- \n";
  } else if (input == "KILL") {
    player.m_head = 0;

  } else if (input == "EXIT") {
    std::cout << "Exiting....";
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    isNotRunning = {true};
  }
}

static void playIntro() {
  std::string line1 = "PROTOCOL OS [Version 0.1.0]...\n";
  std::string line2 = "Initializing biometric link...\n";
  std::string line3 = "Limb status: NOMINAL.\n";
  std::string line4 = "Ready for input.\n\n";

  // TODO: Remove this local lambda — it duplicates the global typeText()
  // free function below. Call the global one directly instead.
  auto typeText = [](const std::string &text) {
    for (char c : text) {
      std::cout << c << std::flush;
      std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }
  };

  typeText(line1);
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  typeText(line2);
  typeText(line3);
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  typeText(line4);
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

void playHealingAnimation(std::string statusMessage) {
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
