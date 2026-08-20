// =============================================================================
// MANTLE - Save System Implementation
// =============================================================================

#include "systems/save.h"
#include "platform/platform.h"
#include "core/config.h"
#include <SDL3/SDL.h>
#include <cstring>

void SaveSystem::init() {
    const char* pref_path = get_save_path();
    if (pref_path) {
        save_path = std::string(pref_path) + "save.dat";
    }
    // El fichero guarda tambien la meta-progresion (ofrendas, mejoras, codice), asi que
    // hay que leerlo al arrancar y no solo al pulsar "Continuar".
    load();
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
    // (items, consumable, miracles, chapter seed); version 3, la meta-progresion comprada
    // y el codice; version 4, el tercer slot de milagro y las cargas del consumible.
    // Los ficheros v3 se migran al cargar (ver load()); v1 y v2 se rechazan.
    u32 version = SAVE_VERSION;
    SDL_WriteIO(file, &version, sizeof(version));
    SDL_WriteIO(file, &data, sizeof(SaveData));
    SDL_CloseIO(file);

    SDL_Log("Game saved to %s", save_path.c_str());
    return true;
}

// -----------------------------------------------------------------------------
// Migracion v3 -> v4
// -----------------------------------------------------------------------------
// El guardado es un volcado binario de SaveData, asi que anadir campos cambia el
// tamano y la posicion de todo lo que viene detras. Para no tirar las partidas de
// quien ya estaba jugando, esta es la disposicion EXACTA del fichero v3: se lee con
// ella y se copia campo a campo al SaveData actual. No tocar el orden.
namespace {
struct SaveDataV3 {
    bool run_active;
    int current_act;
    int current_chapter;
    int current_room;
    f32 run_time;
    int character_type;
    f32 hp;
    f32 faith;
    u32 chapter_seed;
    int item_count;
    u32 item_ids[24];
    u32 consumable_id;
    u32 miracle_ids[2];
    int active_slots;
    int total_runs;
    int runs_completed;
    int offerings;
    u32 upgrades_bought;
    u32 codex_enemies;
    u32 codex_items;
    u32 codex_miracles;
    u32 codex_chapters;
    u64 save_timestamp;
};

void migrate_v3(const SaveDataV3& old, SaveData& out) {
    out = SaveData{};
    out.run_active      = old.run_active;
    out.current_act     = old.current_act;
    out.current_chapter = old.current_chapter;
    out.current_room    = old.current_room;
    out.run_time        = old.run_time;
    out.character_type  = old.character_type;
    out.hp              = old.hp;
    out.faith           = old.faith;
    out.chapter_seed    = old.chapter_seed;
    out.item_count      = old.item_count;
    for (int i = 0; i < 24; i++) out.item_ids[i] = old.item_ids[i];
    out.consumable_id   = old.consumable_id;
    // v3 no tenia cargas: lo que llevabas vale por un uso, que es como se comportaba
    out.consumable_uses = (old.consumable_id != UINT32_MAX) ? 1u : 0u;
    out.miracle_ids[0]  = old.miracle_ids[0];
    out.miracle_ids[1]  = old.miracle_ids[1];
    out.miracle_ids[2]  = UINT32_MAX;   // el tercer slot nace vacio
    out.active_slots    = old.active_slots;
    out.total_runs      = old.total_runs;
    out.runs_completed  = old.runs_completed;
    out.offerings       = old.offerings;
    out.upgrades_bought = old.upgrades_bought;
    out.codex_enemies   = old.codex_enemies;
    out.codex_items     = old.codex_items;
    out.codex_miracles  = old.codex_miracles;
    out.codex_chapters  = old.codex_chapters;
    out.save_timestamp  = old.save_timestamp;
}
}  // namespace

bool SaveSystem::load() {
    if (save_path.empty()) return false;

    SDL_IOStream* file = SDL_IOFromFile(save_path.c_str(), "rb");
    if (!file) return false;

    u32 version = 0;
    SDL_ReadIO(file, &version, sizeof(version));

    if (version == 3) {
        SaveDataV3 old{};
        SDL_ReadIO(file, &old, sizeof(old));
        SDL_CloseIO(file);
        migrate_v3(old, data);
        SDL_Log("Save file v3 migrated to v%u (%s)", SAVE_VERSION, save_path.c_str());
        save();   // se reescribe ya en el formato nuevo
        return true;
    }
    if (version != SAVE_VERSION) {
        SDL_CloseIO(file);
        SDL_Log("Save file version %u is not supported (expected %u) - ignoring it", version, SAVE_VERSION);
        return false;
    }
    SDL_ReadIO(file, &data, sizeof(SaveData));

    SDL_CloseIO(file);
    SDL_Log("Game loaded from %s", save_path.c_str());
    return true;
}

bool SaveSystem::has_save() const {
    // El fichero puede existir con solo meta-progresion dentro: eso no es una partida.
    return !save_path.empty() && data.run_active;
}

void SaveSystem::delete_save() {
    if (save_path.empty()) return;
    // Solo muere la PARTIDA. Las ofrendas, las mejoras compradas y el codice son
    // meta-progresion: sobreviven a la derrota, que es justamente su razon de ser.
    SaveData meta = data;
    data = SaveData{};
    data.total_runs      = meta.total_runs;
    data.runs_completed  = meta.runs_completed;
    data.offerings       = meta.offerings;
    data.upgrades_bought = meta.upgrades_bought;
    data.codex_enemies   = meta.codex_enemies;
    data.codex_items     = meta.codex_items;
    data.codex_miracles  = meta.codex_miracles;
    data.codex_chapters  = meta.codex_chapters;
    data.run_active      = false;
    save();   // reescribe el fichero solo con la meta: el fichero ES el almacen de meta
    SDL_Log("Run ended (meta-progression kept: %d offerings)", data.offerings);
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
