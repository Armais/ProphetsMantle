#pragma once

// =============================================================================
// MANTLE - Meta-Progression System (GDD 3.1 step 7)
// =============================================================================
//
// Persists across runs. Players spend "offerings" to unlock:
// - New starting miracles
// - New items in the pool
// - Character upgrades
// - Lore entries
//
// =============================================================================

#include "core/types.h"
#include <vector>

struct MetaUpgrade {
    u32 id;
    const char* name;
    const char* description;
    int cost;       // Offerings
    bool unlocked;
};

struct ProgressionSystem {
    int offerings = 0;
    std::vector<MetaUpgrade> upgrades;

    void init();
    bool purchase(u32 upgrade_id);
    bool is_unlocked(u32 upgrade_id) const;
    void add_offerings(int amount);
};
