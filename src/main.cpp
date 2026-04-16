#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

// Classes and Structs
enum class BodyPart { Head, Torso, LeftArm, RightArm, LeftLeg, RightLeg, None };
class Player {
public:
  // Health and Limbs
  int m_mainHealth;
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
    return (m_head + m_torso + m_leftArm + m_leftLeg + m_rightArm + m_rightLeg);
  }

  void healLimb() {
    std::string limbChoice = {};
    std::cout << "Which Limb?";
    std::getline(
        std::cin >> std::ws,
        limbChoice); // std::getline gets the whole line including spaces.
    BodyPart part = stringToBodyPart(limbChoice);

    switch (part) {
    case BodyPart::Head:
      m_head = std::clamp(m_head + 15, 0, 40);
      break;
    case BodyPart::Torso:
      m_torso = std::clamp(m_torso + 15, 0, 60);
      break;
    case BodyPart::LeftArm:
      m_leftArm = std::clamp(m_leftArm + 15, 0, 75);
      break;
    case BodyPart::RightArm:
      m_rightArm = std::clamp(m_rightArm + 15, 0, 75);
      break;
    case BodyPart::LeftLeg:
      m_leftLeg = std::clamp(m_leftLeg + 15, 0, 75);
      break;
    case BodyPart::RightLeg:
      m_rightLeg = std::clamp(m_rightLeg + 15, 0, 75);
      break;
    case BodyPart::None:
      std::cout << "Invalid body part!" << '\n';
    }
  };

  void damagePlayer(std::string limb, int damageLevel) {
    // low damage level
    if (damageLevel == 1) {
    }
  };

};
// Function Calling:
void handlePlayerInput(Player &player, bool &running);

  // MAIN
  int main() {
    bool windowClosed = {false};
    Player mainPlayer; // creates the player character

    while (!windowClosed) {
      handlePlayerInput(mainPlayer, windowClosed);
    }
    return 0;
  }

  // Functions
  void handlePlayerInput(Player &player, bool &isNotRunning) {
    std::cout << "HEAL, EXIT." << '\n';
    std::string input = {};
    std::getline(std::cin >> std::ws, input);
    std::transform(input.begin(), input.end(), input.begin(), ::toupper);
    if (input == "HEAL") {
      player.healLimb();
    } else if (input == "EXIT") {
      std::cout << "Exiting....";
      isNotRunning = {true};
    }
  }