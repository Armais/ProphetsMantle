// =============================================================================
// MANTLE - Engine Core Implementation
// =============================================================================

#include "core/engine.h"
#include "render/sprites.h"
#include "platform/platform.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

#ifdef MANTLE_PLATFORM_WEB
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

// =============================================================================
// Web main loop callback (Emscripten requires a callback, not a while loop)
// =============================================================================
#ifdef MANTLE_PLATFORM_WEB
static Engine* g_engine = nullptr;

static void web_main_loop() {
    if (!g_engine || !g_engine->state.is_running) {
        emscripten_cancel_main_loop();
        return;
    }
    g_engine->handle_events();

    u64 now = SDL_GetPerformanceCounter();
    f32 frame_time = static_cast<f32>(now - g_engine->last_time) /
                     static_cast<f32>(SDL_GetPerformanceFrequency());
    g_engine->last_time = now;

    // Clamp to avoid spiral of death
    if (frame_time > 0.1f) frame_time = 0.1f;

    g_engine->accumulator += frame_time;
    while (g_engine->accumulator >= Config::FIXED_DT) {
        g_engine->update(Config::FIXED_DT);
        g_engine->accumulator -= Config::FIXED_DT;
    }

    g_engine->render();
    g_engine->frame_count++;
}
#endif

// =============================================================================
// Init
// =============================================================================
bool Engine::init(const char* title) {
    // SDL init
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        SDL_Log("Failed to init SDL: %s", SDL_GetError());
        return false;
    }

    // Determine window size based on platform
    int win_w = Config::WINDOW_WIDTH;
    int win_h = Config::WINDOW_HEIGHT;
    u64 flags = 0;

#if defined(MANTLE_PLATFORM_MOBILE)
    // Fullscreen on mobile
    flags = SDL_WINDOW_FULLSCREEN;
    SDL_DisplayID display = SDL_GetPrimaryDisplay();
    const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(display);
    if (mode) {
        win_w = mode->w;
        win_h = mode->h;
    }
#elif defined(MANTLE_PLATFORM_DESKTOP)
    flags = SDL_WINDOW_RESIZABLE;
    // Fit the window to the desktop (keeps 16:9 by integer scale)
    {
        SDL_DisplayID display = SDL_GetPrimaryDisplay();
        SDL_Rect usable;
        if (display && SDL_GetDisplayUsableBounds(display, &usable)) {
            int scale = Config::WINDOW_SCALE;
            while (scale > 2 && (Config::GAME_WIDTH * scale > usable.w - 40 || Config::GAME_HEIGHT * scale > usable.h - 80)) scale--;
            win_w = Config::GAME_WIDTH * scale;
            win_h = Config::GAME_HEIGHT * scale;
        }
    }
#endif

    window = SDL_CreateWindow(title, win_w, win_h, flags);
    if (!window) {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        return false;
    }

    // Create SDL renderer with VSync
    sdl_renderer = SDL_CreateRenderer(window, nullptr);
    if (!sdl_renderer) {
        SDL_Log("Failed to create renderer: %s", SDL_GetError());
        return false;
    }
    SDL_SetRenderVSync(sdl_renderer, 1);
    SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);

    // Internal render target: GAME_WIDTH x GAME_HEIGHT game units at RENDER_SCALE px per unit
    // (1280x720). Drawing code keeps using game units; Renderer::begin_frame applies the scale.
    render_target = SDL_CreateTexture(
        sdl_renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        Config::GAME_WIDTH * Config::RENDER_SCALE,
        Config::GAME_HEIGHT * Config::RENDER_SCALE
    );
    if (!render_target) {
        SDL_Log("Failed to create render target: %s", SDL_GetError());
        return false;
    }

    // Smooth when the window size is not an exact multiple of the target
    SDL_SetTextureScaleMode(render_target, SDL_SCALEMODE_LINEAR);

    // Init subsystems
    if (!renderer.init(sdl_renderer, render_target)) {
        SDL_Log("Failed to init renderer");
        return false;
    }

    load_sprites();

    input.init();

    if (!audio.init()) {
        SDL_Log("Failed to init audio (continuing without sound)");
    } else {
        audio.load_all_music();
        audio.load_all_sfx();
    }

    world.init();
    world.tileset = tileset_texture;
    faith.init();
    save.init();
    item_registry.init();
    menus.has_continue = save.has_save();

    // Timer + RNG
    last_time = SDL_GetPerformanceCounter();
    srand(static_cast<unsigned>(SDL_GetPerformanceCounter() & 0xFFFFFFFFu));

    SDL_Log("MANTLE engine initialized successfully");
    SDL_Log("  Logical resolution: %dx%d units, render target %dx%d (x%d), assets x%d",
            Config::GAME_WIDTH, Config::GAME_HEIGHT,
            Config::GAME_WIDTH * Config::RENDER_SCALE, Config::GAME_HEIGHT * Config::RENDER_SCALE,
            Config::RENDER_SCALE, Config::ASSET_SCALE);
    SDL_Log("  Window size: %dx%d", win_w, win_h);
    SDL_Log("  Platform: %s",
        is_mobile() ? "Mobile" :
        (get_platform() == Platform::Web ? "Web" : "Desktop"));

    return true;
}

// =============================================================================
// Sprite sheets (assets/sprites/*.png, generated by tools/blender/mantle_sprites)
// =============================================================================
void Engine::load_sprites() {
    auto load_sheet = [this](const char* path) -> u32 {
        u32 id = renderer.load_texture(path);
        if (id != TEXTURE_NONE) {
            TextureEntry* tex = renderer.get_texture(id);
            if (tex && (tex->width != SHEET_WIDTH || tex->height != SHEET_HEIGHT)) {
                SDL_Log("WARNING: sprite sheet '%s' is %dx%d, expected %dx%d",
                        path, tex->width, tex->height, SHEET_WIDTH, SHEET_HEIGHT);
            }
        }
        return id;
    };

    character_textures[static_cast<u32>(CharacterType::Elias)]  = load_sheet("sprites/elias.png");
    character_textures[static_cast<u32>(CharacterType::Eliseo)] = load_sheet("sprites/eliseo.png");

    for (u32 t = 0; t < static_cast<u32>(EnemyType::COUNT); t++) {
        enemy_textures[t] = load_sheet(get_enemy_def(static_cast<EnemyType>(t)).sprite_path);
    }

    tileset_texture = renderer.load_texture("sprites/tiles.png", /*smooth=*/false);  // atlas: nearest, no cell bleeding
    if (tileset_texture == TEXTURE_NONE) {
        SDL_Log("WARNING: tileset missing (assets/sprites/tiles.png) - using placeholder tiles");
    }
}

// =============================================================================
// Shutdown
// =============================================================================
void Engine::shutdown() {
    audio.shutdown();
    renderer.shutdown();
    world.clear();

    if (render_target) SDL_DestroyTexture(render_target);
    if (sdl_renderer)  SDL_DestroyRenderer(sdl_renderer);
    if (window)        SDL_DestroyWindow(window);
    SDL_Quit();
}

// =============================================================================
// Main Loop
// =============================================================================
void Engine::run() {
#ifdef MANTLE_PLATFORM_WEB
    g_engine = this;
    emscripten_set_main_loop(web_main_loop, 0, 1);
#else
    while (state.is_running) {
        handle_events();

        // Delta time with fixed timestep
        u64 now = SDL_GetPerformanceCounter();
        f32 frame_time = static_cast<f32>(now - last_time) /
                         static_cast<f32>(SDL_GetPerformanceFrequency());
        last_time = now;

        // Clamp frame time to avoid spiral of death (e.g., breakpoint, window drag)
        if (frame_time > 0.1f) frame_time = 0.1f;

        // FPS tracking
        delta_time = frame_time;
        fps = (frame_time > 0.0f) ? (1.0f / frame_time) : 0.0f;

        // Fixed timestep accumulator
        accumulator += frame_time;
        while (accumulator >= Config::FIXED_DT) {
            update(Config::FIXED_DT);
            accumulator -= Config::FIXED_DT;
        }

        render();
        frame_count++;
    }
#endif
}

