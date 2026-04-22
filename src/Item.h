#pragma once

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

// What stat the item affects when used.
enum class ItemEffect { healLimb, reduceRadiation, restoreHydration, restoreEnergy };

struct Item {
  std::string name;
  std::string description; // shown in inventory
  std::string useMessage;  // printed on use
  ItemEffect effect;
  float magnitude;         // amount applied to the target stat
};

class Inventory {
public:
  static constexpr int STACK_LIMIT = 30;

  // Adds one to an existing stack with the same name (up to STACK_LIMIT),
  // or creates a new stack if none exists or the existing stack is full.
  void addItem(const Item &item) {
    for (auto &stack : stacks) {
      if (stack.item.name == item.name && stack.count < STACK_LIMIT) {
        ++stack.count;
        return;
      }
    }
    stacks.push_back({item, 1});
  }

  // Removes one from the stack with the matching name.
  // Deletes the stack entry when the count reaches zero.
  void removeItem(const std::string &name) {
    for (auto it = stacks.begin(); it != stacks.end(); ++it) {
      if (it->item.name == name) {
        if (--it->count <= 0)
          stacks.erase(it);
        return;
      }
    }
  }

  void listItems() const {
    if (stacks.empty()) {
      std::cout << "INVENTORY: EMPTY.\n";
      return;
    }
    for (int i = 0; i < static_cast<int>(stacks.size()); ++i)
      std::cout << "[" << i + 1 << "] " << stacks[i].item.name
                << " x" << stacks[i].count
                << " — " << stacks[i].item.description << "\n";
  }

  // Returns the first item with the given effect, or nullptr.
  const Item *findItem(ItemEffect effect) const {
    for (const auto &stack : stacks)
      if (stack.item.effect == effect) return &stack.item;
    return nullptr;
  }

  bool isEmpty() const { return stacks.empty(); }
  int size() const { return static_cast<int>(stacks.size()); }
  Item getItem(int index) const { return stacks[index].item; }

private:
  struct ItemStack {
    Item item;
    int count = 1;
  };

  std::vector<ItemStack> stacks;
};
