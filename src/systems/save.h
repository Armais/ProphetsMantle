#pragma once

// =============================================================================
// MANTLE - Save System (GDD 4.4)
// =============================================================================
//
// Desktop: Save at end of run / manual save
// Mobile:  Auto-save each room, persist 48h (GDD spec)
//
// =============================================================================

#include "core/types.h"
#include "core/config.h"
#include <string>

// v2: loadout de la partida. v3: meta-progresion comprada + codice + run_active.
// v4: tercer slot de milagro de Eliseo + cargas del consumible. Las partidas v3 SI se
// cargan (SaveSystem::load las migra campo a campo): nadie pierde su partida por esto.
constexpr u32 SAVE_VERSION = 4;

struct SaveData {
    // Run state
    bool run_active      = false;  // false = solo queda meta-progresion, no hay partida que continuar
    int current_act      = 1;
    int current_chapter  = 1;
    int current_room     = 0;
    f32 run_time         = 0.0f;

    // Player
    int character_type   = 0;  // 0=Elias, 1=Eliseo
    f32 hp               = 80.0f;
    f32 faith            = 50.0f;

    // Run inventory / loadout. Items are stored by id and re-applied on load, so the derived
    // stats (max HP, damage, speed, cadence) do not need to be saved.
    u32 chapter_seed     = 0;   // RNG seed the chapter map was generated with (same layout on load)
    int item_count       = 0;
    u32 item_ids[24]     = {};
    u32 consumable_id    = UINT32_MAX;
    u32 consumable_uses  = 0;   // Cargas restantes (Aceite Inagotable); 0 = no llevas nada
    u32 miracle_ids[Config::MAX_MIRACLE_SLOTS] = {UINT32_MAX, UINT32_MAX, UINT32_MAX};
    int active_slots     = 1;

    // Meta-progression (persists across runs)
    int total_runs       = 0;
    int runs_completed   = 0;
    int offerings        = 0;  // Currency for meta-upgrades
    u32 upgrades_bought  = 0;  // Bitmask de MetaUpgrade::id compradas

    // Codice (ui/codex.h): bitmasks de lo descubierto, persisten entre partidas
    u32 codex_enemies    = 0;  // por EnemyType
    u32 codex_items      = 0;  // por ItemDef::id
    u32 codex_miracles   = 0;  // por MiracleID
    u32 codex_chapters   = 0;  // capitulos alcanzados (bit 0 = cap 1)

    // Timestamps (for mobile 48h expiry)
    u64 save_timestamp   = 0;
};

struct SaveSystem {
    SaveData data;
    std::string save_path;

    void init();          // Fija la ruta y carga (la meta-progresion debe estar viva desde el arranque)
    bool save();
    bool load();
    bool has_save() const;   // Hay una partida a medias, no solo meta-progresion
    void delete_save();      // Termina la partida y CONSERVA la meta-progresion

    // Mobile-specific
    bool is_run_expired() const;
    int get_abandoned_offerings() const;
};