// =============================================================================
// Event Handling
// =============================================================================
void Engine::handle_events() {
    input.pre_update();

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                state.is_running = false;
                break;

            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                renderer.handle_resize(event.window.data1, event.window.data2);
                break;

            case SDL_EVENT_KEY_DOWN:
                if (event.key.scancode == SDL_SCANCODE_F12 && !event.key.repeat) {
                    screenshot_requested = true;
                }
                input.process_event(event);
                break;

            default:
                input.process_event(event);
                break;
        }
    }

    input.post_update();
    input.mouse_game = renderer.window_to_game(input.mouse_pos);
}

// =============================================================================
// Update (called at fixed timestep)
// =============================================================================
void Engine::update(f32 dt) {
    if (autoplay.active) update_autoplay(dt);

    if (banner_timer > 0.0f)  banner_timer  -= dt;
    if (message_timer > 0.0f) message_timer -= dt;
    if (room_fade > 0.0f)     room_fade = std::max(0.0f, room_fade - dt / 0.3f);

    // Menu navigation blips (any scene showing a menu)
    if (state.scene != GameScene::Playing) {
        if (input.is_action_just_pressed(Action::MoveUp) || input.is_action_just_pressed(Action::MoveDown))
            audio.play_sfx(SfxID::MenuSelect, 0.6f);
        if (input.is_action_just_pressed(Action::Confirm))
            audio.play_sfx(SfxID::MenuConfirm, 0.7f);
    }
    update_music();

    switch (state.scene) {
        case GameScene::MainMenu:
            if (menus.current == MenuScreen::Settings) {
                update_settings();
            } else if (menus.current == MenuScreen::Controls) {
                if (input.is_action_just_pressed(Action::Confirm) ||
                    input.is_action_just_pressed(Action::Cancel)) {
                    menus.current = MenuScreen::Title;
                    menus.selected_option = 0;
                }
            } else {
                menus.update(input);
                if (input.is_action_just_pressed(Action::Confirm)) {
                    switch (menus.selected_option) {
                        case 0: start_new_run(); break;
                        case 1:
                            if (save.has_save()) continue_run();
                            break;
                        case 2:
                            menus.settings_origin = MenuScreen::Title;
                            menus.settings_music_vol = audio.music_volume;
                            menus.settings_sfx_vol = audio.sfx_volume;
                            menus.settings_auto_aim = input.auto_aim_enabled;
                            menus.current = MenuScreen::Settings;
                            menus.selected_option = 0;
                            break;
                        case 3:
                            menus.current = MenuScreen::Controls;
                            menus.selected_option = 0;
                            break;
                    }
                }
            }
            break;

        case GameScene::Playing: {
            // Impact freeze after a boss hit: keep rendering, stop the world for 3 frames
            if (world.hitstop > 0.0f) {
                world.hitstop -= dt;
                renderer.update_shake(dt);
                break;
            }

            // Age the floating damage numbers (0.7 s each)
            for (size_t i = 0; i < world.popups.size();) {
                world.popups[i].life += dt;
                if (world.popups[i].life >= 0.7f) world.popups.erase(world.popups.begin() + i);
                else i++;
            }

            state.run_time += dt;
            faith.update(dt);
            int shots_before = player.shots_fired;
            bool was_dashing = player.entity != ENTITY_NONE && world.has(player.entity, Component::Movement) &&
                               world.movements[player.entity].is_dashing;
            player.update(world, input, faith, dt);
            if (player.shots_fired != shots_before)
                audio.play_sfx(player.character == CharacterType::Elias ? SfxID::ProyectilFuego : SfxID::ProyectilLuz, 0.5f);
            if (!was_dashing && player.entity != ENTITY_NONE && world.has(player.entity, Component::Movement) &&
                world.movements[player.entity].is_dashing)
                audio.play_sfx(SfxID::DashWoosh, 0.6f);

            // Check room transition before physics/AI (player may have crossed border)
            try_room_transition();

            world.update(dt);
            update_enemy_ai(world, dt);

            combat_events.reset();
            system_collision(world, faith, combat_events);

            // Miracle feedback (kills by miracles, shake, messages)
            for (Entity k : player.pending_kills) on_enemy_killed(world, faith, k, combat_events);
            player.pending_kills.clear();
            if (player.shake_request > 0.0f) { renderer.shake_camera(player.shake_request, 0.4f); player.shake_request = 0.0f; }
            if (player.last_event == Player::MiracleEvent::Cast) {
                show_message(get_miracle_def(player.last_cast).name, 1.5f);
                switch (player.last_cast) {
                    case MiracleID::CuervosDeQuerit:
                    case MiracleID::SanarAguas:
                    case MiracleID::AceiteInagotable:  audio.play_sfx(SfxID::MiracleHeal); break;
                    case MiracleID::AltarConsumido:    audio.play_sfx(SfxID::MiracleUltimate); break;
                    case MiracleID::FuegoDelCielo:
                    case MiracleID::CarroDeFuego:
                    case MiracleID::PalabraDeSequia:   audio.play_sfx(SfxID::MiracleCastFire); break;
                    default:                           audio.play_sfx(SfxID::MiracleCastLight); break;
                }
            } else if (player.last_event == Player::MiracleEvent::NeedFaith) {
                show_message("Fe insuficiente para el milagro", 1.2f);
                audio.play_sfx(SfxID::MenuBack, 0.5f);
            }
            player.last_event = Player::MiracleEvent::None;

            run_kills += combat_events.enemy_kills;
            player.kills += combat_events.enemy_kills;

            if (combat_events.enemy_hits > 0)  audio.play_sfx(SfxID::EnemyHit, 0.6f);
            if (combat_events.enemy_kills > 0) audio.play_sfx(SfxID::EnemyDeath, 0.8f);
            if (combat_events.hearts_picked + combat_events.orbs_picked > 0) audio.play_sfx(SfxID::ItemPickup, 0.7f);
            if (combat_events.player_hits > 0) {
                player.took_damage_this_room = true;
                renderer.shake_camera(2.5f, 0.25f);
                audio.play_sfx(SfxID::FaithLoss, 0.9f);
            }
            if (combat_events.boss_kills > 0) {
                renderer.shake_camera(5.0f, 0.6f);
                audio.play_sfx(SfxID::CriticalHit);
            }
            if (world.altars_smashed > 0) {
                for (u32 i = 0; i < world.altars_smashed; i++) faith.on_destroy_altar();
                world.altars_smashed = 0;
                renderer.shake_camera(2.0f, 0.2f);
                show_message("Altar de Baal destruido  +8 Fe");
                audio.play_sfx(SfxID::FaithGain);
            }

            if (god_mode && player.entity != ENTITY_NONE && world.has(player.entity, Component::Health)) {
                world.healths[player.entity].current = world.healths[player.entity].max;
            }

            // Consumable (R): the Engine owns the registry, so the key is handled here
            if (input.is_action_just_pressed(Action::Consumable)) {
                u32 held = player.consumable_slot;
                if (player.use_consumable(world, item_registry)) {
                    const ItemDef* d = item_registry.get(held);
                    show_message(d ? d->name : "Consumible usado", 1.5f);
                    audio.play_sfx(SfxID::ItemPickup, 0.9f);
                }
            }

            update_room_state();
            renderer.update_shake(dt);

            // Check player death
            if (player.entity != ENTITY_NONE &&
                world.has(player.entity, Component::Health) &&
                !world.healths[player.entity].is_alive()) {
                SDL_Log("Player died! Game Over.");
                menus.stats_chapter = state.current_chapter;
                menus.stats_kills = run_kills;
                menus.stats_time = state.run_time;
                change_scene(GameScene::GameOver);
            }
            break;
        }

        case GameScene::Choice:
            update_item_choice(dt);
            renderer.update_shake(dt);
            break;

        case GameScene::Paused:
            if (menus.current == MenuScreen::Settings) {
                update_settings();
            } else {
                menus.update(input);
                if (input.is_action_just_pressed(Action::Confirm)) {
                    switch (menus.selected_option) {
                        case 0: change_scene(GameScene::Playing); break;
                        case 1:
                            menus.settings_origin = MenuScreen::PauseMenu;
                            menus.settings_music_vol = audio.music_volume;
                            menus.settings_sfx_vol = audio.sfx_volume;
                            menus.settings_auto_aim = input.auto_aim_enabled;
                            menus.current = MenuScreen::Settings;
                            menus.selected_option = 0;
                            break;
                        case 2:
                            populate_save_data();
                            save.save();
                            change_scene(GameScene::MainMenu);
                            break;
                    }
                }
                if (input.is_action_just_pressed(Action::Pause)) {
                    change_scene(GameScene::Playing);
                }
            }
            break;

        case GameScene::Transition:
            // Ascension screen: confirm to continue as Eliseo
            if (input.is_action_just_pressed(Action::Confirm)) {
                finish_ascension();
            }
            break;

        case GameScene::GameOver:
            if (input.is_action_just_pressed(Action::Consumable)) {   // R: retry the chapter
                retry_chapter();
                break;
            }
            if (input.is_action_just_pressed(Action::Confirm)) {
                save.delete_save();
                change_scene(GameScene::MainMenu);
            }
            break;

        case GameScene::Victory:
            if (input.is_action_just_pressed(Action::Confirm)) {
                save.delete_save();
                change_scene(GameScene::MainMenu);
            }
            break;
    }

    // Pause toggle (available during gameplay)
    if (state.scene == GameScene::Playing &&
        input.is_action_just_pressed(Action::Pause)) {
        change_scene(GameScene::Paused);
    }
}

