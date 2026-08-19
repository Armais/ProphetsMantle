// =============================================================================
// MANTLE - Save System Implementation
// =============================================================================

#include "systems/save.h"
#include "platform/platform.h"
#include "core/config.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>

void SaveSystem::init() {
    const char* pref_path = get_save_path();
    if (pref_path) {
        save_path = std::string(pref_path) + "save.dat";
    }
}

bool SaveSystem::save() {
    if (save_path.empty()) return false;

    data.save_timestamp = SDL_GetTicks();

    SDL_IOStream* file = SDL_IOFromFile(save_path.c_str(), "wb");
    if (!file) {
        SDL_Log("Failed to open save file for writing: %s", save_path.c_str());
        return false;
    }

    // Simple binary save (version header + raw data). Version 2 added the run loadout
    // (items, consumable, miracles, chapter seed): older files are rejected, not migrated.
    u32 version = 2;
    SDL_WriteIO(file, &version, sizeof(version));
    SDL_WriteIO(file, &data, sizeof(SaveData));
    SDL_CloseIO(file);

    SDL_Log("Game saved to %s", save_path.c_str());
    return true;
}

bool SaveSystem::load() {
    if (save_path.empty()) return false;

    SDL_IOStream* file = SDL_IOFromFile(save_path.c_str(), "rb");
    if (!file) return false;

    u32 version = 0;
    SDL_ReadIO(file, &version, sizeof(version));

    if (version != 2) {
        SDL_CloseIO(file);
        SDL_Log("Save file version %u is not supported (expected 2) - ignoring it", version);
        return false;
    }
    SDL_ReadIO(file, &data, sizeof(SaveData));

    SDL_CloseIO(file);
    SDL_Log("Game loaded from %s", save_path.c_str());
    return true;
}

bool SaveSystem::has_save() const {
    if (save_path.empty()) return false;
    SDL_IOStream* file = SDL_IOFromFile(save_path.c_str(), "rb");
    if (file) {
        SDL_CloseIO(file);
        return true;
    }
    return false;
}

void SaveSystem::delete_save() {
    if (save_path.empty()) return;
    // Reset data
    data = SaveData{};
    // Note: SDL3 doesn't have a built-in file delete, using stdio
    std::remove(save_path.c_str());
    SDL_Log("Save deleted");
}

bool SaveSystem::is_run_expired() const {
    if (data.save_timestamp == 0) return false;
    u64 now = SDL_GetTicks();
    f32 hours = static_cast<f32>(now - data.save_timestamp) / (1000.0f * 3600.0f);
    return hours > Config::MOBILE_SAVE_TIMEOUT_H;
}

int SaveSystem::get_abandoned_offerings() const {
    return static_cast<int>(data.offerings * Config::MOBILE_ABANDONED_REWARD);
}
