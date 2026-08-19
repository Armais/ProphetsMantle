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
#include <string>

struct SaveData {
    // Run state
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
    u32 miracle_ids[2]   = {UINT32_MAX, UINT32_MAX};
    int active_slots     = 1;

    // Meta-progression (persists across runs)
    int total_runs       = 0;
    int runs_completed   = 0;
    int offerings        = 0;  // Currency for meta-upgrades

    // Timestamps (for mobile 48h expiry)
    u64 save_timestamp   = 0;
};

struct SaveSystem {
    SaveData data;
    std::string save_path;

    void init();
    bool save();
    bool load();
    bool has_save() const;
    void delete_save();

    // Mobile-specific
    bool is_run_expired() const;
    int get_abandoned_offerings() const;
};