// =============================================================================
// Render
// =============================================================================
void Engine::render() {
    renderer.begin_frame();

    auto render_world = [this]() {
        const BiomeStyle biome = get_biome_style(state.current_chapter);
        renderer.clear(biome.background);
        current_room_data.render_ground(renderer, tileset_texture, world.time);

        draw_list.clear();
        current_room_data.collect_props(draw_list, tileset_texture, renderer.shadow_tex);
        world.collect_draws(draw_list, renderer);
        renderer.draw_sorted(draw_list);

        current_room_data.render_exit_barriers(renderer, world.time);

        // Soft vignette over the world (under the HUD); margin covers camera shake
        if (renderer.vignette_tex != TEXTURE_NONE) {
            renderer.draw_sprite_rect(renderer.vignette_tex, {0, 0, 160, 90},
                                      {-8, -8, static_cast<f32>(Config::GAME_WIDTH) + 16, static_cast<f32>(Config::GAME_HEIGHT) + 16},
                                      {0, 0, 0, 120});
        }

        // Floating damage numbers: rise and fade over the enemy that was hit
        for (const auto& p : world.popups) {
            f32 k = 1.0f - p.life / 0.7f;
            f32 scale = p.boss ? 0.85f : 0.65f;
            char buf[8];
            snprintf(buf, sizeof(buf), "%d", static_cast<int>(p.amount + 0.5f));
            f32 w = renderer.measure_text(buf, scale);
            u8 a = static_cast<u8>(255.0f * (k > 0.6f ? 1.0f : k / 0.6f));
            Color c = p.boss ? Color{255, 220, 120, a} : Color{250, 240, 220, a};
            renderer.draw_text(buf, p.pos.x - w * 0.5f, p.pos.y - 10.0f - 14.0f * p.life, c, scale);
        }

        // Room/chapter change: short fade-in over the world, under the HUD
        if (room_fade > 0.0f) {
            renderer.draw_rect({0, 0, static_cast<f32>(Config::GAME_WIDTH), static_cast<f32>(Config::GAME_HEIGHT)},
                               {0, 0, 0, static_cast<u8>(235.0f * room_fade)});
        }

        HUDContext ctx;
        ctx.floor_map = &floor_map;
        ctx.room = &current_room_data;
        ctx.boss = boss_entity;
        ctx.chapter = state.current_chapter;
        ctx.banner = banner_timer > 0.0f ? banner_text.c_str() : nullptr;
        ctx.subbanner = subbanner_text.empty() ? nullptr : subbanner_text.c_str();
        ctx.banner_t = banner_timer;
        ctx.message = message_timer > 0.0f ? message_text.c_str() : nullptr;
        ctx.message_t = message_timer;
        ctx.time = world.time;
        ctx.show_map = input.is_action_pressed(Action::Map);
        ctx.tileset = tileset_texture;
        ctx.registry = &item_registry;

        // Sanar Aguas zone
        if (player.heal_zone_timer > 0.0f) {
            f32 pulse = 0.6f + 0.4f * std::sin(world.time * 5.0f);
            renderer.draw_rect({player.heal_zone_pos.x - 32, player.heal_zone_pos.y - 22, 64, 44},
                               {90, 170, 240, static_cast<u8>(50 + 40 * pulse)});
            renderer.draw_rect_outline({player.heal_zone_pos.x - 32, player.heal_zone_pos.y - 22, 64, 44},
                                       {160, 220, 255, static_cast<u8>(120 + 80 * pulse)});
        }
        hud.render(renderer, player, world, faith, ctx);
    };

    switch (state.scene) {
        case GameScene::MainMenu:
            menus.render(renderer);
            break;

        case GameScene::Playing:
            render_world();
            break;

        case GameScene::Paused:
            render_world();
            menus.render(renderer);  // Pause overlay on top
            break;

        case GameScene::Choice: {
            render_world();
            if (choice.miracles) {
                MiracleID ids[3] = {MiracleID::COUNT, MiracleID::COUNT, MiracleID::COUNT};
                const char* extras[3] = {nullptr, nullptr, nullptr};
                for (int i = 0; i < choice.count; i++) {
                    ids[i] = static_cast<MiracleID>(choice.ids[i]);
                    extras[i] = choice.extras[i].c_str();
                }
                draw_miracle_choice(renderer, choice.title, ids, choice.count, choice.selected,
                                    tileset_texture, extras);
            } else {
                const ItemDef* defs[3] = {nullptr, nullptr, nullptr};
                for (int i = 0; i < choice.count; i++) defs[i] = item_registry.get(choice.ids[i]);
                draw_item_choice(renderer, choice.title, defs, choice.count, choice.selected, tileset_texture);
            }
            break;
        }

        case GameScene::Transition:
            menus.render(renderer);
            break;

        case GameScene::GameOver:
            menus.render(renderer);
            break;

        case GameScene::Victory:
            menus.render(renderer);
            break;
    }

    renderer.end_frame();

    if (screenshot_requested) {
        screenshot_requested = false;
        take_screenshot(pending_shot_name.empty() ? nullptr : pending_shot_name.c_str());
        pending_shot_name.clear();
    }
}

// =============================================================================
// Scene Management
// =============================================================================
void Engine::change_scene(GameScene new_scene) {
    SDL_Log("Scene change: %d -> %d",
            static_cast<int>(state.scene),
            static_cast<int>(new_scene));
    state.scene = new_scene;

    // Sync MenuSystem state
    menus.selected_option = 0;
    switch (new_scene) {
        case GameScene::MainMenu:
            menus.current = MenuScreen::Title;
            menus.has_continue = save.has_save();
            break;
        case GameScene::Paused:     menus.current = MenuScreen::PauseMenu; break;
        case GameScene::GameOver:   menus.current = MenuScreen::GameOver;  break;
        case GameScene::Transition: menus.current = MenuScreen::Ascension; break;
        case GameScene::Victory:    menus.current = MenuScreen::Victory;   break;
        default: break;
    }
}

void Engine::start_new_run() {
    SDL_Log("Starting new run - Act I: Elias, El Profeta de Fuego");
    state.current_act = Act::Act_I;
    state.current_chapter = 1;
    state.current_room = 0;
    state.run_time = 0.0f;
    run_kills = 0;

    world.clear();
    world.tileset = tileset_texture;
    faith.init();  // Reset to initial faith

    // Spawn player
    player = Player{};
    player.init_as_elias(world, character_textures[static_cast<u32>(CharacterType::Elias)]);

    start_chapter(1);
    change_scene(GameScene::Playing);
}

