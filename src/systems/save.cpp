// =============================================================================
// MANTLE - Save System Implementation
// =============================================================================

#include "systems/save.h"
#include "systems/achievements.h"   // migrate_v5 concede los logros que el fichero ya demuestra
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

// Segundos desde la epoca. SDL_GetCurrentTime devuelve nanosegundos; 0 = el sistema no
// sabe la hora, y entonces la partida se queda "sin fecha" y nunca caduca.
static u64 wall_clock_seconds() {
    SDL_Time now = 0;
    if (!SDL_GetCurrentTime(&now) || now <= 0) return 0;
    return static_cast<u64>(now / 1000000000LL);
}

// Cualquier sello por debajo de esto viene de un guardado antiguo (SDL_GetTicks, ms desde
// que arranco el proceso) y no se puede interpretar como fecha.
static constexpr u64 EPOCH_SANITY = 1000000000ULL;   // 2001-09-09

bool SaveSystem::save(bool stamp) {
    if (save_path.empty()) return false;

    if (stamp) data.save_timestamp = wall_clock_seconds();

    SDL_IOStream* file = SDL_IOFromFile(save_path.c_str(), "wb");
    if (!file) {
        SDL_Log("Failed to open save file for writing: %s", save_path.c_str());
        return false;
    }

    // Volcado binario: cabecera de version + SaveData tal cual. El historial de que
    // anadio cada version esta en save.h, junto a SAVE_VERSION. Los ficheros v3..v6 se
    // migran al cargar (ver load()) y se reescriben en el formato actual; v1 y v2 se
    // rechazan.
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

// Disposicion EXACTA del fichero v4: es el SaveData actual sin `unlocks`. Misma regla
// que arriba — no tocar el orden.
struct SaveDataV4 {
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
    u32 consumable_uses;
    u32 miracle_ids[3];   // literal a proposito: es la disposicion del fichero v4, no la actual
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

// v5 = exactamente SaveData sin los tres campos de v6 (achievements/miracles_cast/
// total_kills). Vuelca campo a campo, igual que los dos lectores anteriores.
struct SaveDataV5 {
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
    u32 consumable_uses;
    u32 miracle_ids[Config::MAX_MIRACLE_SLOTS];
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
    u32 unlocks;
};

// v6 = exactamente SaveData sin el bloque de v7 (ajustes + desbloqueos). Misma regla que
// los tres lectores anteriores: es la disposicion del FICHERO, no la del struct actual, y
// por eso los arrays van con su tamano literal donde el actual usa una constante.
struct SaveDataV6 {
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
    u32 consumable_uses;
    u32 miracle_ids[3];
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
    u32 unlocks;
    u32 achievements;
    u32 miracles_cast;
    int total_kills;
};

// v7 es la estructura actual SIN `codex_events` (el campo se anadio al final, que es lo
// unico que permite que este lector sea una copia literal de la anterior).
struct SaveDataV7 {
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
    u32 consumable_uses;
    u32 miracle_ids[3];
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
    u32 unlocks;
    u32 achievements;
    u32 miracles_cast;
    int total_kills;
    f32 music_volume;
    f32 sfx_volume;
    int aim_assist;
    int sensitivity;
    int language;
    u32 settings_flags;
    u32 items_unlocked;
    u32 skins_unlocked;
    u32 skin_equipped;
    u32 entitlements;
};

// v8 es la estructura actual SIN el estado de sala de v9 (`room_flags`/`room_open`, que se
// anadieron al final: por eso este lector vuelve a ser una copia literal de la anterior).
struct SaveDataV8 {
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
    u32 consumable_uses;
    u32 miracle_ids[3];
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
    u32 unlocks;
    u32 achievements;
    u32 miracles_cast;
    int total_kills;
    f32 music_volume;
    f32 sfx_volume;
    int aim_assist;
    int sensitivity;
    int language;
    u32 settings_flags;
    u32 items_unlocked;
    u32 skins_unlocked;
    u32 skin_equipped;
    u32 entitlements;
    u64 codex_events;
};

void migrate_v8(const SaveDataV8& old, SaveData& out) {
    // El estado de sala (`room_flags`/`room_open`) se queda a cero: una partida v8 no guardo
    // que salas dejo despejadas, asi que su Continuar se comporta como siempre se comporto
    // en v8 — el terreno identico (la semilla viaja) y los enemigos de vuelta.
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
    out.consumable_uses = old.consumable_uses;
    for (int i = 0; i < 3 && i < Config::MAX_MIRACLE_SLOTS; i++) out.miracle_ids[i] = old.miracle_ids[i];
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
    out.unlocks         = old.unlocks;
    out.achievements    = old.achievements;
    out.miracles_cast   = old.miracles_cast;
    out.total_kills     = old.total_kills;
    out.music_volume    = old.music_volume;
    out.sfx_volume      = old.sfx_volume;
    out.aim_assist      = old.aim_assist;
    out.sensitivity     = old.sensitivity;
    out.language        = old.language;
    out.settings_flags  = old.settings_flags;
    out.items_unlocked  = old.items_unlocked;
    out.skins_unlocked  = old.skins_unlocked;
    out.skin_equipped   = old.skin_equipped;
    out.entitlements    = old.entitlements;
    out.codex_events    = old.codex_events;
}

void migrate_v7(const SaveDataV7& old, SaveData& out) {
    // `codex_events` se queda a cero a proposito: quien viene de v7 vio los diez encuentros
    // que habia, pero el fichero no guarda CUALES, asi que inventarselo seria mentir en el
    // contador. Se vuelven a descubrir jugando, que es lo que el contador mide.
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
    out.consumable_uses = old.consumable_uses;
    for (int i = 0; i < 3 && i < Config::MAX_MIRACLE_SLOTS; i++) out.miracle_ids[i] = old.miracle_ids[i];
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
    out.unlocks         = old.unlocks;
    out.achievements    = old.achievements;
    out.miracles_cast   = old.miracles_cast;
    out.total_kills     = old.total_kills;
    out.music_volume    = old.music_volume;
    out.sfx_volume      = old.sfx_volume;
    out.aim_assist      = old.aim_assist;
    out.sensitivity     = old.sensitivity;
    out.language        = old.language;
    out.settings_flags  = old.settings_flags;
    out.items_unlocked  = old.items_unlocked;
    out.skins_unlocked  = old.skins_unlocked;
    out.skin_equipped   = old.skin_equipped;
    out.entitlements    = old.entitlements;
}

void migrate_v6(const SaveDataV6& old, SaveData& out) {
    // `out = SaveData{}` deja los campos de v7 en sus VALORES POR DEFECTO, que es
    // justo lo que se quiere: quien viene de v6 nunca eligio un idioma ni una
    // sensibilidad, asi que se lleva los de fabrica (y el auto-apuntado por plataforma).
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
    out.consumable_uses = old.consumable_uses;
    for (int i = 0; i < 3 && i < Config::MAX_MIRACLE_SLOTS; i++) out.miracle_ids[i] = old.miracle_ids[i];
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
    out.unlocks         = old.unlocks;
    out.achievements    = old.achievements;
    out.miracles_cast   = old.miracles_cast;
    out.total_kills     = old.total_kills;
}

void migrate_v5(const SaveDataV5& old, SaveData& out) {
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
    out.consumable_uses = old.consumable_uses;
    for (int i = 0; i < Config::MAX_MIRACLE_SLOTS; i++) out.miracle_ids[i] = old.miracle_ids[i];
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
    out.unlocks         = old.unlocks;
    // Los tres campos de v6 arrancan a cero: quien venia de v5 empieza a coleccionar
    // logros desde ahora. NO se conceden retroactivamente salvo los que ya se pueden
    // deducir sin mentir (ver abajo).
    out.achievements    = 0;
    out.miracles_cast   = 0;
    out.total_kills     = 0;
    // Lo que el fichero YA demuestra: si tiene el epilogo desbloqueado es que gano con
    // la fe alta, y para eso hubo que ganar. Regalar esos dos no inventa nada.
    if (old.unlocks & UNLOCK_EPILOGUE) {
        out.achievements |= (1u << static_cast<u32>(AchievementID::Epilogo)) |
                            (1u << static_cast<u32>(AchievementID::Victoria));
    }
}

void migrate_v4(const SaveDataV4& old, SaveData& out) {
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
    out.consumable_uses = old.consumable_uses;
    for (int i = 0; i < 3 && i < Config::MAX_MIRACLE_SLOTS; i++) out.miracle_ids[i] = old.miracle_ids[i];
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
    // Quien ya se habia pasado el juego no tiene que volver a ganarselo
    out.unlocks         = (old.runs_completed > 0) ? UNLOCK_EPILOGUE : 0u;
}

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
    out.unlocks         = (old.runs_completed > 0) ? UNLOCK_EPILOGUE : 0u;
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
    if (version == 4) {
        SaveDataV4 old{};
        SDL_ReadIO(file, &old, sizeof(old));
        SDL_CloseIO(file);
        migrate_v4(old, data);
        SDL_Log("Save file v4 migrated to v%u (%s)", SAVE_VERSION, save_path.c_str());
        save();   // se reescribe ya en el formato nuevo
        return true;
    }
    if (version == 5) {
        SaveDataV5 old{};
        SDL_ReadIO(file, &old, sizeof(old));
        SDL_CloseIO(file);
        migrate_v5(old, data);
        SDL_Log("Save file v5 migrated to v%u (%s)", SAVE_VERSION, save_path.c_str());
        save();   // se reescribe ya en el formato nuevo
        return true;
    }
    if (version == 6) {
        SaveDataV6 old{};
        SDL_ReadIO(file, &old, sizeof(old));
        SDL_CloseIO(file);
        migrate_v6(old, data);
        SDL_Log("Save file v6 migrated to v%u (%s)", SAVE_VERSION, save_path.c_str());
        save();   // se reescribe ya en el formato nuevo
        return true;
    }
    if (version == 7) {
        SaveDataV7 old{};
        SDL_ReadIO(file, &old, sizeof(old));
        SDL_CloseIO(file);
        migrate_v7(old, data);
        SDL_Log("Save file v7 migrated to v%u (%s)", SAVE_VERSION, save_path.c_str());
        save();   // se reescribe ya en el formato nuevo
        return true;
    }
    if (version == 8) {
        SaveDataV8 old{};
        SDL_ReadIO(file, &old, sizeof(old));
        SDL_CloseIO(file);
        migrate_v8(old, data);
        SDL_Log("Save file v8 migrated to v%u (%s)", SAVE_VERSION, save_path.c_str());
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
    // Solo muere la PARTIDA. Ofrendas, mejoras, codice, logros y desbloqueos son
    // meta-progresion y sobreviven a la derrota, que es justamente su razon de ser.
    //
    // Se reinicia lo de la PARTIDA y se deja intacto todo lo demas, en vez de al reves.
    // La version anterior copiaba a mano la lista de campos meta que habia que salvar, y
    // cada campo nuevo que no se acordara nadie de anadir ahi se borraba en silencio al
    // acabar una partida: asi se perdian los logros y —peor— `unlocks`, o sea el epilogo
    // que el juego promete desbloqueado "para siempre".
    SaveData fresh{};
    data.run_active      = false;
    data.current_act     = fresh.current_act;
    data.current_chapter = fresh.current_chapter;
    data.current_room    = fresh.current_room;
    data.run_time        = fresh.run_time;
    data.character_type  = fresh.character_type;
    data.hp              = fresh.hp;
    data.faith           = fresh.faith;
    data.chapter_seed    = fresh.chapter_seed;
    data.item_count      = fresh.item_count;
    for (int i = 0; i < 24; i++) data.item_ids[i] = 0;
    data.consumable_id   = fresh.consumable_id;
    data.consumable_uses = fresh.consumable_uses;
    for (int i = 0; i < Config::MAX_MIRACLE_SLOTS; i++) data.miracle_ids[i] = fresh.miracle_ids[i];
    data.active_slots    = fresh.active_slots;
    data.save_timestamp  = fresh.save_timestamp;
    // v9: el estado de sala es de la PARTIDA, muere con ella
    std::memset(data.room_flags, 0, sizeof(data.room_flags));
    std::memset(data.room_open,  0, sizeof(data.room_open));
    save();   // reescribe el fichero solo con la meta: el fichero ES el almacen de meta
    SDL_Log("Run ended (meta-progression kept: %d offerings)", data.offerings);
}

bool SaveSystem::is_run_expired() const {
    if (!data.run_active) return false;
    if (data.save_timestamp < EPOCH_SANITY) return false;   // sin fecha fiable: no caduca
    u64 now = wall_clock_seconds();
    if (now < data.save_timestamp) return false;            // reloj movido hacia atras
    f32 hours = static_cast<f32>(now - data.save_timestamp) / 3600.0f;
    return hours > Config::MOBILE_SAVE_TIMEOUT_H;
}

int SaveSystem::get_abandoned_offerings() const {
    // Lo que habria pagado la partida por el capitulo alcanzado (misma tarifa que
    // Engine::award_offerings), al 50%. NO es un porcentaje del banco de ofrendas: eso
    // duplicaba el ahorro del jugador cada vez que abandonaba una partida.
    int earned = data.current_chapter * 12;
    return static_cast<int>(earned * Config::MOBILE_ABANDONED_REWARD);
}
