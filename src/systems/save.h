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
// v8: `codex_events`, la mascara de relatos vistos. Es un u64 y no un u32 porque el pozo
//     ya son 35 encuentros y el epilogo anadira mas; con 32 bits habria que renumerar.
//     (2026-08-30: con el libro entero de Reyes son 60 ids de 64 — el techo esta cerca,
//     otro crecimiento pide una segunda mascara y subir a v10.)
// v9: estado de sala persistido (mapas fijos, 2026-08-27): al Continuar, lo despejado sigue
//     despejado y el secreto abierto sigue abierto. Un byte de flags por sala + UN BIT por
//     celda alterada desde la generacion — el terreno NO viaja: se regenera identico porque
//     la generacion es pura por (capitulo, sala) y la semilla es constante.
// v10: zona continua (2026-09-10). El fichero NO cambia de forma (mismos campos, mismos
//      bytes): cambia el SIGNIFICADO de room_flags/room_open — el indice sigue siendo el
//      nodo de la rejilla (= celda de la zona), pero la celda del jefe pasa de 20x11 a 40x22
//      con la arena dentro y los secretos se reparten por coordenadas de zona. Un resumen v9
//      aplicado tal cual abriria tiles que no eran secretos: la migracion lo pone a cero y
//      deja `current_room = -1` (celda de inicio). La partida no se tira: rebobina el mapa
//      del capitulo, como una derrota.
// v11: mapa grande (2026-09-24). Las rejillas pasan de 4x3/4x4 a 6x4/6x5 y el jefe cambia
//      de celda en cada capitulo, asi que el resumen por celda crece a 36 entradas (la
//      rejilla de 30 + el interior, con holgura) y el de una v9/v10 describe OTRA rejilla:
//      la migracion (SaveDataV10 + migrate_v10) lo descarta y el capitulo rebobina a su
//      celda de inicio, igual que al pasar a v10. Equipo, meta y capitulo se conservan.
// v12: El Santuario con RANGOS, la ALFORJA de consumibles y los avisos de una sola vez
//      (2026-10-03). Bloque nuevo al final, con holgura reservada como hizo la v7. La
//      migracion (SaveDataV11 + migrate_v11) da rango 1 a cada mejora comprada en v11 y
//      mete el consumible que se llevaba en el primer hueco de la alforja.
constexpr u32 SAVE_VERSION = 12;

// Huecos de la alforja que caben en el fichero (Player::BELT_MAX).
constexpr int SAVE_BELT_SLOTS = 4;
// Mejoras de El Santuario que caben en el fichero (Meta::COUNT, con holgura).
constexpr int SAVE_MAX_UPGRADES = 32;

// Dimensiones del resumen por celda. 36 = FloorMap::MAX_ROOMS (engine.cpp lo vigila con
// un static_assert); 110 bytes = los 40x22 tiles de una celda exterior, a bit por tile.
constexpr int SAVE_MAX_ROOMS  = 36;
constexpr int SAVE_ROOM_BYTES = (Config::ROOM_TILES_X * Config::ROOM_TILES_Y + 7) / 8;

// Bits de SaveData::unlocks (contenido que se gana y NO se pierde al morir)
constexpr u32 UNLOCK_EPILOGUE      = 1u << 0;   // "El Legado": ganar con la fe en Profeta Ungido
constexpr u32 UNLOCK_ELISEO_START  = 1u << 1;   // Empezar la partida como Eliseo

// Bits de SaveData::settings_flags. Anadir un bit aqui NO rompe el formato del fichero:
// esa es toda la razon de que los ajustes ON/OFF vivan en una mascara y no en bools sueltos.
constexpr u32 SETTING_RUMBLE       = 1u << 0;   // Vibracion del mando (GDD 4.2), ON por defecto
constexpr u32 SETTING_TOUCH_RSTICK = 1u << 1;   // Segundo joystick virtual en tactil
// El selector de idioma del PRIMER arranque (ui/menus.h, MenuScreen::Language) ya se ha
// contestado. Va aqui y no en un campo nuevo justamente por lo que dice el comentario de
// arriba: un bit mas en la mascara no cambia el formato del fichero ni pide migracion. Un
// guardado anterior a esto trae el bit a cero y ve el selector una vez, que es lo correcto:
// nunca se le pregunto. `defaults` lo limpia, y por eso vuelve a salir tras un reset.
constexpr u32 SETTING_LANG_CHOSEN  = 1u << 2;

// Bits de SaveData::tips_seen: avisos que se ensenan UNA vez en la vida del guardado (meta,
// sobreviven a delete_save). Un bit nuevo no rompe el formato.
constexpr u32 TIP_OFFERINGS   = 1u << 0;   // que es una ofrenda y para que sirve
constexpr u32 TIP_BELT        = 1u << 1;   // primer consumible en la alforja: como se usa
constexpr u32 TIP_SANCTUARY   = 1u << 2;   // primera visita a El Santuario
constexpr u32 TIP_BELT_FULL   = 1u << 3;   // la alforja llena (el consumible se queda en el suelo)


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

    // --- v8: relatos vistos (game/events.h) -------------------------------------
    // Bit por id de `namespace Ev`. Es meta y sobrevive a delete_save: el Codice cuenta
    // lo que el PROFETA ha visto en todas sus partidas, no en la ultima.
    u64 codex_events     = 0;

    // --- v9: estado de sala del capitulo en curso (datos de PARTIDA) -------------
    // Lo escribe Engine::capture_room_states en cada guardado y lo re-aplica
    // Engine::apply_saved_room_state al Continuar, sala a sala segun se regeneran.
    // Flags (bit): 0 visited, 1 cleared, 2 altar_used, 3 chest_opened, 4 event_used,
    // 5 exit_open. room_open = celdas alteradas desde la generacion (fila a fila,
    // stride Config::ROOM_TILES_X).
    u8 room_flags[SAVE_MAX_ROOMS] = {};
    u8 room_open[SAVE_MAX_ROOMS][SAVE_ROOM_BYTES] = {};

    // --- v12: El Santuario con rangos, la alforja y los avisos (2026-10-03) -----
    // META: rango comprado de cada mejora (indice = MetaUpgrade::id). `upgrades_bought`
    // se sigue escribiendo (bit = rango > 0) para los guiones y el SAVEDUMP de siempre.
    u8  upgrade_ranks[SAVE_MAX_UPGRADES] = {};
    u32 tips_seen        = 0;   // TIP_*: avisos de una sola vez ya vistos
    // PARTIDA: la alforja (Player::belt). Sustituye a consumable_id/consumable_uses, que
    // se quedan en el fichero solo para que las versiones viejas se puedan leer.
    u32 belt_ids[SAVE_BELT_SLOTS]     = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
    u8  belt_charges[SAVE_BELT_SLOTS] = {};
    int belt_selected    = 0;
    // PARTIDA: lo que queda de los dones de El Santuario en este intento / capitulo.
    // Sin esto, Continuar los rellenaba: salir al menu y volver era un re-sorteo gratis.
    int revives_left     = 0;   // Aliento de Vida
    int rerolls_left     = 0;   // Echar Suertes
    int prayers_left     = 0;   // Altar Portatil
    u32 reserved_v12[8]  = {};  // holgura: lo siguiente no tiene por que subir la version
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