void Engine::start_chapter(int chapter, bool announce) {
    state.current_chapter = chapter;
    state.current_act = chapter <= Config::CHAPTERS_ACT_I ? Act::Act_I : Act::Act_II;

    // Elias unlocks the second miracle slot from chapter 4 (GDD)
    if (player.character == CharacterType::Elias) {
        player.active_slot_count = (chapter >= 4) ? 2 : Config::ELIAS_MIRACLE_SLOTS;
    }

    // Small heal between chapters so runs stay winnable (not a full reset: arriving hurt
    // at the next chapter has to mean something)
    if (player.entity != ENTITY_NONE && world.has(player.entity, Component::Health)) {
        auto& h = world.healths[player.entity];
        h.heal(h.max * 0.15f);
    }

    // Seed the chapter map so a saved run can regenerate the exact same layout
    chapter_seed = pending_seed ? pending_seed : static_cast<u32>(rand()) * 2654435761u + 1u;
    pending_seed = 0;
    srand(chapter_seed);

    floor_map.generate(chapter);
    entry_direction = Direction::None;  // Center spawn at chapter start
    boss_entity = ENTITY_NONE;
    load_room(floor_map.current_room);

    if (announce) {
        auto def = get_chapter_def(chapter);
        char title[48];
        snprintf(title, sizeof(title), "CAPITULO %d", chapter);
        show_banner(title, def.name, 3.5f);
        audio.play_sfx(SfxID::Notification, 0.8f);
    }
}

void Engine::advance_chapter() {
    int next = state.current_chapter + 1;
    SDL_Log("Chapter %d complete -> %d", state.current_chapter, next);

    if (next == Config::CHAPTERS_ACT_I + 1) {
        trigger_ascension();
        return;
    }
    if (next > Config::CHAPTERS_ACT_I + Config::CHAPTERS_ACT_II) {
        menus.stats_chapter = state.current_chapter;
        menus.stats_kills = run_kills;
        menus.stats_time = state.run_time;
        save.data.runs_completed++;
        change_scene(GameScene::Victory);
        return;
    }
    start_chapter(next);
}

void Engine::retry_chapter() {
    // Same prophet, fresh HP and starting faith, chapter regenerated
    int chapter = state.current_chapter;
    CharacterType ch = player.character;
    world.clear();
    world.tileset = tileset_texture;
    faith.init();
    player = Player{};
    if (ch == CharacterType::Elias) player.init_as_elias(world, character_textures[static_cast<u32>(CharacterType::Elias)]);
    else                            player.init_as_eliseo(world, character_textures[static_cast<u32>(CharacterType::Eliseo)]);
    start_chapter(chapter);
    change_scene(GameScene::Playing);
}

void Engine::trigger_ascension() {
    SDL_Log("THE ASCENSION - Elias rises, the mantle falls to Eliseo");
    state.current_act = Act::Act_II;
    change_scene(GameScene::Transition);
}

void Engine::finish_ascension() {
    // Inheritance: half the faith (floored so Eliseo starts usable), miracles, mantle
    faith.current = std::max(faith.current * Config::INHERITANCE_FAITH_RATIO, 40.0f);
    faith.modify(0.0f);

    Vec2 pos = {Config::GAME_WIDTH * 0.5f, Config::GAME_HEIGHT * 0.5f};
    int rooms_cleared = player.rooms_cleared;
    int kills = player.kills;
    // Heredable items travel with the mantle; everything else is left behind with Elias
    std::vector<u32> inherited;
    for (u32 id : player.items) {
        const ItemDef* d = item_registry.get(id);
        if (d && d->heredable) inherited.push_back(id);
    }
    if (player.entity != ENTITY_NONE) world.destroy(player.entity);
    player = Player{};
    player.init_as_eliseo(world, character_textures[static_cast<u32>(CharacterType::Eliseo)]);
    player.rooms_cleared = rooms_cleared;
    player.kills = kills;
    for (u32 id : inherited) {
        const ItemDef* d = item_registry.get(id);
        if (d) player.grant_item(world, faith, *d);
    }
    if (!inherited.empty()) SDL_Log("Ascension: %d heredable item(s) passed to Eliseo", static_cast<int>(inherited.size()));
    if (player.entity != ENTITY_NONE) world.transforms[player.entity].position = pos;

    start_chapter(Config::CHAPTERS_ACT_I + 1, false);
    show_banner("ACTO II: ELISEO", "Capitulo 6 - Las Aguas de Jerico", 4.0f);
    change_scene(GameScene::Playing);
}

// =============================================================================
// Music selection by scene / room
// =============================================================================
void Engine::update_music() {
    MusicID want = MusicID::COUNT;
    switch (state.scene) {
        case GameScene::MainMenu:   want = MusicID::MainTheme; break;
        case GameScene::Playing:
        case GameScene::Paused: {
            bool boss_room = floor_map.room_count > 0 &&
                             floor_map.rooms[floor_map.current_room].type == RoomType::Boss &&
                             !current_room_data.cleared;
            if (boss_room) want = MusicID::BossTheme;
            else want = (state.current_act == Act::Act_I) ? MusicID::Exploration : MusicID::EliseoCombat;
            break;
        }
        case GameScene::Transition: want = MusicID::Ascension; break;
        case GameScene::GameOver:   want = MusicID::Death; break;
        case GameScene::Victory:    want = MusicID::Victory; break;
    }
    if (want != MusicID::COUNT && want != audio.current_music) {
        audio.play_music(want, true, 800.0f);
    }
}

// =============================================================================
// Feedback
// =============================================================================
void Engine::show_banner(const char* title, const char* sub, f32 seconds) {
    banner_text = title ? title : "";
    subbanner_text = sub ? sub : "";
    banner_timer = seconds;
}

void Engine::show_message(const char* text, f32 seconds) {
    message_text = text ? text : "";
    message_timer = seconds;
}

// =============================================================================
// Save / Settings
// =============================================================================
void Engine::populate_save_data() {
    save.data.current_act     = (state.current_act == Act::Act_I) ? 1 : 2;
    save.data.current_chapter = state.current_chapter;
    save.data.current_room    = state.current_room;
    save.data.run_time        = state.run_time;
    save.data.character_type  = (player.character == CharacterType::Elias) ? 0 : 1;
    save.data.faith           = faith.current;
    save.data.chapter_seed    = chapter_seed;

    // Run loadout: items are stored by id and re-applied on load (stats come back with them)
    save.data.item_count = 0;
    for (u32 id : player.items) {
        if (save.data.item_count >= static_cast<int>(sizeof(save.data.item_ids) / sizeof(u32))) break;
        save.data.item_ids[save.data.item_count++] = id;
    }
    save.data.consumable_id = player.consumable_slot;
    save.data.active_slots  = player.active_slot_count;
    for (int s = 0; s < 2; s++)
        save.data.miracle_ids[s] = static_cast<u32>(player.miracle_slots[s].miracle);

    // Read HP from ECS if player entity is valid
    if (player.entity != ENTITY_NONE &&
        world.has(player.entity, Component::Health)) {
        save.data.hp = world.healths[player.entity].current;
    }
}

