#pragma once

#include "ItemDefs.h"
#include "Player.h"
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

struct GameState {
    Player player;

    void save(const std::string &path) const {
        std::ofstream f(path);
        if (!f) return;

        // Limb health
        f << "head="     << player.m_head     << "\n";
        f << "torso="    << player.m_torso    << "\n";
        f << "leftArm="  << player.m_leftArm  << "\n";
        f << "rightArm=" << player.m_rightArm << "\n";
        f << "leftLeg="  << player.m_leftLeg  << "\n";
        f << "rightLeg=" << player.m_rightLeg << "\n";

        // Survival stats
        f << "radiation=" << player.m_radiation  << "\n";
        f << "energy="    << player.m_energy     << "\n";
        f << "hydration=" << player.m_hydration  << "\n";

        // Inventory — name and count only; full data restored via ItemDefs
        int idx = 0;
        for (int i = 0; i < player.m_inventory.size(); ++i) {
            Item item = player.m_inventory.getItem(i);
            int count = player.m_inventory.getCount(i);
            f << "item_" << idx << "_name="  << item.name << "\n";
            f << "item_" << idx << "_count=" << count     << "\n";
            ++idx;
        }
    }

    // Returns false if the file does not exist or cannot be opened.
    bool load(const std::string &path) {
        std::ifstream f(path);
        if (!f) return false;

        Player fresh;
        fresh.m_inventory = Inventory{};

        std::string line;
        // Collect item entries before adding to inventory
        std::vector<std::pair<std::string, int>> itemEntries; // {name, count}

        while (std::getline(f, line)) {
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string key   = line.substr(0, eq);
            std::string value = line.substr(eq + 1);

            if      (key == "head")      fresh.m_head      = std::stoi(value);
            else if (key == "torso")     fresh.m_torso     = std::stoi(value);
            else if (key == "leftArm")   fresh.m_leftArm   = std::stoi(value);
            else if (key == "rightArm")  fresh.m_rightArm  = std::stoi(value);
            else if (key == "leftLeg")   fresh.m_leftLeg   = std::stoi(value);
            else if (key == "rightLeg")  fresh.m_rightLeg  = std::stoi(value);
            else if (key == "radiation") fresh.m_radiation = std::stoi(value);
            else if (key == "energy")    fresh.m_energy    = std::stof(value);
            else if (key == "hydration") fresh.m_hydration = std::stof(value);
            else {
                // item_N_name or item_N_count
                if (key.size() > 5 && key.substr(0, 5) == "item_") {
                    auto last = key.rfind('_');
                    std::string field = key.substr(last + 1);
                    std::string idxStr = key.substr(5, last - 5);
                    int idx = std::stoi(idxStr);

                    if (static_cast<int>(itemEntries.size()) <= idx)
                        itemEntries.resize(idx + 1, {"", 0});

                    if      (field == "name")  itemEntries[idx].first  = value;
                    else if (field == "count") itemEntries[idx].second = std::stoi(value);
                }
            }
        }

        // Restore inventory from item entries
        for (const auto &entry : itemEntries) {
            auto def = findItemDef(entry.first);
            if (!def) continue;
            for (int i = 0; i < entry.second; ++i)
                fresh.m_inventory.addItem(*def);
        }

        player = fresh;
        player.getTotalHealth();
        return true;
    }
};
