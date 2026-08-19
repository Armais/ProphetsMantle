#pragma once

// =============================================================================
// MANTLE - Engine Core (Game Loop, Init, Shutdown)
// =============================================================================

#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include "core/types.h"
#include "core/config.h"
#include "core/ecs.h"
#include "render/renderer.h"
#include "input/input.h"
#include "audio/audio.h"
#include "game/faith.h"
#include "game/player.h"
#include "game/enemies.h"
#include "game/combat.h"
#include "game/rooms.h"
#include "game/items.h"
#include "ui/menus.h"
#include "ui/hud.h"
#include "systems/save.h"

// =============================================================================
// Game State
// =============================================================================
enum class GameScene : u8 {
    MainMenu,
    Playing,
    Paused,
    Choice,      // "Elige una bendicion": 3 item cards over the frozen world
    Transition,  // Elias -> Eliseo ascension
    GameOver,
    Victory
};

// Pending "choose 1 of 3" offer (GDD core loop step 4)
struct ItemChoice {
    u32 ids[3]  = {UINT32_MAX, UINT32_MAX, UINT32_MAX};
    int count    = 0;
    int selected = 0;
    f32 auto_timer = 0.0f;   // > 0: autoplay picks for us after this delay
    bool miracles = false;   // true: ids are MiracleIDs, not item ids
    std::string extras[3];   // per-card note ("Sustituye a ...")
    const char* title = "ELIGE UNA BENDICION";
};

enum class Act : u8 {
    Act_I,   // Elias - El Profeta de Fuego
    Act_II   // Eliseo - El Profeta de la Gracia
};

struct GameState {
    GameScene scene     = GameScene::MainMenu;
    Act current_act     = Act::Act_I;
    int current_chapter = 1;
    int current_room    = 0;
    f32 run_time        = 0.0f;
    bool is_running     = true;
};

// =============================================================================
// Autoplay script (debug): "--script file" drives the input for automated
// playtests. Commands (one per line):
//   wait <sec> | hold <action> <sec> | press <action> | aim <dx> <dy>
//   shot <name> | goto <room_index> | god | quit | say <text>
// Actions: up down left right attack dash m1 m2 confirm cancel pause map
// =============================================================================
struct AutoplayCmd {
    std::string op;
    std::string arg;
    f32 num = 0.0f;
    f32 num2 = 0.0f;
};

struct Autoplay {
    std::vector<AutoplayCmd> cmds;
    size_t index = 0;
    f32 timer = 0.0f;         // Time left in the current wait/hold
    bool active = false;
    bool holding = false;
    u32 hold_action = 0;
    f32 bot_timer = 0.0f;     // "bot <sec>": auto-fight the nearest enemy
    bool walking = false;     // "walk <x> <y>": steer to a world position
    Vec2 walk_target = {0, 0};
    bool load(const char* path);
};

// =============================================================================
// Engine
// =============================================================================
struct Engine {
    // SDL handles
    SDL_Window*   window   = nullptr;
    SDL_Renderer* sdl_renderer = nullptr;
    SDL_Texture*  render_target = nullptr;  // 320x180 internal resolution

    // Subsystems
    Renderer    renderer;
    Input       input;
    Audio       audio;
    ECSWorld    world;
    FaithSystem faith;
    MenuSystem  menus;
    HUD         hud;
    Player      player;
    SaveSystem  save;
    ItemRegistry item_registry;
    ItemChoice   choice;

    // Room / Map
    FloorMap  floor_map;
    Room      current_room_data;
    Direction entry_direction = Direction::None;  // Which side the player entered from
    Entity    boss_entity = ENTITY_NONE;

    // Sprite sheets (renderer texture ids, TEXTURE_NONE if missing -> placeholders)
    u32 character_textures[2] = {TEXTURE_NONE, TEXTURE_NONE};            // by CharacterType
    u32 enemy_textures[static_cast<u32>(EnemyType::COUNT)] = {};         // by EnemyType
    u32 tileset_texture = TEXTURE_NONE;

    // Frame draw list (reused)
    std::vector<DrawCmd> draw_list;

    // HUD feedback
    std::string banner_text, subbanner_text, message_text;
    f32 banner_timer = 0.0f;
    f32 message_timer = 0.0f;
    f32 room_fade = 0.0f;        // 1 -> 0 fade-in after a room/chapter load (black veil under the HUD)
    u32 chapter_seed = 0;        // RNG seed the current chapter map was generated with (saved)
    u32 pending_seed = 0;        // != 0: use this seed for the next start_chapter (loading a save)
    CombatEvents combat_events;
    int  run_kills = 0;
    int  lock_hints_shown = 0;
    bool god_mode = false;

    // Debug
    Autoplay autoplay;
    bool screenshot_requested = false;
    int  screenshot_counter = 0;
    std::string screenshot_dir = "screenshots";
    std::string pending_shot_name;

    // State
    GameState state;

    // Timing
    u64 last_time      = 0;
    f32 accumulator    = 0.0f;
    f32 delta_time     = 0.0f;
    f32 fps            = 0.0f;
    u32 frame_count    = 0;

    // Init / Shutdown
    bool init(const char* title = "MANTLE - El Manto del Profeta");
    void shutdown();
    void load_sprites();   // Fill character_textures / enemy_textures (after renderer.init)

    // Main loop
    void run();

    // Loop steps
    void handle_events();
    void update(f32 dt);
    void render();

    // Scene management
    void change_scene(GameScene new_scene);
    void start_new_run();
    void start_chapter(int chapter, bool announce = true);
    void advance_chapter();
    void retry_chapter();      // Game over -> same chapter again
    void trigger_ascension();  // Elias -> Eliseo transition
    void finish_ascension();   // Confirm on the ascension screen

    // Room loading
    void load_room(int room_index);
    void spawn_room_enemies();
    void try_room_transition();  // Check if player crossed a room border
    void update_room_state();    // Clear detection, boss death, tile interactions
    void open_item_choice(const char* title);      // Roll 3 item offers -> GameScene::Choice
    void open_miracle_choice(const char* title);   // Roll 3 miracle offers -> GameScene::Choice
    void update_item_choice(f32 dt);            // Navigation + confirm on the choice screen

    // Feedback
    void show_banner(const char* title, const char* sub, f32 seconds = 3.0f);
    void show_message(const char* text, f32 seconds = 2.0f);
    void update_music();

    // Save / Settings
    void populate_save_data();
    void continue_run();
    void update_settings();

    // Debug
    void update_autoplay(f32 dt);
    void take_screenshot(const char* name);
};