void Engine::continue_run() {
    if (!save.load()) {
        SDL_Log("Failed to load save");
        return;
    }

    SDL_Log("Continuing run - Act %d, Chapter %d, Room %d",
            save.data.current_act, save.data.current_chapter, save.data.current_room);

    state.current_act     = (save.data.current_act == 1) ? Act::Act_I : Act::Act_II;
    state.current_chapter = save.data.current_chapter;
    state.current_room    = save.data.current_room;
    state.run_time        = save.data.run_time;
    run_kills = 0;

    world.clear();
    world.tileset = tileset_texture;
    faith.init();
    faith.current = save.data.faith;
    faith.modify(0.0f);

    // Spawn the correct character
    player = Player{};
    if (save.data.character_type == 0) {
        player.init_as_elias(world, character_textures[static_cast<u32>(CharacterType::Elias)]);
    } else {
        player.init_as_eliseo(world, character_textures[static_cast<u32>(CharacterType::Eliseo)]);
    }

    // Same chapter layout as when the run was saved (the seed is stored), then jump straight
    // to the saved room. Room contents are regenerated: cleared rooms have their enemies back.
    pending_seed = save.data.chapter_seed;
    start_chapter(state.current_chapter, false);

    // Loadout: re-applying the items restores max HP / damage / speed / cadence
    for (int i = 0; i < save.data.item_count; i++) {
        const ItemDef* d = item_registry.get(save.data.item_ids[i]);
        if (d) player.grant_item(world, faith, *d);
    }
    player.consumable_slot = save.data.consumable_id;
    player.active_slot_count = std::max(1, std::min(2, save.data.active_slots));
    for (int s = 0; s < 2; s++) {
        if (save.data.miracle_ids[s] < static_cast<u32>(MiracleID::COUNT))
            player.miracle_slots[s].equip(static_cast<MiracleID>(save.data.miracle_ids[s]));
    }

    // Restore HP/faith after the chapter-start heal and the item bonuses
    if (player.entity != ENTITY_NONE &&
        world.has(player.entity, Component::Health)) {
        world.healths[player.entity].current = std::min(save.data.hp, world.healths[player.entity].max);
    }
    faith.current = save.data.faith;
    faith.modify(0.0f);

    if (save.data.current_room >= 0 && save.data.current_room < floor_map.room_count)
        load_room(save.data.current_room);

    change_scene(GameScene::Playing);
}

// =============================================================================
// Room Loading (with persistence)
// =============================================================================
void Engine::load_room(int room_index) {
    if (room_index < 0 || room_index >= floor_map.room_count) {
        SDL_Log("load_room: invalid index %d (max %d)", room_index, floor_map.room_count);
        return;
    }
    room_fade = 1.0f;   // fade in the new room (see Engine::render)
    world.popups.clear();
    world.hitstop = 0.0f;

    // Destroy all non-player entities (enemies, projectiles, items from previous room)
    for (u32 e = 0; e < MAX_ENTITIES; e++) {
        if (static_cast<Entity>(e) != world.player && world.masks[e].any()) {
            world.destroy(static_cast<Entity>(e));
        }
    }
    boss_entity = ENTITY_NONE;

    floor_map.current_room = room_index;
    state.current_room = room_index;

    auto& node = floor_map.rooms[room_index];

    if (floor_map.room_generated[room_index]) {
        // Restore from cache
        current_room_data = floor_map.room_cache[room_index];
    } else {
        // First visit: generate and cache
        current_room_data.door_north = (node.connections[0] >= 0);
        current_room_data.door_south = (node.connections[1] >= 0);
        current_room_data.door_east  = (node.connections[2] >= 0);
        current_room_data.door_west  = (node.connections[3] >= 0);

        int difficulty = (state.current_chapter - 1) % Config::CHAPTERS_ACT_I + 1;   // 1..5 per act
        current_room_data.generate(node.type, state.current_chapter, difficulty);

        floor_map.room_cache[room_index] = current_room_data;
        floor_map.room_generated[room_index] = true;
    }

    current_room_data.visited = true;
    player.took_damage_this_room = false;

    // Link room to ECS for tile collision
    world.active_room = &current_room_data;

    // Position player based on entry direction (inside the opening, just past the border)
    if (player.entity != ENTITY_NONE &&
        world.has(player.entity, Component::Transform)) {

        f32 cx = static_cast<f32>(Config::GAME_WIDTH) * 0.5f;
        f32 cy = static_cast<f32>(Config::ROOM_TILES_Y * Config::TILE_SIZE) * 0.5f;

        Vec2 spawn_pos;
        switch (entry_direction) {
            case Direction::Up:    spawn_pos = {cx, 26.0f}; break;                                     // came from the room above
            case Direction::Down:  spawn_pos = {cx, Config::ROOM_TILES_Y * Config::TILE_SIZE - 26.0f}; break;
            case Direction::Left:  spawn_pos = {26.0f, cy}; break;
            case Direction::Right: spawn_pos = {static_cast<f32>(Config::GAME_WIDTH) - 26.0f, cy}; break;
            default:               spawn_pos = current_room_data.tile_to_world(Config::ROOM_TILES_X / 2, Config::ROOM_TILES_Y / 2); break;
        }
        // Never spawn with the FEET inside a solid tile (same reference point as movement)
        Vec2 foot = world.has(player.entity, Component::Collider) ? world.colliders[player.entity].offset : Vec2{0, 0};
        Vec2 free_feet;
        if (!current_room_data.is_walkable_world(spawn_pos + foot) &&
            current_room_data.nearest_walkable_world(spawn_pos + foot, free_feet)) {
            spawn_pos = free_feet - foot;
        }
        world.transforms[player.entity].position = spawn_pos;
        world.movements[player.entity].knockback = {0, 0};
        world.movements[player.entity].velocity = {0, 0};
    }

    // Enemies / boss (only if the room is not already cleared)
    if (!current_room_data.cleared) {
        if (node.type == RoomType::Boss) {
            auto def = get_chapter_def(state.current_chapter);
            Vec2 pos = current_room_data.tile_to_world(Config::ROOM_TILES_X / 2, 3);
            boss_entity = spawn_boss(world, def.boss_type, pos,
                                     enemy_textures[static_cast<u32>(def.boss_type)],
                                     state.current_chapter, def.boss_name);
            char buf[64];
            snprintf(buf, sizeof(buf), "%s", def.boss_name);
            show_banner("JEFE", buf, 2.5f);
            audio.play_sfx(SfxID::CriticalHit, 0.9f);
        } else {
            spawn_room_enemies();
        }
    }
    current_room_data.exits_locked = is_combat_room(node.type) && !current_room_data.cleared &&
                                     world.count_living_enemies() > 0;
    if (current_room_data.exits_locked && node.type != RoomType::Boss && lock_hints_shown < 3) {
        lock_hints_shown++;
        show_message("Salidas selladas: derrota a todos los enemigos", 2.5f);
    }

    // ASCII dump of the layout (debug aid for autoplay sessions)
    if (autoplay.active) {
        static const char glyph[] = ".#OW^v><BGCTbRg~PFXEt";   // indexed by TileType
        for (int y = 0; y < Config::ROOM_TILES_Y; y++) {
            char line[Config::ROOM_TILES_X + 1];
            for (int x = 0; x < Config::ROOM_TILES_X; x++) {
                u8 t = static_cast<u8>(current_room_data.tiles[y][x]);
                line[x] = t < sizeof(glyph) - 1 ? glyph[t] : '?';
            }
            line[Config::ROOM_TILES_X] = 0;
            SDL_Log("  %s", line);
        }
    }

    auto def = get_chapter_def(state.current_chapter);
    SDL_Log("Loaded room %d [%d,%d] (%s) - %s [%d enemies]%s",
            room_index, node.grid_x, node.grid_y, def.name,
            node.type == RoomType::Start ? "Start" :
            node.type == RoomType::Boss  ? "Boss" :
            node.type == RoomType::Normal ? "Normal" :
            node.type == RoomType::Elite ? "Elite" :
            node.type == RoomType::Treasure ? "Treasure" :
            node.type == RoomType::Altar ? "Altar" : "Special",
            static_cast<int>(current_room_data.enemy_spawns.size()),
            current_room_data.cleared ? " (cleared)" : "");
}

