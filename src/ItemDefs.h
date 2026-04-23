#pragma once

#include "Item.h"
#include <optional>
#include <string>

// Static item definition table.
// All item archetypes are defined here so GameState::load() can restore full
// item data (description, useMessage, effect, magnitude) by name alone.
inline std::optional<Item> findItemDef(const std::string &name) {
    static const Item defs[] = {
        {
            "Med-Kit",
            "Heals a limb by 20 HP.",
            "BIO-RECONSTRUCTION INITIATED.",
            ItemEffect::healLimb,
            20.0f
        },
        {
            "Diazine (Grade B)",
            "Suppresses radiation.",
            "RADIATION SUPPRESSED. CELLULAR INTEGRITY RESTORED.",
            ItemEffect::reduceRadiation,
            30.0f
        },
        {
            "Adreno-Spike 0.5mg",
            "Restores energy.",
            "ENERGY RESERVES REPLENISHED. PERFORMANCE OPTIMAL.",
            ItemEffect::restoreEnergy,
            25.0f
        },
        {
            "Recycled Biometric Coolant",
            "Restores hydration.",
            "HYDRATION NOMINAL. FLUID BALANCE ACHIEVED.",
            ItemEffect::restoreHydration,
            25.0f
        },
    };

    for (const auto &def : defs)
        if (def.name == name)
            return def;

    return std::nullopt;
}
