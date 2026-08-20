#pragma once

// =============================================================================
// MANTLE - Miracles System (GDD 3.3)
// =============================================================================

#include "core/types.h"
#include <vector>
#include <string>

// =============================================================================
// Miracle Owner
// =============================================================================
enum class Prophet : u8 {
    Elias,
    Eliseo,
    Shared   // Inherited (Elias miracles available to Eliseo at reduced power)
};

// =============================================================================
// Miracle Definitions (from GDD)
// =============================================================================
struct MiracleDef {
    const char* name;
    const char* description;
    f32 faith_cost;
    int tier;
    Prophet owner;
    f32 cooldown;       // seconds
    f32 duration;       // 0 = instant
};

enum class MiracleID : u32 {
    // Elias miracles
    CuervosDeQuerit = 0,  // Heal 10 HP, Tier 1
    PalabraDeSequia,      // Slow enemies 50%, Tier 1
    FuegoDelCielo,        // AOE 80 dmg, Tier 3
    CarroDeFuego,         // Devastating dash, Tier 3
    AltarConsumido,       // Screen nuke, Tier 4

    // Eliseo miracles
    SanarAguas,           // Heal zone 5 HP/s, Tier 1
    AceiteInagotable,     // Consumibles x2, Tier 2
    OjosAbiertos,         // Reveal hidden, Tier 2
    EjercitoCelestial,    // 3 warriors 10s, Tier 3
    DoblePorcion,         // Stats x2 for 15s, Tier 4

    COUNT
};

inline MiracleDef get_miracle_def(MiracleID id) {
    switch (id) {
        // Elias
        case MiracleID::CuervosDeQuerit:
            return {"Cuervos de Querit", "Los cuervos traen sanación", 5, 1, Prophet::Elias, 8.0f, 0};
        case MiracleID::PalabraDeSequia:
            return {"Palabra de Sequía", "Ralentiza enemigos 50%", 8, 1, Prophet::Elias, 12.0f, 5.0f};
        case MiracleID::FuegoDelCielo:
            return {"Fuego del Cielo", "Fuego divino en área", 20, 3, Prophet::Elias, 20.0f, 0};
        case MiracleID::CarroDeFuego:
            return {"Carro de Fuego", "Embestida devastadora", 25, 3, Prophet::Elias, 25.0f, 0};
        case MiracleID::AltarConsumido:
            return {"Altar Consumido", "Destruye toda la pantalla", 30, 4, Prophet::Elias, 45.0f, 0};

        // Eliseo
        case MiracleID::SanarAguas:
            return {"Sanar Aguas", "Zona de curación 5 HP/s", 5, 1, Prophet::Eliseo, 10.0f, 6.0f};
        case MiracleID::AceiteInagotable:
            return {"Aceite Inagotable", "+2 usos del consumible", 12, 2, Prophet::Eliseo, 30.0f, 15.0f};
        case MiracleID::OjosAbiertos:
            return {"Ojos Abiertos", "Revela el mapa del capítulo", 15, 2, Prophet::Eliseo, 20.0f, 10.0f};
        case MiracleID::EjercitoCelestial:
            return {"Ejército Celestial", "Invoca 3 guerreros celestiales", 22, 3, Prophet::Eliseo, 35.0f, 10.0f};
        case MiracleID::DoblePorcion:
            return {"Doble Porción", "Duplica tus fuerzas 15 segundos", 30, 4, Prophet::Eliseo, 60.0f, 15.0f};

        default:
            return {"???", "???", 0, 0, Prophet::Elias, 0, 0};
    }
}

// =============================================================================
// Miracle Slot (runtime state)
// =============================================================================
struct MiracleSlot {
    MiracleID miracle = MiracleID::COUNT;  // COUNT = empty
    f32 cooldown_remaining = 0.0f;
    bool active = false;                    // Is the effect currently running
    f32 active_timer = 0.0f;

    bool is_empty() const { return miracle == MiracleID::COUNT; }
    bool is_ready() const { return !is_empty() && cooldown_remaining <= 0.0f; }

    void equip(MiracleID id) {
        miracle = id;
        cooldown_remaining = 0.0f;
        active = false;
    }

    void update(f32 dt) {
        if (cooldown_remaining > 0.0f) cooldown_remaining -= dt;
        if (active) {
            active_timer -= dt;
            if (active_timer <= 0.0f) {
                active = false;
            }
        }
    }

    bool try_cast(f32 current_faith, int max_tier) {
        if (is_empty() || !is_ready()) return false;
        auto def = get_miracle_def(miracle);
        if (def.faith_cost > current_faith) return false;
        if (def.tier > max_tier) return false;

        cooldown_remaining = def.cooldown;
        if (def.duration > 0.0f) {
            active = true;
            active_timer = def.duration;
        }
        return true;
    }
};