// =============================================================================
// Room Transition Detection
// =============================================================================
void Engine::try_room_transition() {
    if (player.entity == ENTITY_NONE ||
        !world.has(player.entity, Component::Transform)) return;

    auto& pos = world.transforms[player.entity].position;
    auto& node = floor_map.rooms[floor_map.current_room];
    const f32 room_h = static_cast<f32>(Config::ROOM_TILES_Y * Config::TILE_SIZE);

    Direction move_dir = Direction::None;
    int next_room = -1;

    if (pos.x < 0.0f) {
        move_dir = Direction::Left;
        next_room = node.connections[3];  // West
    } else if (pos.x >= static_cast<f32>(Config::GAME_WIDTH)) {
        move_dir = Direction::Right;
        next_room = node.connections[2];  // East
    } else if (pos.y < 0.0f) {
        move_dir = Direction::Up;
        next_room = node.connections[0];  // North
    } else if (pos.y >= room_h) {
        move_dir = Direction::Down;
        next_room = node.connections[1];  // South
    }

    if (move_dir == Direction::None) return;

    if (next_room >= 0 && !current_room_data.exits_locked) {
        // Persist this room's full state (cleared flags, smashed altars, opened chests...)
        floor_map.room_cache[floor_map.current_room] = current_room_data;

        // Entry direction is the opposite side: if player moved right, they enter from the left
        switch (move_dir) {
            case Direction::Up:    entry_direction = Direction::Down;  break;
            case Direction::Down:  entry_direction = Direction::Up;    break;
            case Direction::Left:  entry_direction = Direction::Right; break;
            case Direction::Right: entry_direction = Direction::Left;  break;
            default: break;
        }

        SDL_Log("Room transition: %s -> room %d",
                move_dir == Direction::Up    ? "North" :
                move_dir == Direction::Down  ? "South" :
                move_dir == Direction::Left  ? "West"  : "East",
                next_room);

        load_room(next_room);
    } else {
        // No connection or sealed — clamp inside (if this lands the feet on a solid border
        // tile, system_movement's unstick() snaps to the nearest walkable tile next frame)
        pos.x = std::max(1.0f, std::min(pos.x, static_cast<f32>(Config::GAME_WIDTH) - 1.0f));
        pos.y = std::max(1.0f, std::min(pos.y, room_h - 1.0f));
    }
}

// =============================================================================
// Room state: clear detection, boss defeat, tile interactions
// =============================================================================
void Engine::update_room_state() {
    Room& room = current_room_data;
    auto& node = floor_map.rooms[floor_map.current_room];

    if (is_combat_room(node.type) && !room.cleared) {
        int alive = world.count_living_enemies();
        if (alive == 0) {
            room.cleared = true;
            room.exits_locked = false;
            player.rooms_cleared++;

            if (node.type == RoomType::Boss) {
                room.place_exit();
                boss_entity = ENTITY_NONE;
                show_banner("JEFE DERROTADO", "Cruza el arco dorado para continuar", 4.0f);
                audio.play_sfx(SfxID::LevelUp);
            } else if (!player.took_damage_this_room) {
                faith.on_room_no_damage();
                show_message("Sala despejada sin dano  +2 Fe", 2.5f);
            } else {
                show_message("Sala despejada", 1.5f);
            }
            if (node.type != RoomType::Boss) audio.play_sfx(SfxID::DoorOpen, 0.8f);
            if (node.type == RoomType::Elite) {
                // Elite reward: a small offering pile at the centre
                Vec2 c = room.tile_to_world(Config::ROOM_TILES_X / 2, Config::ROOM_TILES_Y / 2);
                spawn_pickup(world, c + Vec2{-12, 0}, ItemComponent::Pickup::Heart, 15.0f);
                spawn_pickup(world, c + Vec2{12, 0}, ItemComponent::Pickup::FaithOrb, 8.0f);
                spawn_pickup(world, c + Vec2{0, 12}, ItemComponent::Pickup::Heart, 15.0f);
            }
            floor_map.room_cache[floor_map.current_room] = room;

            // Auto-save on every cleared room (GDD 4.4: mobile expects it, and it makes
            // "Continuar" land on the room the player actually reached)
            populate_save_data();
            save.save();
            menus.has_continue = true;

            // Core loop step 4: choose an upgrade. Elite/Boss always, normal rooms 1 in 5 —
            // one card per room would hand out all 13 items well before the first chapter ends
            // (bot runs reached chapter 10 carrying 18 items and never dropped below 70% HP).
            if (node.type == RoomType::Elite)      open_item_choice("ELITE DERROTADA");
            else if (node.type == RoomType::Boss)  open_item_choice("BOTIN DEL JEFE");
            else if (randi_range(0, 4) == 0)       open_item_choice("ELIGE UNA BENDICION");
        }
    }

    // Tile interactions at the player's feet
    if (player.entity == ENTITY_NONE || !world.has(player.entity, Component::Transform)) return;
    Vec2 feet = world.transforms[player.entity].position + Vec2{0, 6};
    int tx = static_cast<int>(feet.x) / Config::TILE_SIZE;
    int ty = static_cast<int>(feet.y) / Config::TILE_SIZE;
    if (tx < 0 || tx >= Config::ROOM_TILES_X || ty < 0 || ty >= Config::ROOM_TILES_Y) return;

    // Chapter exit: stepping anywhere on the arch patch (3x3 around the Exit tile)
    if (room.exit_open) {
        Vec2 exit_pos = room.tile_to_world(room.exit_tx, room.exit_ty);
        Vec2 center = world.transforms[player.entity].position;
        if (std::abs(center.x - exit_pos.x) < 20.0f && std::abs(feet.y - exit_pos.y) < 20.0f) {
            floor_map.room_cache[floor_map.current_room] = room;
            advance_chapter();
            return;
        }
    }

    TileType t = room.tiles[ty][tx];
    switch (t) {
        case TileType::AltarGod:
            if (!room.altar_used) {
                room.altar_used = true;
                faith.on_pray();
                if (world.has(player.entity, Component::Health)) world.healths[player.entity].heal(10.0f);
                floor_map.room_cache[floor_map.current_room] = room;
                // Praying is where new miracles are learned (GDD: miracles come from God)
                open_miracle_choice("UN NUEVO MILAGRO");
            }
            break;
        case TileType::Chest:
            if (!room.chest_opened) {
                room.chest_opened = true;
                room.tiles[ty][tx] = TileType::Path;
                if (world.has(player.entity, Component::Health)) world.healths[player.entity].heal(15.0f);
                faith.modify(5.0f);
                floor_map.room_cache[floor_map.current_room] = room;
                open_item_choice("EL COFRE SE ABRE");
            }
            break;
        default:
            break;
    }
}

// =============================================================================
// "Elige una bendicion": core loop step 4 (GDD 1.4)
// =============================================================================
void Engine::open_item_choice(const char* title) {
    std::vector<u32> ids = item_registry.roll_choices(player.items, 3);
    if (ids.empty()) return;
    choice.count = static_cast<int>(ids.size());
    for (int i = 0; i < choice.count; i++) { choice.ids[i] = ids[i]; choice.extras[i].clear(); }
    choice.miracles = false;
    choice.selected = 0;
    choice.title = title;
    // Scripted playtests must not stall on the card screen: the bot takes one on its own
    choice.auto_timer = autoplay.active ? 0.7f : 0.0f;
    audio.play_sfx(SfxID::LevelUp, 0.7f);
    change_scene(GameScene::Choice);
}

// Miracles are offered at prayer altars: three the prophet does not carry yet. Eliseo can
// also be offered Elias' miracles (inherited, cast at 70% power — see Player::cast_miracle).
void Engine::open_miracle_choice(const char* title) {
    std::vector<u32> pool;
    for (u32 m = 0; m < static_cast<u32>(MiracleID::COUNT); m++) {
        MiracleID id = static_cast<MiracleID>(m);
        Prophet owner = get_miracle_def(id).owner;
        bool mine = (player.character == CharacterType::Elias) ? (owner == Prophet::Elias)
                                                               : true;   // Eliseo: his own + inherited
        if (!mine) continue;
        bool equipped = false;
        for (int s = 0; s < player.active_slot_count; s++)
            if (player.miracle_slots[s].miracle == id) equipped = true;
        if (!equipped) pool.push_back(m);
    }
    if (pool.empty()) return;

    choice.count = 0;
    choice.miracles = true;
    // Which slot a new miracle would take: first empty one, else the last active slot
    int target = player.active_slot_count - 1;
    for (int s = 0; s < player.active_slot_count; s++)
        if (player.miracle_slots[s].is_empty()) { target = s; break; }

    for (int i = 0; i < 3 && !pool.empty(); i++) {
        int k = randi_range(0, static_cast<int>(pool.size()) - 1);
        choice.ids[choice.count] = pool[k];
        pool.erase(pool.begin() + k);
        const MiracleSlot& slot = player.miracle_slots[target];
        choice.extras[choice.count] = slot.is_empty()
            ? std::string("SLOT ") + (target == 0 ? "Q" : "E") + " LIBRE"
            : std::string("SUSTITUYE A ") + get_miracle_def(slot.miracle).name;
        choice.count++;
    }
    choice.selected = 0;
    choice.title = title;
    choice.auto_timer = autoplay.active ? 0.7f : 0.0f;
    audio.play_sfx(SfxID::FaithGain, 0.8f);
    change_scene(GameScene::Choice);
}

