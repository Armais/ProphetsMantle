// =============================================================================
// MANTLE - Meta-Progression Implementation
// =============================================================================

#include "systems/progression.h"

void ProgressionSystem::init() {
    upgrades.clear();

    upgrades.push_back({0, "Provision del Cuervo",
        "Empieza con +10 HP", 50, false});
    upgrades.push_back({1, "Fuego Interior",
        "Empieza con +2 dano base", 75, false});
    upgrades.push_back({2, "Sandalias Rapidas",
        "Empieza con +10% velocidad", 60, false});
    upgrades.push_back({3, "Fe Renovada",
        "Empieza con 60 Fe en lugar de 50", 100, false});
    upgrades.push_back({4, "Herencia Ampliada",
        "Eliseo hereda 60% Fe en lugar de 50%", 150, false});
    upgrades.push_back({5, "Altar Portatil",
        "Puedes orar una vez por capitulo sin altar", 120, false});
    upgrades.push_back({6, "Doble Porcion Mejorada",
        "Eliseo empieza con 3 slots de milagro", 200, false});
}

bool ProgressionSystem::purchase(u32 upgrade_id) {
    for (auto& u : upgrades) {
        if (u.id == upgrade_id && !u.unlocked && offerings >= u.cost) {
            offerings -= u.cost;
            u.unlocked = true;
            return true;
        }
    }
    return false;
}

bool ProgressionSystem::is_unlocked(u32 upgrade_id) const {
    for (auto& u : upgrades) {
        if (u.id == upgrade_id) return u.unlocked;
    }
    return false;
}

void ProgressionSystem::add_offerings(int amount) {
    offerings += amount;
}
