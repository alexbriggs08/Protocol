#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

void playHealingAnimation(std::string statusMessage);

// Classes and Structs
enum class BodyPart { Head, Torso, LeftArm, RightArm, LeftLeg, RightLeg, None };
class Player {
public:
  // Health and Limbs
  int m_overallHealth;
  int m_head;
  int m_leftArm;
  int m_rightArm;
  int m_torso;
  int m_leftLeg;
  int m_rightLeg;
  // Armour
  // TODO: Create armour system; later ballistic system.
  int m_armour;
  // Survival Mechanics
  int m_radiation;
  int m_hunger;
  int m_thirst;
  int m_temperature;

  BodyPart stringToBodyPart(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);
    if (str == "HEAD")
      return BodyPart::Head;
    if (str == "TORSO")
      return BodyPart::Torso;
    if (str == "LEFT ARM")
      return BodyPart::LeftArm;
    if (str == "RIGHT ARM")
      return BodyPart::RightArm;
    if (str == "LEFT LEG")
      return BodyPart::LeftLeg;
    if (str == "RIGHT LEG")
      return BodyPart::RightLeg;
    return BodyPart::None;
  }

  Player()
      : m_head(40), m_torso(60), m_leftArm(75), m_leftLeg(75), m_rightArm(75),
        m_rightLeg(75), m_radiation(0), m_hunger(0), m_thirst(0),
        m_temperature(37) {}

  int getTotalHealth() {
    m_overallHealth =
        m_head + m_torso + m_leftArm + m_rightArm + m_leftLeg + m_rightLeg;
    return (m_head + m_torso + m_leftArm + m_leftLeg + m_rightArm + m_rightLeg);
  }

  void healLimb() {
    std::string limbChoice = {};
    std::cout << "Which Limb? ";
    std::getline(std::cin >> std::ws, limbChoice);
    BodyPart part = stringToBodyPart(limbChoice);

    std::string msg = "";

    switch (part) {
    case BodyPart::Head:
      m_head = std::clamp(m_head + 15, 0, 40);
      msg = "COGNITIVE COHERENCE STABILIZING... NEURAL PLASTICITY OPTIMIZED.";
      break;
    case BodyPart::Torso:
      m_torso = std::clamp(m_torso + 15, 0, 60);
      msg = "CAVITY PRESSURE NOMINAL... ORGANIC INTEGRITY RESTORED.";
      break;
    case BodyPart::LeftArm:
    case BodyPart::RightArm:
      if (part == BodyPart::LeftArm)
        m_leftArm = std::clamp(m_leftArm + 15, 0, 75);
      else
        m_rightArm = std::clamp(m_rightArm + 15, 0, 75);
      msg = "MOTOR FUNCTION RECALIBRATING... ACTUATOR TENSION SET TO 100%.";
      break;
    case BodyPart::LeftLeg:
    case BodyPart::RightLeg:
      if (part == BodyPart::LeftLeg)
        m_leftLeg = std::clamp(m_leftLeg + 15, 0, 75);
      else
        m_rightLeg = std::clamp(m_rightLeg + 15, 0, 75);
      msg = "KINETIC STABILIZERS ENGAGED... BONE DENSITY REINFORCED.";
      break;
    default:
      std::cout << "Invalid body part!" << '\n';
      return;
    }

    playHealingAnimation(msg);
  }

  void damagePlayer(BodyPart limb, int damageLevel) {
    if (damageLevel == 1) {
      switch (limb) {
      case BodyPart::Head:
        m_head = std::clamp(m_head - (rand() % 20 + 5), 0, 40);
        break;
      case BodyPart::Torso:
        m_torso = std::clamp(m_torso - (rand() % 20 + 5), 0, 60);
        break;
      case BodyPart::LeftArm:
        m_leftArm = std::clamp(m_leftArm - (rand() % 20 + 5), 0, 75);
        break;
      case BodyPart::RightArm:
        m_rightArm = std::clamp(m_rightArm - (rand() % 20 + 5), 0, 75);
        break;
      case BodyPart::LeftLeg:
        m_leftLeg = std::clamp(m_leftLeg - (rand() % 20 + 5), 0, 75);
        break;
      case BodyPart::RightLeg:
        m_rightLeg = std::clamp(m_rightLeg - (rand() % 20 + 5), 0, 75);
        break;
      case BodyPart::None:
        std::cout << "Something went wrong!" << '\n';
        break;
      }
    } else if (damageLevel == 2) {
      switch (limb) {
      case BodyPart::Head:
        m_head = std::clamp(m_head - (rand() % 30 + 15), 0, 40);
        break;
      case BodyPart::Torso:
        m_torso = std::clamp(m_torso - (rand() % 30 + 15), 0, 60);
        break;
      case BodyPart::LeftArm:
        m_leftArm = std::clamp(m_leftArm - (rand() % 30 + 15), 0, 75);
        break;
      case BodyPart::RightArm:
        m_rightArm = std::clamp(m_rightArm - (rand() % 30 + 15), 0, 75);
        break;
      case BodyPart::LeftLeg:
        m_leftLeg = std::clamp(m_leftLeg - (rand() % 30 + 15), 0, 75);
        break;
      case BodyPart::RightLeg:
        m_rightLeg = std::clamp(m_rightLeg - (rand() % 30 + 15), 0, 75);
        break;
      case BodyPart::None:
        std::cout << "Something went wrong!" << '\n';
        break;
      }
    } else if (damageLevel == 3) {
      switch (limb) {
      case BodyPart::Head:
        m_head = std::clamp(m_head - (rand() % 40 + 25), 0, 40);
        break;
      case BodyPart::Torso:
        m_torso = std::clamp(m_torso - (rand() % 40 + 30), 0, 60);
        break;
      case BodyPart::LeftArm:
        m_leftArm = std::clamp(m_leftArm - (rand() % 50 + 25), 0, 75);
        break;
      case BodyPart::RightArm:
        m_rightArm = std::clamp(m_rightArm - (rand() % 50 + 25), 0, 75);
        break;
      case BodyPart::LeftLeg:
        m_leftLeg = std::clamp(m_leftLeg - (rand() % 50 + 25), 0, 75);
        break;
      case BodyPart::RightLeg:
        m_rightLeg = std::clamp(m_rightLeg - (rand() % 50 + 25), 0, 75);
        break;
      case BodyPart::None:
        std::cout << "Something went wrong!" << '\n';
        break;
      }
    }
  }
    void radiationDamage(int radiation){
      if (radiation >= 40) {
        
        
      }
    }

  };

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

  void typeText(const std::string &text, int speedMs = 40) {
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

  bool isDead() {
    if (m_head <= 0) {
      std::cout << "You have died.";
      return true;
    } else if (m_overallHealth <= 0) {
      std::cout << "You have died.";
      return true;
    } else {
      return false;
    }
  }
};
auto stutterText = [](const std::string &text) {
  for (char c : text) {
    std::cout << "\033[1;31m" << c << "\033[0m" << std::flush;
    // Randomly pause longer to simulate "lag"
    int delay = (rand() % 10 == 0) ? 200 : 30;
    std::this_thread::sleep_for(std::chrono::milliseconds(delay));
  }
};

// MAIN
int main() {
  bool windowClosed = {false};
  Player mainPlayer; // creates the player character
  Player::playIntro();
  mainPlayer.getTotalHealth();

  while (!windowClosed) {
    Player::handlePlayerInput(mainPlayer, windowClosed);
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
      windowClosed = true;
    }
  }

  Player::playDeathSequence();
  return 0;
}