void Engine::update_item_choice(f32 dt) {
    if (choice.count <= 0) { change_scene(GameScene::Playing); return; }

    bool take = false;
    if (choice.auto_timer > 0.0f) {
        choice.auto_timer -= dt;
        if (choice.auto_timer <= 0.0f) {
            choice.selected = randi_range(0, choice.count - 1);
            take = true;
        }
    }

    if (input.is_action_just_pressed(Action::MoveLeft)) {
        choice.selected = (choice.selected - 1 + choice.count) % choice.count;
        audio.play_sfx(SfxID::MenuSelect, 0.6f);
    }
    if (input.is_action_just_pressed(Action::MoveRight)) {
        choice.selected = (choice.selected + 1) % choice.count;
        audio.play_sfx(SfxID::MenuSelect, 0.6f);
    }

    if (take || input.is_action_just_pressed(Action::Confirm) ||
        input.is_action_just_pressed(Action::Attack)) {
        char buf[128];
        if (choice.miracles) {
            MiracleID id = static_cast<MiracleID>(choice.ids[choice.selected]);
            int target = player.active_slot_count - 1;
            for (int s = 0; s < player.active_slot_count; s++)
                if (player.miracle_slots[s].is_empty()) { target = s; break; }
            player.miracle_slots[target].equip(id);
            MiracleDef d = get_miracle_def(id);
            SDL_Log("Miracle equipped in slot %d: %s (tier %d, %.0f faith)", target, d.name, d.tier, d.faith_cost);
            snprintf(buf, sizeof(buf), "%s equipado en [%s]", d.name, target == 0 ? "Q" : "E");
            show_message(buf, 3.0f);
            audio.play_sfx(SfxID::MenuConfirm, 0.9f);
        } else {
            const ItemDef* def = item_registry.get(choice.ids[choice.selected]);
            if (def) {
                player.grant_item(world, faith, *def);
                SDL_Log("Item taken: %s (%s)", def->name, def->description);
                snprintf(buf, sizeof(buf), "%s: %s", def->name, def->description);
                show_message(buf, 3.0f);
                audio.play_sfx(SfxID::MenuConfirm, 0.9f);
            }
        }
        choice.count = 0;
        change_scene(GameScene::Playing);
    }
}

// =============================================================================
// Enemy Spawning (from room data)
// =============================================================================
void Engine::spawn_room_enemies() {
    for (auto& spawn : current_room_data.enemy_spawns) {
        Entity e = spawn_enemy(world, spawn.type, spawn.position,
                               enemy_textures[static_cast<u32>(spawn.type)], state.current_chapter);
        if (e != ENTITY_NONE) {
            SDL_Log("Spawned enemy '%s' at (%.0f, %.0f)",
                    get_enemy_def(spawn.type).name, spawn.position.x, spawn.position.y);
        }
    }
}

void Engine::update_settings() {
    menus.update(input);  // Up/Down navigation

    // Row 2: auto-aim toggle (GDD 4.3 — always available, on by default on mobile)
    if (menus.selected_option == 2 &&
        (input.is_action_just_pressed(Action::MoveLeft) || input.is_action_just_pressed(Action::MoveRight) ||
         input.is_action_just_pressed(Action::Confirm))) {
        menus.settings_auto_aim = !menus.settings_auto_aim;
        input.auto_aim_enabled = menus.settings_auto_aim;
        audio.play_sfx(SfxID::MenuSelect, 0.7f);
    }

    // Left/Right adjust volume on rows 0 and 1
    if (menus.selected_option < 2) {
        f32* vol = (menus.selected_option == 0) ? &menus.settings_music_vol
                                                 : &menus.settings_sfx_vol;
        bool changed = false;

        if (input.is_action_just_pressed(Action::MoveLeft)) {
            *vol -= 0.1f;
            if (*vol < 0.0f) *vol = 0.0f;
            changed = true;
        }
        if (input.is_action_just_pressed(Action::MoveRight)) {
            *vol += 0.1f;
            if (*vol > 1.0f) *vol = 1.0f;
            changed = true;
        }

        if (changed) {
            if (menus.selected_option == 0) {
                audio.set_music_volume(*vol);
            } else {
                audio.set_sfx_volume(*vol);
            }
        }
    }

    // Confirm on "Volver" (last option) or Cancel -> return to origin
    bool back = (input.is_action_just_pressed(Action::Confirm) && menus.selected_option == 3)
             || input.is_action_just_pressed(Action::Cancel);

    if (back) {
        menus.current = menus.settings_origin;
        menus.selected_option = 0;
    }
}

// =============================================================================
// Debug: screenshots + autoplay
// =============================================================================
void Engine::take_screenshot(const char* name) {
    char path[512];
    if (name && *name) {
        snprintf(path, sizeof(path), "%s/%s.png", screenshot_dir.c_str(), name);
    } else {
        snprintf(path, sizeof(path), "%s/shot_%03d.png", screenshot_dir.c_str(), screenshot_counter++);
    }
    SDL_CreateDirectory(screenshot_dir.c_str());
    renderer.save_screenshot(path, 1);
}

bool Autoplay::load(const char* path) {
    std::ifstream f(path);
    if (!f) {
        SDL_Log("Autoplay: cannot open '%s'", path);
        return false;
    }
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        AutoplayCmd c;
        ss >> c.op;
        if (c.op == "wait")       { ss >> c.num; }
        else if (c.op == "hold")  { ss >> c.arg >> c.num; }
        else if (c.op == "press") { ss >> c.arg; }
        else if (c.op == "aim")   { ss >> c.num >> c.num2; }
        else if (c.op == "shot")  { ss >> c.arg; }
        else if (c.op == "goto")  { ss >> c.num; }
        else if (c.op == "bot")   { ss >> c.num; }
        else if (c.op == "walk")  { ss >> c.num >> c.num2; }
        else if (c.op == "seed")  { ss >> c.num; }
        else if (c.op == "chapter") { ss >> c.num; }
        else if (c.op == "say")   { std::getline(ss, c.arg); }
        else if (c.op == "stats") { std::getline(ss, c.arg); }
        cmds.push_back(c);
    }
    active = !cmds.empty();
    SDL_Log("Autoplay: %d commands loaded from %s", static_cast<int>(cmds.size()), path);
    return active;
}

static Action action_from_name(const std::string& n) {
    if (n == "up")      return Action::MoveUp;
    if (n == "down")    return Action::MoveDown;
    if (n == "left")    return Action::MoveLeft;
    if (n == "right")   return Action::MoveRight;
    if (n == "attack")  return Action::Attack;
    if (n == "dash")    return Action::Dash;
    if (n == "m1")      return Action::Miracle1;
    if (n == "m2")      return Action::Miracle2;
    if (n == "confirm") return Action::Confirm;
    if (n == "cancel")  return Action::Cancel;
    if (n == "pause")   return Action::Pause;
    if (n == "map")     return Action::Map;
    if (n == "use")     return Action::Consumable;
    return Action::COUNT;
}

