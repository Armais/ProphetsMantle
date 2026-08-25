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
// v4: tercer slot de milagro de Eliseo + cargas del consumible.
// v5: `unlocks` (contenido desbloqueado entre partidas; hoy solo el epilogo).
// v6: logros (systems/achievements.h) + los dos contadores que necesitan.
// v7: ajustes del jugador (GDD 4.2) + desbloqueos de catalogo y cosmeticos. Se subio UNA
//     sola vez, con TODOS los campos previstos reservados de golpe aunque estuvieran a
//     cero: cinco frentes distintos necesitaban persistir algo y hacerlo por separado
//     habrian sido cinco roturas de formato y cinco migraciones que mantener.
// Las partidas v3, v4, v5 y v6 SI se cargan (SaveSystem::load las migra campo a campo):
// nadie pierde su partida por esto. v1 y v2 se rechazan.
constexpr u32 SAVE_VERSION = 7;

// Bits de SaveData::unlocks (contenido que se gana y NO se pierde al morir)
constexpr u32 UNLOCK_EPILOGUE      = 1u << 0;   // "El Legado": ganar con la fe en Profeta Ungido
constexpr u32 UNLOCK_ELISEO_START  = 1u << 1;   // Empezar la partida como Eliseo

// Bits de SaveData::settings_flags. Anadir un bit aqui NO rompe el formato del fichero:
// esa es toda la razon de que los ajustes ON/OFF vivan en una mascara y no en bools sueltos.
constexpr u32 SETTING_RUMBLE       = 1u << 0;   // Vibracion del mando (GDD 4.2), ON por defecto
constexpr u32 SETTING_TOUCH_RSTICK = 1u << 1;   // Segundo joystick virtual en tactil


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

    // Contenido desbloqueable que persiste entre partidas (UNLOCK_*). Va el ultimo: el
    // lector de compatibilidad de v4 es esta misma estructura sin este campo.
    u32 unlocks          = 0;

    // --- v6: logros (systems/achievements.h) -----------------------------------
    // Los dos contadores viven aqui y no en el sistema de logros porque son datos de la
    // PARTIDA que resulta que algun logro mira; si manana se quita un logro, el contador
    // sigue siendo cierto.
    u32 achievements     = 0;  // bitmask de AchievementID
    u32 miracles_cast    = 0;  // bitmask de MiracleID lanzados alguna vez ("Los Diez Prodigios")
    int total_kills      = 0;  // bajas acumuladas ENTRE partidas ("Cien Caidos")

    // --- v7: ajustes del jugador (GDD 4.2) --------------------------------------
    // Hasta v6 NINGUN ajuste se guardaba: el volumen que ponias volvia a su sitio en el
    // siguiente arranque. Son meta, no partida, asi que sobreviven a delete_save.
    f32 music_volume     = 0.8f;   // espeja Audio::music_volume
    f32 sfx_volume       = 1.0f;   // espeja Audio::sfx_volume
#if defined(MANTLE_PLATFORM_MOBILE)
    int aim_assist       = static_cast<int>(Config::AimMode::On);   // en un telefono el auto-apuntado viene puesto (GDD 4.3)
#else
    int aim_assist       = static_cast<int>(Config::AimMode::Off);  // en teclado apuntas tu, salvo que lo enciendas
#endif
    int sensitivity      = Config::Feel::SENSITIVITY_DEFAULT;  // 1..10; 5 = el tacto de siempre (multiplicador 1.0)
    int language         = 0;      // 0 = ES (base), 1 = EN
    u32 settings_flags   = SETTING_RUMBLE;

    // --- v7: desbloqueos de catalogo y cosmeticos -------------------------------
    // Meta: se compran con ofrendas o se ganan, y NO se pierden al morir.
    u32 items_unlocked   = 0;      // items que nacen bloqueados, por ItemDef::id
    u32 skins_unlocked   = 0;      // variantes cosmeticas del cayado
    u32 skin_equipped    = 0;      // 0 = la de siempre
    u32 entitlements     = 0;      // puerta de contenido; hoy se concede sola (no hay tienda)
};

struct SaveSystem {
    SaveData data;
    std::string save_path;

    void init();          // Fija la ruta y carga (la meta-progresion debe estar viva desde el arranque)
    // stamp = false conserva save_timestamp tal cual (lo usa el comando de guion `agesave`,
    // que envejece una partida a proposito para probar la caducidad).
    bool save(bool stamp = true);
    bool load();
    bool has_save() const;   // Hay una partida a medias, no solo meta-progresion
    void delete_save();      // Termina la partida y CONSERVA la meta-progresion

    // Caducidad movil (GDD 4.4): una partida abandonada 48 h deja de poder continuarse y
    // se cobra al 50%. `save_timestamp` son SEGUNDOS DESDE LA EPOCA (SDL_GetCurrentTime),
    // no SDL_GetTicks: el reloj del proceso arranca a cero en cada sesion y no servia para
    // medir dias. Los guardados viejos traen un valor de ticks (< 1e9) y se dan por
    // "sin fecha": no caducan, y el primer guardado nuevo los sella bien.
    bool is_run_expired() const;
    int get_abandoned_offerings() const;   // 50% de lo que habria pagado la partida
};
