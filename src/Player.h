#pragma once

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include "Item.h"

void playHealingAnimation(const std::string &statusMessage);

enum class BodyPart { Head, Torso, LeftArm, RightArm, LeftLeg, RightLeg, None };

class Player {
public:
  // Health & Limbs — max HP: head=40, torso=60, arms/legs=75 (total max 400)
  int m_overallHealth;
  int m_head;
  int m_leftArm;
  int m_rightArm;
  int m_torso;
  int m_leftLeg;
  int m_rightLeg;

  // Survival stats — all range 0–100
  int m_radiation;   // causes damage when >= 40
  float m_energy;
  float m_hydration;
  Inventory m_inventory;

  // Applies an item. healLimb items require a target BodyPart.
  bool applyItem(const Item &item, BodyPart target = BodyPart::None) {
    switch (item.effect) {
    case ItemEffect::healLimb:
      if (target == BodyPart::None) {
        std::cout << "LIMB UNSPECIFIED, ABORTING RECONSTRUCTION\n";
        return false;
      }
      healLimb(target, static_cast<int>(item.magnitude));
      return true;
    case ItemEffect::reduceRadiation:
      m_radiation = std::max(0, m_radiation - static_cast<int>(item.magnitude));
      std::cout << item.useMessage << "\n";
      return true;
    case ItemEffect::restoreEnergy:
      m_energy = std::clamp(m_energy + item.magnitude, 0.0f, 100.0f);
      std::cout << item.useMessage << "\n";
      return true;
    case ItemEffect::restoreHydration:
      m_hydration = std::clamp(m_hydration + item.magnitude, 0.0f, 100.0f);
      std::cout << item.useMessage << "\n";
      return true;
    }
    return false;
  }

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
        m_rightLeg(75), m_radiation(0), m_energy(100), m_hydration(100) {}


  // Damages a limb. Level 1=light, 2=medium, 3=heavy.
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

  // Dead if head hits 0, or overall health hits 0.
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

  

private:
  void healLimb(BodyPart part, int amount) {
    std::string msg;
    switch (part) {
    case BodyPart::Head:
      m_head = std::clamp(m_head + amount, 0, 40);
      msg = "COGNITIVE COHERENCE STABILIZING... NEURAL PLASTICITY OPTIMIZED.";
      break;
    case BodyPart::Torso:
      m_torso = std::clamp(m_torso + amount, 0, 60);
      msg = "CAVITY PRESSURE NOMINAL... ORGANIC INTEGRITY RESTORED.";
      break;
    case BodyPart::LeftArm:
    case BodyPart::RightArm:
      if (part == BodyPart::LeftArm)
        m_leftArm = std::clamp(m_leftArm + amount, 0, 75);
      else
        m_rightArm = std::clamp(m_rightArm + amount, 0, 75);
      msg = "MOTOR FUNCTION RECALIBRATING... ACTUATOR TENSION SET TO 100%.";
      break;
    case BodyPart::LeftLeg:
    case BodyPart::RightLeg:
      if (part == BodyPart::LeftLeg)
        m_leftLeg = std::clamp(m_leftLeg + amount, 0, 75);
      else
        m_rightLeg = std::clamp(m_rightLeg + amount, 0, 75);
      msg = "KINETIC STABILIZERS ENGAGED... BONE DENSITY REINFORCED.";
      break;
    default:
      std::cout << "INVALID BODY PART.\n";
      return;
    }
    playHealingAnimation(msg);
  }
};
