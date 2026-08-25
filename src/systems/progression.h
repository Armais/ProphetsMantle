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
    // Objeto que esta mejora mete en el sorteo de cartas (ItemDef::id), o NO_ITEM.
    // El GDD prometia "desbloquear items/personajes" en el paso 7 del bucle y hasta ahora
    // las 8 mejoras eran solo cifras: esto es lo que le da sentido a seguir ahorrando.
    // Va el ULTIMO y con valor por defecto: las mejoras que no desbloquean nada compilan
    // igual sin nombrarlo.
    static constexpr u32 NO_ITEM = 0xFFFFFFFFu;
    u32 unlocks_item = NO_ITEM;
    // Skin del cayado que abre esta mejora (ver get_staff_skin), o NO_ITEM. Es lo que el
    // usuario decidio que significaran los "personajes" del GDD: variantes cosmeticas, no
    // un tercer profeta jugable.
    u32 unlocks_skin = NO_ITEM;
};

struct ProgressionSystem {
    int offerings = 0;
    std::vector<MetaUpgrade> upgrades;

    void init();
    bool purchase(u32 upgrade_id);
    bool is_unlocked(u32 upgrade_id) const;
    void add_offerings(int amount);
};