void Engine::update_autoplay(f32 dt) {
    Autoplay& ap = autoplay;
    // Actions the script may drive: reset each tick, then re-assert what is held
    static const Action driven[] = {Action::MoveUp, Action::MoveDown, Action::MoveLeft, Action::MoveRight,
                                    Action::Attack, Action::Dash, Action::Miracle1, Action::Miracle2,
                                    Action::Confirm, Action::Cancel, Action::Pause, Action::Map, Action::Consumable};
    for (Action a : driven) input.actions[static_cast<u32>(a)] = false;

    if (ap.timer > 0.0f) {
        ap.timer -= dt;
        if (ap.holding) input.actions[ap.hold_action] = true;
    } else {
        ap.holding = false;
        // Execute commands until one that takes time
        while (ap.index < ap.cmds.size()) {
            const AutoplayCmd& c = ap.cmds[ap.index++];
            if (c.op == "wait") { ap.timer = c.num; break; }
            if (c.op == "hold") {
                Action a = action_from_name(c.arg);
                if (a != Action::COUNT) { ap.holding = true; ap.hold_action = static_cast<u32>(a); ap.timer = c.num; input.actions[ap.hold_action] = true; }
                break;
            }
            if (c.op == "press") {
                Action a = action_from_name(c.arg);
                if (a != Action::COUNT) input.actions[static_cast<u32>(a)] = true;
                ap.timer = 2.0f * Config::FIXED_DT;   // keep it for two ticks so just_pressed registers
                ap.holding = true; ap.hold_action = static_cast<u32>(a);
                break;
            }
            if (c.op == "aim")  { input.aim_axis = {c.num, c.num2}; continue; }
            if (c.op == "shot") { pending_shot_name = c.arg; screenshot_requested = true; ap.timer = Config::FIXED_DT; break; }
            if (c.op == "goto") { load_room(static_cast<int>(c.num)); continue; }
            if (c.op == "bot")  { ap.bot_timer = c.num; ap.timer = c.num; break; }
            if (c.op == "seed") { srand(static_cast<unsigned>(c.num)); continue; }
            if (c.op == "chapter") {   // jump to a chapter (debug: biome / boss checks)
                if (state.scene == GameScene::Playing && player.entity != ENTITY_NONE) {
                    int ch = std::max(1, std::min(static_cast<int>(c.num), Config::CHAPTERS_ACT_I + Config::CHAPTERS_ACT_II));
                    start_chapter(ch, true);
                }
                continue;
            }
            if (c.op == "killall") {
                for (u32 e = 0; e < world.entity_count; e++) {
                    if (!world.is_alive(e) || !world.has(e, Component::EnemyAI) || !world.has(e, Component::Health)) continue;
                    if (!world.healths[e].is_alive()) continue;
                    if (apply_damage(world, ENTITY_NONE, e, 100000.0f)) on_enemy_killed(world, faith, e, combat_events);
                }
                continue;
            }
            if (c.op == "walk") { ap.walk_target = {c.num, c.num2}; ap.timer = 6.0f; ap.walking = true; break; }
            if (c.op == "god")  { god_mode = !god_mode; continue; }
            if (c.op == "stats") {   // balance probe: one line per checkpoint in the log
                f32 hp = 0.0f, hpmax = 1.0f;
                if (player.entity != ENTITY_NONE && world.has(player.entity, Component::Health)) {
                    hp = world.healths[player.entity].current;
                    hpmax = world.healths[player.entity].max;
                }
                SDL_Log("STATS %s | ch %d room %d | hp %.0f/%.0f (%.0f%%) | faith %.0f | kills %d | items %d | t %.1f",
                        c.arg.empty() ? "-" : c.arg.c_str(), state.current_chapter, state.current_room,
                        hp, hpmax, 100.0f * hp / hpmax, faith.current, run_kills,
                        static_cast<int>(player.items.size()), state.run_time);
                continue;
            }
            if (c.op == "quit") { state.is_running = false; break; }
            if (c.op == "say")  { SDL_Log("Autoplay: %s", c.arg.c_str()); continue; }
        }
        if (ap.index >= ap.cmds.size() && ap.timer <= 0.0f) ap.active = false;
    }

    // Recompute movement axis from the driven actions (Input::update_axes ran before us)
    Vec2 mv = {0, 0};
    if (input.actions[static_cast<u32>(Action::MoveLeft)])  mv.x -= 1.0f;
    if (input.actions[static_cast<u32>(Action::MoveRight)]) mv.x += 1.0f;
    if (input.actions[static_cast<u32>(Action::MoveUp)])    mv.y -= 1.0f;
    if (input.actions[static_cast<u32>(Action::MoveDown)])  mv.y += 1.0f;
    input.move_axis = mv.length_sq() > 0.01f ? mv.normalized() : Vec2{0, 0};

    // ---- Bot: kite the nearest enemy while shooting at it ----
    if (ap.bot_timer > 0.0f && state.scene == GameScene::Playing &&
        player.entity != ENTITY_NONE && world.has(player.entity, Component::Transform)) {
        ap.bot_timer -= dt;
        Vec2 ppos = world.transforms[player.entity].position;
        // Same target priority as the mobile auto-aim, so bot runs measure what players get
        Entity nearest = pick_target(world, ppos, 1e9f);
        f32 best = (nearest != ENTITY_NONE) ? world.transforms[nearest].position.distance_to(ppos) : 1e9f;
        if (nearest != ENTITY_NONE) {
            Vec2 to = world.transforms[nearest].position - ppos;
            input.aim_axis = to.normalized();   // bot-only aim override (players shoot where they move)
            input.actions[static_cast<u32>(Action::Attack)] = true;
            // Keep ~70px away, strafe sideways, flip strafe direction every 1.5s
            Vec2 perp = {-to.normalized().y, to.normalized().x};
            f32 side = (static_cast<int>(world.time / 1.5f) % 2 == 0) ? 1.0f : -1.0f;
            Vec2 mvb = perp * side;
            if (best < 55.0f) mvb -= to.normalized();
            else if (best > 95.0f) mvb += to.normalized() * 0.6f;
            // Dodge incoming enemy projectiles: step perpendicular
            for (u32 e = 0; e < world.entity_count; e++) {
                if (!world.is_alive(e) || !world.has(e, Component::Projectile) || world.projectiles[e].friendly) continue;
                Vec2 pp = world.transforms[e].position;
                if (pp.distance_to(ppos) < 40.0f) { mvb += perp * side * 1.5f; }
            }
            input.move_axis = mvb.normalized();
            // Dash when hurt recently and enemy close
            if (best < 30.0f && world.movements[player.entity].dash_cooldown <= 0.0f)
                input.actions[static_cast<u32>(Action::Dash)] = true;
            // Miracles: slot 1 when enemies are close, slot 2 when hurt
            if (best < 60.0f && player.miracle_slots[0].is_ready() && (static_cast<int>(world.time * 10) % 7 == 0))
                input.actions[static_cast<u32>(Action::Miracle1)] = true;
            if (player.active_slot_count > 1 && player.miracle_slots[1].is_ready() &&
                world.healths[player.entity].ratio() < 0.5f && (static_cast<int>(world.time * 10) % 11 == 0))
                input.actions[static_cast<u32>(Action::Miracle2)] = true;
        } else {
            input.actions[static_cast<u32>(Action::Attack)] = false;
            input.move_axis = {0, 0};
            input.aim_axis = {0, 0};
            ap.bot_timer = 0.0f;   // nothing to fight: end the bot phase early
            ap.timer = 0.0f;
        }
    }

    // ---- Walk to a world position (simple steering, no pathfinding) ----
    if (ap.walking && state.scene == GameScene::Playing &&
        player.entity != ENTITY_NONE && world.has(player.entity, Component::Transform)) {
        Vec2 ppos = world.transforms[player.entity].position;
        Vec2 to = ap.walk_target - ppos;
        if (to.length() < 4.0f || ap.timer <= 0.0f) { ap.walking = false; ap.timer = 0.0f; input.move_axis = {0, 0}; }
        else input.move_axis = to.normalized();
    }
}
