#pragma once

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>

void playHealingAnimation(std::string statusMessage);

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
  // TODO: m_armour, m_hunger, m_thirst, and m_temperature are never initialized
  // in the constructor (garbage values) and are unused — initialize or remove
  // them.
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

  int getTotalHealth() {
    m_overallHealth =
        m_head + m_torso + m_leftArm + m_rightArm + m_leftLeg + m_rightLeg;
    return (m_head + m_torso + m_leftArm + m_leftLeg + m_rightArm + m_rightLeg);
  }

  Player()
      : m_head(40), m_torso(60), m_leftArm(75), m_leftLeg(75), m_rightArm(75),
        m_rightLeg(75), m_radiation(0), m_hunger(0), m_thirst(0),
        m_temperature(37) {}

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

  void radiationDamage(int radiation) {
    // TODO: Implement radiation damage logic — this function is currently a
    // no-op. Apply limb/health degradation when radiation >= 40.
    if (radiation >= 40) {
    }
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
