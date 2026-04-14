#include <chrono>
#include <iostream>
#include <string>
#include <thread>

class Player {
public:
  // TODO: Finish the health system with multiple limbs.
  int m_mainHealth;
  int m_head;
  int m_leftArm;
  int m_rightArm;
  int m_torso;
  int m_leftLeg;
  int m_rightLeg;
  int m_armour;

  Player()
      : m_head(40), m_torso(60), m_leftArm(75), m_leftLeg(75), m_rightArm(75),
        m_rightLeg(75) {}

  int getTotalHealth() {
    return (m_head + m_torso + m_leftArm + m_leftLeg + m_rightArm + m_rightLeg);
  }
};

int main() {
  Player mainPlayer; // creates the player character

  std::cout << mainPlayer.getTotalHealth();

  // TEMP: KEEP WINDOW OPEN
  int hold;
  std::cin >> hold;
  return 0;
}
