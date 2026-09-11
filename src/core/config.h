#pragma once

// =============================================================================
// MANTLE - Game Configuration (from GDD v1.0)
// =============================================================================

#include "platform/platform.h"

namespace Config {

// =============================================================================
// Render
// =============================================================================
// World/UI coordinates are in "game units" (320x180). The render target is RENDER_SCALE
// times larger (1280x720) and sprite sheets / tileset hold ASSET_SCALE texels per unit,
// so gameplay, HUD layout and tile logic never see the resolution change.
constexpr int GAME_WIDTH     = 320;   // Logical resolution (game units)
constexpr int GAME_HEIGHT    = 180;   // 16:9 aspect ratio
constexpr int RENDER_SCALE   = 4;     // Render-target pixels per game unit (1280x720 target)
constexpr int ASSET_SCALE    = 4;     // Texels per game unit in sprite sheets / tileset (128 px frames, 64 px tiles)
constexpr int WINDOW_SCALE   = 4;     // Default window = 1280x720
constexpr int WINDOW_WIDTH   = GAME_WIDTH * WINDOW_SCALE;
constexpr int WINDOW_HEIGHT  = GAME_HEIGHT * WINDOW_SCALE;
constexpr int TARGET_FPS     = 60;
constexpr float FIXED_DT     = 1.0f / TARGET_FPS;
constexpr int MIN_FPS        = 30;

// =============================================================================
// Player - Elias
// =============================================================================
constexpr float ELIAS_HP          = 80.0f;
constexpr float ELIAS_SPEED       = 100.0f;  // game units/sec (the 320x180 logical space)
constexpr float ELIAS_DAMAGE      = 12.0f;
constexpr int   ELIAS_MIRACLE_SLOTS = 1;     // 2 from chapter 4+
// Techo de slots de milagro: 3, el que promete la mejora meta 6 para Eliseo (Q / E / F)
constexpr int   MAX_MIRACLE_SLOTS = 3;

// =============================================================================
// Player - Eliseo
// =============================================================================
constexpr float ELISEO_HP         = 100.0f;
constexpr float ELISEO_SPEED      = 100.0f;
constexpr float ELISEO_DAMAGE     = 10.0f;
constexpr int   ELISEO_MIRACLE_SLOTS = 2;    // "Doble Porcion" from start

// =============================================================================
// Faith System - "El Celo del Senor"
// =============================================================================
constexpr float FAITH_MAX         = 100.0f;
constexpr float FAITH_INITIAL     = 50.0f;   // Start at "Siervo Fiel"

// Thresholds
constexpr float FAITH_LEVEL_0     = 0.0f;    // Bajo la Retama: -30% dmg, no miracles
constexpr float FAITH_LEVEL_1     = 10.0f;   // En el Desierto: -15% dmg, Tier 1 only
constexpr float FAITH_LEVEL_2     = 25.0f;   // Fe Vacilante: normal, Tier 1-2
constexpr float FAITH_LEVEL_3     = 50.0f;   // Siervo Fiel: +10% dmg, Tier 1-3
constexpr float FAITH_LEVEL_4     = 75.0f;   // Profeta Ungido: +25% dmg, Tier 1-4
constexpr float FAITH_LEVEL_5     = 90.0f;   // Fuego del Cielo: +40% dmg, powered miracles

// Faith gain
constexpr float FAITH_DESTROY_ALTAR   = 8.0f;
constexpr float FAITH_KILL_PROPHET    = 3.0f;
constexpr float FAITH_ROOM_NO_DAMAGE  = 2.0f;
constexpr float FAITH_PRAY_ALTAR      = 10.0f;
constexpr float FAITH_HELP_WIDOW      = 5.0f;
constexpr float FAITH_DEFEAT_BOSS     = 15.0f;

// Faith loss
constexpr float FAITH_TAKE_DAMAGE     = -1.0f;  // per hit
constexpr float FAITH_FLEE            = -5.0f;
constexpr float FAITH_INACTIVITY_RATE = -0.5f;   // per 10 seconds

// =============================================================================
// Combat
// =============================================================================
constexpr float DASH_DURATION     = 0.15f;   // seconds
constexpr float DASH_SPEED        = 500.0f;
constexpr float DASH_COOLDOWN     = 0.8f;
constexpr float IFRAMES_DURATION  = 0.3f;    // invulnerability after dash

constexpr float PROJECTILE_SPEED  = 280.0f;
constexpr float PROJECTILE_RANGE  = 120.0f;  // Max distance in game units before despawn
constexpr float ATTACK_COOLDOWN   = 0.25f;

// =============================================================================
// Enemies - Act I (from GDD)
// =============================================================================
// Soldado de Acab
constexpr float ENEMY_SOLDIER_HP     = 20.0f;
constexpr float ENEMY_SOLDIER_DMG    = 8.0f;

// Arquero Real
constexpr float ENEMY_ARCHER_HP      = 15.0f;
constexpr float ENEMY_ARCHER_DMG     = 10.0f;

// Profeta de Baal
constexpr float ENEMY_PROPHET_HP     = 35.0f;
constexpr float ENEMY_PROPHET_DMG    = 12.0f;

// Leon del Desierto
constexpr float ENEMY_LION_HP        = 40.0f;
constexpr float ENEMY_LION_DMG       = 18.0f;

// =============================================================================
// Enemies - Act II
// =============================================================================
constexpr float ENEMY_SYRIAN_HP      = 25.0f;
constexpr float ENEMY_SYRIAN_DMG     = 10.0f;

constexpr float ENEMY_GENERAL_HP     = 60.0f;
constexpr float ENEMY_GENERAL_DMG    = 18.0f;

constexpr float ENEMY_LEPER_HP       = 20.0f;
constexpr float ENEMY_LEPER_DMG      = 8.0f;

constexpr float ENEMY_WATER_HP       = 35.0f;
constexpr float ENEMY_WATER_DMG      = 10.0f;

// =============================================================================
// Game feel - tacto (pasada del 2026-08-25)
// =============================================================================
// Nada de aqui toca el balance: ni dano, ni vida, ni cadencia. Solo gobierna cuanto
// PESA un golpe en pantalla. Se afinan contra scripts/feel_baseline.txt, que mide TTK y
// golpes recibidos; una tanda que empeore esa medida se revierte entera.
namespace Feel {

// Curva de una sacudida de camara: magnitud (unidades), duracion (s) y frecuencia
// (rad/s). Corta y rapida para un golpe, larga y lenta para un estallido: con una sola
// magnitud para todo, un roce y un jefe muerto se sentian igual.
struct Shake { float mag, dur, freq; };

// -- Impacto ------------------------------------------------------------------
// Congelacion del mundo al conectar, escalada por el mordisco que el golpe le pega a la
// vida del objetivo: un roce apenas se nota, un remate pesa.
constexpr float HITSTOP_MIN      = 0.012f;
constexpr float HITSTOP_PER_BITE = 0.085f;  // segundos por 100% de la vida del objetivo
constexpr float HITSTOP_MAX      = 0.070f;
constexpr float HITSTOP_BOSS     = 0.050f;  // suelo para los jefes (lo que habia antes)
constexpr float HITSTOP_PLAYER   = 0.060f;  // recibir tambien pesa

constexpr float HIT_FLASH_TIME   = 0.075f;  // corto y casi blanco al empezar
constexpr float SHOT_RECOIL      = 18.0f;   // retroceso del disparo en el profeta

// -- Sacudidas ----------------------------------------------------------------
constexpr Shake SHAKE_HIT        = {0.9f, 0.12f, 55.0f};
constexpr Shake SHAKE_BOSS_HIT   = {1.6f, 0.18f, 46.0f};
constexpr Shake SHAKE_PLAYER_HIT = {2.4f, 0.30f, 34.0f};
constexpr Shake SHAKE_KILL       = {1.2f, 0.18f, 44.0f};
constexpr Shake SHAKE_BOSS_KILL  = {5.0f, 0.70f, 20.0f};
constexpr Shake SHAKE_EXPLOSION  = {3.2f, 0.40f, 24.0f};
constexpr Shake SHAKE_ALTAR      = {2.0f, 0.22f, 30.0f};
constexpr Shake SHAKE_DASH       = {0.7f, 0.10f, 60.0f};

// -- Muerte del enemigo -------------------------------------------------------
constexpr float DEATH_PUSH       = 130.0f;  // el cadaver sale despedido y frena
constexpr float DEATH_FADE_FRAC  = 0.40f;   // ultimo tramo de la animacion: se apaga

// -- Movimiento ---------------------------------------------------------------
constexpr float MOVE_ACCEL       = 22.0f;   // 1/s hacia la velocidad pedida (~0,13 s al tope)
constexpr float MOVE_BRAKE       = 10.0f;   // identico a la friccion que tenia el profeta
constexpr float DASH_TRAIL_RATE  = 0.022f;  // segundos entre estelas del dash
constexpr float COYOTE_TIME      = 0.12f;   // margen sin transiciones tras cambiar de sala

// -- Rumble de mando (0..1) ---------------------------------------------------
constexpr float RUMBLE_HIT        = 0.22f;
constexpr float RUMBLE_PLAYER_HIT = 0.75f;
constexpr float RUMBLE_DASH       = 0.30f;
constexpr float RUMBLE_KILL       = 0.35f;
constexpr float RUMBLE_BOSS_KILL  = 1.00f;

// -- Auto-mira "Asistida" (GDD 4.2 pide tres estados, no un interruptor) ------
// ON fija el objetivo y el tiro deja de ser tuyo; OFF no ayuda en nada. "Asistido" es el
// termino medio: el disparo sale en TU direccion, y solo si hay un objetivo dentro de un
// cono estrecho se corrige el angulo una fraccion hacia el. Un cono ancho lo convierte en
// ON con otro nombre; uno estrecho no se nota. 22 grados es aproximadamente lo que separa
// dos de las ocho direcciones del teclado (45 grados), asi que corrige el error de apuntar
// y no elige objetivo por ti.
constexpr float AIM_ASSIST_CONE_DEG = 22.0f;
constexpr float AIM_ASSIST_PULL     = 0.65f;   // 0 = no corrige, 1 = corrige del todo (= ON)

// -- Sensibilidad de apuntado (1..10, 5 = 1.0) --------------------------------
// El ajuste NO cambia una velocidad de camara (aqui no hay camara que apuntar): cambia
// CUANTO hay que arrastrar el dedo (o desviar el stick) para que el apuntado registre y
// llegue a su tope. Multiplicador = 1 + (valor - 5) * SENSITIVITY_STEP.
constexpr float SENSITIVITY_STEP     = 0.14f;   // 1 -> x0.44, 5 -> x1.0, 10 -> x1.70
constexpr int   SENSITIVITY_DEFAULT  = 5;

} // namespace Feel

// =============================================================================
// Ajustes del jugador (GDD 4.2)
// =============================================================================
// Vive aqui y no en save.h porque lo nombran Input, Player y el menu de Opciones, y
// ninguno de ellos tiene por que saber que existe un fichero de guardado.
enum class AimMode : u8 {
    Off      = 0,   // apuntas tu y solo tu
    Assisted = 1,   // iman suave dentro de AIM_ASSIST_CONE_DEG
    On       = 2    // fija el objetivo (el comportamiento de siempre en movil)
};

// =============================================================================
// Rooms
// =============================================================================
constexpr int TILE_SIZE           = 16;

// Una PANTALLA en tiles. Es el tamano de un interior (palacio, cueva, casa) y de la arena
// del jefe, donde la camara se queda clavada — que es lo que el juego hacia en TODAS las
// salas hasta la pasada del mundo exterior.
constexpr int SCREEN_TILES_X      = GAME_WIDTH / TILE_SIZE;   // 20
constexpr int SCREEN_TILES_Y      = GAME_HEIGHT / TILE_SIZE;  // 11 (+ resto)

// Una sala EXTERIOR: 2x2 pantallas, con la camara siguiendo al profeta (referencia ALTTP,
// ver CLAUDE.md). Tambien es el tope de `Room::tiles`, asi que un interior de una pantalla
// usa la misma matriz y deja el resto sin tocar.
constexpr int ROOM_TILES_X        = SCREEN_TILES_X * 2;       // 40
constexpr int ROOM_TILES_Y        = SCREEN_TILES_Y * 2;       // 22
constexpr int ROOM_W_UNITS        = ROOM_TILES_X * TILE_SIZE; // 640
constexpr int ROOM_H_UNITS        = ROOM_TILES_Y * TILE_SIZE; // 352

// ZONA continua (2026-09-10, decision del usuario: "el mapa sea una exploracion libre, no
// por cuadrados"). Un capitulo es UN solo mapa. La celda (40x22) sigue siendo la unidad de
// generacion pura (mapas fijos: (capitulo, celda) -> mismo terreno); la zona es lo que se
// juega — las celdas se cosen sin anillos entre ellas, sin fundidos, con la camara siguiendo
// al profeta de punta a punta. Tope: rejilla 4x4 = 160x88 tiles (14 KB por Room).
constexpr int ZONE_CELLS_X        = 4;
constexpr int ZONE_CELLS_Y        = 4;
constexpr int ZONE_CELLS_MAX      = ZONE_CELLS_X * ZONE_CELLS_Y;   // 16
constexpr int ZONE_TILES_X        = ROOM_TILES_X * ZONE_CELLS_X;   // 160
constexpr int ZONE_TILES_Y        = ROOM_TILES_Y * ZONE_CELLS_Y;   // 88
// La arena del jefe vive DENTRO de la celda B: un recinto de una pantalla de suelo (20x11)
// con un cerco de piedras y un vano de 4 tiles abajo, sellado mientras el jefe vive.
constexpr int ARENA_TILES_X       = SCREEN_TILES_X;                // 20
constexpr int ARENA_TILES_Y       = SCREEN_TILES_Y;                // 11
constexpr int ARENA_GAP           = 4;
// Celdas VIVAS (ALTTP): los enemigos de una celda nacen al acercarse y se reponen al
// alejarse. Distancias en PANTALLAS desde el rectangulo de la celda.
constexpr float CELL_ACTIVATE_SCREENS = 0.5f;
constexpr float CELL_DESPAWN_SCREENS  = 1.2f;

// Sube con el area: doce enemigos repartidos en cuatro pantallas es un descampado.
constexpr int MAX_ENEMIES_PER_ROOM = 18;

// Mundo FIJO (mandato del usuario, 2026-08-27): la semilla de cada capitulo es una
// CONSTANTE, no un sorteo por partida. El terreno y la colocacion de enemigos salen
// siempre iguales — entre partidas, al Continuar y entre lanzamientos (modelo ALTTP).
// La generacion sigue siendo procedural en runtime, pero como funcion pura de
// (capitulo, sala): Engine::load_room re-siembra rand() con CHAPTER_SEEDS ^ sala antes
// de cada Room::generate. Cambiar un valor aqui REDIBUJA ese capitulo para todo el
// mundo (y pudre las coordenadas clavadas en scripts/): son diseno, no azar.
constexpr u32 CHAPTER_SEEDS[10] = {
    736051u, 421907u, 918273u, 250781u, 664529u,
    133087u, 579341u, 806429u, 347881u, 990151u,
};

// =============================================================================
// Run / Session
// =============================================================================
constexpr float RUN_DURATION_MIN  = 20.0f * 60.0f;  // 20 min in seconds
constexpr float RUN_DURATION_MAX  = 35.0f * 60.0f;  // 35 min in seconds
constexpr int   CHAPTERS_ACT_I   = 5;
constexpr int   CHAPTERS_ACT_II  = 5;

// =============================================================================
// Mobile-specific
// =============================================================================
constexpr float MOBILE_ENEMY_SPEED_MULT  = 0.9f;   // -10% enemy speed on mobile
constexpr float WATER_SPEED_MULT         = 0.6f;   // Wading through a shallow-water tile
constexpr float MOBILE_AUTOAIM_RANGE     = 80.0f;   // game units
constexpr float MOBILE_SAVE_TIMEOUT_H    = 48.0f;   // hours before run expires
constexpr float MOBILE_ABANDONED_REWARD  = 0.5f;    // 50% offerings from abandoned runs

// =============================================================================
// Inheritance (Elias -> Eliseo transition)
// =============================================================================
constexpr float INHERITANCE_FAITH_RATIO  = 0.5f;    // 50% of accumulated faith
constexpr float INHERITED_MIRACLE_POWER  = 0.7f;    // Elias miracles at 70% power for Eliseo

// =============================================================================
// Sprite sizes (in game units; the sheets hold ASSET_SCALE texels per unit)
// =============================================================================
constexpr int SPRITE_CHAR_SIZE  = 32;  // Characters: 32x32
constexpr int SPRITE_ENEMY_SIZE = 32;  // Enemies: 32x32
constexpr int SPRITE_TILE_SIZE  = 16;  // Tiles: 16x16
constexpr int SPRITE_ITEM_SIZE  = 16;  // Items: 16x16

} // namespace Config
