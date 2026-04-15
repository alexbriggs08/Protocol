#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

// Forward declaration
class Player;

// Function Calling:
void handlePlayerInput(Player &player, bool &running);

// Classes and Structs
class Player {
public:
  // TODO: add damage and healing on keykinds + and -.
  int m_mainHealth;
  int m_head;
  int m_leftArm;
  int m_rightArm;
  int m_torso;
  int m_leftLeg;
  int m_rightLeg;
  // TODO: Create armour system; later ballistic system.
  int m_armour;
  Player()
      : m_head(40), m_torso(60), m_leftArm(75), m_leftLeg(75), m_rightArm(75),
        m_rightLeg(75) {}

  int getTotalHealth() {
    return (m_head + m_torso + m_leftArm + m_leftLeg + m_rightArm + m_rightLeg);
  }

  void healLimb() {
    std::string limbChoice = {};
    std::cout << "Which Limb?";
    std::getline(
        std::cin >> std::ws,
        limbChoice); // std::getline gets the whole line including spaces.
    transform(limbChoice.begin(), limbChoice.end(), limbChoice.begin(),
              ::toupper); // changes string to all capital.
    // TODO: Change second if statements to std::clamp
    if (limbChoice == "HEAD") {
      m_head += 15;
      if (m_head > 40) {
        m_head = {40};
      }
    } else if (limbChoice == "TORSO") {
      m_torso += 15;
      if (m_torso > 60) {
        m_torso = {60};
      }
    } else if (limbChoice == "LEFT ARM") {
      m_leftArm += 15;
      if (m_leftArm > 75) {
        m_leftArm = {75};
      }
    } else if (limbChoice == "RIGHT ARM") {
      m_rightArm += 15;
      if (m_rightArm > 75) {
        m_rightArm = {75};
      }
    } else if (limbChoice == "LEFT LEG") {
      m_leftLeg += 15;
      if (m_leftLeg > 75) {
        m_leftLeg = {75};
      }
    } else if (limbChoice == "RIGHT LEG") {
      m_rightLeg += 15;
      if (m_rightLeg > 75) {
        m_rightLeg = {75};
      }
    } else {
      std::cout << "Invalid Limb.";
    }
  }
};

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
void handlePlayerInput(Player &player, bool &running) {
  std::cout << "HEAL, EXIT." << '\n';
  std::string input = {};
  std::getline(std::cin >> std::ws, input);
  std::transform(input.begin(), input.end(), input.begin(), ::toupper);
  if (input == "HEAL") {
    player.healLimb();
  }
};