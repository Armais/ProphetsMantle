// =============================================================================
// MANTLE - Engine Core Implementation
// =============================================================================

#include "core/engine.h"
#include "core/i18n.h"
#include "render/sprites.h"
#include "game/scenes.h"
#include "game/ambience.h"
#include "platform/platform.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

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
        g_engine->input.consume_presses();   // tras cada paso: ver el bucle de escritorio
    }

    g_engine->render();
    g_engine->frame_count++;
}
#endif

// =============================================================================
// Init
// =============================================================================
bool Engine::init(const char* title) {
#if defined(MANTLE_PLATFORM_MOBILE)
    // Horizontal forzado (decision 2026-09-11). SDL lo traduce a
    // SCREEN_ORIENTATION_USER_LANDSCAPE al crear la ventana; el manifest ademas nace con
    // screenOrientation="sensorLandscape" para que no haya un giro visible al arrancar.
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
#endif

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

    if (forced_win_w > 0 && forced_win_h > 0) {
        win_w = forced_win_w;
        win_h = forced_win_h;
        flags = SDL_WINDOW_RESIZABLE;
    }

    window = SDL_CreateWindow(title, win_w, win_h, flags);
    if (!window) {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        return false;
    }

    // Icono de la ventana (barra de tareas y alt-tab). El icono del FICHERO .exe no sale de
    // aqui sino de mantle.rc, que solo existe en Windows; esto es lo que cubre Linux y Mac,
    // y ademas es lo que se ve mientras el juego corre. Si falta el PNG no pasa nada: se
    // sigue arrancando con el icono por defecto.
    {
        std::string icon_path = std::string(get_asset_path()) + "branding/mark_256.png";
        if (SDL_Surface* icon = IMG_Load(icon_path.c_str())) {
            SDL_SetWindowIcon(window, icon);
            SDL_DestroySurface(icon);
        } else {
            SDL_Log("Window icon not loaded ('%s'): %s", icon_path.c_str(), SDL_GetError());
        }
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
    init_hazard_art(renderer, hazard_art, tileset_texture);

    input.init();
    // El reparto de los botones tactiles depende de cuanta banda negra deje el letterbox,
    // y eso solo se sabe con la ventana ya creada.
    input.init_touch_layout(renderer.bottom_band_units());
    input.set_touch_viewport(renderer.viewport_norm());

    if (!audio.init()) {
        SDL_Log("Failed to init audio (continuing without sound)");
    } else {
        audio.load_all_music();
        audio.load_all_sfx();
    }

    world.init();
    world.tileset = tileset_texture;
    world.disc_tex = renderer.disc_tex;
    faith.init();
    save.init();          // lee el fichero: la meta-progresion tiene que estar viva ya
    apply_settings_from_save();   // idioma, volumenes, mira, sensibilidad y vibracion
    // PRIMER ARRANQUE: antes del titulo se pregunta el idioma. El arranque no pasa por
    // change_scene (la escena inicial es MainMenu por defecto), asi que la pantalla se
    // elige aqui; a partir de ahi `change_scene` siempre devuelve al Title.
    if (!(save.data.settings_flags & SETTING_LANG_CHOSEN)) {
        menus.current = MenuScreen::Language;
        menus.selected_option = (save.data.language == 1) ? 1 : 0;
    }
    item_registry.init();
    progression.init();
    sync_progression_from_save();
    mobile_rules = is_mobile();
    refresh_continue_state();

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
    // Acab y Jezabel de pie en el escenario de las escenas guionizadas (start_scene)
    acab_texture    = load_sheet("sprites/acab.png");
    jezabel_texture = load_sheet("sprites/jezabel.png");

    // ECSWorld::enemy_textures tiene 8 huecos fijos: un noveno EnemyType escribiria fuera
    static_assert(static_cast<u32>(EnemyType::COUNT) <= 8,
                  "amplia ECSWorld::enemy_textures antes de anadir un EnemyType");
    for (u32 t = 0; t < static_cast<u32>(EnemyType::COUNT); t++) {
        enemy_textures[t] = load_sheet(get_enemy_def(static_cast<EnemyType>(t)).sprite_path);
        world.enemy_textures[t] = enemy_textures[t];   // los jefes invocan sin pasar por el Engine
    }

    // Atlas a 8 texeles por unidad con margen extruido (rooms.h TILESET_GUTTER): va en LINEAL
    tileset_texture = renderer.load_texture("sprites/tiles.png", /*smooth=*/true);
    if (tileset_texture == TEXTURE_NONE) {
        SDL_Log("WARNING: tileset missing (assets/sprites/tiles.png) - using placeholder tiles");
    }
    // Nubes, luz y motas: texturas procedurales (game/ambience.h)
    ambience_init(renderer);

    // Hoja de efectos propia. Va con filtrado SUAVE (al reves que el tileset): sus celdas
    // son halos degradados, y el vecino mas proximo los convierte en escalones. El sangrado
    // entre celdas no molesta porque cada fotograma acaba en transparente por los bordes.
    vfx_texture = renderer.load_texture("sprites/vfx.png", /*smooth=*/true);
    world.vfx_sheet = vfx_texture;
    if (vfx_texture == TEXTURE_NONE)
        SDL_Log("WARNING: hoja de efectos ausente (assets/sprites/vfx.png) - spawn_fx no dibujara");

    // Iconos propios de cada objeto y milagro (namespace Icon). Filtrado SUAVE: se pintan de
    // 9 a 34 unidades y el vecino mas proximo los escalonaria; las celdas llevan margen
    // transparente, asi que el sangrado entre celdas no se ve. Sin la hoja, el HUD vuelve a
    // los iconos genericos del tileset.
    icons_texture = renderer.load_texture("sprites/icons.png", /*smooth=*/true);
    set_icon_sheet(icons_texture);
    if (icons_texture == TEXTURE_NONE)
        SDL_Log("Iconos ausentes (assets/sprites/icons.png) - HUD con los iconos del tileset");

    // El Carro de Fuego del milagro (tools/blender/mantle_sprites/chariot.py). Sin la hoja,
    // el milagro se ve como antes: la embestida y su estela de fuego.
    chariot_texture = renderer.load_texture("sprites/chariot.png", /*smooth=*/true);
    if (chariot_texture == TEXTURE_NONE)
        SDL_Log("Carro ausente (assets/sprites/chariot.png) - el Carro de Fuego ira sin carro");

    // Retratos del Bestiario. Su ausencia NO es un fallo: el Códice se dibuja igual.
    portrait_texture = renderer.load_texture("sprites/portraits.png", /*smooth=*/true);
    if (portrait_texture == TEXTURE_NONE)
        SDL_Log("Retratos ausentes (assets/sprites/portraits.png) - el Bestiario ira sin ellos");

    // PNJ de los encuentros. Su ausencia tampoco es un fallo: la sala marca el sitio con
    // una hoguera, que es lo que hacia antes de que existiera la hoja.
    npc_texture = renderer.load_texture("sprites/npcs.png", /*smooth=*/true);
    if (npc_texture == TEXTURE_NONE)
        SDL_Log("PNJ ausentes (assets/sprites/npcs.png) - los encuentros iran con hoguera");

    // Fondo del menu principal (Elias, Eliseo y el manto). Su ausencia no es un fallo:
    // el titulo conserva su degradado de siempre.
    menus.title_bg = renderer.load_texture("sprites/menu_bg.png", /*smooth=*/true);
    if (menus.title_bg == TEXTURE_NONE)
        SDL_Log("Fondo de menu ausente (assets/sprites/menu_bg.png) - titulo sin estampa");
}

// Hojas propias del jefe del capitulo (rooms.h get_boss_sheet). Se mira ANTES si el
// fichero existe: `load_texture` loguea "Failed to load" con cada intento, y eso es un
// gate negativo de varios guiones (escenas_check, audio_check). Una hoja que aun no se ha
// entregado no es un fallo: una linea de log y la del enemigo base, como hasta ahora.
void Engine::load_boss_sheets(int chapter) {
    const EnemyType base = get_chapter_def(chapter).boss_type;
    auto load_one = [this](const char* path, u32 fallback, u32& out, bool& own, const char* who) {
        out = fallback;
        own = false;
        if (!path) return;
        auto it = renderer.texture_map.find(path);
        if (it != renderer.texture_map.end() && renderer.get_texture(it->second) &&
            renderer.get_texture(it->second)->texture) {
            out = it->second;
            own = true;
            return;
        }
        const std::string full = std::string(get_asset_path()) + path;
        SDL_IOStream* io = SDL_IOFromFile(full.c_str(), "rb");
        if (!io) {
            SDL_Log("Boss sheet ausente: %s -> %s usa la hoja del enemigo base", path, who);
            return;
        }
        SDL_CloseIO(io);
        const u32 id = renderer.load_texture(path);
        if (id == TEXTURE_NONE) return;
        TextureEntry* tex = renderer.get_texture(id);
        if (tex && (tex->width != SHEET_WIDTH || tex->height != SHEET_HEIGHT))
            SDL_Log("WARNING: sprite sheet '%s' is %dx%d, expected %dx%d",
                    path, tex->width, tex->height, SHEET_WIDTH, SHEET_HEIGHT);
        out = id;
        own = true;
    };
    // Cap. 5: Acab y Jezabel tienen hoja desde la audiencia; el resto, la del jefe
    const u32 base_tex = enemy_textures[static_cast<u32>(base)];
    load_one(get_boss_sheet(chapter, 0), (chapter == 5) ? enemy_textures[static_cast<u32>(EnemyType::SoldadoDeAcab)] : base_tex,
             boss_sheets.main, boss_sheets.main_own, get_chapter_def(chapter).boss_name);
    load_one(get_boss_sheet(chapter, 1), enemy_textures[static_cast<u32>(EnemyType::ProfetaDeBaal)],
             boss_sheets.partner, boss_sheets.partner_own, "Jezabel");
    SDL_Log("Boss sheets cap %d: %s%s", chapter, boss_sheets.main_own ? "propia" : "prestada",
            get_boss_sheet(chapter, 1) ? (boss_sheets.partner_own ? " + pareja propia" : " + pareja prestada") : "");
}

// =============================================================================
// Shutdown
// =============================================================================
void Engine::shutdown() {
    input.shutdown();
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

        // Fixed timestep accumulator. Una pulsacion = una accion: tras CADA paso se
        // consumen las teclas, o un frame lento (cargar un capitulo) daria varios pasos y
        // el mismo ENTER pasaria varias paginas de historia de golpe. Tras cada paso y no
        // solo tras el primero: las pulsaciones del autoplay (`press`, dos ticks) se
        // inyectan DENTRO de un paso, y si ese no era el primero del frame el siguiente
        // paso las veia como recien pulsadas otra vez — en la audiencia con Acab, a 15-20
        // fps, cada `press confirm` pasaba dos o tres paginas (2026-09-09). Para el teclado
        // real no cambia nada: sus eventos entran antes del primer paso.
        accumulator += frame_time;
        while (accumulator >= Config::FIXED_DT) {
            update(Config::FIXED_DT);
            accumulator -= Config::FIXED_DT;
            input.consume_presses();
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
                // Girar el telefono cambia cuanta banda negra hay, y con ella si los
                // botones tactiles caben fuera del mundo o no.
                input.init_touch_layout(renderer.bottom_band_units());
                input.set_touch_viewport(renderer.viewport_norm());
                break;

            case SDL_EVENT_KEY_DOWN:
                if (event.key.scancode == SDL_SCANCODE_F12 && !event.key.repeat) {
                    screenshot_requested = true;
                }
                input.process_event(event);
                break;

            // --- Ciclo de vida (Android / iOS) ---------------------------------
            // El sistema puede matar la app sin avisar otra vez, asi que aqui se guarda
            // SIEMPRE: es el ultimo momento garantizado. SDL ya pausa el audio y bloquea
            // el bucle por su cuenta, de eso no hay que ocuparse.
            case SDL_EVENT_WILL_ENTER_BACKGROUND:
            case SDL_EVENT_TERMINATING:
                SDL_Log("Lifecycle: entering background (scene %d)", static_cast<int>(state.scene));
                if (state.scene == GameScene::Playing) change_scene(GameScene::Paused);
                if (state.scene == GameScene::Paused) {
                    populate_save_data();
                    save.save();
                }
                // Android no entrega los FINGER_UP al irse: sin esto el profeta volvia
                // corriendo y disparando.
                input.release_touch();
                break;

            case SDL_EVENT_DID_ENTER_FOREGROUND:
                // El reloj ha seguido corriendo con la app parada: sin rearmarlo, el primer
                // frame de vuelta traeria un dt enorme.
                last_time = SDL_GetPerformanceCounter();
                input.release_touch();
                SDL_Log("Lifecycle: back in foreground");
                break;

            case SDL_EVENT_LOW_MEMORY:
                SDL_Log("Lifecycle: LOW MEMORY warning from the system");
                break;

            default:
                input.process_event(event);
                break;
        }
    }

    input.post_update();
    input.mouse_game = renderer.window_to_game(input.mouse_pos);
}

// Un toque en una pantalla de escena (prologo, intro de capitulo, carta de historia):
// sobre el boton de SALTAR emite Cancel —cada sistema ya sabe saltarse entero con el—, en
// cualquier otro sitio avanza. Una sola formula para las tres pantallas que lo llevan.
static void tap_skip_or_advance(Input& input) {
    Vec2 tp;
    if (!input.take_tap(tp)) return;
    input.pulse(input.touch_ui && skip_button_rect().contains(tp) ? Action::Cancel
                                                                 : Action::Confirm);
}

// =============================================================================
// Update (called at fixed timestep)
// =============================================================================
void Engine::update(f32 dt) {
    if (autoplay.active) update_autoplay(dt);
    // Solo se juega en `Playing`: fuera de ahi el dedo no mueve al profeta ni dispara,
    // solo deja taps para los menus.
    // Durante la entrada del jefe el profeta no lo mueve el dedo, asi que no se dibuja ni
    // se atiende ningun control tactil: un stick que no responde es peor que ninguno.
    input.touch_gameplay = (state.scene == GameScene::Playing && boss_approach_t <= 0.0f);
    menus.touch_hints = input.touch_ui;   // las lineas de ayuda hablan de teclas o de dedos
    menus.in_tutorial = tutorial.active;  // la ultima fila de la pausa es la salida del tutorial
    // Lo mismo para las dos pantallas de texto, que ademas pintan el boton de SALTAR:
    // sin esto anunciaban [ENTER] y [ESC] en un telefono.
    cinematic.touch_ui = input.touch_ui;
    story.touch_ui     = input.touch_ui;
    if (input.touch_ui) {
        if (input.touch_gameplay) {
            // Botones contextuales. Las condiciones son las MISMAS que usa el gameplay:
            // ORAR repite la del "Altar Portatil" (mejora 5), ARMA la del arma equipada.
            input.set_touch_enabled(player.belt_count() > 0,
                                    player.active_slot_count,
                                    player.weapon_slot != UINT32_MAX,
                                    prayers_left > 0,
                                    true);
            input.touch_minimap = HUD::minimap_rect(floor_map);
            // El SALTAR del tutorial es el unico boton de pantalla que convive con el HUD:
            // se atiende antes que el "tap = ataque" para que tocarlo no dispare de paso.
            input.touch_ui_rect = tutorial.active ? tutorial_skip_rect() : Rect{0, 0, 0, 0};
        } else {
            // Fuera de la partida no hay botones que tocar (ni huecos muertos)
            input.set_touch_enabled(false, 0, false, false, false);
            input.touch_minimap = {0, 0, 0, 0};
            input.touch_map_open = false;
            input.touch_ui_rect = {0, 0, 0, 0};
        }
    }
    input.update_touch(dt);   // hold sobre un boton de milagro = su ficha
    audio.update(dt);   // recuperacion del ducking de la musica
    if (voice_cooldown > 0.0f) voice_cooldown -= dt;   // espera entre exclamaciones

    if (banner_timer > 0.0f)  banner_timer  -= dt;
    if (message_timer > 0.0f) message_timer -= dt;
    if (pickup_t > 0.0f)      pickup_t      -= dt;
    if (offering_pop > 0.0f && state.scene != GameScene::Sanctuary)
        offering_pop = std::max(0.0f, offering_pop - dt * 1.4f);
    if (room_fade > 0.0f)     room_fade = std::max(0.0f, room_fade - dt / 0.3f);
    if (scene_fade > 0.0f)    scene_fade = std::max(0.0f, scene_fade - dt / SCENE_FADE_TIME);
    {
        const bool scripted = (state.scene == GameScene::Dialogue && (scene_step >= 0 || scene_walk_t > 0.0f)) ||
                              (state.scene == GameScene::Playing && boss_approach_t > 0.0f);
        scene_bars = scripted ? std::min(1.0f, scene_bars + dt / 0.45f) : std::max(0.0f, scene_bars - dt / 0.35f);
    }
    if (world_flash > 0.0f)   world_flash = std::max(0.0f, world_flash - dt);

    // Menu navigation blips (any scene showing a menu)
    if (state.scene != GameScene::Playing) {
        if (input.is_action_just_pressed(Action::MoveUp) || input.is_action_just_pressed(Action::MoveDown))
            audio.play_sfx(SfxID::MenuSelect, 0.6f);
        if (input.is_action_just_pressed(Action::Confirm))
            audio.play_sfx(SfxID::MenuConfirm, 0.7f);
    }
    update_music();

    // La escena con la que ENTRA el frame: la lee la guarda del toggle de pausa de mas abajo.
    const GameScene scene_at_frame_start = state.scene;

    switch (state.scene) {
        case GameScene::Intro:
            tap_skip_or_advance(input);
            if (cinematic.update(input, dt, renderer)) {
                // Saltada a mano: el tramo siguiente entra desde negro, o no habria forma
                // de distinguirlo de "el boton de SALTAR no ha hecho nada".
                if (cinematic.skipped) { scene_fade = 1.0f; cinematic.skipped = false; }
                if (cine_then == CineThen::Play) {
                    // Intro de capitulo: se ve una vez por partida, sus capas no vuelven
                    cinematic.unload_art(renderer);
                    change_scene(GameScene::Playing);
                } else if (cine_then == CineThen::FinishAscension) {
                    // Saltarla con ESC tambien reparte la herencia: la cinematica es
                    // presentacion, nunca la puerta del Manto.
                    cinematic.unload_art(renderer);
                    finish_ascension();
                } else {
                    change_scene(GameScene::MainMenu);
                }
            }
            break;

        case GameScene::MainMenu:
            if (menus.current == MenuScreen::Language) {
                menus.update(input);   // en tactil, tocar un boton lo elige y lo confirma
                // Izquierda/derecha tambien valen: con dos opciones en columna, el stick
                // horizontal de un mando es lo primero que se prueba.
                if (input.is_action_just_pressed(Action::MoveLeft))  menus.selected_option = 0;
                if (input.is_action_just_pressed(Action::MoveRight)) menus.selected_option = 1;
                // `Cancel` (el boton atras de Android) acepta lo que este marcado en vez de
                // no hacer nada: la primera pantalla del juego no puede tener una tecla muerta.
                if (input.is_action_just_pressed(Action::Confirm) ||
                    input.is_action_just_pressed(Action::Cancel)) {
                    // El bit va ANTES: `set_language` ya escribe el fichero, y asi se guarda
                    // una sola vez. `false` = no re-sella la fecha de la partida.
                    save.data.settings_flags |= SETTING_LANG_CHOSEN;
                    set_language(menus.selected_option == 1 ? Language::EN : Language::ES);
                    load_settings_mirror();   // Opciones abrira ya con el idioma elegido
                    menus.current = MenuScreen::Title;
                    menus.selected_option = 0;
                    // En el primer arranque, lo siguiente es el prologo (main.cpp no lo
                    // lanzo para no contarlo antes de saber en que idioma).
                    if (intro_after_language) { intro_after_language = false; start_intro(); }
                }
            } else if (menus.current == MenuScreen::Settings) {
                update_settings();
            } else if (menus.current == MenuScreen::Controls) {
                menus.update(input);   // el unico boton es el del tutorial; fuera de el, volver
                if (input.is_action_just_pressed(Action::Confirm)) {
                    // Desde aqui el tutorial VUELVE al titulo al acabar: quien solo iba a
                    // repasar los controles no puede perder la partida que tenia a medias.
                    menus.current = MenuScreen::Title;
                    menus.selected_option = 0;
                    start_tutorial(TutorialThen::Title);
                } else if (input.is_action_just_pressed(Action::Cancel)) {
                    menus.current = MenuScreen::Title;
                    menus.selected_option = 0;
                }
            } else if (menus.current == MenuScreen::TutorialAsk) {
                menus.update(input);   // en tactil, tocar un boton lo elige y lo confirma
                // Con dos opciones en columna, el stick horizontal es lo primero que se prueba.
                if (input.is_action_just_pressed(Action::MoveLeft))  menus.selected_option = 0;
                if (input.is_action_just_pressed(Action::MoveRight)) menus.selected_option = 1;
                if (input.is_action_just_pressed(Action::Confirm)) {
                    const bool yes = menus.selected_option == 0;
                    menus.current = MenuScreen::Title;
                    menus.selected_option = 0;
                    if (yes) start_tutorial(TutorialThen::NewRun);
                    else     start_new_run();
                } else if (input.is_action_just_pressed(Action::Cancel)) {
                    // El boton atras de Android: se vuelve al titulo sin empezar nada.
                    menus.current = MenuScreen::Title;
                    menus.selected_option = 0;
                }
            } else {
                menus.update(input);
                if (input.is_action_just_pressed(Action::Confirm)) {
                    switch (menus.selected_option) {
                        case 0:
                            // Antes de la primera sala, la pregunta del tutorial. Bajo
                            // autoplay no: los guiones esperan entrar a jugar.
                            if (autoplay.active) { start_new_run(); break; }
                            menus.current = MenuScreen::TutorialAsk;
                            menus.selected_option = 0;
                            break;
                        case 1:
                            if (!save.has_save())            break;
                            if (menus.continue_expired)      claim_abandoned_run();
                            else                             continue_run();
                            break;
                        case 2:
                            open_sanctuary(GameScene::MainMenu);
                            break;
                        case 3:
                            codex_return = GameScene::MainMenu;
                            codex = CodexScreen{};
                            change_scene(GameScene::Codex);
                            break;
                        case 4:
                            start_intro();
                            break;
                        case 5:
                            menus.settings_origin = MenuScreen::Title;
                            load_settings_mirror();
                            menus.current = MenuScreen::Settings;
                            menus.selected_option = 0;
                            break;
                        case 6:
                            menus.current = MenuScreen::Controls;
                            menus.selected_option = 0;
                            break;
                    }
                }
            }
            break;

        case GameScene::Playing: {
            // La ENTRADA del jefe va DELANTE de todo lo demas de Playing, y sobre todo
            // delante de `pending_scene`: el dialogo espera a que el profeta haya llegado.
            if (boss_approach_t > 0.0f) { update_boss_approach(dt); break; }
            // Escena pedida por load_room (el dialogo con el jefe): arranca aqui, con la
            // transicion de sala ya cerrada y antes de que el mundo de un solo paso.
            if (pending_scene >= 0) {
                int s = pending_scene;
                pending_scene = -1;
                start_scene(s);
                break;
            }
            // Primicias (El Santuario): la carta de objeto con la que se empieza. Aqui y no
            // en start_new_run porque delante van la audiencia y la intro del capitulo.
            if (primicias_pending && !tutorial.active) {
                primicias_pending = false;
                SDL_Log("Primicias: carta de objeto al empezar");
                open_item_choice(tr("PRIMICIAS: ELIGE CON QUÉ EMPIEZAS", "FIRSTFRUITS: CHOOSE YOUR START"));
                break;
            }
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
            f32 secondary_before = player.secondary_cd;
            bool was_dashing = player.entity != ENTITY_NONE && world.has(player.entity, Component::Movement) &&
                               world.movements[player.entity].is_dashing;
            player.update(world, input, faith, dt);
            if (player.shots_fired != shots_before)
                audio.play_sfx_variant(player.character == CharacterType::Elias ? SfxID::ProyectilFuego
                                                                                : SfxID::ProyectilLuz, 3, 0.5f);
            // El arma secundaria acaba de disparar (la recarga pasa de 0 a su valor): sonido propio
            if (secondary_before <= 0.0f && player.secondary_cd > 0.0f)
                audio.play_sfx(player.weapon_slot == 1 ? SfxID::CayadoSwing2 : SfxID::CayadoSwing3, 0.9f);
            if (!was_dashing && player.entity != ENTITY_NONE && world.has(player.entity, Component::Movement) &&
                world.movements[player.entity].is_dashing) {
                audio.play_sfx(SfxID::DashWoosh, 0.6f);
                play_voice(Voice::Esfuerzo, 0.8f);
                renderer.shake_camera(Config::Feel::SHAKE_DASH);
                input.rumble(Config::Feel::RUMBLE_DASH, 90);
            }

            // Gracia de la puerta (ver use_doorway). Las transiciones por borde ya no
            // existen: el capitulo es una zona continua.
            if (room_change_grace > 0.0f) room_change_grace -= dt;

            // El equipo del profeta que toca el combate (combat.cpp no conoce al Player):
            // se copia ANTES de mover el mundo y se lee de vuelta despues (el escudo).
            world.mods.heart_drop_mult = player.heart_drop_mult;
            world.mods.heart_heal_mult = player.heart_heal_mult;
            world.mods.kill_faith      = player.kill_faith;
            world.mods.burning_shots   = player.burning_shots;
            world.mods.shield_up       = player.has_shield && player.shield_up;
            world.mods.belt_stack      = 0;
            world.mods.belt_free       = 0;
            for (int i = 0; i < player.belt_cap && i < Player::BELT_MAX; i++) {
                const auto& b = player.belt[i];
                if (b.empty()) world.mods.belt_free++;
                else if (b.charges < Player::BELT_STACK && b.id < 32) world.mods.belt_stack |= (1u << b.id);
            }

            world.update(dt);
            update_enemy_ai(world, dt);
            update_pickup_magnet(world, dt);

            system_collision(world, faith, combat_events);
            // Braseros prendidos: despues de las colisiones (usa fe/eventos) y antes de
            // update_room_state (sus bajas tienen que contar ya)
            update_death_fuses(world, faith, combat_events, dt);
            // Antorcha del Altar: lo que arde, arde (mismas razones que las mechas)
            update_burns(world, faith, combat_events, dt);
            // Piedra del Memorial: si el mundo rompio el escudo, el Player se entera
            if (world.mods.shield_broke) {
                world.mods.shield_broke = false;
                player.shield_up = false;
                player.shield_timer = Player::SHIELD_RECHARGE;
                audio.play_sfx(SfxID::CayadoHitEnemy, 1.0f);
                renderer.shake_camera(Config::Feel::SHAKE_HIT);
                show_message(tr("¡La piedra te cubre!", "The stone shields you!"), 1.2f);
                SDL_Log("Shield: Piedra del Memorial absorbed a hit");
            }
            // Peligros telegrafiados de los jefes (game/hazards.h): mismo sitio y mismas
            // razones que las mechas — usan fe y eventos, y sus bajas cuentan ya.
            update_hazards(world, faith, combat_events, dt);
            // Sanar Aguas disuelve las charcas que toca el agua sanada (2 Reyes 2:21)
            if (player.heal_zone_timer > 0.0f) cleanse_pools(world, player.heal_zone_pos, 34.0f);
            if (world.hazard_bursts > 0) {
                audio.play_sfx(SfxID::CriticalHit, 0.35f);
                input.rumble(Config::Feel::RUMBLE_HIT, 50);
                world.hazard_bursts = 0;
            }
            if (world.shield_blocks > 0) {
                audio.play_sfx(SfxID::CayadoHitEnemy, 0.8f);   // el "clinc" del escudo
                world.shield_blocks = 0;
            }
            if (world.flash_request > 0.0f) {
                world_flash = std::max(world_flash, world.flash_request);
                world.flash_request = 0.0f;
            }
            // La oscuridad de la Muerte: la pide cada frame mientras vive, y se va sola
            {
                const bool dark = world.darkness_req > 0.0f;
                if (dark) darkness_radius = world.darkness_req;
                darkness_level = dark ? std::min(1.0f, darkness_level + dt * 1.2f)
                                      : std::max(0.0f, darkness_level - dt * 0.8f);
                world.darkness_req = 0.0f;
            }
            if (world.shake_request > 0.0f) {
                renderer.shake_camera(world.shake_request, Config::Feel::SHAKE_EXPLOSION.dur,
                                      Config::Feel::SHAKE_EXPLOSION.freq);
                input.rumble(Config::Feel::RUMBLE_KILL, 160);
                audio.play_sfx(SfxID::EnemyDeath, 0.9f);
                world.shake_request = 0.0f;
            }

            // Miracle feedback (kills by miracles, shake, messages)
            // El id se apunto el frame anterior: si entre medias la entidad murio y su slot
            // se reciclo (un VFX, un secuaz recien invocado), on_enemy_killed dispararia
            // conductas de muerte sobre un vivo. Solo se cobra lo que siga siendo hostil.
            for (Entity k : player.pending_kills)
                if (is_hostile(world, k)) on_enemy_killed(world, faith, k, combat_events);
            player.pending_kills.clear();
            if (player.shake_request > 0.0f) {
                renderer.shake_camera(player.shake_request, Config::Feel::SHAKE_EXPLOSION.dur,
                                      Config::Feel::SHAKE_EXPLOSION.freq);
                input.rumble(Config::Feel::RUMBLE_KILL, 180);
                player.shake_request = 0.0f;
            }
            if (player.flash_request > 0.0f) { world_flash = player.flash_request; player.flash_request = 0.0f; }
            // El Fuego del Cielo toca suelo: el golpe suena AHI, no al invocarlo
            if (player.sky_fire_landed) {
                player.sky_fire_landed = false;
                audio.play_sfx(SfxID::CriticalHit, 1.0f);
                audio.play_sfx(SfxID::FireCrackle, 0.85f);
                input.rumble(Config::Feel::RUMBLE_KILL, 220);
            }
            if (player.last_event == Player::MiracleEvent::Cast) {
                show_message(get_miracle_def(player.last_cast).name, 1.5f);
                save.data.miracles_cast |= (1u << static_cast<u32>(player.last_cast));
                award(AchievementID::PrimerMilagro);
                // Cada milagro con su sonido: diez poderes compartiendo cuatro efectos
                // hacian que todos sonaran igual aunque se vieran distintos.
                switch (player.last_cast) {
                    case MiracleID::CuervosDeQuerit:   audio.play_sfx(SfxID::MiracleRavens); break;
                    case MiracleID::PalabraDeSequia:   audio.play_sfx(SfxID::MiracleCastFire, 0.7f); break;
                    case MiracleID::FuegoDelCielo:     audio.play_sfx(SfxID::MiracleCastFire); break;
                    case MiracleID::CarroDeFuego:      audio.play_sfx(SfxID::DashWoosh);
                                                       audio.play_sfx(SfxID::MiracleCastFire, 0.8f); break;
                    case MiracleID::AltarConsumido:    audio.play_sfx(SfxID::MiracleUltimate); break;
                    case MiracleID::SanarAguas:        audio.play_sfx(SfxID::MiracleHeal); break;
                    case MiracleID::AceiteInagotable:  audio.play_sfx(SfxID::MiracleHeal, 0.75f); break;
                    case MiracleID::OjosAbiertos:      audio.play_sfx(SfxID::MiracleCastLight, 0.8f); break;
                    case MiracleID::EjercitoCelestial: audio.play_sfx(SfxID::MiracleCastLight); break;
                    case MiracleID::DoblePorcion:      audio.play_sfx(SfxID::MiracleUltimate, 0.85f);
                                                       audio.play_sfx(SfxID::LevelUp, 0.7f); break;
                    default:                           audio.play_sfx(SfxID::MiracleCastLight); break;
                }
                // Solo los milagros GRANDES llevan voz: si el profeta gritara tambien al
                // curarse con los cuervos, la exclamacion dejaria de significar nada.
                if (get_miracle_def(player.last_cast).tier >= 3) play_voice(Voice::Milagro, 1.0f);
            } else if (player.last_event != Player::MiracleEvent::None) {
                // Decir exactamente que falta: nivel de fe, fe para pagarlo o recarga
                MiracleDef md = get_miracle_def(player.last_cast);
                char buf[128];
                switch (player.last_event) {
                    case Player::MiracleEvent::NeedTier:
                        snprintf(buf, sizeof(buf), tr("%s: necesitas %.0f de fe (%s)", "%s: you need %.0f faith (%s)"), md.name,
                                 faith_required_for_tier(md.tier), faith_level_name_for_tier(md.tier));
                        break;
                    case Player::MiracleEvent::NeedCost:
                        snprintf(buf, sizeof(buf), tr("%s: te faltan %.0f de fe", "%s: %.0f faith short"), md.name,
                                 player.miracle_cost(player.last_cast) - faith.current);
                        break;
                    default:
                        snprintf(buf, sizeof(buf), tr("%s: en recarga", "%s: on cooldown"), md.name);
                        break;
                }
                show_message(buf, 2.0f);
                audio.play_sfx(SfxID::MenuBack, 0.5f);
            }
            player.last_event = Player::MiracleEvent::None;

            run_kills += combat_events.enemy_kills;
            player.kills += combat_events.enemy_kills;
            tally.kills  += combat_events.enemy_kills;
            tally.bosses += combat_events.boss_kills;

            // Cada evento con su curva de sacudida y su rumble (Config::Feel). Antes todo
            // compartia una magnitud suelta escrita aqui mismo, asi que conectar un tiro y
            // reventar a un jefe se sentian parecido.
            if (combat_events.enemy_hits > 0) {
                audio.play_sfx_variant(SfxID::EnemyHit, 3, 0.6f);
                renderer.shake_camera(combat_events.boss_hits > 0 ? Config::Feel::SHAKE_BOSS_HIT
                                                                  : Config::Feel::SHAKE_HIT);
                input.rumble(Config::Feel::RUMBLE_HIT, 60);
            }
            if (combat_events.enemy_kills > 0) {
                // Una recompensa audible por lo que muere: un leon y una vasija de Agua
                // Corrupta compartian el mismo golpe seco y ninguna de las dos muertes
                // se leia. Los ids son EnemyType (game/enemies.h).
                // Con varios muertos en el mismo frame manda el mas llamativo: el agua
                // derramandose tapa a un soldado, no al reves.
                auto died = [&](EnemyType t) {
                    return (combat_events.kill_mask >> static_cast<u32>(t)) & 1u;
                };
                if (died(EnemyType::AguaCorrupta)) {
                    audio.play_sfx(SfxID::WaterFlow, 0.9f);
                    audio.play_sfx(SfxID::EnemyDeath, 0.5f);
                } else if (died(EnemyType::IdolatraDeRimon)) {
                    audio.play_sfx(SfxID::FireCrackle, 0.9f);   // el brasero cae con el
                    audio.play_sfx(SfxID::EnemyDeath, 0.6f);
                } else if (died(EnemyType::LeonDelDesierto) || died(EnemyType::ProfetaDeBaal) ||
                           died(EnemyType::GeneralSirio)) {
                    audio.play_sfx(SfxID::EnemyDeath, 1.0f);
                    audio.play_sfx(SfxID::CriticalHit, 0.45f);
                } else {
                    audio.play_sfx_variant(SfxID::EnemyDeath, 2, 0.8f);
                }
                renderer.shake_camera(Config::Feel::SHAKE_KILL);
                input.rumble(Config::Feel::RUMBLE_KILL, 100);
            }
            if (combat_events.hearts_picked + combat_events.orbs_picked > 0) audio.play_sfx(SfxID::ItemPickup, 0.7f);
            if (combat_events.player_hits > 0) {
                player.took_damage_this_room = true;
                run_hits_taken += combat_events.player_hits;
                renderer.shake_camera(Config::Feel::SHAKE_PLAYER_HIT);
                input.rumble(Config::Feel::RUMBLE_PLAYER_HIT, 220);
                audio.play_sfx(SfxID::FaithLoss, 0.9f);
                play_voice(Voice::Dolor);
            }
            if (combat_events.enemy_kills > 0) {
                save.data.total_kills += combat_events.enemy_kills;
                if ((combat_events.kill_mask >> static_cast<u32>(EnemyType::LeonDelDesierto)) & 1u)
                    award(AchievementID::LeonDelDesierto);
            }
            if (combat_events.boss_kills > 0) {
                award(AchievementID::PrimerJefe);
                renderer.shake_camera(Config::Feel::SHAKE_BOSS_KILL);
                input.rumble(Config::Feel::RUMBLE_BOSS_KILL, 500);
                audio.play_sfx(SfxID::CriticalHit);
            }
            if (world.altars_smashed > 0) {
                for (u32 i = 0; i < world.altars_smashed; i++) faith.on_destroy_altar();
                world.altars_smashed = 0;
                renderer.shake_camera(Config::Feel::SHAKE_ALTAR);
                show_message(tr("Altar de Baal destruido  +8 Fe", "Altar of Baal torn down  +8 Faith"));
                audio.play_sfx(SfxID::FaithGain);
                award(AchievementID::PrimerAltar);
            }
            // Hallazgos: lo que habia debajo de un matorral, dentro de una tinaja, detras de
            // una grieta o bajo una charca. Aqui, y solo aqui, se decide QUE paga un secreto
            // — por eso los sitios que abren cosas apuntan la posicion en `loot_drops` en vez
            // de soltar la pieza ellos mismos.
            if (!world.loot_drops.empty()) {
                int paid[4] = {0, 0, 0, 0};   // ofrendas / fe / vida / consumible, para que el log diga QUE pago
                for (Vec2 at : world.loot_drops) {
                    // QUE paga el hallazgo se saca de SU POSICION, no de `randi_range`: asi la
                    // misma semilla reparte siempre lo mismo (igual que `tile_hides_loot`) y
                    // un guion de regresion puede exigir una ofrenda concreta. Sortearlo aqui
                    // ademas moveria el generador de mapas, que es el mismo `rand()`.
                    u32 h = static_cast<u32>(at.x) * 73856093u ^ static_cast<u32>(at.y) * 19349663u;
                    h = (h ^ (h >> 13)) * 1274126177u;
                    int roll = static_cast<int>((h ^ (h >> 16)) % 10);
                    // La OFRENDA es lo mas probable, y a proposito: vida y fe ya las suelta
                    // cualquier enemigo, asi que si un secreto pagara sobre todo en eso no
                    // habria motivo para desviarse. Lo que hace que explorar de mas signifique
                    // algo es que pague en meta-progresion, o sea entre partidas.
                    // 1 de cada 10 secretos guarda un CONSUMIBLE para la alforja (cual, tambien
                    // por la posicion: la misma tinaja paga siempre lo mismo).
                    if (roll < 5)      { spawn_pickup(world, at, ItemComponent::Pickup::Offering, 3.0f); paid[0]++; }
                    else if (roll < 7) { spawn_pickup(world, at, ItemComponent::Pickup::FaithOrb, 6.0f); paid[1]++; }
                    else if (roll < 9) { spawn_pickup(world, at, ItemComponent::Pickup::Heart, 10.0f);   paid[2]++; }
                    else               { spawn_consumable_at(at, static_cast<f32>((h >> 8) % 1000u) / 1000.0f); paid[3]++; }
                    spawn_fx(world, at, FxID::Holy, {255, 226, 150, 220}, 22.0f, 0.35f, 5);
                }
                // Se loguea la CAIDA y no solo la recogida: entre las dos hay que caminar
                // hasta la pieza, y un guion que solo mirara la recogida no sabria distinguir
                // "el secreto no pago nada" de "el bot no paso por encima".
                SDL_Log("Secret loot: %d hallazgo(s) en el suelo (%d ofrenda, %d fe, %d vida, %d consumible)",
                        static_cast<int>(world.loot_drops.size()), paid[0], paid[1], paid[2], paid[3]);
                world.loot_drops.clear();
                audio.play_sfx(SfxID::Notification, 0.55f);
            }
            // La ofrenda es lo unico del suelo que SOBREVIVE a la partida: va derecha a la
            // meta-progresion y se guarda en el acto, para que morirse no se la lleve.
            if (world.offerings_found > 0 && tutorial.active) {
                // En el tutorial la ofrenda CUENTA para su paso pero no se ingresa: jugarlo
                // no puede regalar moneda meta, y el guardado ni se toca.
                tutorial.offerings_picked += static_cast<int>(world.offerings_found);
                show_message(tr("+1 ofrenda (en el tutorial no se guarda)",
                                "+1 offering (not banked during the tutorial)"), 1.8f);
                world.offerings_found = 0;
                audio.play_sfx(SfxID::FaithGain, 0.7f);
            } else if (world.offerings_found > 0) {
                // El contador del HUD salta y dice "+N": ya no hace falta un mensaje que tape
                // el centro de la pantalla. La primera vez, el cartel que explica que es.
                const int got = static_cast<int>(world.offerings_found);
                world.offerings_found = 0;
                bank_offerings(got, true);
                SDL_Log("Secret offerings: +%d (total %d)", got, progression.offerings);
                audio.play_sfx(SfxID::FaithGain, 0.7f);
            }
            // Consumibles: los que alguien solto (enemigo raro, jefe) y los recogidos
            for (Vec2 at : world.consumable_drops) spawn_consumable_at(at, randf());
            world.consumable_drops.clear();
            update_belt();
            // El buzon se vacia AQUI, justo despues de vaciarlo de verdad, y no antes de
            // llenarlo. La diferencia importa: lo que escribe fuera de esta ventana —el
            // `killall` del autoplay, que corre antes en el mismo frame— sobrevive hasta
            // que alguien lo lee, y si ese frame se congela por hit-stop (que ahora lo
            // pone CUALQUIER golpe, no solo los de jefe) espera al siguiente en vez de
            // perderse. Con el reinicio delante, `killall` mataba la sala entera y el
            // marcador de bajas no se enteraba de nada.
            combat_events.reset();

            if (god_mode && player.entity != ENTITY_NONE && world.has(player.entity, Component::Health)) {
                world.healths[player.entity].current = world.healths[player.entity].max;
            }

            // Pista de milagro: una sola vez por partida, la primera vez que CUALQUIER
            // celda se pueda lanzar de verdad (nada de enseñar un boton bloqueado).
            // Antes estaba atada al cap. 1 y a la celda Q: quien empezaba por otro
            // capitulo, o cuyo primer milagro pagable caia en E, no veia la pista nunca —
            // y el usuario probo el APK sin saber como lanzar un milagro.
            if (!miracle_hint_shown) {
                int hint_slot = -1;
                for (int i = 0; i < player.active_slot_count && i < Config::MAX_MIRACLE_SLOTS; i++) {
                    const MiracleSlot& s = player.miracle_slots[i];
                    if (s.is_empty() || !s.is_ready()) continue;
                    auto def = get_miracle_def(s.miracle);
                    if (def.tier <= faith.get_max_miracle_tier() && def.faith_cost <= faith.current) {
                        hint_slot = i;
                        break;
                    }
                }
                if (hint_slot >= 0) miracle_hint_timer -= dt;
                if (hint_slot >= 0 && miracle_hint_timer <= 0.0f && message_timer <= 0.0f) {
                    miracle_hint_shown = true;
                    auto def = get_miracle_def(player.miracle_slots[hint_slot].miracle);
                    SDL_Log("Miracle hint shown (chapter %d, slot %d: %s)",
                            state.current_chapter, hint_slot, def.name);
                    static const char* keys[Config::MAX_MIRACLE_SLOTS] = {"Q", "E", "F"};
                    char hint[160];
                    if (input.touch_ui) {
                        // Dos lineas a proposito: la segunda es la que nadie descubre solo.
                        // La ficha por hold (0,4 s) existe desde siempre y no la encontro
                        // nadie porque no se anunciaba en ninguna parte.
                        snprintf(hint, sizeof(hint),
                                 tr("Toca el milagro para lanzarlo: %s  ·  %.0f de fe\nMantenlo pulsado para ver qué hace",
                                    "Tap the miracle to cast it: %s  ·  %.0f faith\nHold it down to see what it does"),
                                 def.name, def.faith_cost);
                    } else {
                        snprintf(hint, sizeof(hint),
                                 tr("Pulsa [%s]: %s  ·  %.0f de fe", "Press [%s]: %s  ·  %.0f faith"),
                                 keys[hint_slot], def.name, def.faith_cost);
                    }
                    show_message(hint, 4.5f);
                }
            }

            // "Altar Portátil" (El Santuario): oraciones por capitulo sin necesitar altar
            if (input.is_action_just_pressed(Action::Pray) && prayers_left > 0 &&
                player.entity != ENTITY_NONE) {
                prayers_left--;
                faith.on_pray();
                if (world.has(player.entity, Component::Health)) world.healths[player.entity].heal(10.0f);
                const Vec2 pp = world.transforms[player.entity].position;
                spawn_fx(world, pp, FxID::Holy, {255, 235, 170, 235}, 40.0f, 0.6f, 11);
                spawn_particles(world, pp, {255, 240, 190, 255}, 12, 50.0f, 0.6f);
                SDL_Log("Portable altar: prayed in place (+10 faith), quedan %d", prayers_left);
                char pb[96];
                snprintf(pb, sizeof(pb), tr("Oras donde estás  +10 fe  +10 vida  (quedan %d)",
                                            "You pray where you stand  +10 faith  +10 health  (%d left)"),
                         prayers_left);
                show_message(pb, 2.2f);
                audio.play_sfx(SfxID::FaithGain);
            }

            update_cells();
            update_room_state();
            // Despues de update_room_state: si la pista la dio una elite o un altar, su carta
            // ya ha cambiado la escena y el aviso espera a que se vuelva a jugar.
            if (state.scene == GameScene::Playing) update_boss_intel();
            check_achievements();
            renderer.update_shake(dt);

            // La camara sigue al profeta. Va DESPUES de update_room_state para que la sala
            // (y por tanto sus limites) sea ya la definitiva este frame. `Camera::update`
            // recorta contra ellos, asi que en una sala de una pantalla no se mueve nada.
            if (player.entity != ENTITY_NONE && world.has(player.entity, Component::Transform)) {
                camera.set_target(world.transforms[player.entity].position);
                camera.update(dt);
            }

            // El tutorial (game/tutorial.h) va DELANTE de la comprobacion de muerte: es
            // donde sujeta la vida en 1, y perder el tutorial no esta en el guion.
            if (tutorial.active) update_tutorial(dt);

            // Check player death
            if (player.entity != ENTITY_NONE &&
                world.has(player.entity, Component::Health) &&
                !world.healths[player.entity].is_alive()) {
                const bool relic_saves = !has_revive && relic_revive_ready &&
                                         player.relic_slot == 20;
                // Aliento de Vida (El Santuario): el ULTIMO en saltar, porque es el que se
                // rellena menos a menudo (por intento) y es el que el jugador compro.
                const bool breath_saves = !has_revive && !relic_saves && revives_left > 0;
                if (breath_saves) {
                    revives_left--;
                    auto& h = world.healths[player.entity];
                    h.current = h.max * 0.5f;
                    h.iframes_timer = 2.5f;
                    faith.modify(10.0f);
                    world_flash = std::max(world_flash, 0.6f);
                    renderer.shake_camera(Config::Feel::SHAKE_BOSS_KILL);
                    const Vec2 pp = world.transforms[player.entity].position;
                    spawn_fx(world, pp, FxID::Holy, {255, 245, 210, 240}, 56.0f, 0.8f, 11);
                    spawn_fx(world, pp, FxID::Ring, {255, 230, 160, 230}, 70.0f, 0.6f, 11);
                    SDL_Log("Revive used: Aliento de Vida (El Santuario), quedan %d", revives_left);
                    char sub[96];
                    snprintf(sub, sizeof(sub), tr("El Santuario te levanta  ·  quedan %d", "The Sanctuary raises you  ·  %d left"),
                             revives_left);
                    show_banner(tr("ALIENTO DE VIDA", "BREATH OF LIFE"), sub, 3.5f);
                    audio.play_sfx(SfxID::LevelUp);
                    audio.play_sfx(SfxID::MiracleHeal, 0.8f);
                } else if (has_revive || relic_saves) {
                    // El aposento sunamita (2 Reyes 4:35): el niño estornudó siete veces
                    if (relic_saves) relic_revive_ready = false;
                    else             has_revive = false;
                    auto& h = world.healths[player.entity];
                    h.current = h.max * 0.5f;
                    h.iframes_timer = 2.0f;
                    faith.modify(10.0f);
                    SDL_Log("Revive used: %s", relic_saves ? "El Reloj de Acaz (relic)"
                                                            : "aposento sunamita (event)");
                    if (relic_saves)
                        show_banner(tr("LA SOMBRA RETROCEDE", "THE SHADOW GOES BACK"),
                                    tr("El Reloj de Acaz te devuelve el golpe",
                                       "Ahaz's dial gives you the blow back"), 3.5f);
                    else
                        show_banner(tr("EL NIÑO ESTORNUDÓ", "THE CHILD SNEEZED"), tr("La sunamita te levanta una vez", "The Shunammite raises you once"), 3.5f);
                    audio.play_sfx(SfxID::LevelUp);
                } else {
                    SDL_Log("Player died! Game Over.");
                    menus.stats_chapter = state.current_chapter;
                    menus.stats_kills = run_kills;
                    menus.stats_time = state.run_time;
                    award_offerings(false);
                    save_after_defeat();
                    change_scene(GameScene::GameOver);
                    summary.t = 0.0f;
                }
            }
            break;
        }

        case GameScene::Choice:
            update_item_choice(dt);
            renderer.update_shake(dt);
            break;

        case GameScene::Paused:
            if (equipment_open) {
                update_equipment();
            } else if (menus.current == MenuScreen::Settings) {
                update_settings();
            } else {
                menus.update(input);
                if (input.is_action_just_pressed(Action::Confirm)) {
                    switch (menus.selected_option) {
                        case 0: change_scene(GameScene::Playing); break;
                        case 1:
                            // Equipo: que llevas y que hace cada cosa, con las cifras
                            equipment_open = true;
                            equipment_scroll = 0;
                            SDL_Log("Equipo abierto (%d objetos, %d en la alforja)",
                                    static_cast<int>(player.items.size()), player.belt_count());
                            break;
                        case 2:
                            codex_return = GameScene::Paused;
                            codex = CodexScreen{};
                            change_scene(GameScene::Codex);
                            break;
                        case 3:
                            menus.settings_origin = MenuScreen::PauseMenu;
                            load_settings_mirror();
                            menus.current = MenuScreen::Settings;
                            menus.selected_option = 0;
                            break;
                        case 4:
                            // En el tutorial esta fila es la salida (en PC no hay boton de
                            // SALTAR, y ESC ya es la pausa que el paso 14 enseña).
                            if (tutorial.active) { finish_tutorial(false); break; }
                            populate_save_data();
                            save.save();
                            change_scene(GameScene::MainMenu);
                            break;
                    }
                }
                if (input.is_action_just_pressed(Action::Pause) && !equipment_open) {
                    change_scene(GameScene::Playing);
                }
            }
            break;

        case GameScene::Story:
            // En tactil, un toque en cualquier sitio pasa de linea (story.update ya lee
            // Confirm); sobre el boton de SALTAR se salta la secuencia entera.
            tap_skip_or_advance(input);
            if (story.update(input, dt, renderer)) {
                if (story.skipped) { scene_fade = 1.0f; story.skipped = false; }
                finish_story();
            }
            break;

        case GameScene::Dialogue:
            update_dialogue(dt);
            break;

        case GameScene::Codex:
            if (codex.update(input, save.data, item_registry)) change_scene(codex_return);
            break;

        case GameScene::Sanctuary:
            update_sanctuary();
            break;

        case GameScene::GameOver:
            summary.t += dt;
            menus.update(input);
            // [R] sigue siendo atajo directo; la opcion 0 del menu hace lo mismo
            if (input.is_action_just_pressed(Action::Consumable) ||
                (input.is_action_just_pressed(Action::Confirm) && menus.selected_option == 0)) {
                retry_chapter();
                break;
            }
            // El Santuario DESDE la derrota: gastar lo que acabas de ganar y volver aqui a
            // reintentar con ello puesto. Es el bucle que no se entendia.
            if (input.is_action_just_pressed(Action::Confirm) && menus.selected_option == 1) {
                open_sanctuary(GameScene::GameOver);
                summary.t = 99.0f;   // al volver, la cuenta ya esta hecha
                break;
            }
            if (input.is_action_just_pressed(Action::Confirm)) {
                // La partida NO se borra al perder: save_after_defeat() la dejo rebobinada
                // al principio del capitulo, asi que "Continuar" sigue vivo en el menu.
                change_scene(GameScene::MainMenu);
            }
            break;

        case GameScene::Victory:
            summary.t += dt;
            menus.update(input);   // en tactil, un toque en cualquier sitio confirma
            if (input.is_action_just_pressed(Action::Confirm)) {
                save.delete_save();
                change_scene(GameScene::MainMenu);
            }
            break;
    }

    // Pause toggle (available during gameplay). `scene_at_frame_start` es lo que impide que
    // ESC se anule a si mismo: el `case Paused` de arriba ya ha reanudado en ESTE frame, y
    // sin la guarda esta linea volvia a pausar en el acto — o sea que ESC abria la pausa y
    // no la cerraba nunca. Se vio al hacer comprobable el paso de la pausa del tutorial,
    // que es justo el que manda pulsarla.
    if (scene_at_frame_start == GameScene::Playing && state.scene == GameScene::Playing &&
        input.is_action_just_pressed(Action::Pause)) {
        // El paso `Pause` del tutorial se cierra AQUI, y solo aqui: la pausa automatica al
        // irse la app a segundo plano (arriba, en el manejo de eventos) no la pide nadie.
        if (tutorial.active && tutorial.current().goal == TutorialGoal::Pause)
            tutorial.pause_seen = true;
        change_scene(GameScene::Paused);
    }
}

// =============================================================================
// Render
// =============================================================================
void Engine::render() {
    renderer.begin_frame();

    // El boton que hay bajo el dedo se ilumina mientras el dedo sigue apoyado: en movil no
    // hay cursor que resaltar, asi que esto es la unica señal de que el toque ha entrado.
    // Se apunta AQUI y no en update porque a 90 o 120 Hz hay frames que no ejecutan ningun
    // paso de update, y el resalte parpadearia al ritmo de los pasos en vez del dedo.
    menus.press_active = input.touch_ui && input.ui_press(menus.press_pos);
    codex.press_active = menus.press_active;
    codex.press_pos    = menus.press_pos;
    // El boton de SALTAR lo comparten las tres pantallas de escena, y el dato es el mismo.
    const bool skip_down = menus.press_active && skip_button_rect().contains(menus.press_pos);
    cinematic.skip_pressed = skip_down;
    story.skip_pressed     = skip_down;
    dialogue_skip_pressed  = skip_down;

    auto render_world = [this]() {
        const BiomeStyle biome = get_biome_style(state.current_chapter);
        renderer.clear(biome.background);

        // Pasada de MUNDO: de aqui a `end_world`, todo se desplaza con la camara. Fuera se
        // quedan la vinieta, el fogonazo, el fundido de sala y el HUD, que son de PANTALLA
        // — sin ese corte se irian con el profeta, porque `world_to_screen` se aplica a todo
        // lo que dibuja el Renderer, `draw_text` incluido.
        renderer.begin_world(camera.position);
        // Solo lo que se ve (mas un margen): la zona son hasta 160x88 tiles y pintarla
        // entera cada frame serian 14.000 celdas por pasada.
        const Rect view = {camera.position.x - Config::GAME_WIDTH * 0.5f,
                           camera.position.y - Config::GAME_HEIGHT * 0.5f,
                           static_cast<f32>(Config::GAME_WIDTH), static_cast<f32>(Config::GAME_HEIGHT)};
        current_room_data.render_ground(renderer, tileset_texture, world.time, view);
        // Telegrafos EN EL SUELO: encima del terreno, debajo de sombras, props y personajes
        render_boss_ground(renderer, world, hazard_art);
        render_hazards(renderer, world, hazard_art, view);

        draw_list.clear();
        current_room_data.collect_props(draw_list, tileset_texture, renderer.shadow_tex,
                                        npc_texture, world.time, view, renderer.ring_tex);
        world.collect_draws(draw_list, renderer);
        collect_chariot_draws(draw_list);
        renderer.draw_sorted(draw_list);
        // Sombras de nubes: sobre el suelo Y lo que hay en el (una nube no distingue)
        ambience_draw_world(renderer, current_room_data, state.current_chapter, view, world.time);
        // Ataduras, curacion, cadena, aturdido: por ENCIMA de los personajes (y de las nubes,
        // que no deben apagar una senal de combate)
        render_boss_overlays(renderer, world, hazard_art);

        current_room_data.render_exit_barriers(renderer, world.time);
        {
            const Vec2 pp = (player.entity != ENTITY_NONE && world.has(player.entity, Component::Transform))
                            ? world.transforms[player.entity].position : camera.position;
            current_room_data.render_event_markers(renderer, world.time, view, pp);
        }

        // Piedra del Memorial: mientras el escudo esta arriba, un aro de piedra a los pies del
        // profeta. Un objeto que protege tiene que VERSE protegiendo.
        if (player.has_shield && player.shield_up && player.entity != ENTITY_NONE &&
            world.has(player.entity, Component::Transform)) {
            const Vec2 pp = world.transforms[player.entity].position;
            const f32 k = 0.5f + 0.5f * std::sin(world.time * 2.6f);
            renderer.draw_ring({pp.x, pp.y + 3.0f}, 11.5f + k, {215, 210, 196, static_cast<u8>(90 + 60 * k)});
        }

        // Floating numbers: dano (blanco / oro de jefe), curacion (verde, "+N"), fe (oro,
        // "+N") y quemadura (naranja). Suben y se apagan sobre quien los causa.
        for (const auto& p : world.popups) {
            f32 k = 1.0f - p.life / 0.7f;
            f32 scale = p.boss ? 0.85f : 0.65f;
            char buf[16];
            const int v = static_cast<int>(p.amount + 0.5f);
            u8 a = static_cast<u8>(255.0f * (k > 0.6f ? 1.0f : k / 0.6f));
            Color c = p.boss ? Color{255, 220, 120, a} : Color{250, 240, 220, a};
            switch (p.kind) {
                case ECSWorld::DamagePopup::Heal:  snprintf(buf, sizeof(buf), "+%d", v); c = {130, 245, 140, a}; scale = 0.75f; break;
                case ECSWorld::DamagePopup::Faith: snprintf(buf, sizeof(buf), tr("+%d fe", "+%d faith"), v); c = {255, 222, 120, a}; scale = 0.6f; break;
                case ECSWorld::DamagePopup::Burn:  snprintf(buf, sizeof(buf), "%d", v); c = {255, 150, 70, a}; scale = 0.55f; break;
                default:                           snprintf(buf, sizeof(buf), "%d", v); break;
            }
            f32 w = renderer.measure_text(buf, scale);
            renderer.draw_text(buf, p.pos.x - w * 0.5f, p.pos.y - 10.0f - 14.0f * p.life, c, scale);
        }
        renderer.end_world();

        // Luz del capitulo y motas (polvo, vilanos, ceniza, pavesas), bajo la vineta
        ambience_draw_screen(renderer, current_room_data, state.current_chapter, camera.position, world.time);
        // La oscuridad de la Muerte (cap. 8): sobre el mundo (y sobre las motas), bajo el HUD
        render_darkness();

        // Soft vignette over the world (under the HUD); margin covers camera shake
        if (renderer.vignette_tex != TEXTURE_NONE) {
            renderer.draw_sprite_rect(renderer.vignette_tex, {0, 0, 160, 90},
                                      {-8, -8, static_cast<f32>(Config::GAME_WIDTH) + 16, static_cast<f32>(Config::GAME_HEIGHT) + 16},
                                      {0, 0, 0, 120});
        }

        // El Fuego del Cielo cayendo: cielo rojo y las bolas POR ENCIMA de todo el mundo
        render_sky_fire();

        // Fogonazo blanco de milagro (Altar Consumido): sobre el mundo, bajo el HUD
        if (world_flash > 0.0f) {
            renderer.draw_rect({0, 0, static_cast<f32>(Config::GAME_WIDTH), static_cast<f32>(Config::GAME_HEIGHT)},
                               {255, 250, 235, static_cast<u8>(235.0f * std::min(1.0f, world_flash / 0.38f))});
        }

        // Bandas de cine de las escenas guionizadas (bajo el HUD y bajo la caja de dialogo)
        if (scene_bars > 0.0f) {
            const f32 k = scene_bars * scene_bars * (3.0f - 2.0f * scene_bars);
            const f32 bh = 11.0f * k;
            renderer.draw_rect({-8, -8, static_cast<f32>(Config::GAME_WIDTH) + 16, bh + 8}, {0, 0, 0, 255});
            renderer.draw_rect({-8, static_cast<f32>(Config::GAME_HEIGHT) - bh, static_cast<f32>(Config::GAME_WIDTH) + 16, bh + 8},
                               {0, 0, 0, 255});
        }

        // Room/chapter change: short fade-in over the world, under the HUD
        if (room_fade > 0.0f) {
            renderer.draw_rect({0, 0, static_cast<f32>(Config::GAME_WIDTH), static_cast<f32>(Config::GAME_HEIGHT)},
                               {0, 0, 0, static_cast<u8>(235.0f * room_fade)});
        }

        HUDContext ctx;
        ctx.floor_map = &floor_map;
        ctx.room = &current_room_data;
        ctx.zone = zone_ptr();
        ctx.boss = boss_entity;
        ctx.boss_hp = boss_bar_hp;
        ctx.boss_hp_max = boss_bar_max;
        ctx.boss_heads = boss_bar_heads;
        ctx.boss_group = boss_group_max > 0.0f;
        ctx.boss_invulnerable = boss_bar_invulnerable;
        ctx.boss_name = boss_bar_name;
        if (boss_entity != ENTITY_NONE && world.is_alive(boss_entity) && world.has(boss_entity, Component::EnemyAI))
            ctx.boss_invul_hint = boss_invulnerable_hint(static_cast<BossPattern>(world.enemy_ais[boss_entity].boss_pattern));
        ctx.chapter = state.current_chapter;
        ctx.banner = banner_timer > 0.0f ? banner_text.c_str() : nullptr;
        ctx.subbanner = subbanner_text.empty() ? nullptr : subbanner_text.c_str();
        ctx.banner_t = banner_timer;
        ctx.message = message_timer > 0.0f ? message_text.c_str() : nullptr;
        ctx.message_t = message_timer;
        ctx.time = world.time;
        // Solo vale si la sala activa ES la zona: dentro de un interior la posicion del
        // profeta esta en coordenadas del interior y en el mapa del capitulo seria mentira.
        if (player.entity != ENTITY_NONE && world.has(player.entity, Component::Transform) &&
            ctx.zone == &current_room_data) {
            ctx.player_pos = world.transforms[player.entity].position;
            ctx.player_in_zone = true;
        }
        ctx.show_map = input.is_action_pressed(Action::Map) || input.touch_map_open;
        ctx.tutorial = tutorial.active;
        ctx.map_revealed = player.map_revealed;
        ctx.tileset = tileset_texture;
        ctx.registry = &item_registry;
        ctx.input    = &input;   // controles tactiles (solo si Input::touch_ui)
        // Ofrendas y dones del Santuario: lo comprado y lo ganado, a la vista
        ctx.offerings = tutorial.active ? -1 : progression.offerings;
        ctx.offering_pop = offering_pop;
        ctx.offering_pop_amount = offering_pop_amount;
        ctx.revives = revives_left;
        ctx.prayers = prayers_left;
        ctx.shield  = player.has_shield ? (player.shield_up ? 1 : 0) : -1;
        ctx.shield_recharge = player.has_shield && !player.shield_up
                            ? std::max(0.0f, std::min(1.0f, player.shield_timer / Player::SHIELD_RECHARGE)) : 0.0f;
        if (pickup_t > 0.0f) {
            ctx.pickup_icon  = pickup_icon;
            ctx.pickup_name  = pickup_name.c_str();
            ctx.pickup_desc  = pickup_desc.empty() ? nullptr : pickup_desc.c_str();
            ctx.pickup_hint  = pickup_hint.empty() ? nullptr : pickup_hint.c_str();
            ctx.pickup_color = pickup_color;
            ctx.pickup_t     = pickup_t;
        }

        // Sanar Aguas: disco en el suelo. El efecto es circular (distancia < 34), asi que
        // se pinta con la elipse suave de las sombras en vez del rectangulo que habia.
        if (player.heal_zone_timer > 0.0f && renderer.shadow_tex != TEXTURE_NONE) {
            f32 pulse = 0.6f + 0.4f * std::sin(world.time * 5.0f);
            f32 r = 34.0f + 2.0f * pulse;
            renderer.draw_sprite_rect(renderer.shadow_tex, {0, 0, 64, 32},
                                      {player.heal_zone_pos.x - r, player.heal_zone_pos.y - r * 0.62f, r * 2, r * 1.24f},
                                      {110, 200, 250, static_cast<u8>(70 + 45 * pulse)});
        }
        hud.render(renderer, player, world, faith, ctx);
    };

    switch (state.scene) {
        case GameScene::Intro:
            cinematic.render(renderer);
            break;

        case GameScene::MainMenu:
            menus.render(renderer);
            break;

        case GameScene::Playing:
            render_world();
            if (tutorial.active) render_tutorial_card();
            break;

        case GameScene::Paused:
            render_world();
            if (equipment_open) render_equipment();
            else                menus.render(renderer);  // Pause overlay on top
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
                                    tileset_texture, extras, input.touch_ui);
            } else {
                const ItemDef* defs[3] = {nullptr, nullptr, nullptr};
                for (int i = 0; i < choice.count; i++) defs[i] = item_registry.get(choice.ids[i]);
                const ItemDef* held = (player.relic_slot != UINT32_MAX)
                                    ? item_registry.get(player.relic_slot) : nullptr;
                draw_item_choice(renderer, choice.title, defs, choice.count, choice.selected,
                                 tileset_texture, state.current_act == Act::Act_I,
                                 held ? held->name : nullptr, input.touch_ui);
                // Echar Suertes (El Santuario): "otras tres" con su contador. Una placa bajo
                // las cartas, que se toca en el movil y anuncia su tecla en el PC.
                choice_reroll_rect = {0, 0, 0, 0};
                if (rerolls_left > 0) {
                    char lab[64];
                    snprintf(lab, sizeof(lab), input.touch_ui ? tr("Echar suertes (%d)", "Cast lots (%d)")
                                                              : tr("[R] Echar suertes (%d)", "[R] Cast lots (%d)"),
                             rerolls_left);
                    const f32 w = renderer.measure_text(lab, 0.65f) + 14.0f;
                    const Rect r = {(static_cast<f32>(Config::GAME_WIDTH) - w) * 0.5f, 136.0f, w, 12.0f};
                    Vec2 pp;
                    const bool down = input.touch_ui && input.ui_press(pp) && r.contains(pp);
                    renderer.draw_panel(r, down ? Color{120, 96, 52, 245} : Color{64, 52, 40, 235}, 3.5f);
                    renderer.draw_text(lab, r.x + 7.0f, r.y + 2.5f, {250, 226, 170, 255}, 0.65f);
                    choice_reroll_rect = r;
                }
            }
            break;
        }

        case GameScene::Story:
            story.render(renderer);
            break;

        case GameScene::Dialogue: {
            if (dialogue.dark) renderer.draw_rect({0, 0, static_cast<f32>(Config::GAME_WIDTH),
                                                   static_cast<f32>(Config::GAME_HEIGHT)}, {8, 6, 12, 255});
            else               render_world();
            // Durante el paseo de una apertura se ve el mundo y NO la caja: lo que hay que
            // mirar es al profeta llegando. El boton de SALTAR si sigue en pantalla.
            if (scene_walk_t > 0.0f) {
                if (input.touch_ui) draw_skip_button(renderer, world.time);
                break;
            }
            EventDef def = get_dialogue_def(dialogue.event_id);
            DialogueView v;
            v.speaker  = def.speaker;
            v.portrait = def.portrait;
            v.touch    = input.touch_ui;
            // Solo una escena guionizada se salta entera: el pie lo dice en PC ([ESC]) y en
            // tactil aparece el boton. Un encuentro del pozo no se salta.
            v.skippable = scene_step >= 0;
            v.skip_pressed = dialogue_skip_pressed;
            if (dialogue.event_id >= 0 && dialogue.event_id < Ev::COUNT && dialogue.event_id == plate_event) {
                v.plate = plate_texture;
                v.open_t = dialogue.t;
            }
            if (dialogue.resolved) {
                v.body = dialogue.result;
                // Y debajo, lo que acaba de pasar: las mismas etiquetas que tenia la opcion
                v.result_chip_count = build_event_chips(dialogue.event_id, dialogue.selected,
                                                        v.result_chips, DIALOGUE_MAX_CHIPS);
            } else if (dialogue.choosing) {
                // Ultima linea visible mientras se decide, para no perder el contexto
                int last = 0;
                while (last + 1 < 3 && def.lines[last + 1]) last++;
                v.body = def.lines[last] ? def.lines[last] : "";
                v.option_count = def.options[1].label ? 2 : 1;
                v.selected = dialogue.selected;
                for (int i = 0; i < v.option_count; i++) {
                    v.options[i] = def.options[i].label;
                    v.chip_count[i] = build_event_chips(dialogue.event_id, i, v.chips[i],
                                                        DIALOGUE_MAX_CHIPS);
                }
            } else {
                v.body = (dialogue.line < 3 && def.lines[dialogue.line]) ? def.lines[dialogue.line] : "";
                v.more_lines = (dialogue.line + 1 < 3) && def.lines[dialogue.line + 1] != nullptr;
            }
            draw_dialogue(renderer, v, tileset_texture, world.time, portrait_texture);
            break;
        }

        case GameScene::Codex:
            codex.render(renderer, save.data, item_registry, tileset_texture, portrait_texture);
            break;

        case GameScene::Sanctuary:
            render_sanctuary();
            break;

        case GameScene::GameOver:
            menus.render(renderer);
            render_run_summary(38.0f);
            break;

        case GameScene::Victory:
            menus.render(renderer);
            render_run_summary(44.0f);
            break;
    }

    // El velo del salto va el ULTIMO: tapa la escena entera, botones incluidos.
    if (scene_fade > 0.0f)
        renderer.draw_rect({0, 0, static_cast<f32>(Config::GAME_WIDTH),
                            static_cast<f32>(Config::GAME_HEIGHT)},
                           {0, 0, 0, static_cast<u8>(245.0f * scene_fade)});

    renderer.end_frame();

    // Vertical (GDD 4.5): los controles tactiles se pintan DESPUES del letterbox, sobre la
    // ventana entera, para poder caer en la banda negra de abajo. En apaisado no hay banda
    // y se dibujan como siempre, dentro del HUD.
    if (input.touch_ui && input.touch_band >= 62.0f) {
        renderer.begin_overlay();
        HUDContext ctx;
        ctx.tileset  = tileset_texture;
        ctx.registry = &item_registry;
        ctx.input    = &input;
        hud.draw_touch_controls(renderer, player, ctx);
        hud.draw_miracle_slots(renderer, player, ctx, faith);
        renderer.end_overlay();
    }
    // La captura va ANTES de present(): en vertical se lee la ventana, y despues de
    // presentar el contenido del backbuffer ya no es fiable.
    if (screenshot_requested) {
        screenshot_requested = false;
        take_screenshot(pending_shot_name.empty() ? nullptr : pending_shot_name.c_str());
        pending_shot_name.clear();
    }

    renderer.present();
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
            refresh_continue_state();
            break;
        case GameScene::Paused:     menus.current = MenuScreen::PauseMenu; break;
        case GameScene::GameOver:
            menus.current = MenuScreen::GameOver;
            menus.option_count = 3;   // render_game_over lo confirma; aqui evita el frame de mas
            menus.sanctuary_ready = progression.cheapest_affordable() != nullptr;
            break;
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
    run_hits_taken = 0;
    feel_mark = FeelMark{};
    has_revive = false;
    miracle_hint_shown = false;
    miracle_hint_timer = 5.0f;

    world.clear();
    world.tileset = tileset_texture;
    faith.init();  // Reset to initial faith

    // Spawn player
    player = Player{};
    player.init_as_elias(world, character_textures[static_cast<u32>(CharacterType::Elias)]);

    apply_meta_upgrades();   // lo comprado en El Santuario entra antes de la primera sala
    // Fe Renovada: la fe de arranque (solo al empezar de cero; Continuar trae la suya)
    if (progression.rank_of(Meta::FE) > 0) {
        faith.current = std::max(faith.current, meta_start_faith(progression.rank_of(Meta::FE)));
        faith.modify(0.0f);
    }
    refill_attempt_gifts();
    // Primicias: la carta de objeto espera a que se juegue (tras la audiencia y la intro)
    primicias_pending = progression.is_unlocked(Meta::PRIMICIAS);

    // El mundo queda listo detras y lo primero que se ve es la audiencia con Acab
    // (1 R 17:1-4, guionizada sobre el palacio, game/scenes.h); al cerrarse, finish_scene lanza la
    // intro cinematica del cap. 1 (o la carta si falta la estampa). El prologo de TEXTO ya
    // no se lee aqui: duplicaba al prologo CINEMATICO que el jugador acaba de ver al
    // arrancar el juego. Bajo autoplay se conserva el flujo de texto integro (lore_check
    // fotografia prologo + carta); la audiencia la fuerza el comando `audiencia`.
    start_chapter(1, ChapterAnnounce::None);
    boss_talk_seen = 0;
    run_events_done = 0;
    pending_scene  = -1;
    boss_approach_t = 0.0f;   // ningun paseo de jefe sobrevive a un reinicio
    if (!autoplay.active) {
        start_scene(Scene::AUDIENCIA);
    } else {
        SDL_Log("Audiencia con Acab suprimida (autoplay): flujo de texto");
        story.begin();
        story_prologue(story);
        story_chapter_card(story, 1);
        play_story(StoryThen::Play);
    }
}

// =============================================================================
// El tutorial (game/tutorial.h): tres salas propias, todo el equipo, sin poder perder
// =============================================================================
// Corre sobre GameScene::Playing con `tutorial.active` puesto y monta sus salas sobre
// `current_room_data`, igual que el escenario de una escena guionizada. Las razones y lo
// que NO puede tocar estan en el bloque de engine.h.

// Tinajas y matorral que quedan en la sala: es la sonda de "he roto algo de un tiro", y
// vale igual para los blancos de la sala 1 y para los escondrijos de la 3 (ecs.cpp abre
// los tres tipos con el mismo impacto).
static int tutorial_count_props(const Room& room) {
    int n = 0;
    for (int y = 0; y < room.tiles_h; y++)
        for (int x = 0; x < room.tiles_w; x++) {
            const TileType t = room.tiles[y][x];
            if (t == TileType::Jar || t == TileType::Bush || t == TileType::TallGrass) n++;
        }
    return n;
}

void Engine::start_tutorial(TutorialThen then) {
    tutorial = TutorialSystem{};
    tutorial.active = true;
    tutorial.then   = then;
    // Nada de lo que pase aqui llega al guardado: se copia entero y finish_tutorial lo
    // devuelve. Mas barato que sembrar guardas por cada sitio que toca la meta al jugar.
    tutorial_save_backup = save.data;

    state = GameState{};
    state.scene = GameScene::Playing;
    state.current_chapter = 1;
    state.current_act = Act::Act_I;
    run_kills = 0;
    run_hits_taken = 0;
    combat_events = CombatEvents{};
    boss_talk_seen  = 0;
    run_events_done = 0;
    pending_scene   = -1;
    scene_id = scene_step = -1;
    boss_approach_t = 0.0f;
    scene_walk_t    = 0.0f;
    prayers_left = 0;
    revives_left = 0;
    rerolls_left = 0;
    primicias_pending = false;
    god_mode = false;
    miracle_hint_shown = true;   // la pista suelta del cap. 1 sobra: aqui hay un paso entero

    world.clear();
    world.tileset = tileset_texture;
    faith.init();

    player = Player{};
    player.init_as_elias(world, character_textures[static_cast<u32>(CharacterType::Elias)]);

    // TODO el equipo puesto (decision del usuario): sin esto, "explicalo todo" y "se valida
    // haciendolo" no caben juntos — no se puede pedir que uses un arma que no llevas.
    player.active_slot_count = Config::MAX_MIRACLE_SLOTS;
    player.miracle_slots[0].equip(MiracleID::CuervosDeQuerit);
    player.miracle_slots[1].equip(MiracleID::PalabraDeSequia);
    player.miracle_slots[2].equip(MiracleID::FuegoDelCielo);
    if (const ItemDef* d = item_registry.get(0)) player.grant_item(world, faith, *d, false);  // Honda de David
    if (const ItemDef* d = item_registry.get(9)) player.grant_item(world, faith, *d, false);  // Agua del Jordán
    // Fuego del Cielo es Tier 3 y pide 50 de fe: con la fe inicial no se podria lanzar el
    // paso que manda lanzarlo.
    faith.current = Config::FAITH_MAX;

    // El minimapa necesita una rejilla que pintar para el paso del mapa. `FloorMap::generate`
    // no consume rand(), y start_new_run vuelve a generar el capitulo 1 entero despues.
    floor_map.generate(1);

    hud.visible = true;
    tutorial_enter_room(0);
    change_scene(GameScene::Playing);
    SDL_Log("Tutorial: empieza (%d pasos, vuelve a %s)", TUTORIAL_STEP_COUNT,
            then == TutorialThen::NewRun ? "la partida" : "el titulo");
}

void Engine::tutorial_enter_room(int index) {
    tutorial.room = index;
    tutorial.door_open = false;
    tutorial.waiting_door = false;

    world.popups.clear();
    world.loot_drops.clear();
    world.hitstop = 0.0f;
    for (u32 e = 0; e < MAX_ENTITIES; e++)
        if (static_cast<Entity>(e) != world.player && world.masks[e].any())
            world.destroy(static_cast<Entity>(e));
    boss_entity = ENTITY_NONE;
    reset_boss_bar();

    tutorial_build_room(current_room_data, index, 1);
    world.active_room = &current_room_data;
    camera.set_room({0, 0, current_room_data.world_w(), current_room_data.world_h()}, true);

    int tx = 0, ty = 0;
    tutorial_spawn_tile(index, tx, ty);
    place_player(current_room_data.tile_to_world(tx, ty));
    room_fade = 1.0f;
    room_change_grace = Config::Feel::COYOTE_TIME;   // el Doorway recien pisado no rebota
    player.took_damage_this_room = false;
    // Sala nueva, profeta entero: el arquero del paso del dash deja media vida por el camino y
    // llegar tocado a la sala de la fe no significa nada aqui (esto no es una partida).
    if (player.entity != ENTITY_NONE && world.has(player.entity, Component::Health)) {
        auto& h = world.healths[player.entity];
        h.current = h.max;
    }
    faith.current = Config::FAITH_MAX;   // los tres milagros tienen que poder lanzarse

    tutorial.prev_props = tutorial_count_props(current_room_data);
    SDL_Log("Tutorial: sala %d de 3", index + 1);
}

// Sondas del frame: el tutorial MIRA el mundo en vez de que medio motor tenga que avisarle.
// Se refrescan siempre, tambien mientras corre el visto bueno de un paso.
void Engine::tutorial_probe() {
    tutorial.prev_secondary_cd = player.secondary_cd;
    for (int i = 0; i < 3; i++) tutorial.prev_cooldown[i] = player.miracle_slots[i].cooldown_remaining;
    tutorial.prev_charges = 0;
    for (int i = 0; i < Player::BELT_MAX; i++) tutorial.prev_charges += player.belt[i].empty() ? 0 : player.belt[i].charges;
}

bool Engine::tutorial_goal_done(TutorialGoal goal, f32 dt) {
    const Room& room = current_room_data;
    const bool has_player = player.entity != ENTITY_NONE;

    switch (goal) {
        case TutorialGoal::Move: {
            if (!has_player || !world.has(player.entity, Component::Movement)) return false;
            const Vec2 v = world.movements[player.entity].velocity;
            tutorial.acc += std::sqrt(v.x * v.x + v.y * v.y) * dt;
            // 220 unidades (~2,2 s de andar de verdad) y no 90: con 90 el paso se cerraba
            // en 0,9 s, o de un solo dash suelto (75), antes de que nadie leyera el cartel.
            return tutorial.acc >= 220.0f;
        }
        case TutorialGoal::Attack: {
            // Props de la sala 1, donde los unicos que hay son las 5 tinajas plantadas.
            const int left = tutorial_count_props(room);
            tutorial.progress = std::max(tutorial.progress, tutorial.prev_props - left);
            return tutorial.progress >= tutorial.current().target;
        }
        case TutorialGoal::Secrets:
            // Sonda PROPIA, por posicion de tile: en el exterior el bioma decora con
            // matorral y contar props cerraba el paso con dos tiros perdidos.
            return tutorial_secrets_opened(room) >= tutorial.current().target;
        case TutorialGoal::Secondary:
            return tutorial.prev_secondary_cd <= 0.0f && player.secondary_cd > 0.0f;
        case TutorialGoal::Dash:
            return has_player && world.has(player.entity, Component::Movement) &&
                   world.movements[player.entity].is_dashing;
        case TutorialGoal::Consumable:
        {
            int now = 0;
            for (int i = 0; i < Player::BELT_MAX; i++) now += player.belt[i].empty() ? 0 : player.belt[i].charges;
            return now < tutorial.prev_charges;
        }
        case TutorialGoal::Miracle1:
        case TutorialGoal::Miracle2:
        case TutorialGoal::Miracle3: {
            // El flanco del enfriamiento, y no `Player::last_event`: el bloque de Playing lo
            // limpia varias lineas antes de que esto llegue a mirarlo.
            const int slot = static_cast<int>(goal) - static_cast<int>(TutorialGoal::Miracle1);
            return tutorial.prev_cooldown[slot] <= 0.0f &&
                   player.miracle_slots[slot].cooldown_remaining > 0.0f;
        }
        case TutorialGoal::Pray:   return room.cells[0].altar_used;
        case TutorialGoal::Chest:  return room.cells[0].chest_opened;
        case TutorialGoal::Offering: return tutorial.offerings_picked > 0;
        case TutorialGoal::Map:    return input.is_action_pressed(Action::Map) || input.touch_map_open;
        // Las dos que el motor apunta a mano, porque pasan con el juego fuera de Playing:
        // la pausa (change_scene a Paused) y el arco (update_room_state).
        case TutorialGoal::Pause:     return tutorial.pause_seen;
        case TutorialGoal::CrossExit: return tutorial.exit_reached;
        default:                   return false;
    }
}

// ¿Queda alguna ofrenda en el suelo? TODA recogida caduca a los 14 s (spawn_pickup), asi
// que la del paso de las ofrendas hay que sembrarla cuando ESE paso empieza y reponerla si
// el jugador se entretiene: sembrada al entrar en la sala se esfumaba tres pasos antes.
static bool tutorial_offering_alive(const ECSWorld& world) {
    for (u32 e = 0; e < MAX_ENTITIES; e++)
        if (world.masks[e].test(static_cast<size_t>(Component::Item)) &&
            world.items[e].type == ItemComponent::Type::Pickup &&
            world.items[e].pickup == ItemComponent::Pickup::Offering)
            return true;
    return false;
}

// Lo que el paso que acaba de abrirse necesita EN EL MUNDO.
void Engine::tutorial_setup_step() {
    const TutorialGoal goal = tutorial.current().goal;
    const Vec2 at = current_room_data.tile_to_world(current_room_data.tiles_w / 2 - 4, 4);

    // La ofrenda de su paso: a mano y en un sitio FIJO de la sala, porque el hallazgo normal
    // es un sorteo y un paso del tutorial no puede depender de que toque. Fija y no pegada al
    // profeta: el paso es ir a por ella, y ademas asi el guion sabe adonde caminar.
    if (goal == TutorialGoal::Offering && !tutorial_offering_alive(world)) {
        int tx = 0, ty = 0;
        tutorial_spawn_tile(2, tx, ty);
        // A la ALTURA del profeta: tres filas mas arriba caia detras del cartel y el paso
        // pedia recoger algo que no se veia (se vio en la captura inglesa).
        spawn_pickup(world, current_room_data.tile_to_world(tx + 3, ty - 1),
                     ItemComponent::Pickup::Offering, 3.0f);
    }

    // Las tinajas del paso de atacar, REPLANTADAS. La sala 1 solo trae cinco y el paso pide
    // romper dos: quien se entretiene a tiros durante el paso de moverse llegaba aqui con
    // cuatro o cinco rotas y el objetivo ya era inalcanzable. Ningun paso se cierra solo, asi
    // que eso colgaba el tutorial para siempre.
    if (goal == TutorialGoal::Attack) {
        tutorial_plant_jars(current_room_data);
        // Y el recuento se rehace DESPUES de sembrar: el bucle de update_tutorial fija
        // prev_props antes de llamar aqui, asi que sin esto la replantacion dejaria el
        // contador desfasado y el paso pediria romper mas tinajas de la cuenta.
        tutorial.prev_props = tutorial_count_props(current_room_data);
    }

    // La pocion del paso del consumible, por lo mismo: solo hay una carga, se da al arrancar
    // y quien se la bebio antes de que se la pidieran dejaba el paso sin nada que usar. La
    // guarda evita apilar una segunda carga a quien todavia la lleve.
    if (goal == TutorialGoal::Consumable && player.belt_count() == 0) {
        if (const ItemDef* d = item_registry.get(9)) player.grant_item(world, faith, *d, false);
    }

    // El arquero del paso del dash: dispara de verdad, que es lo que hace que esquivar
    // signifique algo. Y el mordisco que te lleva deja el paso siguiente de la sala (el
    // consumible) con algo que curar.
    if (goal == TutorialGoal::Dash)
        spawn_enemy(world, EnemyType::ArqueroReal, at, enemy_textures[static_cast<u32>(EnemyType::ArqueroReal)], 1);

    if (goal == TutorialGoal::Miracle1 || goal == TutorialGoal::Miracle2 ||
        goal == TutorialGoal::Miracle3) {
        // La FE, y esto no es un lujo desde que ningun paso se cierra solo: Fuego del Cielo
        // es Tier 3 (pide 50) y cuesta 20, asi que llegar aqui con la fe mordida por el
        // arquero —o gastada en los dos milagros anteriores— dejaba el paso IMPOSIBLE y el
        // tutorial colgado para siempre. Se repone entera al abrirse cada paso de milagro:
        // el tutorial ensena a lanzarlos, no a administrar la fe.
        if (faith.current < Config::FAITH_MAX) faith.current = Config::FAITH_MAX;
        // Y a quien lanzarselos. Se repone solo si no queda nadie: sin esto, el que se
        // despacha con el primer milagro deja los dos pasos siguientes sin blanco.
        if (world.count_living_enemies(0) == 0)
            spawn_enemy(world, EnemyType::SoldadoDeAcab, at, enemy_textures[static_cast<u32>(EnemyType::SoldadoDeAcab)], 1);
    }
}

void Engine::update_tutorial(f32 dt) {
    if (!tutorial.active) return;

    // El tutorial no se puede perder: la vida se sujeta ANTES de que la comprobacion de
    // muerte del bloque de Playing pueda verla en cero.
    if (player.entity != ENTITY_NONE && world.has(player.entity, Component::Health)) {
        auto& h = world.healths[player.entity];
        if (h.current < 1.0f) h.current = 1.0f;
    }

    // NADA se evalua fuera de Playing, y esto es lo que arregla el orden del cofre: pisarlo
    // llama a open_item_choice, que fija `state.scene` EN EL ACTO (change_scene), asi que el
    // mismo frame en que se abre la carta el tutorial ya no mira nada. El paso `Chest` se
    // cierra al VOLVER de la pantalla de eleccion, con la carta ya en la mano. Igual valen
    // el altar del paso `Pray` y la pausa.
    if (state.scene != GameScene::Playing) return;

    // Saltarlo en tactil: el mismo boton de SALTAR de las escenas. En PC se sale por la
    // pausa (ESC ya es Pausa aqui, y el paso 14 la enseña: no se le puede dar otro oficio).
    Vec2 tp;
    if (input.touch_ui && input.peek_tap(tp) && tutorial_skip_rect().contains(tp)) {
        input.take_tap(tp);
        SDL_Log("Tutorial: saltado en el paso %d", tutorial.step + 1);
        finish_tutorial(false);
        return;
    }

    if (tutorial.finished()) { finish_tutorial(true); return; }

    // Puerta abierta y sin cruzar: no se evalua ningun paso hasta estar en la sala nueva.
    if (tutorial.waiting_door) { tutorial_probe(); return; }

    // Visto bueno entre un paso y el siguiente, para que se vea que ha entrado.
    if (tutorial.done_t > 0.0f) {
        tutorial.done_t -= dt;
        tutorial_probe();
        if (tutorial.done_t <= 0.0f) {
            const int prev_room = tutorial.current().room;
            tutorial.step++;
            tutorial.step_t = 0.0f;
            tutorial.acc = 0.0f;
            tutorial.progress = 0;
            tutorial.prev_props = tutorial_count_props(current_room_data);
            if (tutorial.finished()) { finish_tutorial(true); return; }
            if (tutorial.current().room != prev_room) {
                tutorial_open_north_door(current_room_data);
                tutorial.door_open = true;
                tutorial.waiting_door = true;
                audio.play_sfx(SfxID::DoorOpen, 0.9f);
            } else {
                tutorial_setup_step();
            }
        }
        return;
    }

    const TutorialStep& st = tutorial.current();
    // La ofrenda caduca a los 14 s: mientras su paso siga abierto, se repone. Un jugador que
    // se entretiene mirando el mapa no puede quedarse sin el objeto que le estan pidiendo.
    if (st.goal == TutorialGoal::Offering && tutorial.step_t > 0.0f && !tutorial_offering_alive(world))
        tutorial_setup_step();
    tutorial.step_t += dt;
    const bool done = tutorial_goal_done(st.goal, dt);
    tutorial_probe();
    if (done) {
        tutorial.done_t = 0.9f;
        audio.play_sfx(SfxID::Notification, 0.7f);
        SDL_Log("Tutorial: paso %d/%d cumplido", tutorial.step + 1, TUTORIAL_STEP_COUNT);
    }
}

// Desde la pantalla de Controles se vuelve al titulo: quien solo iba a repasar los controles
// no puede perder la partida que tenia a medias. Separado de finish_tutorial porque tambien
// es la salida de las paginas de cierre (StoryThen::TutorialToTitle).
void Engine::tutorial_back_to_title() {
    world.clear();
    player = Player{};
    hud.visible = true;
    refresh_continue_state();
    change_scene(GameScene::MainMenu);
    menus.current = MenuScreen::Title;
    menus.selected_option = 0;
}

void Engine::finish_tutorial(bool completed) {
    const TutorialThen then = tutorial.then;
    SDL_Log("Tutorial: fin (paso %d de %d)", std::min(tutorial.step + 1, TUTORIAL_STEP_COUNT),
            TUTORIAL_STEP_COUNT);
    tutorial = TutorialSystem{};   // active = false

    // La meta vuelve a estar como antes de empezar: ni ofrendas, ni logros, ni contadores.
    save.data = tutorial_save_backup;
    sync_progression_from_save();

    // Terminado: las dos paginas de cierre cuentan lo que no era una accion (la vida, la fe
    // y sus grados, las ofrendas) y son la pantalla que faltaba antes de Acab y Jezabel. El
    // destino viaja en el StoryThen porque `tutorial` ya esta limpio a estas alturas.
    if (completed) {
        story.begin();
        story_tutorial_outro(story);
        play_story(then == TutorialThen::NewRun ? StoryThen::TutorialToRun
                                                : StoryThen::TutorialToTitle);
        return;
    }

    // Saltado: quien pulsa SALTAR pide salir, no leer dos pantallas mas.
    if (then == TutorialThen::NewRun) {
        start_new_run();
        return;
    }
    tutorial_back_to_title();
}

void Engine::start_chapter(int chapter, ChapterAnnounce announce) {
    state.current_chapter = chapter;
    boss_hp_scale = 1.0f;   // el encuentro de cada capitulo se gana de nuevo
    relic_revive_ready = true;   // el Reloj de Acaz perdona una muerte POR capitulo — se
                                 // re-arma aqui para cubrir tambien avanzar y Continuar
    player.map_revealed = false;   // "Ojos Abiertos" revela ESTE capitulo, no los siguientes
    player.reveal_timer = 0.0f;
    boss_intel_shown = -1;         // el rastro del jefe se vuelve a aprender en cada capitulo
    codex_see_chapter(save.data, chapter);
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
    // Re-sorteos, oraciones sin altar y el agua de la tinaja: por capitulo. (Continuar los
    // pisa despues con lo que quedaba al guardar.)
    refill_chapter_gifts();

    // Mundo FIJO (2026-08-27): la semilla es una CONSTANTE por capitulo, no un sorteo por
    // partida. `pending_seed` (Continuar una partida guardada con su semilla de entonces) y
    // `forced_seed` (comando `seed` del autoplay) siguen mandando como override, en ese orden.
    int seed_slot = std::min(std::max(chapter, 1), 10) - 1;
    chapter_seed = pending_seed ? pending_seed
                 : forced_seed  ? forced_seed
                                : Config::CHAPTER_SEEDS[seed_slot];
    pending_seed = 0;
    srand(chapter_seed);

    // Estado de sala guardado (v9): solo se re-aplica cuando este arranque ES un Continuar.
    apply_saved_rooms = restore_room_state;
    restore_room_state = false;

    floor_map.generate(chapter);
    load_boss_sheets(chapter);

    boss_entity = ENTITY_NONE;
    reset_boss_bar();
    spawn_tx = spawn_ty = -1;
    // Construye la ZONA entera (y, al Continuar, re-aplica el estado guardado de cada
    // celda) y deja al profeta en el nudo de la celda de inicio.
    load_room(floor_map.current_room);
    // Continuar (v9/v10): el interior con estado guardado se pre-genera y restaura YA, para
    // que su altar rezado o su cofre abierto no vuelvan a nacer sin usar.
    if (apply_saved_rooms) {
        for (int i = floor_map.grid_w * floor_map.grid_h; i < floor_map.room_count && i < SAVE_MAX_ROOMS; i++) {
            if (floor_map.rooms[i].type != RoomType::Interior || save.data.room_flags[i] == 0) continue;
            generate_pristine_cell(i, floor_map.room_cache[1]);
            apply_saved_interior_state(i, floor_map.room_cache[1]);
            floor_map.room_generated[i] = true;
        }
    }

    // `announce`: que se lee antes de jugar (el mundo ya esta cargado detras).
    // `Cinematic` = la intro animada de la primera entrada natural; bajo autoplay o sin
    // estampa entregada cae a la carta de siempre, para que los guiones no vean cambio.
    // La APERTURA guionizada va antes que la intro (game/scenes.h): al cerrarse,
    // finish_scene lanza la intro (o la carta). El cap. 1 la lanza start_new_run.
    if (announce == ChapterAnnounce::Cinematic && !autoplay.active && chapter >= 2 &&
        Scene::opening_of(chapter) >= 0) {
        start_scene(Scene::opening_of(chapter));
    } else if (announce == ChapterAnnounce::Cinematic && !autoplay.active &&
        cinematic_chapter_ready(chapter)) {
        start_chapter_intro(chapter);
    } else if (announce != ChapterAnnounce::None) {
        if (announce == ChapterAnnounce::Cinematic) {
            if (autoplay.active && chapter >= 2)
                SDL_Log("Apertura del capitulo %d suprimida (autoplay)", chapter);
            SDL_Log("Intro de capitulo %d suprimida (%s): carta de texto", chapter,
                    autoplay.active ? "autoplay" : "sin estampa");
        }
        story.begin();
        story_chapter_card(story, chapter);
        play_story(StoryThen::Play);
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
        award_offerings(true);

        // "El Legado" es contenido DESBLOQUEBLE (GDD): se gana derrotando a Ben-Adad con
        // el celo intacto — fe en Profeta Ungido (75) o mas. Quien gane con la fe por los
        // suelos se lleva su pantalla de victoria igual, pero no el epilogo: es el motivo
        // para volver a jugar. Una vez desbloqueado lo es para siempre (SaveData::unlocks).
        bool zeal = faith.current >= faith_required_for_tier(4);
        award(AchievementID::Victoria);
        if (zeal) { save.data.unlocks |= UNLOCK_EPILOGUE; award(AchievementID::Epilogo); }
        if (save.data.unlocks & UNLOCK_EPILOGUE) {
            SDL_Log("Victory: epilogue unlocked (faith %.0f)", faith.current);
            save.save();
            menus.victory_note = nullptr;
            // El epilogo tambien se RECORRE: tres escenas con decision antes de la pagina
            // de cierre, igual que el Ascenso. Cierra los dos arcos de los profetas:
            // Jehu/Jezabel (la palabra de Elias), el arco de Joas y los huesos de Eliseo.
            epilogue_step = 0;
            joash_full = true;
            audio.play_music(MusicID::Victory);
            open_dialogue(Ev::EPI_JEHU);
            dialogue.dark = true;
        } else {
            SDL_Log("Victory without the epilogue (faith %.0f < 75)", faith.current);
            save.save();
            menus.victory_note = tr("El epílogo se queda sin contar: vuelve con la fe en alto.", "The epilogue goes untold: come back with faith high.");
            finish_story_action(StoryThen::VictoryMenu);
        }
        return;
    }
    start_chapter(next, ChapterAnnounce::Cinematic);   // primera entrada natural: la intro
}

void Engine::retry_chapter() {
    // Same prophet, fresh HP and starting faith, chapter regenerated.
    // El equipo del run SOBREVIVE al reintento: morir ya cuesta el capitulo entero, y
    // volver a empezarlo desnudo dejaba el reintento mas duro que la partida perdida.
    int chapter = state.current_chapter;
    CharacterType ch = player.character;

    // Se guarda por id y se vuelve a otorgar: asi los bonus de HP/dano/velocidad/fe se
    // reaplican sobre el profeta recien creado en vez de perderse con el Player anterior.
    std::vector<u32> kept_items   = player.items;
    Player::BeltSlot kept_belt[Player::BELT_MAX];
    for (int i = 0; i < Player::BELT_MAX; i++) kept_belt[i] = player.belt[i];
    const int kept_sel = player.belt_sel;
    MiracleID kept_miracles[Config::MAX_MIRACLE_SLOTS];
    for (int s = 0; s < Config::MAX_MIRACLE_SLOTS; s++) kept_miracles[s] = player.miracle_slots[s].miracle;

    has_revive = false;
    world.clear();
    world.tileset = tileset_texture;
    faith.init();
    player = Player{};
    if (ch == CharacterType::Elias) player.init_as_elias(world, character_textures[static_cast<u32>(CharacterType::Elias)]);
    else                            player.init_as_eliseo(world, character_textures[static_cast<u32>(CharacterType::Eliseo)]);
    apply_meta_upgrades();
    if (progression.rank_of(Meta::FE) > 0) {
        faith.current = std::max(faith.current, meta_start_faith(progression.rank_of(Meta::FE)));
        faith.modify(0.0f);
    }
    refill_attempt_gifts();   // un intento nuevo: el Aliento de Vida vuelve entero

    for (u32 id : kept_items) {
        const ItemDef* d = item_registry.get(id);
        if (d) player.grant_item(world, faith, *d);
    }
    // Recalcular la reliquia SIEMPRE al rehacer el equipo: sus efectos no viven en el
    // Player sino en FaithSystem::floor_value y Player::miracle_power, y en la Ascension
    // la fe NO se reinicia — sin esto, un Elias con las Tablas dejaba a Eliseo con el
    // suelo de fe en 25 para siempre, sin llevar la reliquia.
    player.apply_relic_effects(faith);
    // La alforja sobrevive al reintento, como el resto del equipo
    for (int i = 0; i < Player::BELT_MAX; i++) player.belt[i] = kept_belt[i];
    player.belt_sel = kept_sel;
    player.belt_fix_selection();
    for (int s = 0; s < Config::MAX_MIRACLE_SLOTS; s++)
        if (kept_miracles[s] != MiracleID::COUNT) player.miracle_slots[s].equip(kept_miracles[s]);

    SDL_Log("Retry chapter %d: %d item(s) kept", chapter, static_cast<int>(kept_items.size()));
    // Card, no Cinematic: el reintento no es una primera entrada, y repetir la intro en
    // cada muerte cansaria. La carta lleva de vuelta a Playing.
    start_chapter(chapter);
}

void Engine::trigger_ascension() {
    SDL_Log("THE ASCENSION - Gilgal, Betel, Jerico, Jordan");
    state.current_act = Act::Act_II;
    // Ya no son cinco lineas de texto: son las CUATRO PARADAS del pasaje, con decision en
    // cada una. Tres veces le dice que se quede y tres veces hay que negarse — eso es lo que
    // 2 Reyes 2 repite, asi que es lo que el jugador repite.
    ascent_step = 0;
    ascent_follows = 0;
    ascent_double = true;
    audio.play_music(MusicID::Ascension);
    open_dialogue(Ev::ASC_GILGAL);
    dialogue.dark = true;
}

// Siguiente escena del epilogo, o la pagina de cierre si ya no quedan.
void Engine::epilogue_next() {
    epilogue_step++;
    if (epilogue_step < Ev::EPILOGUE_STOPS) {
        open_dialogue(Ev::EPI_JEHU + epilogue_step);
        dialogue.dark = true;
        return;
    }
    epilogue_step = -1;
    SDL_Log("Epilogo: el arco de Joas %s", joash_full ? "hasta el final" : "solo tres veces");
    story.begin();
    story_epilogue(story, joash_full);
    play_story(StoryThen::VictoryMenu);
}

// =============================================================================
// Escenas guionizadas (game/scenes.h)
// =============================================================================
// La historia contada con el motor del gameplay. Un escenario montado con las piezas de
// siempre —una sala del bioma (interior de silleria o exterior con un claro), figuras de
// pie (`spawn_figure` / `spawn_npc_figure`), el profeta delante, HUD apagado— y una cadena
// de cajas de `get_scene_line` sin decision. Se reutiliza GameScene::Dialogue: ningun
// GameScene nuevo. Las aperturas se llaman con start_chapter YA hecho: el mapa esta
// generado y la sala inicial cacheada, asi que al cerrarse basta con load_room para
// volver a ella tal como estaba (el escenario nunca se cachea ni se guarda). Los dialogos
// de jefe (`StageKind::Live`) no montan nada: congelan la arena con el jefe ya de pie.
void Engine::start_scene(int id) {
    if (id < 0 || id >= Scene::COUNT) return;
    const SceneDef def = get_scene_def(id);
    scene_id      = id;
    scene_step    = -1;
    pending_scene = -1;
    boss_approach_t = 0.0f;   // ningun paseo de jefe sobrevive a un reinicio

    for (Entity& fe : scene_figures) fe = ENTITY_NONE;
    int ax, ay;
    current_room_data.arena_anchor(ax, ay);   // ancla: en la arena, el tile del jefe
    if (def.stage != StageKind::Live) {
        // El escenario se genera SOBRE current_room_data: la zona se guarda antes de
        // pisarla (finish_scene -> load_room la restaura de la ranura 0).
        if (current_room_data.type == RoomType::Zone) floor_map.room_cache[0] = current_room_data;
        // start_chapter ya sembro lo que hubiera en la sala inicial: saldria dibujado
        // dentro del escenario. Se barre igual que hace load_room.
        for (u32 e = 0; e < MAX_ENTITIES; e++) {
            if (static_cast<Entity>(e) != world.player && world.masks[e].any())
                world.destroy(static_cast<Entity>(e));
        }
        boss_entity = ENTITY_NONE;
        reset_boss_bar();
        clear_hazards(world);
        world.popups.clear();

        Room& room = current_room_data;
        const int biome_chapter = def.stage_chapter ? def.stage_chapter : state.current_chapter;
        // Sin puertas: de aqui no se sale andando, y sin aberturas no hay sendas que crucen
        room.door_north = room.door_south = room.door_east = room.door_west = false;
        if (def.stage == StageKind::Interior) {
            room.generate(RoomType::Interior, biome_chapter, 1, -1, false);
            // No hay trono en el tileset: fuera el altar (o el cofre) del fondo
            room.tiles[2][room.tiles_w / 2] = TileType::Floor;
            ax = room.tiles_w / 2;
            ay = 4;
        } else {
            room.generate(RoomType::Normal, biome_chapter, 1);
            ax = room.tiles_w / 2;
            ay = room.tiles_h / 2;
            // Un claro para la escena: la decoracion del bioma no tapa a nadie
            for (int y = ay - 4; y <= ay + 3; y++)
                for (int x = ax - 7; x <= ax + 7; x++)
                    if (x > 0 && y > 0 && x < room.tiles_w - 1 && y < room.tiles_h - 1)
                        room.tiles[y][x] = TileType::Floor;
        }
        // Con paseo (SceneDef::walk_after) el profeta ARRANCA fuera del claro: se abre
        // tambien su pasillo, o llegaria atascado en un matorral o metido en una charca.
        if (def.walk_after >= 0) {
            const int sx = ax + def.player_dx + def.walk_from_dx;
            const int sy = ay + def.player_dy + def.walk_from_dy;
            const int ex = ax + def.player_dx, ey = ay + def.player_dy;
            for (int y = std::min(sy, ey) - 2; y <= std::max(sy, ey) + 2; y++)
                for (int x = std::min(sx, ex) - 3; x <= std::max(sx, ex) + 3; x++)
                    if (x > 0 && y > 0 && x < room.tiles_w - 1 && y < room.tiles_h - 1)
                        room.tiles[y][x] = TileType::Floor;
        }
        room.door_north = room.door_south = room.door_east = room.door_west = false;
        room.has_doorway = false;
        room.enemy_spawns.clear();
        room_fade = 1.0f;   // el escenario entra desde negro: un corte seco al decorado se leia como un salto
        room.cells[0].cleared = true;
        room.cells[0].visited = true;
        world.active_room = &room;
        camera.set_room({0, 0, room.world_w(), room.world_h()});
    }

    // Las figuras. En la arena tambien (Acab y Jezabel flanqueando a su arquero): las de
    // una escena Live se retiran en finish_scene, las demas las barre load_room.
    for (int i = 0; i < 3; i++) {
        const SceneFigure& f = def.figures[i];
        if (f.kind == FigureKind::None) continue;
        const Vec2 fp = current_room_data.tile_to_world(ax + f.dx, ay + f.dy);
        Entity fe = ENTITY_NONE;
        switch (f.kind) {
            case FigureKind::Acab:    fe = spawn_figure(world, fp, acab_texture, f.facing); break;
            case FigureKind::Jezabel: fe = spawn_figure(world, fp, jezabel_texture, f.facing); break;
            case FigureKind::Enemy:
                if (f.index >= 0 && f.index < static_cast<int>(EnemyType::COUNT))
                    fe = spawn_figure(world, fp, enemy_textures[f.index], f.facing);
                break;
            case FigureKind::Npc:     fe = spawn_npc_figure(world, fp, npc_texture, f.index); break;
            default: break;
        }
        scene_figures[i] = fe;
    }

    // El profeta se coloca cara a quien habla (en la arena, tres filas bajo el jefe: donde
    // entro por el borde sur la caja lo tapaba entero) y se hornea su fila de reposo,
    // porque system_animation no corre en Dialogue y se quedaria a medio paso.
    // El paseo: el destino es el sitio de SIEMPRE y el profeta arranca desplazado. Se
    // apunta aqui porque `ax/ay` solo existen en esta funcion —el escenario los recalcula
    // al generarse— y fuera no hay forma de rehacerlos.
    scene_walk_t  = 0.0f;
    scene_walked  = false;
    scene_walk_to = current_room_data.tile_to_world(ax + def.player_dx, ay + def.player_dy);

    if (player.entity != ENTITY_NONE && world.has(player.entity, Component::Transform)) {
        auto& t = world.transforms[player.entity];
        t.position = (def.walk_after >= 0)
                   ? current_room_data.tile_to_world(ax + def.player_dx + def.walk_from_dx,
                                                     ay + def.player_dy + def.walk_from_dy)
                   : scene_walk_to;
        t.facing = def.player_facing;
        if (world.has(player.entity, Component::Movement)) {
            world.movements[player.entity].velocity  = {0, 0};
            world.movements[player.entity].knockback = {0, 0};
        }
        const AnimID idle = facing_anim(AnimID::IdleDown, def.player_facing);
        if (world.has(player.entity, Component::Animation))
            play_anim(world.animations[player.entity], idle, true);
        if (world.has(player.entity, Component::Sprite)) {
            auto& sp = world.sprites[player.entity];
            sp.src_rect.x = 0.0f;
            sp.src_rect.y = static_cast<f32>(get_anim_def(idle).row * SHEET_FRAME_SIZE);
            sp.flip_x = false;
        }
        if (def.stage != StageKind::Live) {
            camera.set_target(t.position);
            camera.position = t.position;
            camera.update(0.0f);
        }
    }

    hud.visible = false;   // render_world pinta el HUD tambien en Dialogue
    SDL_Log("%s", def.name);
    scene_next();
}

// ¿Se resolvio ese relato del pozo EN ESTA PARTIDA? Dos fuentes: la mascara de la sesion
// (tambien la llena el comando `event` del autoplay) y `event_used` de la sala que lo cuenta
// (lo que sobrevive al Continuar: start_chapter rehidrata room_cache desde el v9).
bool Engine::event_resolved_in_run(int ev) const {
    if (ev < 0 || ev >= 64) return false;
    if ((run_events_done >> ev) & 1ull) return true;
    const Room* zone = zone_ptr();
    if (!zone) return false;
    for (int i = 0; i < zone->cell_count && i < floor_map.room_count; i++) {
        const auto& node = floor_map.rooms[i];
        if (node.type == RoomType::Event && node.event_id == ev && zone->cells[i].event_used) return true;
    }
    return false;
}

// Siguiente caja de la escena, o el cierre si ya no quedan.
void Engine::scene_next() {
    const SceneDef def = get_scene_def(scene_id);
    // Paseo cinematico en la juntura de la cadena (SceneDef::walk_after): en vez de abrir
    // la caja siguiente, el profeta CAMINA hasta su sitio y solo entonces se sigue.
    // `scene_walked` es lo que evita repetirlo, porque al terminar el paseo se vuelve a
    // pasar por aqui con la misma parada.
    if (def.walk_after >= 0 && !scene_walked && scene_step == def.walk_after) {
        scene_walked = true;
        scene_walk_t = Config::Feel::SCENE_WALK_MAX;
        SDL_Log("%s: paseo tras la parada %d/%d", def.tag, scene_step + 1, def.count);
        return;
    }
    scene_step++;
    if (scene_step < def.count) {
        int line = def.first_line + scene_step;
        // El jefe responde a lo jugado: primera caja alternativa si el relato se resolvio
        const bool variant = (scene_step == 0 && def.alt_first_line >= 0 && event_resolved_in_run(def.alt_event));
        if (variant) line = def.alt_first_line;
        open_dialogue(Sc::LINE_BASE + line);              // dark = false: se ve el escenario
        if (scene_manual) dialogue.auto_timer = 0.0f;     // parada, como chintro
        SDL_Log("%s: parada %d/%d (id %d)%s", def.tag, scene_step + 1, def.count, line, variant ? " variante" : "");
        return;
    }
    finish_scene(false);
}

// Cierre (completa o saltada con ESC). Una apertura vuelve la sala real desde room_cache
// —load_room barre las figuras, recoloca profeta y camara y re-siembra lo que hubiera— y
// arranca la intro del capitulo; un dialogo de jefe deja la arena como estaba y suelta
// al mundo.
void Engine::finish_scene(bool skipped) {
    const SceneDef def = get_scene_def(scene_id);
    SDL_Log("%s terminada (%s)", def.tag, skipped ? "saltada" : "completa");
    scene_id     = -1;
    scene_step   = -1;
    scene_manual = false;
    hud.visible  = true;
    // Ningun paseo sobrevive al cierre (ni saltado a mitad): si el profeta se quedara con
    // velocidad, seguiria andando solo en cuanto el mundo vuelva a correr.
    scene_walk_t = 0.0f;
    scene_walked = false;
    if (player.entity != ENTITY_NONE && world.has(player.entity, Component::Movement))
        world.movements[player.entity].velocity = {0, 0};

    if (def.stage == StageKind::Live) {
        // Las figuras se retiran (el rey y la reina dejan la pelea a su guardia)
        for (Entity& fe : scene_figures) {
            if (fe != ENTITY_NONE && world.masks[fe].any()) world.destroy(fe);
            fe = ENTITY_NONE;
        }
        // El rotulo del jefe se perdio detras de la caja: vuelve, que ahora si empieza
        if (boss_entity != ENTITY_NONE)
            show_banner(tr("JEFE", "BOSS"), get_chapter_def(state.current_chapter).boss_name, 2.5f);
        // Un respiro: el profeta esta a tres tiles del jefe y el mundo arranca de golpe.
        // Sin esto el arquero del cap. 5 acertaba en el primer paso (visto en escenas_check).
        if (player.entity != ENTITY_NONE && world.has(player.entity, Component::Health))
            world.healths[player.entity].iframes_timer = std::max(world.healths[player.entity].iframes_timer, 1.5f);
        change_scene(GameScene::Playing);
        return;
    }

    if (player.entity != ENTITY_NONE && world.has(player.entity, Component::Transform))
        world.transforms[player.entity].facing = Direction::Down;
    spawn_tx = spawn_ty = -1;   // como lo deja start_chapter: aparece en el nudo de su celda
    load_room(floor_map.current_room);

    if (def.then == SceneThen::ChapterIntro) {
        const int chapter = state.current_chapter;
        if (cinematic_chapter_ready(chapter)) {
            start_chapter_intro(chapter);
        } else {
            story.begin();
            if (chapter == 1) story_prologue(story);
            story_chapter_card(story, chapter);
            play_story(StoryThen::Play);
        }
    } else {
        change_scene(GameScene::Playing);
    }
}

// Siguiente parada del Ascenso, o el carro de fuego si ya no quedan.
void Engine::ascent_next() {
    ascent_step++;
    if (ascent_step < Ev::ASCENT_STOPS) {
        open_dialogue(Ev::ASC_GILGAL + ascent_step);
        dialogue.dark = true;
        return;
    }
    ascent_step = -1;
    SDL_Log("Ascension: le seguiste %d de 3 veces, doble porcion=%s",
            ascent_follows, ascent_double ? "si" : "no");
    // El carro de fuego (2 Reyes 2:11-14) se ve como CINEMATICA cuando se puede: son
    // las mismas frases de get_ascension_lore sobre las estampas, asi que las paginas
    // de texto no se leen despues (el precedente del prologo: no repetir lo recien
    // visto). Bajo autoplay se suprime — los guiones cronometran el flujo de texto —
    // y sin estampas entregadas el fallback es el texto de siempre.
    if (!autoplay.active && cinematic_ascension_ready()) {
        start_ascension_intro();
        return;
    }
    SDL_Log("Cinematica del Ascenso suprimida (%s): paginas de texto",
            autoplay.active ? "autoplay" : "sin estampa");
    story.begin();
    story_ascension(story, ascent_double);
    play_story(StoryThen::FinishAscension);
}

void Engine::finish_ascension() {
    award(AchievementID::Ascension);
    // Inheritance: half the faith (floored so Eliseo starts usable), miracles, mantle.
    // "Herencia Ampliada" (El Santuario) sube la porción heredada al 60%.
    f32 ratio = meta_inherit_ratio(progression.rank_of(Meta::HERENCIA));   // 50 / 60 / 70 %
    // Cada vez que te quedaste atras en el camino, un 10% menos. Seguirle las TRES veces —que
    // es lo que hace Eliseo en el pasaje— deja la herencia EXACTAMENTE como estaba antes de
    // que el Ascenso fuera jugable: eso era la condicion del track.
    ratio = std::max(0.20f, ratio - 0.10f * static_cast<f32>(3 - ascent_follows));
    faith.current = std::max(faith.current * ratio, 40.0f);
    faith.modify(0.0f);

    Vec2 pos = {Config::GAME_WIDTH * 0.5f, Config::GAME_HEIGHT * 0.5f};
    int rooms_cleared = player.rooms_cleared;
    int kills = player.kills;
    // La alforja cruza el Jordan con el manto: lo que Elias llevaba encima lo recoge Eliseo
    Player::BeltSlot kept_belt[Player::BELT_MAX];
    for (int i = 0; i < Player::BELT_MAX; i++) kept_belt[i] = player.belt[i];
    // Heredable items travel with the mantle; everything else is left behind with Elias
    std::vector<u32> inherited;
    for (u32 id : player.items) {
        const ItemDef* d = item_registry.get(id);
        if (d && d->heredable) inherited.push_back(id);
    }
    // "Doble porcion" (2 Reyes 2:9): el milagro en el que Elias invirtio —el de mayor
    // tier de los que llevaba equipados— viaja con el manto. Eliseo lo lanza al 70%
    // (el factor `inherit` de Player::cast_miracle), y NO pisa a los suyos: el tercer
    // slot que abre el Manto es justo para el.
    MiracleID legacy = MiracleID::COUNT;
    int legacy_tier = -1;
    for (int s = 0; s < Config::MAX_MIRACLE_SLOTS; s++) {
        MiracleID m = player.miracle_slots[s].miracle;
        if (m == MiracleID::COUNT) continue;
        auto md = get_miracle_def(m);
        if (md.owner != Prophet::Elias || md.tier <= legacy_tier) continue;
        legacy_tier = md.tier;
        legacy = m;
    }
    if (player.entity != ENTITY_NONE) world.destroy(player.entity);
    // Si el cap. 5 se cerro en plena embestida del Carro, gap_crosser aun apunta al id de
    // Elias — que create() va a reciclar AHORA MISMO para Eliseo, dejandole cruzar todos
    // los precipicios del Acto II.
    world.gap_crosser = ENTITY_NONE;
    player = Player{};
    player.init_as_eliseo(world, character_textures[static_cast<u32>(CharacterType::Eliseo)]);
    // Lo comprado en El Santuario vale tambien para Eliseo (antes se perdia en el Ascenso:
    // ni la vida, ni el dano, ni la velocidad le llegaban)
    apply_meta_upgrades();
    for (int i = 0; i < Player::BELT_MAX; i++) player.belt[i] = kept_belt[i];
    player.belt_fix_selection();
    player.rooms_cleared = rooms_cleared;
    player.kills = kills;
    // Primicias rango 2: otra carta al empezar el Acto II
    primicias_pending = progression.rank_of(Meta::PRIMICIAS) >= 2;
    for (u32 id : inherited) {
        const ItemDef* d = item_registry.get(id);
        if (d) player.grant_item(world, faith, *d);
    }
    // Recalcular la reliquia SIEMPRE al rehacer el equipo: sus efectos no viven en el
    // Player sino en FaithSystem::floor_value y Player::miracle_power, y en la Ascension
    // la fe NO se reinicia — sin esto, un Elias con las Tablas dejaba a Eliseo con el
    // suelo de fe en 25 para siempre, sin llevar la reliquia.
    player.apply_relic_effects(faith);
    if (!inherited.empty()) SDL_Log("Ascension: %d heredable item(s) passed to Eliseo", static_cast<int>(inherited.size()));

    // "Alzo el manto que se le habia caido a Elias" (2 Reyes 2:13). Es el unico camino
    // hacia el item 15: event_only, nunca sale en una carta de eleccion.
    if (const ItemDef* mantle = item_registry.get(15)) {
        player.grant_item(world, faith, *mantle);
        codex_see_item(save.data, mantle->id);
        award(AchievementID::ElManto);
        SDL_Log("Ascension: El Manto de Elias (slot %d, recarga x%.2f)",
                player.active_slot_count, player.miracle_haste);
    }
    // El milagro heredado entra en el primer slot libre, que con el Manto siempre existe.
    // Solo si pediste la DOBLE PORCION: «si me vieres cuando fuere quitado de ti, te sera
    // hecho asi; y si no, no» (2 Reyes 2:10). El Manto cae igual — se le cayo a el, no te lo
    // dio —, pero el espiritu doblado se pidio o no se pidio.
    if (!ascent_double) {
        legacy = MiracleID::COUNT;
        SDL_Log("Ascension: sin milagro heredado (no se pidio la doble porcion)");
    }
    if (legacy != MiracleID::COUNT) {
        for (int s = 0; s < player.active_slot_count && s < Config::MAX_MIRACLE_SLOTS; s++) {
            if (!player.miracle_slots[s].is_empty()) continue;
            player.miracle_slots[s].equip(legacy);
            codex_see_miracle(save.data, legacy);
            SDL_Log("Ascension: inherited miracle %s in slot %d (cast at 70%%)",
                    get_miracle_def(legacy).name, s);
            break;
        }
    }

    if (player.entity != ENTITY_NONE) world.transforms[player.entity].position = pos;

    // La intro del cap. 6 abre el Acto II (primera entrada natural de Eliseo)
    start_chapter(Config::CHAPTERS_ACT_I + 1, ChapterAnnounce::Cinematic);
}

// =============================================================================
// Historia (ui/story.h)
// =============================================================================
// Las paginas se encolan en `story` antes de llamar aqui. Durante GameScene::Story el
// mundo ya esta cargado detras, de modo que al cerrar la ultima pagina se entra directo.
void Engine::play_story(StoryThen then) {
    if (story.page_count == 0) { finish_story_action(then); return; }
    story.then = then;
    story.active = true;
    // Los playtests con --script no pueden quedarse clavados leyendo
    story.auto_timer = autoplay.active ? 1.2f : 0.0f;
    change_scene(GameScene::Story);
}

void Engine::finish_story() {
    story.active = false;
    finish_story_action(story.then);
}

// =============================================================================
// Prologo cinematico (ui/cinematic.h)
// =============================================================================
// Lo llama main.cpp al arrancar (si no hay --script) y la opcion "Prólogo" del menu.
// Las texturas se cargan la primera vez y quedan en la cache del Renderer.
void Engine::start_intro() {
    cinematic_prologue(cinematic);
    // load_art SIEMPRE: begin() resetea art_loaded porque reconstruye los CineScene con
    // tex = TEXTURE_NONE. La cache por path del Renderer hace la recarga gratis.
    cinematic.load_art(renderer);
    cinematic.active = true;
    cinematic.clock = 0.0f;
    // Los playtests con --script no pueden quedarse mirando 80 segundos de cine
    cinematic.auto_timer = autoplay.active ? 0.9f : 0.0f;
    cinematic.go_to(0, renderer);
    cine_then = CineThen::MainMenu;
    change_scene(GameScene::Intro);
}

// Igual que start_intro pero con las dos escenas del capitulo y entrando a jugar al
// terminar. Quien decide SI se ve es start_chapter (primera entrada natural, sin
// autoplay, estampa entregada); esto solo la monta y la lanza.
void Engine::start_chapter_intro(int chapter) {
    cinematic_chapter(cinematic, chapter);
    cinematic.load_art(renderer);
    cinematic.active = true;
    cinematic.clock = 0.0f;
    cinematic.auto_timer = autoplay.active ? 0.9f : 0.0f;
    cinematic.go_to(0, renderer);
    cine_then = CineThen::Play;
    change_scene(GameScene::Intro);
    SDL_Log("Cinematica de capitulo %d", chapter);
}

// La cinematica del Ascenso. A diferencia de las intros de capitulo, debajo no hay
// mundo cargado (las cuatro paradas van sobre negro), y al terminar no entra a jugar:
// finish_ascension reparte la herencia y es quien abre el capitulo 6 con su intro.
void Engine::start_ascension_intro() {
    cinematic_ascension(cinematic, ascent_double);
    cinematic.load_art(renderer);
    cinematic.active = true;
    cinematic.clock = 0.0f;
    cinematic.auto_timer = autoplay.active ? 0.9f : 0.0f;
    cinematic.go_to(0, renderer);
    cine_then = CineThen::FinishAscension;
    change_scene(GameScene::Intro);
    SDL_Log("Cinematica del Ascenso");
}

void Engine::finish_story_action(StoryThen then) {
    switch (then) {
        case StoryThen::Play:
            change_scene(GameScene::Playing);
            break;
        case StoryThen::AdvanceChapter:  advance_chapter();                break;
        case StoryThen::FinishAscension: finish_ascension();               break;
        case StoryThen::VictoryMenu:
            menus.stats_chapter = state.current_chapter;
            menus.stats_kills   = run_kills;
            menus.stats_time    = state.run_time;
            change_scene(GameScene::Victory);
            break;
        // Cierre del tutorial (Engine::finish_tutorial): a donde se iba viaja en el StoryThen
        // porque `tutorial` ya se limpio antes de encolar las paginas.
        case StoryThen::TutorialToRun:   start_new_run();          break;
        case StoryThen::TutorialToTitle: tutorial_back_to_title(); break;
    }
}

// =============================================================================
// Music selection by scene / room
// =============================================================================
void Engine::update_music() {
    MusicID want = MusicID::COUNT;
    switch (state.scene) {
        case GameScene::Intro:
            // El prologo tiene el suyo; el Ascenso conserva el que ya suena desde
            // trigger_ascension (cortarlo justo cuando aparece el carro seria lo
            // contrario de una cinematica). La cinematica de capitulo estrena el
            // tema de ese capitulo: es la primera vez que el jugador lo oye.
            if      (cine_then == CineThen::MainMenu)        want = MusicID::Prologue;
            else if (cine_then == CineThen::FinishAscension) want = MusicID::Ascension;
            // Las dos estampas del capitulo estrenan pista cada una: la A es la
            // estampa y el versiculo, la B trae el pulso y el objetivo.
            else want = (cinematic.scene <= 0) ? chapter_cine_a(state.current_chapter)
                                               : chapter_cine_b(state.current_chapter);
            break;
        case GameScene::MainMenu:   want = MusicID::MainTheme; break;
        case GameScene::Playing:
        case GameScene::Paused:
        case GameScene::Choice:
        case GameScene::Dialogue: {
            // El tema del jefe suena mientras la arena esta sellada (zona continua)
            const Room* zone = zone_ptr();
            bool boss_room = current_room_data.type == RoomType::Zone && zone && zone->arena_sealed;
            if (boss_room) want = MusicID::BossTheme;
            // La escena guionizada de APERTURA se juega antes de la cinematica y
            // sonaba con el tema de juego, o sea el capitulo empezaba con su
            // musica ya gastada. Lleva la de apertura, como la carta.
            else if (scene_id >= 0 && scene_id == Scene::opening_of(state.current_chapter))
                want = chapter_intro(state.current_chapter);
            else want = chapter_theme(state.current_chapter);
            break;
        }
        case GameScene::Story:
            // La ascension y el epilogo tienen tema propio. El tutorial no es un
            // capitulo, asi que se queda con el tema del titulo. Lo demas —carta de
            // apertura y outro— es el tema desnudo del capitulo.
            if      (story.then == StoryThen::FinishAscension) want = MusicID::Ascension;
            else if (story.then == StoryThen::VictoryMenu)     want = MusicID::Victory;
            else if (story.then == StoryThen::TutorialToRun ||
                     story.then == StoryThen::TutorialToTitle) want = MusicID::MainTheme;
            else                                               want = chapter_intro(state.current_chapter);
            break;
        case GameScene::Codex:      break;   // sin cambio: se abre sobre lo que ya suene
        // El Santuario deja de sonar como el titulo: `menu.ogg` existia desde la
        // primera tanda y no la pedia nadie.
        case GameScene::Sanctuary:  want = MusicID::Menu; break;
        case GameScene::GameOver:   want = MusicID::Death; break;
        case GameScene::Victory:    want = MusicID::Victory; break;
    }
    if (want != MusicID::COUNT && want != audio.current_music) {
        audio.play_music(want, true, 800.0f);
    }
}

// =============================================================================
// Meta-progresion (systems/progression.h): ofrendas y El Santuario
// =============================================================================
// El estado vive en SaveData (sobrevive a la derrota, ver SaveSystem::delete_save)
// y se copia al ProgressionSystem, que es quien conoce costes y nombres.
// -----------------------------------------------------------------------------
// Ajustes del jugador (GDD 4.2)
// -----------------------------------------------------------------------------
// SaveData (v7) es la fuente de la verdad; esto los reparte a quien los usa. Se llama al
// arrancar y cada vez que el menu de Opciones toca algo, asi que tiene que ser idempotente.
void Engine::apply_settings_from_save() {
    int l = save.data.language;
    if (l < 0 || l >= static_cast<int>(Language::COUNT)) l = 0;
    I18N::current = static_cast<Language>(l);

    audio.set_music_volume(save.data.music_volume);
    audio.set_sfx_volume(save.data.sfx_volume);

    int m = save.data.aim_assist;
    if (m < 0 || m > 2) m = 0;
    input.aim_mode = static_cast<Config::AimMode>(m);

    int sens = save.data.sensitivity;
    if (sens < 1)  sens = 1;
    if (sens > 10) sens = 10;
    input.aim_sensitivity = 1.0f + (sens - Config::Feel::SENSITIVITY_DEFAULT) *
                                    Config::Feel::SENSITIVITY_STEP;

    input.rumble_enabled    = (save.data.settings_flags & SETTING_RUMBLE) != 0;
    input.touch_right_stick = (save.data.settings_flags & SETTING_TOUCH_RSTICK) != 0;
    player.staff_skin       = save.data.skin_equipped;   // cosmetico: no toca ninguna cifra

    // El idioma acaba de escribirse arriba: lo que se tradujo una sola vez hay que volver a
    // traducirlo. Es el camino de Opciones; el selector del primer arranque pasa por
    // `set_language`, que hace lo mismo.
    relocalize();
}

void Engine::set_language(Language l) {
    if (l >= Language::COUNT) l = Language::ES;
    I18N::current = l;
    save.data.language = static_cast<int>(l);
    save.save(false);   // un ajuste NO re-sella la fecha: no reinicia la caducidad de 48 h
    relocalize();
}

// Vuelve a traducir lo que se tradujo UNA SOLA VEZ.
//
// `tr()` no copia nada: devuelve un puntero al literal del idioma activo EN EL MOMENTO DE
// LA LLAMADA. Casi todo el juego lo llama al pintar, asi que cambia solo; lo que se rompe
// es lo que GUARDA ese puntero en una estructura de larga vida, porque se queda congelado
// en el idioma de entonces. Cambiar el idioma en Opciones dejaba "Honda de David" en una
// partida en ingles, y al reves.
//
// Hoy son tres, y los tres se ven en pantalla:
//   - `ItemRegistry` (nombre y efecto de los 22 objetos: HUD, cartas de eleccion, Codice),
//   - `ProgressionSystem::upgrades` (las 15 mejoras de El Santuario),
//   - `Engine::boss_bar_name` (el rotulo de la barra del jefe que se este peleando).
//
// Si algun dia otra estructura de larga vida guarda un `tr()`, va AQUI. Lo transitorio (un
// mensaje del HUD, una carta abierta, una pagina de historia) no cuenta: dura segundos y
// nadie cambia el idioma a mitad.
void Engine::relocalize() {
    if (item_registry.items.empty()) return;   // arranque: los registros aun no existen

    item_registry.init();
    progression.init();
    sync_progression_from_save();   // `init()` deja `unlocked` a cero: se rehidrata del guardado
    if (boss_bar_name) boss_bar_name = get_chapter_def(state.current_chapter).boss_name;

    SDL_Log("i18n: registros re-traducidos (%s)", I18N::language_name(I18N::current));
}

void Engine::sync_progression_from_save() {
    progression.offerings = save.data.offerings;
    for (auto& u : progression.upgrades) {
        int r = (u.id < static_cast<u32>(SAVE_MAX_UPGRADES)) ? save.data.upgrade_ranks[u.id] : 0;
        // Un bit de `upgrades_bought` sin rango (un guion viejo que solo pone el bit) es rango 1
        if (r == 0 && ((save.data.upgrades_bought >> u.id) & 1u)) r = 1;
        u.rank     = std::max(0, std::min(r, u.max_rank));
        u.unlocked = u.rank > 0;
        u.cost     = u.next_cost();
    }
    achievements.unlocked = save.data.achievements;
    apply_meta_unlocks();
}

// Una mejora comprada mete su objeto en el sorteo, y de ahi ya no sale: `items_unlocked`
// es meta y sobrevive a delete_save, que es lo que hace que ahorrar ofrendas signifique
// algo entre partidas. Solo pone bits, nunca los quita.
void Engine::apply_meta_unlocks() {
    for (const auto& u : progression.upgrades) {
        if (u.unlocked && u.unlocks_item != MetaUpgrade::NO_ITEM)
            save.data.items_unlocked |= (1u << u.unlocks_item);
        if (u.unlocked && u.unlocks_skin != MetaUpgrade::NO_ITEM)
            save.data.skins_unlocked |= (1u << u.unlocks_skin);
    }
    save.data.skins_unlocked |= 1u;   // la skin 0 es la de siempre y siempre esta

    // Puerta de contenido (GDD 4.x, modelo Free-to-Start). HOY SE CONCEDE SOLA: es una
    // PUERTA, no una caja. El dia que haya tienda, quien la rellene sera ella y no esto;
    // integrar Google Play Billing / StoreKit / Steamworks son dependencias externas y
    // este repo no las mete sin orden explicita.
    save.data.entitlements = 0xFFFFFFFFu;

    // Una skin que ya no este desbloqueada no puede quedarse puesta.
    if (!(save.data.skins_unlocked & (1u << save.data.skin_equipped)))
        save.data.skin_equipped = 0;
}

// =============================================================================
// Logros
// =============================================================================
void Engine::award(AchievementID id) {
    if (!achievements.unlock(id)) return;   // ya lo tenia: ni aviso ni guardado
    AchievementDef d = get_achievement_def(id);
    save.data.achievements = achievements.unlocked;
    save.save(false);   // sin resellar la fecha: un logro no reinicia la caducidad de 48 h
    SDL_Log("Achievement unlocked: %s (%s)  [%d/%d]", d.name, d.description,
            achievements.count_unlocked(), static_cast<int>(AchievementID::COUNT));
    char buf[128];
    snprintf(buf, sizeof(buf), tr("LOGRO  ·  %s", "ACHIEVEMENT  ·  %s"), d.name);
    show_banner(buf, d.description, 3.0f);
    audio.play_sfx(SfxID::LevelUp, 0.9f);
    achievements.pending = AchievementID::COUNT;
}

// Los que dependen del ESTADO y no de un evento puntual. Se miran una vez por frame
// jugando; `award` filtra las repeticiones, asi que no hace falta recordar nada.
void Engine::check_achievements() {
    if (player.entity == ENTITY_NONE) return;

    if (faith.current >= faith.max) award(AchievementID::FuegoDelCielo);
    if (save.data.total_kills >= 100) award(AchievementID::CienCaidos);
    if (state.current_chapter >= Config::CHAPTERS_ACT_I + 1) award(AchievementID::ActoII);
    if (player.items.size() >= 8) award(AchievementID::Coleccionista);
    if (player.relic_slot != UINT32_MAX) award(AchievementID::PrimeraReliquia);

    // Tres slots ocupados A LA VEZ: tener el tercero abierto y vacio no cuenta
    if (player.active_slot_count >= Config::MAX_MIRACLE_SLOTS) {
        bool all = true;
        for (int i = 0; i < Config::MAX_MIRACLE_SLOTS; i++)
            if (player.miracle_slots[i].is_empty()) { all = false; break; }
        if (all) award(AchievementID::TriplePorcion);
    }

    // Los diez milagros lanzados alguna vez (la mascara persiste entre partidas)
    constexpr u32 ALL_MIRACLES = (1u << static_cast<u32>(MiracleID::COUNT)) - 1u;
    if ((save.data.miracles_cast & ALL_MIRACLES) == ALL_MIRACLES)
        award(AchievementID::DiezProdigios);

    // Tres discipulos en pie. `is_ally` es la unica marca de faccion del juego.
    int allies = 0;
    for (u32 e = 0; e < world.entity_count; e++) {
        if (!world.is_alive(e) || !is_ally(world, e)) continue;
        if (world.has(e, Component::Health) && !world.healths[e].is_alive()) continue;
        allies++;
    }
    if (allies >= 3) award(AchievementID::EjercitoCelestial);
}

void Engine::sync_progression_to_save() {
    save.data.achievements = achievements.unlocked;
    save.data.offerings = progression.offerings;
    save.data.upgrades_bought = 0;
    for (const auto& u : progression.upgrades) {
        if (u.rank > 0) save.data.upgrades_bought |= (1u << u.id);
        if (u.id < static_cast<u32>(SAVE_MAX_UPGRADES))
            save.data.upgrade_ranks[u.id] = static_cast<u8>(u.rank);
    }
    apply_meta_unlocks();
}

// Lo comprado en El Santuario que va EN EL CUERPO del profeta. Se llama cada vez que se
// CREA al profeta: partida nueva, reintento, Continuar y Ascension. Hasta el 2026-10-03 solo
// lo llamaban la partida nueva y el reintento, asi que al Continuar y al pasar a Eliseo se
// perdian la vida, el dano y la velocidad comprados — y el jugador no notaba lo que pagaba.
// La fe inicial NO va aqui (Continuar trae la suya y el Ascenso la hereda): ver
// apply_meta_start_faith en start_new_run / retry_chapter.
void Engine::apply_meta_upgrades() {
    if (player.entity == ENTITY_NONE) return;
    // La skin del cayado vive en el guardado, pero el Player se REHACE al empezar una
    // partida (`player = Player{}`), asi que hay que volver a ponersela aqui. Es lo unico
    // cosmetico de esta funcion: no toca ninguna cifra.
    player.staff_skin = save.data.skin_equipped;
    const int r_hp  = progression.rank_of(Meta::VIDA);
    const int r_dmg = progression.rank_of(Meta::DANO);
    const int r_spd = progression.rank_of(Meta::VELOCIDAD);
    if (r_hp > 0 && world.has(player.entity, Component::Health)) {
        world.healths[player.entity].max     += meta_hp(r_hp);
        world.healths[player.entity].current += meta_hp(r_hp);
    }
    if (r_dmg > 0 && world.has(player.entity, Component::Combat))
        world.combats[player.entity].damage += meta_damage(r_dmg);
    if (r_spd > 0 && world.has(player.entity, Component::Movement))
        world.movements[player.entity].speed *= meta_speed_mult(r_spd);
    if (progression.is_unlocked(Meta::DOBLE) && player.character == CharacterType::Eliseo)
        player.active_slot_count = Config::MAX_MIRACLE_SLOTS;   // Q / E / F
    player.belt_cap = std::min(Player::BELT_MAX, meta_belt_slots(progression.rank_of(Meta::ALFORJA)));
    SDL_Log("Meta upgrades applied: +%.0f HP, +%.0f dmg, x%.2f speed, alforja %d",
            meta_hp(r_hp), meta_damage(r_dmg), meta_speed_mult(r_spd), player.belt_cap);
}

// Los dones que se gastan, rellenos. Por INTENTO: levantarse (Aliento de Vida).
void Engine::refill_attempt_gifts() {
    revives_left = meta_revives(progression.rank_of(Meta::ALIENTO));
    tally = RunTally{};
}

// Por CAPITULO: re-sorteos, oraciones sin altar y el agua de la tinaja.
void Engine::refill_chapter_gifts() {
    rerolls_left = meta_rerolls(progression.rank_of(Meta::SUERTES));
    prayers_left = meta_altar_prayers(progression.rank_of(Meta::ALTAR));
    const int water = meta_jar_water(progression.rank_of(Meta::TINAJA));
    if (water > 0 && player.entity != ENTITY_NONE && !tutorial.active) {
        // La tinaja llena el hueco del agua hasta `water` cargas, sin pasarse ni robar sitio
        // a otro tipo si la alforja va llena (entonces no cabe, y no se fuerza).
        const int have = player.belt_slot_of(9) >= 0 ? player.belt[player.belt_slot_of(9)].charges : 0;
        if (have < water) player.belt_add(9, water - have);
        SDL_Log("Tinaja de Sarepta: %d Agua(s) del Jordan en la alforja", water);
    }
}

// Ingresa ofrendas en el banco, guarda y anima el contador del HUD. `from_pickup` = un
// hallazgo del suelo (cuenta para el desglose del final como "ya ingresado").
void Engine::bank_offerings(int amount, bool from_pickup) {
    if (amount <= 0) return;
    progression.add_offerings(amount);
    if (from_pickup) tally.found += amount;
    save.data.offerings = progression.offerings;
    save.save(false);
    offering_pop = 1.0f;
    offering_pop_amount = amount;
    // La PRIMERA ofrenda de la vida del guardado explica que es: la queja del usuario era
    // literalmente "el tema de las ofrendas no se entiende".
    show_tip_once(TIP_OFFERINGS, tr("OFRENDAS", "OFFERINGS"),
                  tr("No se pierden al morir. Gástalas en El Santuario para empezar cada partida más fuerte.",
                     "You keep them when you die. Spend them in the Sanctuary to start every run stronger."),
                  6.0f);
}

// =============================================================================
// La alforja (2026-10-03)
// =============================================================================
void Engine::spawn_consumable_at(Vec2 pos, f32 roll) {
    const u32 id = ItemRegistry::roll_consumable(roll, save.data.items_unlocked);
    spawn_pickup(world, pos, ItemComponent::Pickup::Consumable, static_cast<f32>(id));
    spawn_fx(world, pos, FxID::Holy, {190, 230, 255, 220}, 22.0f, 0.4f, 5);
    const ItemDef* d = item_registry.get(id);
    SDL_Log("Consumable dropped: %s at (%.0f,%.0f)", d ? d->name : "?", pos.x, pos.y);
}

// Una vez por frame en Playing: lo recogido entra, y las teclas de la alforja.
void Engine::update_belt() {
    if (belt_full_cd > 0.0f) belt_full_cd -= delta_time;
    for (u32 id : world.consumables_found) {
        const ItemDef* d = item_registry.get(id);
        if (!d || !player.belt_add(id, 1)) continue;
        codex_see_item(save.data, id);
        const int slot = player.belt_slot_of(id);
        char hint[96];
        // Dice QUE es, QUE hace y COMO se usa: el usuario no veia logico usar los objetos
        if (input.touch_ui)
            snprintf(hint, sizeof(hint), "%s", tr("A la alforja: botón USAR (mantén para cambiar)",
                                                  "To your satchel: USE button (hold to switch)"));
        else
            snprintf(hint, sizeof(hint), tr("A la alforja: pulsa [%d] para usarlo", "To your satchel: press [%d] to use it"),
                     slot + 1);
        announce_item(*d, hint);
        audio.play_sfx(SfxID::ItemPickup, 0.9f);
        SDL_Log("Belt: +1 %s (hueco %d, %d cargas)", d->name, slot,
                slot >= 0 ? static_cast<int>(player.belt[slot].charges) : 0);
        show_tip_once(TIP_BELT, tr("LA ALFORJA", "THE SATCHEL"),
                      input.touch_ui ? tr("Toca USAR para usar el consumible elegido; mantenlo para cambiar de uno a otro.",
                                          "Tap USE to use the chosen consumable; hold it to switch between them.")
                                     : tr("[1]-[4] usan cada hueco  ·  [R] el elegido  ·  rueda o [C] para cambiar.",
                                          "[1]-[4] use each slot  ·  [R] the chosen one  ·  wheel or [C] to switch."),
                      6.0f);
    }
    world.consumables_found.clear();
    if (world.mods.belt_full_touch) {
        world.mods.belt_full_touch = false;
        if (belt_full_cd <= 0.0f) {
            belt_full_cd = 3.0f;
            SDL_Log("Belt full: el consumible se queda en el suelo");
            show_message(tr("Alforja llena: usa algo para hacer sitio", "Satchel full: use something to make room"), 2.0f);
            audio.play_sfx(SfxID::MenuBack, 0.5f);
        }
    }

    // Teclas: 1-4 usan ese hueco, R / X / USAR el elegido; rueda, C y cruceta cambian.
    static const Action SLOT_KEYS[Player::BELT_MAX] = {Action::Belt1, Action::Belt2, Action::Belt3, Action::Belt4};
    for (int i = 0; i < Player::BELT_MAX; i++)
        if (input.is_action_just_pressed(SLOT_KEYS[i])) use_belt_slot(i);
    if (input.is_action_just_pressed(Action::Consumable)) use_belt_slot(-1);
    const bool next = input.is_action_just_pressed(Action::BeltNext);
    const bool prev = input.is_action_just_pressed(Action::BeltPrev);
    if ((next || prev) && player.belt_count() > 0) {
        const int before = player.belt_sel;
        player.belt_cycle(next ? 1 : -1);
        if (player.belt_sel != before || player.belt_count() == 1) {
            const ItemDef* d = item_registry.get(player.belt[player.belt_sel].id);
            if (d) {
                char msg[128];
                snprintf(msg, sizeof(msg), "%s  ·  %s", d->name, d->description);
                show_message(msg, 1.8f);
            }
            audio.play_sfx(SfxID::MenuSelect, 0.6f);
            input.rumble(Config::Feel::RUMBLE_HIT, 30);
        }
    }
}

void Engine::use_belt_slot(int slot) {
    char fx[96];
    const int used_slot = slot < 0 ? player.belt_sel : slot;
    const u32 id = player.use_belt(slot, world, faith, item_registry, fx, sizeof(fx));
    if (id == UINT32_MAX) {
        // No se ha gastado: decir por que (la vida ya llena), o que la alforja esta vacia
        // (antes la R sin nada no hacia NADA)
        if (fx[0]) show_message(fx, 1.6f);
        else if (player.belt_count() == 0)
            show_message(tr("La alforja está vacía: los consumibles se encuentran por el mundo",
                            "Your satchel is empty: consumables are found around the world"), 2.0f);
        audio.play_sfx(SfxID::MenuBack, 0.4f);
        return;
    }
    const ItemDef* d = item_registry.get(id);
    char msg[160];
    snprintf(msg, sizeof(msg), "%s  ·  %s", d ? d->name : "?", fx);
    show_message(msg, 2.0f);
    audio.play_sfx(SfxID::ItemPickup, 0.9f);
    if (id == 9 || id == 17) audio.play_sfx(SfxID::MiracleHeal, 0.6f);
    if (id == 18)            audio.play_sfx(SfxID::FaithGain, 0.8f);
    input.rumble(Config::Feel::RUMBLE_HIT, 50);
    SDL_Log("Belt used: %s (hueco %d) -> %s", d ? d->name : "?", used_slot, fx);
}

// =============================================================================
// Pantalla de fin: el desglose de ofrendas
// =============================================================================
// Lo que faltaba para que "las ofrendas" se entendieran: DE DONDE salen (capitulo, bajas,
// grupos, jefes, el diezmo), CUANTAS entran ahora, cuantas tienes, y QUE puedes hacer con
// ellas ya ("Ya puedes ofrecer: X" o "Te faltan N para X"). La cuenta sube linea a linea,
// con su tic, como en los roguelikes que se entienden a la primera.
void Engine::render_run_summary(f32 top) {
    const f32 W = static_cast<f32>(Config::GAME_WIDTH);
    const f32 PW = 220.0f, PX = (W - PW) * 0.5f;
    const int n = summary.count;
    const f32 LINE = 7.2f;
    const f32 PH = 15.0f + n * LINE + 22.0f;
    renderer.draw_panel({PX, top + 1.5f, PW, PH}, {0, 0, 0, 120}, 4.5f);
    renderer.draw_panel({PX, top, PW, PH}, {28, 22, 30, 238}, 4.5f);
    renderer.draw_text(tr("OFRENDAS GANADAS", "OFFERINGS EARNED"), PX + 8.0f, top + 4.0f, {245, 214, 140, 255}, 0.7f);

    // Linea a linea: una cada 0,22 s, y el total cuenta hacia arriba al final
    const f32 t = summary.t;
    static int last_shown = -1;
    int shown = std::min(n, static_cast<int>(t / 0.22f));
    if (shown != last_shown && shown > 0 && shown <= n && t < 10.0f) audio.play_sfx(SfxID::MenuSelect, 0.35f);
    last_shown = shown;
    f32 y = top + 14.0f;
    for (int i = 0; i < n; i++) {
        if (i >= shown) break;
        const OfferingLine& l = summary.lines[i];
        const bool found = (i == n - 1) && std::strstr(l.label, tr("Hallazgos", "Finds")) == l.label;
        renderer.draw_text(l.label, PX + 10.0f, y, found ? Color{150, 140, 125, 220} : Color{210, 200, 186, 240}, 0.58f);
        char v[16];
        snprintf(v, sizeof(v), "+%d", l.value);
        renderer.draw_text(v, PX + PW - 10.0f - renderer.measure_text(v, 0.58f), y,
                           found ? Color{150, 140, 125, 220} : Color{255, 214, 110, 250}, 0.58f);
        y += LINE;
    }
    // Total que entra ahora (los hallazgos ya entraron al recogerse)
    const f32 tt = std::max(0.0f, t - n * 0.22f);
    const f32 k = std::min(1.0f, tt / 0.8f);
    const int total_now = static_cast<int>(summary.total * k + 0.5f);
    y = top + 14.0f + n * LINE + 1.0f;
    renderer.draw_rect({PX + 8.0f, y - 1.5f, PW - 16.0f, 0.75f}, {255, 220, 150, 90});
    char tot[48];
    snprintf(tot, sizeof(tot), "+%d", total_now);
    renderer.draw_text(tr("Total", "Total"), PX + 10.0f, y + 1.0f, {255, 236, 190, 255}, 0.75f);
    renderer.draw_text(tot, PX + PW - 10.0f - renderer.measure_text(tot, 0.85f), y, {255, 214, 110, 255}, 0.85f);
    // El banco y lo que ya se puede hacer con el
    char bank[96];
    const int bank_now = summary.bank - summary.total + total_now;
    snprintf(bank, sizeof(bank), tr("Tienes %d ofrendas: no se pierden al morir", "You have %d offerings: they survive death"),
             bank_now);
    renderer.draw_text(bank, PX + 10.0f, y + 9.0f, {175, 165, 150, 230}, 0.52f);
    if (k >= 1.0f) {
        char next[128];
        const MetaUpgrade* a = progression.cheapest_affordable();
        const MetaUpgrade* p = progression.cheapest_pending();
        if (a)      snprintf(next, sizeof(next), tr("Ya puedes ofrecer en El Santuario: %s", "Now you can offer in the Sanctuary: %s"), a->name);
        else if (p) snprintf(next, sizeof(next), tr("Te faltan %d para: %s", "%d more for: %s"), p->next_cost() - progression.offerings, p->name);
        else        snprintf(next, sizeof(next), "%s", tr("El Santuario está completo", "The Sanctuary is complete"));
        const f32 pulse = a ? 0.5f + 0.5f * std::sin(world.time * 5.0f) : 0.0f;
        renderer.draw_text(next, PX + 10.0f, y + 15.5f,
                           a ? Color{255, static_cast<u8>(210 + 30 * pulse), 120, 255} : Color{200, 150, 130, 230}, 0.52f);
        // Y su icono detras del nombre, el mismo que vas a ver en El Santuario (si cabe)
        const MetaUpgrade* shown = a ? a : p;
        const f32 ix = PX + 10.0f + renderer.measure_text(next, 0.52f) + 2.0f;
        if (shown && ix + 8.0f <= PX + PW - 6.0f)
            draw_icon(renderer, shown->icon, {ix, y + 13.5f, 8.0f, 8.0f},
                      a ? Color::white() : Color{170, 160, 150, 220});
    }
}

// =============================================================================
// Equipo (pausa)
// =============================================================================
// Que llevas y que hace cada cosa. Antes la unica forma de saberlo era una fila de iconos
// de 9 unidades sin nombre.
void Engine::update_equipment() {
    const int n = static_cast<int>(player.items.size());
    if (input.is_action_just_pressed(Action::MoveUp))   equipment_scroll--;
    if (input.is_action_just_pressed(Action::MoveDown)) equipment_scroll++;
    equipment_scroll = std::max(0, std::min(equipment_scroll, std::max(0, n - 6)));
    Vec2 tp;
    const bool tapped = input.take_tap(tp);
    if (input.is_action_just_pressed(Action::Cancel) || input.is_action_just_pressed(Action::Confirm) ||
        input.is_action_just_pressed(Action::Pause) || (tapped && tp.y > 160.0f)) {
        equipment_open = false;
        menus.selected_option = 1;
        audio.play_sfx(SfxID::MenuBack, 0.6f);
    }
}

void Engine::render_equipment() {
    const f32 W = static_cast<f32>(Config::GAME_WIDTH);
    const f32 H = static_cast<f32>(Config::GAME_HEIGHT);
    renderer.draw_rect({0, 0, W, H}, {10, 8, 14, 242});
    renderer.draw_text(tr("EQUIPO", "EQUIPMENT"), 10.0f, 5.0f, {240, 215, 150, 255}, 1.1f);
    const char* who = player.character == CharacterType::Elias ? tr("Elías", "Elijah") : tr("Eliseo", "Elisha");
    renderer.draw_text(who, 10.0f + renderer.measure_text(tr("EQUIPO", "EQUIPMENT"), 1.1f) + 6.0f, 9.0f, {180, 165, 140, 230}, 0.65f);

    // --- Columna izquierda: cifras y dones -----------------------------------------
    const f32 LX = 10.0f;
    f32 y = 24.0f;
    auto stat = [&](const char* label, const char* value, Color c) {
        renderer.draw_text(label, LX, y, {165, 155, 140, 230}, 0.55f);
        renderer.draw_text(value, LX + 52.0f, y, c, 0.55f);
        y += 7.0f;
    };
    char v[64];
    if (player.entity != ENTITY_NONE && world.has(player.entity, Component::Health)) {
        const auto& h = world.healths[player.entity];
        snprintf(v, sizeof(v), "%.0f / %.0f", h.current, h.max);
        stat(tr("Vida", "Health"), v, {150, 230, 150, 250});
    }
    if (player.entity != ENTITY_NONE && world.has(player.entity, Component::Combat)) {
        const auto& c = world.combats[player.entity];
        snprintf(v, sizeof(v), "%.0f  (x%.2f fe)", c.damage, faith.get_damage_multiplier());
        stat(tr("Daño", "Damage"), v, {255, 200, 140, 250});
        snprintf(v, sizeof(v), tr("%.1f disparos/s", "%.1f shots/s"), 1.0f / std::max(0.05f, c.attack_speed));
        stat(tr("Cadencia", "Fire rate"), v, {230, 220, 200, 240});
    }
    if (player.entity != ENTITY_NONE && world.has(player.entity, Component::Movement)) {
        snprintf(v, sizeof(v), "%.0f", world.movements[player.entity].speed);
        stat(tr("Velocidad", "Speed"), v, {230, 220, 200, 240});
    }
    snprintf(v, sizeof(v), "%.0f  (%s)", faith.current, faith.get_level_name());
    stat(tr("Fe", "Faith"), v, {160, 210, 250, 250});
    if (player.relic_slot != UINT32_MAX) {
        const ItemDef* d = item_registry.get(player.relic_slot);
        stat(tr("Reliquia", "Relic"), d ? d->name : "?", {245, 205, 90, 250});
    }
    if (player.weapon_slot != UINT32_MAX) {
        const ItemDef* d = item_registry.get(player.weapon_slot);
        stat(tr("Arma", "Weapon"), d ? d->name : "?", {230, 220, 200, 240});
    }
    y += 3.0f;
    renderer.draw_text(tr("DONES DEL SANTUARIO", "SANCTUARY GIFTS"), LX, y, {240, 215, 150, 230}, 0.55f);
    y += 8.0f;
    bool any_gift = false;
    // Cada don con el icono de su mejora: el mismo que se compro en El Santuario
    auto gift = [&](u32 meta_id, const char* txt) {
        const MetaUpgrade* u = progression.get(meta_id);
        const bool icon = u && draw_icon(renderer, u->icon, {LX, y - 1.0f, 7.0f, 7.0f});
        renderer.draw_text(txt, icon ? LX + 9.0f : LX, y, {215, 205, 190, 235}, 0.52f);
        y += 7.0f;
        any_gift = true;
    };
    if (progression.is_unlocked(Meta::ALIENTO)) {
        snprintf(v, sizeof(v), tr("Aliento de Vida: %d", "Breath of Life: %d"), revives_left); gift(Meta::ALIENTO, v);
    }
    if (progression.is_unlocked(Meta::SUERTES)) {
        snprintf(v, sizeof(v), tr("Echar Suertes: %d", "Casting Lots: %d"), rerolls_left); gift(Meta::SUERTES, v);
    }
    if (progression.is_unlocked(Meta::ALTAR)) {
        snprintf(v, sizeof(v), tr("Altar Portátil: %d", "Portable Altar: %d"), prayers_left); gift(Meta::ALTAR, v);
    }
    for (u32 id : {Meta::VIDA, Meta::DANO, Meta::VELOCIDAD, Meta::DIEZMO}) {
        const MetaUpgrade* u = progression.get(id);
        if (!u || u->rank <= 0) continue;
        char t[64];
        meta_rank_text(*u, u->rank, t, sizeof(t));
        gift(id, t);
    }
    if (!any_gift) gift(Meta::COUNT, tr("Ninguno: El Santuario espera", "None yet: the Sanctuary awaits"));

    // --- Columna derecha: la alforja y los objetos -----------------------------------
    const f32 RX = 128.0f, RW = W - RX - 8.0f;
    f32 ry = 24.0f;
    renderer.draw_text(tr("ALFORJA", "SATCHEL"), RX, ry, {240, 215, 150, 230}, 0.55f);
    for (int i = 0; i < player.belt_cap && i < Player::BELT_MAX; i++) {
        const Rect r = {RX + 38.0f + i * 15.0f, ry - 3.0f, 12.0f, 12.0f};
        const auto& b = player.belt[i];
        renderer.draw_rect({r.x - 1, r.y - 1, r.w + 2, r.h + 2}, (!b.empty() && i == player.belt_sel) ? Color{245, 205, 90, 240} : Color{90, 84, 90, 200});
        renderer.draw_rect(r, {24, 19, 28, 230});
        if (!b.empty()) {
            draw_icon(renderer, Icon::item(b.id), r);
            if (b.charges > 1) {
                char c[6];
                snprintf(c, sizeof(c), "%u", static_cast<unsigned>(b.charges));
                renderer.draw_text(c, r.right() - 3.0f, r.bottom() - 5.0f, {255, 238, 190, 255}, 0.5f);
            }
        }
    }
    ry += 14.0f;
    renderer.draw_text(tr("OBJETOS", "ITEMS"), RX, ry, {240, 215, 150, 230}, 0.55f);
    ry += 8.0f;
    const int n = static_cast<int>(player.items.size());
    if (n == 0)
        renderer.draw_text(tr("Aún nada: elites, cofres y jefes dan a elegir", "Nothing yet: elites, chests and bosses offer choices"),
                           RX, ry, {150, 140, 125, 220}, 0.52f);
    const int first = std::max(0, std::min(equipment_scroll, std::max(0, n - 6)));
    for (int i = first; i < n && i < first + 6; i++) {
        const ItemDef* d = item_registry.get(player.items[i]);
        if (!d) continue;
        const Rect ic = {RX, ry, 14.0f, 14.0f};
        renderer.draw_rect({ic.x - 1, ic.y - 1, ic.w + 2, ic.h + 2}, item_rarity_color(d->rarity));
        renderer.draw_rect(ic, {24, 19, 28, 230});
        draw_icon(renderer, Icon::item(d->id), ic);
        renderer.draw_text(d->name, RX + 18.0f, ry, item_rarity_color(d->rarity), 0.58f);
        std::string dl[2];
        const int dn = renderer.wrap_text(d->description, RW - 20.0f, 0.48f, dl, 2);
        for (int k = 0; k < dn; k++)
            renderer.draw_text(dl[k].c_str(), RX + 18.0f, ry + 6.5f + k * 5.5f, {200, 192, 180, 230}, 0.48f);
        ry += 21.0f;
    }
    if (first > 0)         renderer.draw_text("^", W - 10.0f, 46.0f, {200, 180, 130, 220}, 0.6f);
    if (first + 6 < n)     renderer.draw_text("v", W - 10.0f, H - 22.0f, {200, 180, 130, 220}, 0.6f);
    renderer.draw_text_centered(input.touch_ui ? tr("Toca abajo para volver", "Tap below to go back")
                                               : tr("[W/S] recorrer   [ESC] volver", "[W/S] scroll   [ESC] back"),
                                H - 9.0f, {140, 130, 118, 210}, 0.55f);
}

void Engine::announce_item(const ItemDef& d, const char* hint) {
    pickup_icon  = Icon::item(d.id);
    pickup_name  = d.name;
    pickup_desc  = d.description;
    pickup_hint  = hint ? hint : "";
    pickup_color = item_rarity_color(d.rarity);
    pickup_t     = 4.0f;
}

void Engine::show_tip_once(u32 bit, const char* title, const char* body, f32 seconds) {
    if (save.data.tips_seen & bit) return;
    if (tutorial.active || autoplay.active) {
        // Bajo autoplay no se tapa la pantalla de los guiones de siempre, pero SI se apunta
        // el bit y se loguea: es el gate de santuario_check.
        save.data.tips_seen |= bit;
        SDL_Log("Tip shown: %s", title);
        return;
    }
    save.data.tips_seen |= bit;
    save.save(false);
    show_banner(title, body, seconds);
    SDL_Log("Tip shown: %s", title);
}

// Al morir o al ganar. Se paga lo hecho en ESTE INTENTO (desde el ultimo cobro): el
// capitulo alcanzado, las bajas, los grupos vencidos y los jefes, con el Diezmo encima. Los
// hallazgos del suelo ya entraron al recogerse y se listan aparte, sin volver a sumarse.
void Engine::award_offerings(bool victory) {
    summary = OfferingSummary{};
    summary.victory = victory;
    auto add_line = [this](const char* label, int value) {
        if (summary.count >= 7) return;
        OfferingLine& l = summary.lines[summary.count++];
        snprintf(l.label, sizeof(l.label), "%s", label);
        l.value = value;
    };
    char buf[48];
    const int ch = state.current_chapter;
    int earned = 0;
    snprintf(buf, sizeof(buf), tr("Capítulo %d alcanzado", "Reached chapter %d"), ch);
    add_line(buf, ch * 12);                         earned += ch * 12;
    if (tally.kills > 0) {
        if (tally.kills == 1) snprintf(buf, sizeof(buf), "%s", tr("1 enemigo abatido", "1 enemy struck down"));
        else snprintf(buf, sizeof(buf), tr("%d enemigos abatidos", "%d enemies struck down"), tally.kills);
        add_line(buf, tally.kills);                 earned += tally.kills;
    }
    if (tally.cells > 0) {
        if (tally.cells == 1) snprintf(buf, sizeof(buf), "%s", tr("1 grupo vencido", "1 group defeated"));
        else snprintf(buf, sizeof(buf), tr("%d grupos vencidos", "%d groups defeated"), tally.cells);
        add_line(buf, tally.cells * 4);             earned += tally.cells * 4;
    }
    if (tally.bosses > 0) {
        if (tally.bosses == 1) snprintf(buf, sizeof(buf), "%s", tr("1 jefe derrotado", "1 boss defeated"));
        else snprintf(buf, sizeof(buf), tr("%d jefes derrotados", "%d bosses defeated"), tally.bosses);
        add_line(buf, tally.bosses * 25);           earned += tally.bosses * 25;
    }
    if (victory) {
        add_line(tr("Victoria (x2)", "Victory (x2)"), earned);
        earned *= 2;
    }
    const int r_tithe = progression.rank_of(Meta::DIEZMO);
    if (r_tithe > 0) {
        const int extra = static_cast<int>(earned * (meta_offering_mult(r_tithe) - 1.0f) + 0.5f);
        snprintf(buf, sizeof(buf), tr("Diezmo +%.0f%%", "Tithe +%.0f%%"), (meta_offering_mult(r_tithe) - 1.0f) * 100.0f);
        add_line(buf, extra);
        earned += extra;
    }
    if (tally.found > 0) {
        snprintf(buf, sizeof(buf), "%s", tr("Hallazgos (ya guardados)", "Finds (already banked)"));
        add_line(buf, tally.found);
    }
    summary.total = earned;
    progression.add_offerings(earned);
    sync_progression_to_save();
    save.data.total_runs++;
    summary.bank = progression.offerings;
    SDL_Log("Run over: +%d offerings (total %d) [cap %d, %d bajas, %d grupos, %d jefes, %d hallazgos, diezmo %d]",
            earned, progression.offerings, ch, tally.kills, tally.cells, tally.bosses, tally.found, r_tithe);
    tally = RunTally{};   // lo cobrado no se vuelve a cobrar en la siguiente derrota
    // La pantalla de fin ya explica que son (render_run_summary): el aviso queda visto.
    save.data.tips_seen |= TIP_OFFERINGS;
}

// =============================================================================
// El Santuario (rediseno 2026-10-03)
// =============================================================================
// Pestanas por categoria (El Profeta / Dones / Eliseo / Tesoro / Aspecto), una lista con
// rangos a la izquierda y, a la derecha, el DETALLE de la elegida: que da ahora, que dara
// el siguiente rango y cuanto cuesta. La queja era doble —"no se entiende" y "lo que compras
// no afecta"— y la respuesta tambien: cada fila dice su efecto con la cifra exacta, y las
// mejoras nuevas cambian la partida (levantarse, re-sortear, empezar con un objeto...).
//
// Se abre desde el titulo y desde la pantalla de DERROTA (sanctuary_return): lo que acabas
// de ganar se gasta en el acto y el reintento ya sale con ello puesto.

// Las mejoras de una pestana, en el orden del catalogo (progression.cpp).
static int sanctuary_tab_items(const ProgressionSystem& p, int tab, int* out, int cap) {
    int n = 0;
    for (int i = 0; i < static_cast<int>(p.upgrades.size()) && n < cap; i++)
        if (static_cast<int>(p.upgrades[i].category) == tab) out[n++] = i;
    return n;
}

static const char* sanctuary_tab_name(int tab) {
    switch (static_cast<MetaCategory>(tab)) {
        case MetaCategory::Prophet:  return tr("El Profeta", "The Prophet");
        case MetaCategory::Gifts:    return tr("Dones", "Gifts");
        case MetaCategory::Elisha:   return tr("Eliseo", "Elisha");
        case MetaCategory::Treasure: return tr("Tesoro", "Treasure");
        default:                     return tr("Aspecto", "Looks");
    }
}

void Engine::open_sanctuary(GameScene back_to) {
    sanctuary_return = back_to;
    sanctuary_tab = 0;
    sanctuary_sel = 0;
    sanctuary_scroll = 0;
    sanctuary_flash = 0.0f;
    change_scene(GameScene::Sanctuary);
    save.data.tips_seen |= TIP_SANCTUARY;
}

void Engine::update_sanctuary() {
    const int TABS = static_cast<int>(MetaCategory::COUNT);
    int idx[24];
    int n = sanctuary_tab_items(progression, sanctuary_tab, idx, 24);
    const int rows = sanctuary_rows();
    if (sanctuary_flash > 0.0f) sanctuary_flash = std::max(0.0f, sanctuary_flash - delta_time * 1.6f);
    if (offering_pop > 0.0f)    offering_pop    = std::max(0.0f, offering_pop - delta_time * 1.2f);

    auto set_tab = [&](int t) {
        sanctuary_tab = (t % TABS + TABS) % TABS;
        sanctuary_sel = 0;
        sanctuary_scroll = 0;
        n = sanctuary_tab_items(progression, sanctuary_tab, idx, 24);
        audio.play_sfx(SfxID::MenuSelect, 0.6f);
    };
    auto go_back = [&]() {
        save.save(false);
        change_scene(sanctuary_return);
    };

    bool buy = false;
    if (input.is_action_just_pressed(Action::MoveLeft))  set_tab(sanctuary_tab - 1);
    if (input.is_action_just_pressed(Action::MoveRight)) set_tab(sanctuary_tab + 1);
    if (input.is_action_just_pressed(Action::Miracle1))  set_tab(sanctuary_tab - 1);   // Q / E tambien
    if (input.is_action_just_pressed(Action::Miracle2))  set_tab(sanctuary_tab + 1);
    if (n > 0) {
        if (input.touch_ui) {
            // En tactil arrastrar mueve la VENTANA, no la seleccion
            if (input.is_action_just_pressed(Action::MoveUp))   sanctuary_scroll--;
            if (input.is_action_just_pressed(Action::MoveDown)) sanctuary_scroll++;
        } else {
            if (input.is_action_just_pressed(Action::MoveUp))   { sanctuary_sel = (sanctuary_sel - 1 + n) % n; audio.play_sfx(SfxID::MenuSelect, 0.45f); }
            if (input.is_action_just_pressed(Action::MoveDown)) { sanctuary_sel = (sanctuary_sel + 1) % n;     audio.play_sfx(SfxID::MenuSelect, 0.45f); }
            if (sanctuary_sel < sanctuary_scroll) sanctuary_scroll = sanctuary_sel;
            if (sanctuary_sel >= sanctuary_scroll + rows) sanctuary_scroll = sanctuary_sel - rows + 1;
        }
        sanctuary_sel    = std::max(0, std::min(sanctuary_sel, n - 1));
        sanctuary_scroll = std::max(0, std::min(sanctuary_scroll, std::max(0, n - rows)));
    }

    // Toque: pestana = cambiar; fila = elegirla; OFRECER = comprar; volver = salir. Los rects
    // los apunta render_sanctuary al pintar: una sola formula para el dibujo y el dedo.
    Vec2 tp;
    if (input.take_tap(tp)) {
        int hit = -1;
        for (int t = 0; t < TABS; t++)
            if (sanctuary_tab_rects[t].w > 0.0f && sanctuary_tab_rects[t].contains(tp)) { set_tab(t); hit = 100 + t; break; }
        if (hit < 0) {
            for (int k = 0; k < sanctuary_row_count; k++) {
                if (!sanctuary_row_rects[k].contains(tp)) continue;
                const int i = sanctuary_row_first + k;
                if (i >= n) break;
                if (sanctuary_sel != i) audio.play_sfx(SfxID::MenuSelect, 0.45f);
                sanctuary_sel = i;
                hit = i;
                break;
            }
        }
        if (hit < 0 && sanctuary_buy_rect.w > 0.0f && sanctuary_buy_rect.contains(tp)) { buy = true; hit = 50; }
        if (hit < 0 && sanctuary_back_rect.w > 0.0f && sanctuary_back_rect.contains(tp)) { go_back(); SDL_Log("Santuario tap: volver"); return; }
        SDL_Log("Santuario tap: %d (%.0f,%.0f)", hit, tp.x, tp.y);
    }

    if (input.is_action_just_pressed(Action::Cancel)) { go_back(); return; }
    if (input.is_action_just_pressed(Action::Confirm)) buy = true;
    if (!buy || n <= 0) return;

    MetaUpgrade& u = progression.upgrades[idx[sanctuary_sel]];
    if (u.maxed()) {
        show_message(tr("Ya está al máximo", "Already at its peak"), 1.2f);
        audio.play_sfx(SfxID::MenuSelect, 0.5f);
        return;
    }
    if (progression.offerings < u.next_cost()) {
        char msg[80];
        snprintf(msg, sizeof(msg), tr("Te faltan %d ofrendas", "You need %d more offerings"),
                 u.next_cost() - progression.offerings);
        show_message(msg, 1.6f);
        audio.play_sfx(SfxID::MenuSelect, 0.5f);
        return;
    }
    const int paid = u.next_cost();
    const u32 unlocks = u.unlocks_item;
    progression.purchase(u.id);
    sync_progression_to_save();   // llama a apply_meta_unlocks
    save.save(false);
    audio.play_sfx(SfxID::LevelUp, 0.9f);
    sanctuary_flash = 1.0f;
    sanctuary_flash_id = u.id;
    offering_pop = 1.0f;
    offering_pop_amount = -paid;
    char rank_txt[64];
    meta_rank_text(u, u.rank, rank_txt, sizeof(rank_txt));
    SDL_Log("Santuario: %s rango %d/%d (-%d, quedan %d) -> %s", u.name, u.rank, u.max_rank, paid,
            progression.offerings, rank_txt);
    char msg[128];
    if (unlocks != MetaUpgrade::NO_ITEM) {
        const ItemDef* d = item_registry.get(unlocks);
        snprintf(msg, sizeof(msg), "%s", d ? d->name : "?");
        SDL_Log("Meta unlock: item %u (%s)", unlocks, d ? d->name : "?");
        snprintf(msg, sizeof(msg), tr("%s ya está en juego", "%s is now in play"), d ? d->name : "?");
    } else {
        snprintf(msg, sizeof(msg), "%s: %s", u.name, rank_txt);
    }
    show_message(msg, 2.5f);
}

// El Santuario: las ocho mejoras de progression.cpp con su coste y estado.
// El cartel del tutorial: la instruccion arriba y la linea en voz del juego debajo
// (game/tutorial.h). Va bajo el panel de vitales, que llega a y=30,75, y bien por encima de
// los mensajes del HUD, que salen a y=136: es la unica franja libre de las 180 unidades.
void Engine::render_tutorial_card() {
    if (!tutorial.active || tutorial.finished()) return;

    const bool touch = input.touch_ui;
    const TutorialText txt = tutorial.waiting_door ? tutorial_door_text()
                                                   : tutorial_text(tutorial.current().goal, touch);
    if (!txt.head || !txt.head[0]) return;

    const bool done = tutorial.done_t > 0.0f;

    // Se MIDE antes de pintar y se baja de escala lo que no quepa: `draw_text` no recorta y
    // lo que se pasa de largo se planta sobre el HUD (mismo apaño que `draw_message`).
    constexpr f32 MAXW = 236.0f;
    f32 hs = 0.8f, fs = 0.62f;
    f32 hw = renderer.measure_text(txt.head, hs);
    if (hw > MAXW) { hs *= MAXW / hw; hw = renderer.measure_text(txt.head, hs); }
    f32 fw = renderer.measure_text(txt.flavor, fs);
    if (fw > MAXW) { fs *= MAXW / fw; fw = renderer.measure_text(txt.flavor, fs); }

    const f32 w = std::max(hw, fw) + 16.0f;
    const Rect r = {(static_cast<f32>(Config::GAME_WIDTH) - w) * 0.5f, 34.0f, w, 28.0f};

    renderer.draw_panel({r.x, r.y + 1.7f, r.w, r.h}, {0, 0, 0, 110}, 4.2f);
    renderer.draw_panel(r, done ? Color{38, 72, 44, 238} : Color{40, 34, 30, 238}, 4.2f);

    renderer.draw_text(txt.head, r.x + (r.w - hw) * 0.5f, r.y + 4.0f,
                       done ? Color{150, 240, 160, 255} : Color{245, 214, 132, 255}, hs);
    renderer.draw_text(txt.flavor, r.x + (r.w - fw) * 0.5f, r.y + 16.0f, {186, 172, 150, 225}, fs);

    // Cuantos van: sin la cuenta, nadie sabe si esto dura dos pasos o veinte.
    char n[16];
    snprintf(n, sizeof(n), "%d/%d", std::min(tutorial.step + 1, TUTORIAL_STEP_COUNT), TUTORIAL_STEP_COUNT);
    renderer.draw_text(n, r.x + r.w - renderer.measure_text(n, 0.5f) - 3.0f, r.y + r.h - 7.0f,
                       {150, 138, 120, 200}, 0.5f);

    // Salir: en tactil, el boton de SALTAR de siempre. En PC no lo hay (ESC ya es la pausa
    // que el paso 14 enseña), asi que se dice donde esta la puerta — y solo al principio.
    if (touch)
        draw_skip_button_at(renderer, tutorial_skip_rect(), world.time,
                            menus.press_active && tutorial_skip_rect().contains(menus.press_pos));
    else if (tutorial.step <= 1) {
        // Con su propia placa: suelto sobre el suelo de la sala no se leia (se vio en la
        // captura del paso 1, encima de la fila de tinajas).
        const char* quit = tr("Para dejarlo: ESC y «Dejar el tutorial»",
                              "To quit: ESC, then «Leave the tutorial»");
        const f32 qw = renderer.measure_text(quit, 0.6f);
        renderer.draw_rect({(static_cast<f32>(Config::GAME_WIDTH) - qw) * 0.5f - 4.0f,
                            r.y + r.h + 2.0f, qw + 8.0f, 9.0f}, {18, 15, 13, 190});
        renderer.draw_text_centered(quit, r.y + r.h + 3.0f, {188, 174, 152, 225}, 0.6f);
    }
}

// Icono de una mejora: su celda propia de icons.png (Icon::SANCT_*). Las skins ya traen su
// color pintado: antes eran la antorcha de un objeto tenida, y el tinte la ensuciaba.
static void draw_upgrade_icon(Renderer& r, const MetaUpgrade& u, Rect dst, bool dim) {
    const Color tint = dim ? Color{150, 145, 140, 220} : Color::white();
    if (!draw_icon(r, u.icon, dst, tint))
        r.draw_disc({dst.x + dst.w * 0.5f, dst.y + dst.h * 0.5f}, dst.w * 0.35f, {245, 205, 90, 220});
}

// La moneda: un disco dorado con brillo. Es el mismo color que la ofrenda del suelo.
static void draw_offering_coin(Renderer& r, f32 cx, f32 cy, f32 rad) {
    r.draw_disc({cx, cy + rad * 0.18f}, rad, {120, 80, 20, 200});
    r.draw_disc({cx, cy}, rad, {255, 206, 90, 255});
    r.draw_disc({cx - rad * 0.3f, cy - rad * 0.3f}, rad * 0.35f, {255, 245, 200, 230});
}

void Engine::render_sanctuary() {
    const f32 W = static_cast<f32>(Config::GAME_WIDTH);
    const f32 H = static_cast<f32>(Config::GAME_HEIGHT);
    const bool touch = input.touch_ui;
    renderer.clear({16, 12, 20, 255});
    if (renderer.vignette_tex != TEXTURE_NONE)
        renderer.draw_sprite_rect(renderer.vignette_tex, {0, 0, 160, 90}, {-8, -8, W + 16, H + 16}, {0, 0, 0, 160});

    // --- Cabecera: titulo, que es esto, y el banco ---------------------------------------
    renderer.draw_text(tr("EL SANTUARIO", "THE SANCTUARY"), 10.0f, 4.0f, {240, 215, 150, 255}, 1.15f);
    renderer.draw_text(tr("Lo que ofreces aquí es para siempre: se nota en cada partida.",
                          "What you offer here is forever: you feel it in every run."),
                       10.0f, 18.0f, {170, 152, 120, 225}, 0.55f);
    {
        char bank[24];
        snprintf(bank, sizeof(bank), "%d", progression.offerings);
        const f32 sc = 1.0f;
        const f32 bw = renderer.measure_text(bank, sc);
        const f32 bx = W - 10.0f - bw;
        draw_offering_coin(renderer, bx - 7.0f, 9.5f, 4.2f);
        renderer.draw_text(bank, bx, 4.0f, {255, 214, 110, 255}, sc);
        // Lo que acaba de entrar o salir, flotando bajo el banco
        if (offering_pop > 0.0f && offering_pop_amount != 0) {
            char d[16];
            snprintf(d, sizeof(d), "%+d", offering_pop_amount);
            const u8 a = static_cast<u8>(255.0f * std::min(1.0f, offering_pop * 1.5f));
            renderer.draw_text(d, W - 10.0f - renderer.measure_text(d, 0.7f), 15.0f + (1.0f - offering_pop) * 6.0f,
                               offering_pop_amount > 0 ? Color{150, 240, 150, a} : Color{255, 150, 120, a}, 0.7f);
        }
        char prog[40];
        snprintf(prog, sizeof(prog), tr("Bendiciones %d/%d", "Blessings %d/%d"),
                 progression.ranks_bought(), progression.ranks_total());
        renderer.draw_text(prog, W - 10.0f - renderer.measure_text(prog, 0.5f), 24.0f, {150, 140, 125, 210}, 0.5f);
    }

    // --- Pestanas --------------------------------------------------------------------
    const int TABS = static_cast<int>(MetaCategory::COUNT);
    {
        f32 widths[8];
        f32 total = 0.0f;
        for (int t = 0; t < TABS; t++) {
            // Anchos y x ENTEROS: con fracciones el panel de 9 trozos dejaba una costura
            // oscura vertical en el borde derecho de algunas pestanas.
            widths[t] = std::ceil(renderer.measure_text(sanctuary_tab_name(t), 0.62f) + 12.0f);
            total += widths[t] + 3.0f;
        }
        f32 x = std::floor((W - total + 3.0f) * 0.5f);
        Vec2 pp;
        const bool pressing = touch && input.ui_press(pp);
        for (int t = 0; t < TABS; t++) {
            const bool on = (t == sanctuary_tab);
            const Rect r = {x, 32.0f, widths[t], 11.0f};
            sanctuary_tab_rects[t] = {r.x, r.y - 3.0f, r.w, r.h + 6.0f};
            // ¿Hay algo comprable en esta pestana? Un punto dorado lo dice desde fuera.
            int idx[24];
            const int n = sanctuary_tab_items(progression, t, idx, 24);
            bool can = false;
            for (int k = 0; k < n; k++) {
                const MetaUpgrade& u = progression.upgrades[idx[k]];
                if (!u.maxed() && u.next_cost() <= progression.offerings) { can = true; break; }
            }
            Color fill = on ? Color{150, 112, 44, 245} : Color{48, 40, 44, 230};
            if (!on && pressing && sanctuary_tab_rects[t].contains(pp)) fill = {80, 64, 48, 240};
            renderer.draw_panel(r, fill, 3.5f);
            renderer.draw_text(sanctuary_tab_name(t), r.x + 6.0f, r.y + 1.5f,
                               on ? Color{255, 244, 214, 255} : Color{200, 186, 165, 235}, 0.62f);
            if (can) renderer.draw_disc({r.x + r.w - 2.5f, r.y + 1.5f}, 2.2f, {255, 206, 90, 255});
            x += widths[t] + 3.0f;
        }
        for (int t = TABS; t < 8; t++) sanctuary_tab_rects[t] = {0, 0, 0, 0};
    }

    // --- Lista ------------------------------------------------------------------------
    int idx[24];
    const int n = sanctuary_tab_items(progression, sanctuary_tab, idx, 24);
    const int rows  = sanctuary_rows();
    const f32 PITCH = touch ? 20.0f : 16.0f;
    const f32 LX = 8.0f, LW = 168.0f, LY = 48.0f;
    const int first = std::max(0, std::min(sanctuary_scroll, std::max(0, n - rows)));
    const int last  = std::min(n, first + rows);
    sanctuary_row_first = first;
    sanctuary_row_count = last - first;
    Vec2 pp;
    const bool pressing = touch && input.ui_press(pp);
    renderer.draw_panel({LX - 2.0f, LY - 3.0f, LW + 4.0f, PITCH * rows + 5.0f}, {24, 19, 26, 200}, 4.0f);
    if (first > 0) renderer.draw_text("^", LX + LW - 6.0f, LY - 3.0f, {200, 180, 130, 220}, 0.6f);
    if (last < n)  renderer.draw_text("v", LX + LW - 6.0f, LY + PITCH * rows - 6.0f, {200, 180, 130, 220}, 0.6f);
    for (int i = first; i < last; i++) {
        const MetaUpgrade& u = progression.upgrades[idx[i]];
        const f32 y = LY + (i - first) * PITCH;
        const Rect row = {LX, y, LW, PITCH - 1.0f};
        sanctuary_row_rects[i - first] = row;
        const bool sel = (i == sanctuary_sel);
        const bool afford = !u.maxed() && progression.offerings >= u.next_cost();
        if (sel) renderer.draw_panel(row, {92, 70, 38, 235}, 3.0f);
        else if (pressing && row.contains(pp)) renderer.draw_panel(row, {60, 48, 36, 220}, 3.0f);
        if (sanctuary_flash > 0.0f && u.id == sanctuary_flash_id)
            renderer.draw_panel(row, {255, 230, 150, static_cast<u8>(150.0f * sanctuary_flash)}, 3.0f);

        const f32 is = PITCH - 4.0f;
        draw_upgrade_icon(renderer, u, {LX + 2.0f, y + 1.0f, is, is}, u.rank == 0 && !afford);
        Color name_c = u.maxed() ? Color{150, 220, 150, 250}
                     : sel       ? Color{255, 240, 205, 255}
                                 : Color{214, 202, 184, 240};
        renderer.draw_text(u.name, LX + is + 6.0f, y + 1.0f, name_c, 0.66f);
        // Rango: pepitas llenas = compradas
        if (u.max_rank > 1) {
            for (int k = 0; k < u.max_rank; k++) {
                const Rect pip = {LX + is + 6.5f + k * 5.0f, y + PITCH - 6.5f, 3.5f, 3.0f};
                renderer.draw_rect(pip, k < u.rank ? Color{255, 206, 90, 255} : Color{70, 62, 66, 230});
            }
        } else if (u.rank > 0) {
            renderer.draw_text(tr("tuyo", "yours"), LX + is + 6.5f, y + PITCH - 8.5f, {150, 210, 150, 220}, 0.48f);
        }
        // Coste del siguiente rango, o MAX
        char right[24];
        Color rc;
        if (u.maxed()) { snprintf(right, sizeof(right), "%s", tr("MAX", "MAX")); rc = {150, 220, 150, 240}; }
        else           { snprintf(right, sizeof(right), "%d", u.next_cost()); rc = afford ? Color{255, 214, 110, 250} : Color{170, 100, 90, 230}; }
        const f32 rw = renderer.measure_text(right, 0.62f);
        renderer.draw_text(right, LX + LW - 4.0f - rw, y + (PITCH - 8.0f) * 0.5f, rc, 0.62f);
        if (!u.maxed()) draw_offering_coin(renderer, LX + LW - 8.0f - rw, y + PITCH * 0.5f - 0.5f, 2.4f);
    }

    // --- Detalle ----------------------------------------------------------------------
    sanctuary_buy_rect = {0, 0, 0, 0};
    f32 detail_free_y = 0.0f;
    const f32 DX = 182.0f, DW = W - DX - 8.0f, DY = 45.0f, DH = PITCH * rows + 5.0f + (LY - 3.0f - DY);
    renderer.draw_panel({DX, DY, DW, DH}, {30, 24, 32, 225}, 4.0f);
    if (n > 0) {
        const MetaUpgrade& u = progression.upgrades[idx[std::max(0, std::min(sanctuary_sel, n - 1))]];
        const f32 cx = DX + DW * 0.5f;
        const f32 big = 30.0f;
        if (renderer.shadow_tex != TEXTURE_NONE)
            renderer.draw_sprite_rect(renderer.shadow_tex, {0, 0, 64, 32}, {cx - 17.0f, DY + 27.0f, 34.0f, 8.0f}, {0, 0, 0, 150});
        if (sanctuary_flash > 0.0f && u.id == sanctuary_flash_id)
            renderer.draw_ring({cx, DY + 18.0f}, 12.0f + 16.0f * (1.0f - sanctuary_flash),
                               {255, 225, 140, static_cast<u8>(230.0f * sanctuary_flash)});
        draw_upgrade_icon(renderer, u, {cx - big * 0.5f, DY + 3.0f, big, big}, false);
        f32 y = DY + 35.0f;
        std::string lines[4];
        int ln = renderer.wrap_text(u.name, DW - 8.0f, 0.75f, lines, 2);
        for (int k = 0; k < ln; k++) {
            renderer.draw_text(lines[k].c_str(), cx - renderer.measure_text(lines[k].c_str(), 0.75f) * 0.5f, y, {255, 236, 190, 255}, 0.75f);
            y += 8.0f;
        }
        ln = renderer.wrap_text(u.description, DW - 8.0f, 0.55f, lines, 3);
        for (int k = 0; k < ln; k++) {
            renderer.draw_text(lines[k].c_str(), cx - renderer.measure_text(lines[k].c_str(), 0.55f) * 0.5f, y + 1.0f, {205, 195, 180, 240}, 0.55f);
            y += 6.5f;
        }
        y += 3.0f;
        // Un desbloqueo de objeto dice QUE objeto y QUE hace, con su color de rareza
        if (u.unlocks_item != MetaUpgrade::NO_ITEM) {
            if (const ItemDef* it = item_registry.get(u.unlocks_item)) {
                renderer.draw_text(it->name, DX + 5.0f, y, item_rarity_color(it->rarity), 0.55f);
                y += 6.5f;
                std::string il[2];
                const int in = renderer.wrap_text(it->description, DW - 10.0f, 0.5f, il, 2);
                for (int k = 0; k < in; k++) { renderer.draw_text(il[k].c_str(), DX + 5.0f, y, {195, 186, 172, 230}, 0.5f); y += 6.0f; }
                y += 2.0f;
            }
        }
        char now_t[64], next_t[64];
        meta_rank_text(u, u.rank, now_t, sizeof(now_t));
        renderer.draw_text(tr("Ahora:", "Now:"), DX + 5.0f, y, {150, 140, 128, 230}, 0.52f);
        renderer.draw_text(now_t, DX + 5.0f + renderer.measure_text(tr("Ahora:", "Now:"), 0.52f) + 3.0f, y,
                           u.rank > 0 ? Color{170, 230, 170, 245} : Color{180, 170, 160, 220}, 0.52f);
        y += 7.0f;
        if (!u.maxed()) {
            meta_rank_text(u, u.rank + 1, next_t, sizeof(next_t));
            const char* lab = (u.max_rank > 1) ? tr("Siguiente:", "Next:") : tr("Al ofrecer:", "Once offered:");
            renderer.draw_text(lab, DX + 5.0f, y, {150, 140, 128, 230}, 0.52f);
            renderer.draw_text(next_t, DX + 5.0f + renderer.measure_text(lab, 0.52f) + 3.0f, y, {255, 222, 140, 250}, 0.52f);
        }
        detail_free_y = y + 9.0f;   // aqui cabe el aviso de la compra (ver abajo)
        // La cita, abajo y apagada: sabor, no instruccion
        ln = renderer.wrap_text(u.quote, DW - 10.0f, 0.48f, lines, 2);
        const f32 qy = DY + DH - 26.0f - ln * 5.5f;
        for (int k = 0; k < ln; k++)
            renderer.draw_text(lines[k].c_str(), cx - renderer.measure_text(lines[k].c_str(), 0.48f) * 0.5f, qy + k * 5.5f,
                               {150, 132, 104, 200}, 0.48f);
        // El boton de OFRECER (o lo que falta)
        const Rect btn = {DX + 6.0f, DY + DH - 17.0f, DW - 12.0f, 13.0f};
        char label[48];
        Color fill, txt;
        if (u.maxed()) {
            snprintf(label, sizeof(label), "%s", tr("COMPLETO", "COMPLETE"));
            fill = {44, 70, 48, 235}; txt = {170, 235, 170, 255};
        } else if (progression.offerings >= u.next_cost()) {
            snprintf(label, sizeof(label), tr("OFRECER  %d", "OFFER  %d"), u.next_cost());
            const f32 pulse = 0.5f + 0.5f * std::sin(world.time * 5.0f);
            fill = {static_cast<u8>(170 + 30 * pulse), static_cast<u8>(124 + 20 * pulse), 44, 245};
            if (pressing && btn.contains(pp)) fill = {220, 170, 70, 255};
            txt = {255, 248, 228, 255};
        } else {
            snprintf(label, sizeof(label), tr("Te faltan %d", "You need %d more"), u.next_cost() - progression.offerings);
            fill = {58, 38, 38, 230}; txt = {220, 150, 130, 240};
        }
        renderer.draw_panel(btn, fill, 3.5f);
        renderer.draw_text(label, btn.x + (btn.w - renderer.measure_text(label, 0.66f)) * 0.5f, btn.y + 2.5f, txt, 0.66f);
        sanctuary_buy_rect = btn;
    }

    // --- Pie: volver y teclas -----------------------------------------------------------
    sanctuary_back_rect = touch ? Rect{8.0f, H - 15.0f, 52.0f, 13.0f} : Rect{0, 0, 0, 0};
    if (touch) {
        renderer.draw_panel(sanctuary_back_rect,
                            (pressing && sanctuary_back_rect.contains(pp)) ? Color{96, 74, 40, 240} : Color{58, 46, 30, 225}, 3.5f);
        renderer.draw_text(tr("< Volver", "< Back"), sanctuary_back_rect.x + 6.0f, sanctuary_back_rect.y + 2.5f,
                           {240, 226, 196, 250}, 0.66f);
        renderer.draw_text(tr("Toca una mejora y luego OFRECER", "Tap an upgrade, then OFFER"),
                           70.0f, H - 12.0f, {140, 130, 118, 210}, 0.55f);
    } else {
        renderer.draw_text_centered(tr("[A/D] pestaña   [W/S] elegir   [ENTER] ofrecer   [ESC] volver",
                                       "[A/D] tab   [W/S] choose   [ENTER] offer   [ESC] back"),
                                    H - 11.0f, {140, 130, 118, 210}, 0.55f);
    }
    // El aviso de la compra ("Fuego Interior: +4 de dano por disparo"), DENTRO del panel de
    // detalle, justo bajo las cifras: habla de la mejora que se ve ahi.
    if (message_timer > 0.0f && !message_text.empty() && detail_free_y > 0.0f) {
        const f32 a = std::min(1.0f, message_timer / 0.4f);
        std::string ml[2];
        const int mn = renderer.wrap_text(message_text.c_str(), DW - 16.0f, 0.58f, ml, 2);
        const Rect m = {DX + 6.0f, detail_free_y, DW - 12.0f, 6.0f + mn * 6.8f};
        renderer.draw_panel(m, {46, 64, 44, static_cast<u8>(235 * a)}, 3.5f);
        for (int k = 0; k < mn; k++)
            renderer.draw_text(ml[k].c_str(), m.x + (m.w - renderer.measure_text(ml[k].c_str(), 0.58f)) * 0.5f,
                               m.y + 2.5f + k * 6.8f, {210, 250, 200, static_cast<u8>(255 * a)}, 0.58f);
    }
}

// =============================================================================
// Feedback
// =============================================================================
// Una voz cada VOICE_GAP segundos como mucho. El grito de jefe y el grito de guerra del
// milagro se saltan la espera: el de jefe suena una vez por capitulo, y el del milagro va con
// un poder de 20+ s de recarga — y con la espera, un dash o un golpe recibido justo antes se
// lo comian, que es exactamente el momento en el que tiene que oirse.
void Engine::play_voice(Voice v, f32 volume) {
    constexpr f32 VOICE_GAP = 2.2f;
    if (v != Voice::Jefe && v != Voice::Milagro && voice_cooldown > 0.0f) return;
    voice_cooldown = VOICE_GAP;

    const bool elias = (player.character == CharacterType::Elias);
    // Una linea por exclamacion: es lo que permite comprobar desde un guion que la voz
    // ha sonado y con que toma. Sin esto, `audio_check` solo podia decir que el fichero
    // se habia cargado, no que se hubiera oido.
    static const char* NAMES[] = {"esfuerzo", "dolor", "milagro", "jefe"};
    SDL_Log("Voice: %s (%s)", NAMES[static_cast<int>(v)],
            (v == Voice::Jefe) ? "-" : (elias ? "Elias" : "Eliseo"));
    switch (v) {
        case Voice::Esfuerzo:
            audio.play_sfx_variant(elias ? SfxID::VozEliasEsfuerzo1 : SfxID::VozEliseoEsfuerzo1,
                                   3, volume);
            break;
        case Voice::Dolor:
            audio.play_sfx_variant(elias ? SfxID::VozEliasDolor1 : SfxID::VozEliseoDolor1,
                                   3, volume);
            break;
        case Voice::Milagro:
            // Grito de guerra, uno por profeta (tools/audio/gen_audio.py, grito_guerra):
            // Elias grave, Eliseo mas alto. Ya no se sortea entre los dos.
            audio.play_sfx(elias ? SfxID::VozMilagro1 : SfxID::VozMilagro2, volume);
            break;
        case Voice::Jefe:
            audio.play_sfx(SfxID::VozJefe, volume);
            break;
    }
}

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
    save.data.run_active      = true;   // hay partida que continuar (ver SaveSystem::has_save)
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
    // v12: la alforja (y el campo viejo con el elegido, por si alguien lee v11)
    for (int i = 0; i < SAVE_BELT_SLOTS && i < Player::BELT_MAX; i++) {
        save.data.belt_ids[i]     = player.belt[i].empty() ? UINT32_MAX : player.belt[i].id;
        save.data.belt_charges[i] = player.belt[i].empty() ? 0 : player.belt[i].charges;
    }
    save.data.belt_selected   = player.belt_sel;
    save.data.consumable_id   = player.belt[player.belt_sel].empty() ? UINT32_MAX : player.belt[player.belt_sel].id;
    save.data.consumable_uses = player.belt[player.belt_sel].empty() ? 0 : player.belt[player.belt_sel].charges;
    save.data.revives_left    = revives_left;
    save.data.rerolls_left    = rerolls_left;
    save.data.prayers_left    = prayers_left;
    save.data.active_slots  = player.active_slot_count;
    for (int s = 0; s < Config::MAX_MIRACLE_SLOTS; s++)
        save.data.miracle_ids[s] = static_cast<u32>(player.miracle_slots[s].miracle);

    // Read HP from ECS if player entity is valid
    if (player.entity != ENTITY_NONE &&
        world.has(player.entity, Component::Health)) {
        save.data.hp = world.healths[player.entity].current;
    }

    // v9: resumen por sala (despejada / cofre / secretos), re-aplicable al Continuar
    capture_room_states();
}

// =============================================================================
// Mapas fijos: generacion pura + estado de sala persistido (v9)
// =============================================================================
// LA definicion de "como se genera una sala": re-siembra rand() con una semilla derivada de
// (chapter_seed, indice de sala) y genera. La usan load_room (primera visita) y
// capture_room_states (copia pristina para el diff del guardado); si alguna vez divergen,
// el guardado restauraria salas que no son las que se jugaron.
void Engine::generate_pristine_cell(int node_index, Room& out) {
    const auto& node = floor_map.rooms[node_index];
    out.door_north = (node.connections[0] >= 0);
    out.door_south = (node.connections[1] >= 0);
    out.door_east  = (node.connections[2] >= 0);
    out.door_west  = (node.connections[3] >= 0);
    int difficulty = (state.current_chapter - 1) % Config::CHAPTERS_ACT_I + 1;   // 1..5 per act
    srand(chapter_seed ^ ((static_cast<u32>(node_index) + 1u) * 2654435761u));
    out.generate(node.type, state.current_chapter, difficulty, node.event_id,
                 node.interior_room >= 0);
}

// La ZONA del capitulo: cada celda de la rejilla generada en limpio (pura por (capitulo,
// celda), igual que siempre) y cosida en su sitio; luego las costuras. Es una funcion pura
// de (chapter_seed, capitulo, rejilla): la misma zona en cada arranque, al Continuar y en
// cada guardado (pristine_zone la cachea para el diff).
void Engine::build_pristine_zone(Room& zone) {
    const int gw = floor_map.grid_w, gh = floor_map.grid_h;
    const BiomeStyle biome = get_biome_style(state.current_chapter);
    zone.type       = RoomType::Zone;
    zone.chapter    = state.current_chapter;
    zone.border     = biome.border;
    zone.cells_x    = gw;
    zone.cells_y    = gh;
    zone.cell_w     = Config::ROOM_TILES_X;
    zone.cell_h     = Config::ROOM_TILES_Y;
    zone.cell_count = std::min(gw * gh, Config::ZONE_CELLS_MAX);
    zone.tiles_w    = gw * zone.cell_w;
    zone.tiles_h    = gh * zone.cell_h;
    zone.has_doorway = false;
    zone.has_arena = false;
    zone.arena_sealed = false;
    zone.boss_cell = -1;
    zone.exit_open = false;
    zone.door_north = zone.door_south = zone.door_east = zone.door_west = false;
    zone.enemy_spawns.clear();
    for (auto& cs : zone.cells) cs = CellState{};
    SDL_memset(zone.tiles, static_cast<int>(TileType::Floor), sizeof(zone.tiles));

    static Room cell_tmp;   // ~15 KB: fuera de la pila
    for (int i = 0; i < zone.cell_count && i < floor_map.room_count; i++) {
        const auto& node = floor_map.rooms[i];
        if (node.grid_x < 0 || node.grid_y < 0) continue;
        generate_pristine_cell(i, cell_tmp);
        zone.blit_cell(cell_tmp, node.grid_x, node.grid_y, i);
    }
    zone.open_seams();
    static const char* const SIDE_NAME[4] = {"North", "South", "West", "East"};
    SDL_Log("Zone built: %dx%d cells (%dx%d tiles), boss cell %d, arena %s (%s), door %s",
            gw, gh, zone.tiles_w, zone.tiles_h, zone.boss_cell,
            zone.has_arena ? "yes" : "no", SIDE_NAME[static_cast<int>(zone.arena_side) & 3],
            zone.has_doorway ? "yes" : "no");
}

// Comprobacion de la zona recien cosida. Con el jefe en un lado distinto por capitulo y la
// rejilla al doble, "se genera" ya no garantiza "se juega": se inunda desde el nudo de la
// celda de inicio y se mira que se llegue al vano de la arena, a cada nudo, a cada punto de
// interes y a la puerta, y que el cerco de la arena no tenga mas agujero que su vano. Es
// una linea de log (`Zone check: ...`) y el gate de mapas_check.
void Engine::check_zone(const Room& zone) {
    static bool reach[Config::ZONE_TILES_Y][Config::ZONE_TILES_X];
    SDL_memset(reach, 0, sizeof(reach));
    int start = -1;
    for (int i = 0; i < zone.cell_count && i < floor_map.room_count; i++)
        if (floor_map.rooms[i].type == RoomType::Start) start = i;
    if (start < 0) return;
    std::vector<std::pair<int, int>> q;
    q.reserve(4096);
    const int sx = zone.cells[start].hub_tx, sy = zone.cells[start].hub_ty;
    if (zone.is_tile_walkable(sx, sy)) { reach[sy][sx] = true; q.push_back({sx, sy}); }
    for (size_t k = 0; k < q.size(); k++) {
        const int x = q[k].first, y = q[k].second;
        const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
        for (int d = 0; d < 4; d++) {
            const int nx = x + dx[d], ny = y + dy[d];
            if (nx < 0 || ny < 0 || nx >= zone.tiles_w || ny >= zone.tiles_h || reach[ny][nx]) continue;
            if (!zone.is_tile_walkable(nx, ny)) continue;
            reach[ny][nx] = true;
            q.push_back({nx, ny});
        }
    }
    int hubs = 0, hubs_ok = 0, pois = 0, pois_ok = 0;
    for (int i = 0; i < zone.cell_count && i < floor_map.room_count; i++) {
        const CellState& cs = zone.cells[i];
        hubs++;
        if (reach[cs.hub_ty][cs.hub_tx]) hubs_ok++;
        else SDL_Log("Zone check: nudo de la celda %d (%d,%d) INALCANZABLE", i, cs.hub_tx, cs.hub_ty);
        if (cs.poi_tx >= 0) {
            pois++;
            if (reach[cs.poi_ty][cs.poi_tx]) pois_ok++;
            else SDL_Log("Zone check: punto de interes de la celda %d (%d,%d) INALCANZABLE", i, cs.poi_tx, cs.poi_ty);
        }
    }
    bool gap_ok = false, ring_ok = true, door_ok = !zone.has_doorway;
    if (zone.has_arena) {
        int ox, oy;
        zone.arena_gap_outside(ox, oy);
        gap_ok = reach[oy][ox];
        const int x0 = zone.arena_x0, y0 = zone.arena_y0;
        const int x1 = x0 + Config::ARENA_TILES_X + 1, y1 = y0 + Config::ARENA_TILES_Y + 1;
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++)
                if (zone.in_arena_ring(x, y) && !zone.is_arena_gap(x, y) && zone.is_tile_walkable(x, y)) {
                    ring_ok = false;
                    SDL_Log("Zone check: agujero en el cerco de la arena en (%d,%d)", x, y);
                }
    }
    if (zone.has_doorway) {
        const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
        for (int d = 0; d < 4 && !door_ok; d++) {
            const int nx = zone.door_tx + dx[d], ny = zone.door_ty + dy[d];
            if (nx >= 0 && ny >= 0 && nx < zone.tiles_w && ny < zone.tiles_h && reach[ny][nx]) door_ok = true;
        }
    }
    SDL_Log("Zone check: vano %s, cerco %s, nudos %d/%d, puntos %d/%d, puerta %s -> %s",
            gap_ok ? "ok" : "INALCANZABLE", ring_ok ? "entero" : "ROTO", hubs_ok, hubs, pois_ok, pois,
            door_ok ? "ok" : "INALCANZABLE",
            (gap_ok && ring_ok && hubs_ok == hubs && pois_ok == pois && door_ok) ? "OK" : "FALLA");
}

const Room& Engine::pristine_zone() {
    static Room pz;
    static u32 pz_seed = 0;
    static int pz_chapter = -1, pz_gw = 0, pz_gh = 0;
    if (pz_seed != chapter_seed || pz_chapter != state.current_chapter ||
        pz_gw != floor_map.grid_w || pz_gh != floor_map.grid_h) {
        build_pristine_zone(pz);
        pz_seed = chapter_seed;
        pz_chapter = state.current_chapter;
        pz_gw = floor_map.grid_w;
        pz_gh = floor_map.grid_h;
    }
    return pz;
}

Room* Engine::zone_ptr() {
    if (current_room_data.type == RoomType::Zone) return &current_room_data;
    return floor_map.zone_built ? &floor_map.room_cache[0] : nullptr;
}

const Room* Engine::zone_ptr() const {
    if (current_room_data.type == RoomType::Zone) return &current_room_data;
    return floor_map.zone_built ? &floor_map.room_cache[0] : nullptr;
}

// Vuelca a save.data el estado de cada CELDA de la zona (y del interior): un byte de flags +
// UN BIT por tile alterado desde la generacion, en coordenadas LOCALES de la celda (stride
// Config::ROOM_TILES_X, como siempre). El diff se calcula contra la zona PRISTINA
// (pristine_zone, cacheada: regenerar 16 celdas por guardado daria un tiron en mitad de la
// pelea), asi que caza CUALQUIER mecanica que toque tiles sin contabilidad aparte. No se
// guarda el tilemap: el terreno se regenera identico y solo viajan las celdas cambiadas.
void Engine::capture_room_states() {
    static_assert(FloorMap::MAX_ROOMS <= SAVE_MAX_ROOMS, "SaveData::room_flags se queda corto");
    static_assert(Config::ZONE_CELLS_MAX <= SAVE_MAX_ROOMS, "SaveData::room_flags se queda corto para la zona");
    SDL_memset(save.data.room_flags, 0, sizeof(save.data.room_flags));
    SDL_memset(save.data.room_open,  0, sizeof(save.data.room_open));

    const Room* zone = zone_ptr();
    if (zone && floor_map.zone_built) {
        const Room& pz = pristine_zone();
        for (int i = 0; i < zone->cell_count && i < SAVE_MAX_ROOMS && i < floor_map.room_count; i++) {
            const CellState& cs = zone->cells[i];
            u8 f = 0;
            if (cs.visited)      f |= 1u << 0;
            if (cs.cleared)      f |= 1u << 1;
            if (cs.altar_used)   f |= 1u << 2;
            if (cs.chest_opened) f |= 1u << 3;
            if (cs.event_used)   f |= 1u << 4;
            if (i == zone->boss_cell && zone->exit_open) f |= 1u << 5;
            save.data.room_flags[i] = f;

            const auto& node = floor_map.rooms[i];
            const int ox = node.grid_x * zone->cell_w, oy = node.grid_y * zone->cell_h;
            u8* bits = save.data.room_open[i];
            for (int y = 0; y < zone->cell_h; y++)
                for (int x = 0; x < zone->cell_w; x++)
                    if (zone->tiles[oy + y][ox + x] != pz.tiles[oy + y][ox + x]) {
                        int cell = y * Config::ROOM_TILES_X + x;
                        bits[cell >> 3] |= static_cast<u8>(1u << (cell & 7));
                    }
        }
    }
    // El interior (nodo detras de la rejilla), si se ha generado en esta sesion
    for (int i = floor_map.grid_w * floor_map.grid_h; i < floor_map.room_count && i < SAVE_MAX_ROOMS; i++) {
        if (floor_map.rooms[i].type != RoomType::Interior || !floor_map.room_generated[i]) continue;
        const Room& r = (i == floor_map.current_room && current_room_data.type == RoomType::Interior)
                      ? current_room_data : floor_map.room_cache[1];
        const CellState& cs = r.cells[0];
        u8 f = 0;
        if (cs.visited)      f |= 1u << 0;
        if (cs.cleared)      f |= 1u << 1;
        if (cs.altar_used)   f |= 1u << 2;
        if (cs.chest_opened) f |= 1u << 3;
        if (cs.event_used)   f |= 1u << 4;
        save.data.room_flags[i] = f;

        static Room pristine;   // ~15 KB: fuera de la pila
        generate_pristine_cell(i, pristine);
        u8* bits = save.data.room_open[i];
        for (int y = 0; y < r.tiles_h; y++)
            for (int x = 0; x < r.tiles_w; x++)
                if (r.tiles[y][x] != pristine.tiles[y][x]) {
                    int cell = y * Config::ROOM_TILES_X + x;
                    bits[cell >> 3] |= static_cast<u8>(1u << (cell & 7));
                }
    }
    // La generacion pristina deja el rand() global sembrado con la semilla de la ULTIMA celda
    // regenerada — y esto corre en CADA guardado. Sin re-sembrar aqui, todo lo que consuma
    // rand() despues (la tirada 1-de-5 de cartas de update_room_state, drops, deriva de IA)
    // se volvia funcion constante de esa semilla: o todas las celdas normales soltaban carta o
    // ninguna. El contador da una semilla distinta por guardado pero determinista para los
    // guiones (misma secuencia de guardados -> misma secuencia de semillas).
    srand(chapter_seed ^ (++rng_epoch * 2654435761u) ^ 0x85EBCA6Bu);
}

// El tile final se DERIVA del recien generado: todas las transformaciones de runtime van a
// Floor (matorral quemado, tinaja rota, grieta abierta, charca seca) salvo cofre / encuentro /
// altar de Baal, que van a Path (ecs.cpp y update_room_state). El arco de salida no viaja en
// el diff como valor: lo replica place_exit(), que pinta exactamente las mismas celdas. Si una
// mecanica nueva cambia tiles a OTRA cosa, tiene que anadirse aqui o el Continuar la perdera.
static TileType restored_tile(TileType t) {
    return (t == TileType::Chest || t == TileType::Event || t == TileType::AltarBaal)
           ? TileType::Path : TileType::Floor;
}

// Re-aplica sobre la ZONA recien construida lo que el guardado dice de UNA celda.
void Engine::apply_saved_cell_state(int cell, Room& zone) {
    if (cell < 0 || cell >= SAVE_MAX_ROOMS || cell >= zone.cell_count || cell >= floor_map.room_count) return;
    // Sin nada guardado no se toca: f == 0 implica "sin visitar" (y sin tiles alterados)
    const u8 f = save.data.room_flags[cell];
    if (f == 0) return;
    const u8* bits = save.data.room_open[cell];
    const auto& node = floor_map.rooms[cell];
    const int ox = node.grid_x * zone.cell_w, oy = node.grid_y * zone.cell_h;
    int reopened = 0;
    for (int y = 0; y < zone.cell_h; y++) {
        for (int x = 0; x < zone.cell_w; x++) {
            int c = y * Config::ROOM_TILES_X + x;
            if (!(bits[c >> 3] & (1u << (c & 7)))) continue;
            zone.tiles[oy + y][ox + x] = restored_tile(zone.tiles[oy + y][ox + x]);
            reopened++;
        }
    }
    CellState& cs = zone.cells[cell];
    cs.visited      = (f >> 0) & 1;
    cs.cleared      = (f >> 1) & 1;
    cs.altar_used   = (f >> 2) & 1;
    cs.chest_opened = (f >> 3) & 1;
    cs.event_used   = (f >> 4) & 1;
    if (((f >> 5) & 1) && cell == zone.boss_cell) zone.place_exit();
    SDL_Log("Room %d: estado restaurado del guardado (flags %02x, %d celdas)", cell, f, reopened);
}

// Idem sobre el INTERIOR recien generado (una sola celda de estado).
void Engine::apply_saved_interior_state(int node_index, Room& r) {
    if (node_index < 0 || node_index >= SAVE_MAX_ROOMS) return;
    const u8 f = save.data.room_flags[node_index];
    if (f == 0) return;   // escribir ceros pisaba el `cleared = true` con el que nace un interior
    const u8* bits = save.data.room_open[node_index];
    int reopened = 0;
    for (int y = 0; y < r.tiles_h; y++) {
        for (int x = 0; x < r.tiles_w; x++) {
            int c = y * Config::ROOM_TILES_X + x;
            if (!(bits[c >> 3] & (1u << (c & 7)))) continue;
            r.tiles[y][x] = restored_tile(r.tiles[y][x]);
            reopened++;
        }
    }
    CellState& cs = r.cells[0];
    cs.visited      = (f >> 0) & 1;
    cs.cleared      = (f >> 1) & 1;
    cs.altar_used   = (f >> 2) & 1;
    cs.chest_opened = (f >> 3) & 1;
    cs.event_used   = (f >> 4) & 1;
    SDL_Log("Room %d: estado restaurado del guardado (flags %02x, %d celdas)", node_index, f, reopened);
}

// Morir no borra la partida. El guardado se rebobina al principio del capitulo (profeta
// entero, mapa nuevo), que es justo donde deja "Reintentar": asi las dos salidas de la
// pantalla de derrota llevan al mismo sitio y "Continuar" sigue existiendo. El equipo
// (objetos, consumible y milagros, tal como los dejo populate_save_data) se conserva,
// igual que en retry_chapter(): lo que se pierde al morir es el capitulo, no el run.
void Engine::save_after_defeat() {
    populate_save_data();
    save.data.run_active   = true;
    save.data.current_room = -1;          // el capitulo se regenera: la sala guardada ya no existe
    save.data.chapter_seed = 0;           // se reintenta con la semilla fija del capitulo
    // La derrota rebobina el capitulo ENTERO: el estado de sala no sobrevive (los enemigos
    // vuelven), asi que el resumen v9 que populate_save_data acaba de volcar se borra.
    SDL_memset(save.data.room_flags, 0, sizeof(save.data.room_flags));
    SDL_memset(save.data.room_open,  0, sizeof(save.data.room_open));
    save.data.hp           = 9999.0f;     // continue_run() lo recorta al maximo del profeta
    // Fe de arranque + lo que aporten los objetos conservados: continue_run() reaplica los
    // items pero luego fija la fe a este valor, asi que el bonus tiene que venir ya sumado
    // (si no, "Continuar" y "Reintentar" salian del mismo sitio con distinta fe)
    save.data.faith        = meta_start_faith(progression.rank_of(Meta::FE));   // Fe Renovada incluida
    // El capitulo y el intento empiezan de nuevo: los dones vuelven enteros al Continuar
    save.data.revives_left = -1;
    save.data.rerolls_left = -1;
    save.data.prayers_left = -1;
    for (int i = 0; i < save.data.item_count; i++) {
        const ItemDef* d = item_registry.get(save.data.item_ids[i]);
        if (d) save.data.faith += d->faith_bonus;
    }
    save.save();
}

// GDD 4.4: en movil una partida abandonada 48 h ya no se continua. En escritorio la
// regla no aplica (`mobile_rules`), salvo con --touch para poder probarla.
void Engine::refresh_continue_state() {
    menus.has_continue        = save.has_save();
    menus.continue_expired    = menus.has_continue && mobile_rules && save.is_run_expired();
    menus.abandoned_offerings = menus.continue_expired ? save.get_abandoned_offerings() : 0;
    menus.offerings           = progression.offerings;
    menus.sanctuary_ready     = progression.cheapest_affordable() != nullptr;
    if (menus.continue_expired)
        SDL_Log("Abandoned run (chapter %d): %d offerings on the table",
                save.data.current_chapter, menus.abandoned_offerings);
}

// Aceptar la partida abandonada: se cobra el 50% y el guardado se BORRA (la
// meta-progresion sobrevive: delete_save reescribe el fichero solo con ella).
void Engine::claim_abandoned_run() {
    int reward = save.get_abandoned_offerings();
    progression.add_offerings(reward);
    sync_progression_to_save();
    save.delete_save();
    refresh_continue_state();
    SDL_Log("Abandoned run claimed: +%d offerings (total %d)", reward, progression.offerings);
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
    boss_talk_seen = 0;
    run_events_done = 0;
    pending_scene  = -1;
    boss_approach_t = 0.0f;   // ningun paseo de jefe sobrevive a un reinicio
    state.run_time        = save.data.run_time;
    run_kills = 0;
    run_hits_taken = 0;
    feel_mark = FeelMark{};

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
    // Lo comprado en El Santuario: hasta el 2026-10-03 el Continuar NO lo aplicaba y la vida,
    // el dano y la velocidad comprados desaparecian al volver a la partida.
    apply_meta_upgrades();
    tally = RunTally{};
    primicias_pending = false;

    // Same chapter layout as when the run was saved (the seed is stored), then jump straight
    // to the saved room. Con el estado de sala persistido (v9), las salas despejadas siguen
    // despejadas y los secretos abiertos siguen abiertos: load_room re-aplica el resumen
    // guardado sobre cada sala al regenerarla.
    pending_seed = save.data.chapter_seed;
    restore_room_state = true;
    start_chapter(state.current_chapter, ChapterAnnounce::None);   // sin intro: no es una primera entrada

    // Loadout: re-applying the items restores max HP / damage / speed / cadence
    for (int i = 0; i < save.data.item_count; i++) {
        const ItemDef* d = item_registry.get(save.data.item_ids[i]);
        // Un guardado viejo traia el consumible tambien en la lista: ahora vive en la alforja
        if (d && d->category != ItemCategory::Consumable) player.grant_item(world, faith, *d);
    }
    // Recalcular la reliquia SIEMPRE al rehacer el equipo: sus efectos no viven en el
    // Player sino en FaithSystem::floor_value y Player::miracle_power, y en la Ascension
    // la fe NO se reinicia — sin esto, un Elias con las Tablas dejaba a Eliseo con el
    // suelo de fe en 25 para siempre, sin llevar la reliquia.
    player.apply_relic_effects(faith);
    // La alforja tal cual se guardo (pisa el agua que start_chapter acaba de poner)
    for (int i = 0; i < Player::BELT_MAX && i < SAVE_BELT_SLOTS; i++) {
        player.belt[i] = Player::BeltSlot{};
        if (save.data.belt_ids[i] != UINT32_MAX && save.data.belt_charges[i] > 0) {
            player.belt[i].id = save.data.belt_ids[i];
            player.belt[i].charges = std::min<u8>(9, save.data.belt_charges[i]);
        }
    }
    player.belt_sel = save.data.belt_selected;
    player.belt_fix_selection();
    // Los dones gastados: lo que quedaba al guardar (-1 = el intento empieza de nuevo)
    if (save.data.revives_left >= 0) revives_left = save.data.revives_left;
    else                             refill_attempt_gifts();
    if (save.data.rerolls_left >= 0) rerolls_left = save.data.rerolls_left;
    if (save.data.prayers_left >= 0) prayers_left = save.data.prayers_left;
    player.active_slot_count = std::max(1, std::min(Config::MAX_MIRACLE_SLOTS, save.data.active_slots));
    for (int s = 0; s < Config::MAX_MIRACLE_SLOTS; s++) {
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

    // La carta del capitulo tambien al continuar: el jugador vuelve dias despues y hay que
    // recordarle donde esta y que se le pide (el mundo ya esta cargado detras).
    story.begin();
    story_chapter_card(story, state.current_chapter);
    play_story(StoryThen::Play);
}

// =============================================================================
// Zona continua: cargar el mapa, entrar en una celda, celdas vivas y la arena
// =============================================================================
static const char* room_type_name(RoomType t) {
    switch (t) {
        case RoomType::Start:    return "Start";
        case RoomType::Boss:     return "Boss";
        case RoomType::Interior: return "Interior";
        case RoomType::Event:    return "Event";
        case RoomType::Normal:   return "Normal";
        case RoomType::Elite:    return "Elite";
        case RoomType::Treasure: return "Treasure";
        case RoomType::Altar:    return "Altar";
        case RoomType::Rest:     return "Rest";
        case RoomType::Zone:     return "Zone";
        default:                 return "Special";
    }
}

// Volcado ASCII de UNA celda (40 columnas x 22 filas; un interior, 20 x 11): el mismo
// formato de siempre, para que los gates de los guiones sigan valiendo.
void Engine::dump_cell_ascii(const Room& r, int cell) {
    if (!autoplay.active) return;
    static const char glyph[] = ".#OW^v><BGCTbRg~PFXEtNjcD";  // indexed by TileType (j tinaja, c grieta, D puerta)
    static_assert(sizeof(glyph) - 1 == static_cast<int>(TileType::COUNT),
                  "un TileType nuevo necesita su glifo en el volcado ASCII");
    const Rect rc = r.cell_rect_world(cell);
    const int x0 = static_cast<int>(rc.x) / Config::TILE_SIZE, y0 = static_cast<int>(rc.y) / Config::TILE_SIZE;
    const int w = std::min(Config::ROOM_TILES_X, static_cast<int>(rc.w) / Config::TILE_SIZE);
    const int h = static_cast<int>(rc.h) / Config::TILE_SIZE;
    for (int y = 0; y < h && y0 + y < r.tiles_h; y++) {
        char line[Config::ROOM_TILES_X + 1];
        int n = 0;
        for (int x = 0; x < w && x0 + x < r.tiles_w; x++) {
            u8 t = static_cast<u8>(r.tiles[y0 + y][x0 + x]);
            line[n++] = t < sizeof(glyph) - 1 ? glyph[t] : '?';
        }
        line[n] = 0;
        SDL_Log("  %s", line);
    }
}

// Coloca al profeta SIN dejar los pies dentro de un solido (misma referencia que el
// movimiento), salta la camara a su sitio y presenta al discipulo si toca.
void Engine::place_player(Vec2 spawn_pos) {
    if (player.entity == ENTITY_NONE || !world.has(player.entity, Component::Transform)) return;
    Vec2 foot = world.has(player.entity, Component::Collider) ? world.colliders[player.entity].offset : Vec2{0, 0};
    Vec2 free_feet;
    if (!current_room_data.is_walkable_world(spawn_pos + foot) &&
        current_room_data.nearest_walkable_world(spawn_pos + foot, free_feet)) {
        spawn_pos = free_feet - foot;
    }
    world.transforms[player.entity].position = spawn_pos;
    player.chariot_fx_t = -1.0f;   // un carro a medio despedirse no viaja con el teletransporte
    player.sky_fire_t = -1.0f;     // ni un Fuego del Cielo a medio caer (sus VFX ya se barrieron)
    clear_hazards(world);          // ni un telegrafo de jefe pintado donde ya no esta el profeta
    if (world.has(player.entity, Component::Movement)) {
        world.movements[player.entity].knockback = {0, 0};
        world.movements[player.entity].velocity = {0, 0};
    }
    // La camara SALTA a su sitio; sin esto haria un barrido desde donde estaba
    camera.set_target(spawn_pos);
    camera.position = spawn_pos;
    camera.update(0.0f);

    // "La Escuela de Profetas" (mejora meta 7): Eliseo entra acompanado. El discipulo se
    // vuelve a presentar en cada carga (el mundo se vacia), que es lo que el jugador
    // percibe como "permanente".
    if (progression.is_unlocked(Meta::ESCUELA) && player.character == CharacterType::Eliseo) {
        spawn_disciple(world, spawn_pos + Vec2{18.0f, 6.0f},
                       enemy_textures[static_cast<u32>(EnemyType::SoldadoDeAcab)],
                       0.0f, 40.0f, 10.0f);
    }
}

// `load_room(nodo)` ya no carga "una sala": para un nodo de la rejilla construye la ZONA
// entera si hace falta (o la recupera de la ranura 0 al volver de un interior o de un
// escenario) y teletransporta al profeta al nudo de esa celda (o a `spawn_tx/ty`, si vuelve
// por una puerta); para el nodo del interior carga la pantalla del interior (ranura 1).
// Barre todas las entidades salvo el profeta: las celdas vivas se vuelven a sembrar solas
// (update_cells) en el frame siguiente, lo que hace el teletransporte determinista.
void Engine::load_room(int room_index) {
    if (room_index < 0 || room_index >= floor_map.room_count) {
        SDL_Log("load_room: invalid index %d (max %d)", room_index, floor_map.room_count);
        return;
    }
    world.popups.clear();
    world.hitstop = 0.0f;
    room_change_grace = Config::Feel::COYOTE_TIME;   // ver use_doorway

    for (u32 e = 0; e < MAX_ENTITIES; e++) {
        if (static_cast<Entity>(e) != world.player && world.masks[e].any()) {
            world.destroy(static_cast<Entity>(e));
        }
    }
    boss_entity = ENTITY_NONE;
    reset_boss_bar();
    clear_hazards(world);   // ningun telegrafo sobrevive a una carga de sala
    darkness_level = 0.0f;
    if (Room* z = zone_ptr()) {
        for (int i = 0; i < z->cell_count; i++) z->cells[i].active = false;
        z->arena_sealed = false;
    }

    const auto& node = floor_map.rooms[room_index];
    auto def = get_chapter_def(state.current_chapter);

    if (node.type == RoomType::Interior) {
        if (current_room_data.type == RoomType::Zone) floor_map.room_cache[0] = current_room_data;
        if (floor_map.room_generated[room_index]) {
            current_room_data = floor_map.room_cache[1];
        } else {
            generate_pristine_cell(room_index, current_room_data);
            if (apply_saved_rooms) apply_saved_interior_state(room_index, current_room_data);   // Continuar
            floor_map.room_cache[1] = current_room_data;
            floor_map.room_generated[room_index] = true;
        }
        room_fade = 1.0f;   // el fundido queda para la puerta
        floor_map.current_room = room_index;
        state.current_room = room_index;
        current_room_data.cells[0].visited = true;
        player.took_damage_this_room = false;
        world.active_room = &current_room_data;
        camera.set_room({0, 0, current_room_data.world_w(), current_room_data.world_h()}, true);
        Vec2 spawn_pos = (spawn_tx >= 0 && spawn_ty >= 0)
                       ? current_room_data.tile_to_world(spawn_tx, spawn_ty)
                       : current_room_data.tile_to_world(current_room_data.cells[0].hub_tx, current_room_data.cells[0].hub_ty);
        spawn_tx = spawn_ty = -1;
        place_player(spawn_pos);
        dump_cell_ascii(current_room_data, 0);
        show_message(get_interior_name(state.current_chapter), 2.2f);
        SDL_Log("Loaded room %d [%d,%d] (%s) - Interior [0 enemies]%s",
                room_index, node.grid_x, node.grid_y, def.name,
                current_room_data.cells[0].cleared ? " (cleared)" : "");
        return;
    }

    // Nodo de la rejilla: la zona
    if (!floor_map.zone_built) {
        build_pristine_zone(current_room_data);
        check_zone(current_room_data);   // la zona que se va a JUGAR (no la copia pristina del guardado)
        if (apply_saved_rooms)   // Continuar (v9/v10): cada celda con estado guardado
            for (int i = 0; i < current_room_data.cell_count; i++) apply_saved_cell_state(i, current_room_data);
        for (int i = 0; i < current_room_data.cell_count && i < floor_map.room_count; i++)
            floor_map.room_generated[i] = true;
        floor_map.room_cache[0] = current_room_data;
        floor_map.zone_built = true;
        room_fade = 1.0f;
    } else if (current_room_data.type != RoomType::Zone) {
        // Volviendo de un interior o de un escenario de escena: la zona esta en la ranura 0
        // (el interior ya se cacheo en use_doorway; un escenario no se guarda)
        current_room_data = floor_map.room_cache[0];
        room_fade = 1.0f;
    }
    world.active_room = &current_room_data;
    enter_cell(room_index);
}

// Teletransporte al nudo de una celda (o a spawn_tx/ty). Escribe la misma linea de log
// `Loaded room ...` de siempre: es el gate de media docena de guiones.
void Engine::enter_cell(int cell) {
    Room& zone = current_room_data;
    if (zone.type != RoomType::Zone || cell < 0 || cell >= zone.cell_count || cell >= floor_map.room_count) return;
    const auto& node = floor_map.rooms[cell];
    floor_map.current_room = cell;
    state.current_room = cell;
    zone.cells[cell].visited = true;
    player.took_damage_this_room = false;
    camera.set_room({0, 0, zone.world_w(), zone.world_h()}, true);

    int hx = zone.cells[cell].hub_tx, hy = zone.cells[cell].hub_ty;
    // La puerta del interior se coloca sobre la senda y puede caer JUSTO en el nudo:
    // aparecer encima la dispara sola tras la gracia. Un tile mas abajo.
    if (hy + 1 < zone.tiles_h && zone.tiles[hy][hx] == TileType::Doorway) hy++;
    Vec2 spawn_pos = (spawn_tx >= 0 && spawn_ty >= 0)
                   ? zone.tile_to_world(spawn_tx, spawn_ty)
                   : zone.tile_to_world(hx, hy);
    spawn_tx = spawn_ty = -1;
    place_player(spawn_pos);

    dump_cell_ascii(zone, cell);
    auto def = get_chapter_def(state.current_chapter);
    SDL_Log("Loaded room %d [%d,%d] (%s) - %s [%d enemies]%s",
            cell, node.grid_x, node.grid_y, def.name, room_type_name(node.type),
            static_cast<int>(zone.cells[cell].spawns.size()),
            zone.cells[cell].cleared ? " (cleared)" : "");
}

void Engine::spawn_cell_enemies(int cell) {
    Room& zone = current_room_data;
    if (zone.type != RoomType::Zone || cell < 0 || cell >= zone.cell_count) return;
    for (const auto& spawn : zone.cells[cell].spawns) {
        Entity e = spawn_enemy(world, spawn.type, spawn.position,
                               enemy_textures[static_cast<u32>(spawn.type)], state.current_chapter);
        codex_see_enemy(save.data, spawn.type);   // el bestiario se llena viéndolos
        if (e != ENTITY_NONE) {
            world.enemy_ais[e].home_cell = cell;
            SDL_Log("Spawned enemy '%s' at (%.0f, %.0f)",
                    get_enemy_def(spawn.type).name, spawn.position.x, spawn.position.y);
        }
    }
}

// Todo hostil nacido en esa celda, cadaveres incluidos. Por celda de ORIGEN, nunca por
// posicion: un enemigo que cruzo una costura persiguiendo sigue siendo de su celda.
void Engine::despawn_cell(int cell) {
    int n = 0;
    for (u32 e = 0; e < world.entity_count; e++) {
        if (!world.masks[e].any() || static_cast<Entity>(e) == world.player) continue;
        if (!world.has(e, Component::EnemyAI) || world.enemy_ais[e].ally) continue;
        if (world.enemy_ais[e].home_cell != cell) continue;
        world.destroy(static_cast<Entity>(e));
        n++;
    }
    SDL_Log("Cell %d despawned (%d entities)", cell, n);
}

// Celdas VIVAS (ALTTP). Cada frame de Playing: (1) la celda del profeta pasa a ser la
// actual; (2) una celda de combate a menos de media pantalla siembra sus enemigos, y a
// mas de 1,2 pantallas los repone (asi ni el presupuesto de 512 entidades ni la IA se
// disparan); (3) la arena se dispara al ENTRAR con los pies, y la camara se clava a ella
// mientras el profeta esta dentro y la pelea no ha acabado.
void Engine::update_cells() {
    Room& zone = current_room_data;
    if (zone.type != RoomType::Zone) return;
    if (player.entity == ENTITY_NONE || !world.has(player.entity, Component::Transform)) return;
    const Vec2 feet = world.transforms[player.entity].position + Vec2{0, 6};
    // "Dentro de la arena" = pies sobre su SUELO, con el borde de abajo EXCLUSIVO: la fila
    // del vano no cuenta, o el sellado cerraria el tile que el profeta esta pisando.
    auto in_arena = [&]() {
        if (!zone.has_arena) return false;
        const Rect a = zone.arena_rect_world();
        return feet.x >= a.x && feet.x < a.right() && feet.y >= a.y && feet.y < a.bottom();
    };

    const int pc = zone.cell_of_world(feet);
    if (pc >= 0 && pc < floor_map.room_count && pc != floor_map.current_room) {
        floor_map.current_room = pc;
        state.current_room = pc;
        zone.cells[pc].visited = true;
        const auto& node = floor_map.rooms[pc];
        SDL_Log("Entered cell %d [%d,%d] (%s)%s", pc, node.grid_x, node.grid_y, room_type_name(node.type),
                zone.cells[pc].cleared ? " (cleared)" : "");
    }

    const f32 sw = static_cast<f32>(Config::GAME_WIDTH), sh = static_cast<f32>(Config::GAME_HEIGHT);
    for (int i = 0; i < zone.cell_count && i < floor_map.room_count; i++) {
        CellState& cs = zone.cells[i];
        const auto& node = floor_map.rooms[i];
        if (!is_combat_room(node.type)) continue;
        if (node.type == RoomType::Boss) {
            if (!cs.cleared && boss_entity == ENTITY_NONE && !zone.arena_sealed && in_arena())
                enter_arena();
            continue;
        }
        const Rect r = zone.cell_rect_world(i);
        auto within = [&](f32 k) {
            return feet.x >= r.x - sw * k && feet.x < r.right() + sw * k &&
                   feet.y >= r.y - sh * k && feet.y < r.bottom() + sh * k;
        };
        if (!cs.active && !cs.cleared && within(Config::CELL_ACTIVATE_SCREENS)) {
            spawn_cell_enemies(i);
            cs.active = true;
            player.took_damage_this_room = false;   // aproximacion aceptada con celdas solapadas
        } else if (cs.active && !cs.cleared && !within(Config::CELL_DESPAWN_SCREENS)) {
            despawn_cell(i);
            cs.active = false;
        }
    }

    // Camara: la arena mientras se esta dentro y el jefe no ha caido; si no, la zona
    if (zone.boss_cell >= 0 && !zone.cells[zone.boss_cell].cleared && in_arena())
        camera.set_room(zone.arena_bounds_world(), false);
    else
        camera.set_room({0, 0, zone.world_w(), zone.world_h()}, false);
}

// =============================================================================
// El Carro de Fuego visible (2026-09-24)
// =============================================================================
// assets/sprites/chariot.png: 8x3 celdas de 256 px (64 unidades). Fila = direccion (este,
// sur, norte; el oeste es el este en espejo), columnas 0-3 = capa `back` de los cuatro
// fotogramas del galope y 4-7 = capa `front`. `back` va justo DETRAS del profeta en el orden
// por Y y `front` justo DELANTE: su propio sprite queda dentro de la caja. El ancla (el suelo
// de la caja) cae en CHARIOT_ANCHOR — la tabla ANCHOR de chariot.py: si cambia alli, aqui.
void Engine::collect_chariot_draws(std::vector<DrawCmd>& out) {
    if (chariot_texture == TEXTURE_NONE || player.chariot_fx_t < 0.0f) return;
    static const f32 CHARIOT_ANCHOR[3][2] = {{52.0f, 158.0f}, {128.0f, 92.0f}, {128.0f, 200.0f}};
    constexpr f32 CELL = 256.0f;
    const Vec2 d = player.chariot_fx_dir;
    int row = 0;               // este
    bool flip = false;
    if (std::fabs(d.y) > std::fabs(d.x) * 1.15f) row = d.y > 0.0f ? 1 : 2;   // sur (hacia camara) / norte
    else flip = d.x < 0.0f;                                                  // oeste = este en espejo
    const int frame = static_cast<int>(player.chariot_fx_t * 14.0f) % 4;
    f32 ax = CHARIOT_ANCHOR[row][0], ay = CHARIOT_ANCHOR[row][1];
    if (flip) ax = CELL - ax;
    // Un 20% mayor que la escala de las hojas: a escala real (caballos tan altos como el
    // profeta) el carro se perdia en una pantalla de 320x180; asi el profeta cabe en la caja
    // y el tiro se lee de lejos. Se escala sobre el ancla, que es lo que tiene que casar.
    const f32 inv = 1.2f / static_cast<f32>(Config::ASSET_SCALE);
    const Vec2 a = player.chariot_fx_pos - Vec2{0, player.chariot_fx_lift};
    const Rect dst = {a.x - ax * inv, a.y - ay * inv, CELL * inv, CELL * inv};
    // Aparece en 0,08 s y se apaga en la segunda mitad de la despedida
    f32 alpha = std::min(1.0f, player.chariot_fx_t / 0.08f);
    const f32 fade0 = Config::CHARIOT_FX_DEPART * 0.45f;
    if (player.chariot_fx_free > fade0)
        alpha *= std::max(0.0f, 1.0f - (player.chariot_fx_free - fade0) / (Config::CHARIOT_FX_DEPART - fade0));
    const u8 a8 = static_cast<u8>(255.0f * alpha);
    // El orden por Y: pegado al del profeta (sus pies = el ancla) mientras lo lleva; al
    // soltarlo, el del propio carro, sin la subida (la sombra de lo que vuela no cambia).
    const f32 sort = player.chariot_fx_pos.y;
    for (int layer = 0; layer < 2; layer++) {
        DrawCmd c;
        c.texture_id = chariot_texture;
        c.src = {(layer * 4 + frame) * CELL, row * CELL, CELL, CELL};
        c.dst = dst;
        c.flip_x = flip;
        c.tint = {255, 255, 255, a8};
        c.sort_y = sort + (layer == 0 ? -0.5f : 0.5f);
        out.push_back(c);
    }
}

// =============================================================================
// Fuego del Cielo (2 Reyes 1:10): el cielo se cierra y el fuego BAJA
// =============================================================================
// Por encima del mundo y de la vinieta, y por debajo del fogonazo y del HUD: el fuego viene
// del cielo, asi que no lo tapan ni los arboles ni el propio profeta. Solo es dibujo; el
// tiempo, el dano y la llave del bioma son de Player::update_sky_fire.
void Engine::render_sky_fire() {
    const f32 t = player.sky_fire_t;
    if (t < 0.0f) return;
    const f32 FALL = Config::SKY_FIRE_FALL;
    const f32 W = static_cast<f32>(Config::GAME_WIDTH), H = static_cast<f32>(Config::GAME_HEIGHT);

    // El cielo: rojo oscuro mientras baja el fuego; al golpe, un fogonazo naranja que se apaga
    if (t < FALL) {
        const f32 k = std::min(1.0f, t / 0.12f);
        renderer.draw_rect({0, 0, W, H}, {70, 12, 0, static_cast<u8>(100.0f * k)});
    } else {
        const f32 k = std::max(0.0f, 1.0f - (t - FALL) / Config::SKY_FIRE_AFTER);
        renderer.draw_rect({0, 0, W, H}, {255, 150, 60, static_cast<u8>(130.0f * k * k)});
    }
    if (t >= FALL || tileset_texture == TEXTURE_NONE) return;

    renderer.begin_world(camera.position);
    const Rect fire = get_tile_src(TileSprite::Fireball);
    // Sale justo por encima del borde de arriba (el profeta va centrado: 90 de media pantalla
    // + media bola) y entra algo ladeada, desde arriba a la izquierda
    constexpr f32 H0 = 130.0f, DRIFT = -45.0f;
    const f32 ang = std::atan2(H0, -DRIFT) * 57.2957795f;   // la celda apunta a +x
    for (int i = 0; i < player.sky_bolt_count; i++) {
        const Player::SkyBolt& b = player.sky_bolts[i];
        // Tres fantasmas detras de la cabeza hacen la estela; el ultimo (g = 0) es la bola
        for (int g = 3; g >= 0; g--) {
            const f32 tg = t - g * 0.03f;
            if (tg < b.start) continue;
            const f32 u = std::min(1.0f, (tg - b.start) / (FALL - b.start));
            const f32 left = 1.0f - std::pow(u, 1.4f);   // ACELERA: cae, no flota
            const Vec2 p = {b.target.x + DRIFT * left, b.target.y - H0 * left};
            const f32 shrink = 1.0f - g * 0.16f;
            const f32 len = 16.0f * 3.4f * b.size * shrink, wid = 16.0f * 2.3f * b.size * shrink;
            if (g == 0) {
                // Resplandor detras: la misma bola mas grande y translucida. Es lo que hace que
                // se lea "fuego" y no "piedra naranja"
                const f32 gl = len * 1.35f, gw = wid * 1.6f;
                renderer.draw_sprite_rect(tileset_texture, fire, {p.x - gl * 0.5f, p.y - gw * 0.5f, gl, gw},
                                          {255, 110, 30, 120}, false, ang);
                renderer.draw_sprite_rect(tileset_texture, fire, {p.x - len * 0.5f, p.y - wid * 0.5f, len, wid},
                                          {255, 238, 200, 255}, false, ang);
            } else {
                renderer.draw_sprite_rect(tileset_texture, fire, {p.x - len * 0.5f, p.y - wid * 0.5f, len, wid},
                                          {255, 130, 50, static_cast<u8>(170 - g * 45)}, false, ang);
            }
        }
    }
    renderer.end_world();
}

// La oscuridad de La Muerte (cap. 8, 2026-10-03). Se pinta en PANTALLA, despues de cerrar
// la pasada de mundo: un velo casi negro con un agujero de luz alrededor del profeta. El
// agujero es la vinieta del Renderer (alfa 0 en el centro -> 1 en el borde) estirada a un
// cuadrado, y el resto de la pantalla, cuatro rectangulos lisos. Los ojos del jefe brillan
// a traves: hay que poder saber DONDE esta aunque no se le vea.
void Engine::render_darkness() {
    if (darkness_level <= 0.01f) return;
    if (player.entity == ENTITY_NONE || !world.has(player.entity, Component::Transform)) return;
    const f32 W = static_cast<f32>(Config::GAME_WIDTH), H = static_cast<f32>(Config::GAME_HEIGHT);
    const Vec2 view_o = {camera.position.x - W * 0.5f, camera.position.y - H * 0.5f};
    const Vec2 pp = world.transforms[player.entity].position - view_o;
    const f32 R = darkness_radius * 1.6f * (1.0f + 0.03f * std::sin(world.time * 2.1f));
    const Color dark = {6, 4, 14, static_cast<u8>(228.0f * darkness_level)};
    auto rect = [&](f32 x, f32 y, f32 w, f32 h) { if (w > 0.0f && h > 0.0f) renderer.draw_rect({x, y, w, h}, dark); };
    if (renderer.vignette_tex != TEXTURE_NONE)
        renderer.draw_sprite_rect(renderer.vignette_tex, {0, 0, 160, 90}, {pp.x - R, pp.y - R, R * 2.0f, R * 2.0f}, dark);
    else
        rect(pp.x - R, pp.y - R, R * 2.0f, R * 2.0f);
    rect(-12.0f, -12.0f, W + 24.0f, pp.y - R + 12.0f);                       // arriba
    rect(-12.0f, pp.y + R, W + 24.0f, H + 12.0f - (pp.y + R));               // abajo
    rect(-12.0f, pp.y - R, pp.x - R + 12.0f, R * 2.0f);                      // izquierda
    rect(pp.x + R, pp.y - R, W + 12.0f - (pp.x + R), R * 2.0f);              // derecha
    // Los ojos de la Muerte se ven en lo oscuro
    if (boss_entity != ENTITY_NONE && world.is_alive(boss_entity) && world.has(boss_entity, Component::Health) &&
        world.healths[boss_entity].is_alive() && world.sprites[boss_entity].visible) {
        const auto& bt = world.transforms[boss_entity];
        const Vec2 eye = bt.position - view_o - Vec2{0.0f, 11.0f * bt.scale.y};
        const f32 pulse = 0.6f + 0.4f * std::sin(world.time * 3.0f);
        const u8 a = static_cast<u8>(200.0f * darkness_level * pulse);
        renderer.draw_disc(eye, 7.0f, {120, 140, 255, static_cast<u8>(a / 4)});
        renderer.draw_disc(eye + Vec2{-2.0f, 0.0f}, 1.2f, {200, 220, 255, a});
        renderer.draw_disc(eye + Vec2{ 2.0f, 0.0f}, 1.2f, {200, 220, 255, a});
    }
}

// =============================================================================
// Pistas del jefe (2026-09-24)
// =============================================================================
// El nivel se DERIVA del estado de las celdas (rooms.h, get_boss_intel); aqui solo se avisa
// cuando sube. El aviso es un BANNER y no un mensaje de pie: es lo que cambia el rumbo del
// jugador, y un texto pequeno que dura dos segundos se pierde en mitad de una pelea.
void Engine::update_boss_intel() {
    if (tutorial.active) return;
    const Room* z = zone_ptr();
    if (!z || !z->has_arena) return;
    const BossIntel bi = get_boss_intel(*z, floor_map, player.map_revealed);
    if (boss_intel_shown < 0) { boss_intel_shown = bi.level; return; }
    if (bi.level <= boss_intel_shown) return;
    SDL_Log("Boss intel %d -> %d (pistas %d%s)", boss_intel_shown, bi.level, bi.clues,
            bi.seen ? ", visto" : "");
    boss_intel_shown = bi.level;
    static char sub[128];
    if (bi.level == 1) {
        f32 cx = 0.0f, cy = 0.0f, r = 0.0f;
        boss_rumor_circle(*z, cx, cy, r);
        Vec2 from = {z->world_w() * 0.5f, z->world_h() * 0.5f};
        if (player.entity != ENTITY_NONE && world.has(player.entity, Component::Transform) && z == &current_room_data)
            from = world.transforms[player.entity].position;
        const f32 dx = cx * Config::TILE_SIZE - from.x, dy = cy * Config::TILE_SIZE - from.y;
        snprintf(sub, sizeof(sub), tr("Dicen que el jefe aguarda hacia el %s. Míralo en el mapa",
                                      "They say the boss waits to the %s. Check the map"),
                 compass_name(dx, dy, false));
    } else if (bi.seen && !player.map_revealed && bi.clues < BOSS_CLUES_FOR_EXACT) {
        snprintf(sub, sizeof(sub), "%s", tr("Desde aquí se ve su arena: ya está en el mapa",
                                            "You can see its arena from here: it is on the map"));
    } else {
        snprintf(sub, sizeof(sub), "%s", tr("Ya sabes dónde está el jefe: está en el mapa",
                                            "Now you know where the boss is: it is on the map"));
    }
    show_banner(tr("PISTA", "CLUE"), sub, 3.5f);
    audio.play_sfx(SfxID::Notification, 0.8f);
}

// El profeta pisa la arena: nace el jefe (o el grupo del cap. 3), se sella el vano, banner y
// voz, y el dialogo del jefe (game/scenes.h) se pide para el siguiente paso de Playing.
void Engine::enter_arena() {
    Room& zone = current_room_data;
    if (zone.type != RoomType::Zone || !zone.has_arena || zone.boss_cell < 0) return;
    const int bc = zone.boss_cell;
    auto def = get_chapter_def(state.current_chapter);
    int ax, ay;
    zone.arena_anchor(ax, ay);
    Vec2 pos = zone.tile_to_world(ax, ay);
    codex_see_enemy(save.data, def.boss_type);
    // Cap. 3: sumo sacerdote (la cabeza, en el ancla de la escena Live) + corro de profetas
    // bailando alrededor del centro de la arena, con barra compartida.
    // La hoja del jefe se cargo al empezar el capitulo; si el capitulo se abrio por otro
    // camino (escena forzada), se carga aqui.
    if (boss_sheets.main == TEXTURE_NONE) load_boss_sheets(state.current_chapter);
    clear_hazards(world);
    boss_entity = spawn_boss_group(world, def.boss_type, pos, pos + Vec2{0, 40}, boss_sheets,
                                   state.current_chapter, def.boss_name, def.boss_pattern);
    // El encuentro del capitulo puede haber debilitado al jefe (agua sobre el altar, ojos
    // abiertos): se aplica al desangrarlo, no al definirlo. A TODO el grupo.
    for (u32 e = 0; e < world.entity_count; e++) {
        if (!world.is_alive(e) || !world.has(e, Component::EnemyAI) || !world.has(e, Component::Health)) continue;
        if (!world.enemy_ais[e].is_boss && !world.enemy_ais[e].boss_group) continue;
        world.enemy_ais[e].home_cell = bc;
        if (boss_hp_scale < 1.0f) {
            auto& bh = world.healths[e];
            bh.max *= boss_hp_scale;
            bh.current = bh.max;
        }
    }
    if (boss_hp_scale < 1.0f && boss_entity != ENTITY_NONE)
        show_message(tr("El jefe entra debilitado", "The boss comes in weakened"), 2.5f);
    // La barra: el maximo del grupo se fija AQUI (las reposiciones no lo mueven)
    reset_boss_bar();
    boss_bar_name = def.boss_name;
    if (boss_entity != ENTITY_NONE) {
        for (u32 e = 0; e < world.entity_count; e++) {
            if (!world.is_alive(e) || !world.has(e, Component::EnemyAI) || !world.has(e, Component::Health)) continue;
            if (world.enemy_ais[e].boss_group) boss_group_max += world.healths[e].max;
        }
        if (boss_group_max > 0.0f) boss_group_max += world.healths[boss_entity].max;
    }
    update_boss_bar();

    zone.arena_sealed = true;
    zone.cells[bc].active = true;
    player.took_damage_this_room = false;
    // DESLIZANDO, no de golpe: el recorte pasa de la zona entera al recinto en ~0,12 s
    // (Camera::update). Con `true` la camara daba un salto en el mismo frame en que el
    // jefe nacia, y ese salto era la mitad del "corte brusco" del que se quejo el usuario.
    camera.set_room(zone.arena_bounds_world(), false);
    SDL_Log("Arena sealed: boss cell %d", bc);

    char buf[64];
    snprintf(buf, sizeof(buf), "%s", def.boss_name);
    show_banner(tr("JEFE", "BOSS"), buf, 2.5f);
    play_voice(Voice::Jefe, 0.9f);   // se salta la espera: suena una vez por capitulo
    audio.play_sfx(SfxID::CriticalHit, 0.9f);
    // El jefe habla antes de pelear (game/scenes.h): se pide aqui y arranca en el siguiente
    // paso de Playing. Una vez por partida y capitulo: morir y reintentar no lo repite. Bajo
    // autoplay se suprime (los guiones cronometran las peleas) y lo fuerza `escena`.
    const u32 talk_bit = 1u << state.current_chapter;
    if (Scene::boss_of(state.current_chapter) >= 0 && !(boss_talk_seen & talk_bit)) {
        boss_talk_seen |= talk_bit;
        if (autoplay.active && !autoplay_boss_talk)
            SDL_Log("Dialogo del jefe %d suprimido (autoplay)", state.current_chapter);
        else {
            pending_scene = Scene::boss_of(state.current_chapter);
            // ...y delante de la escena, el paseo. Va atado al dialogo a proposito: quien
            // vuelve tras morir ya lo vio (`boss_talk_seen`) y lo que quiere es pelear, no
            // ver otra vez al profeta andando.
            start_boss_approach(pending_scene);
        }
    }
}

// =============================================================================
// La entrada del jefe (pulido post-APK, 2026-09-11)
// =============================================================================
// El destino NO se inventa: es el punto exacto al que `start_scene` teletransporta al
// profeta para el dialogo (tres filas bajo el jefe). Si fuera otro, el teletransporte de
// `start_scene` se veria como un salto y el corte seguiria ahi. El primer frame de la
// escena y el ultimo del paseo tienen que ser el MISMO encuadre.
void Engine::start_boss_approach(int scene) {
    if (player.entity == ENTITY_NONE || !world.has(player.entity, Component::Transform)) return;
    Room& zone = current_room_data;
    if (!zone.has_arena) return;

    const SceneDef def = get_scene_def(scene);
    int ax, ay;
    zone.arena_anchor(ax, ay);
    boss_approach_from = world.transforms[player.entity].position;
    boss_approach_to   = zone.tile_to_world(ax + def.player_dx, ay + def.player_dy);
    boss_approach_eye  = zone.tile_to_world(ax, ay);
    boss_approach_t    = Config::Feel::BOSS_APPROACH_MAX;
    SDL_Log("boss approach start: chapter %d, (%.0f,%.0f) -> (%.0f,%.0f)",
            state.current_chapter, boss_approach_from.x, boss_approach_from.y,
            boss_approach_to.x, boss_approach_to.y);
}

// El mundo sigue vivo y dibujandose: esto NO es una escena. Lo unico que cambia es quien
// manda sobre el profeta (el motor, no el dedo) y que la camara se adelanta.
void Engine::update_boss_approach(f32 dt) {
    boss_approach_t -= dt;

    // Quien ya ha visto esto diez veces no puede quedarse atrapado: Cancel (el boton atras
    // en Android) corta el paseo y va derecho a la escena.
    const bool skipped = input.is_action_just_pressed(Action::Cancel);

    Vec2 pos = boss_approach_to;
    if (player.entity != ENTITY_NONE && world.has(player.entity, Component::Transform)) {
        auto& t = world.transforms[player.entity];
        pos = t.position;
        Vec2 d = boss_approach_to - pos;
        const f32 dist = d.length();
        if (world.has(player.entity, Component::Movement)) {
            auto& m = world.movements[player.entity];
            // Se EMPUJA con velocidad, no se desliza la posicion: asi `system_movement` le
            // pone el rumbo y `system_animation` la fila de andar, igual que si caminara el
            // jugador. Cerca del destino se frena, para que no llegue derrapando.
            m.velocity = (dist > 1.0f)
                       ? d.normalized() * m.speed * Config::Feel::BOSS_APPROACH_SPEED
                                        * std::min(1.0f, dist / 12.0f)
                       : Vec2{0, 0};
            m.knockback = {0, 0};
        }
        if (dist <= 2.0f) boss_approach_t = 0.0f;
    }

    world.update(dt);          // movimiento, rumbo, animacion, proyectiles y efectos
    renderer.update_shake(dt);

    // La camara no se limita a seguirlo: se ADELANTA hacia el jefe, y vuelve al profeta
    // justo al final para que el encuadre con el que acaba el paseo sea el mismo con el
    // que empieza la escena.
    const f32 total = std::max(1.0f, boss_approach_from.distance_to(boss_approach_to));
    f32 k = 1.0f - std::min(1.0f, pos.distance_to(boss_approach_to) / total);
    f32 lead = Config::Feel::BOSS_APPROACH_LEAD * (k * (1.0f - k) * 4.0f);   // 0 -> pico -> 0
    camera.set_target(Vec2::lerp(pos, boss_approach_eye, lead));
    camera.update(dt);

    if (skipped || boss_approach_t <= 0.0f) {
        boss_approach_t = 0.0f;
        if (player.entity != ENTITY_NONE && world.has(player.entity, Component::Movement))
            world.movements[player.entity].velocity = {0, 0};
        // La camara se ASIENTA aqui, no durante la escena: en Dialogue nadie la actualiza,
        // asi que un paseo saltado a mitad dejaba el recorte a medio deslizar y la escena
        // empezaba con la cabeza del jefe cortada por arriba.
        camera.room_bounds = camera.bounds_goal;
        camera.set_target(boss_approach_to);
        camera.position = boss_approach_to;
        camera.update(0.0f);
        SDL_Log("boss approach end (%s)", skipped ? "saltado" : "completo");
    }
}

// El paseo de una apertura guionizada. Misma maquinaria que la entrada del jefe —se
// EMPUJA con velocidad, no se desliza la posicion, para que `system_movement` le ponga el
// rumbo y `system_animation` la fila de andar— pero sin adelanto de camara: aqui lo que
// hay que mirar es al profeta LLEGANDO, no el sitio al que va.
void Engine::update_scene_walk(f32 dt) {
    scene_walk_t -= dt;

    Vec2 pos = scene_walk_to;
    if (player.entity != ENTITY_NONE && world.has(player.entity, Component::Transform)) {
        auto& t = world.transforms[player.entity];
        pos = t.position;
        const Vec2 d = scene_walk_to - pos;
        const f32 dist = d.length();
        if (world.has(player.entity, Component::Movement)) {
            auto& m = world.movements[player.entity];
            m.velocity = (dist > 1.0f)
                       ? d.normalized() * m.speed * Config::Feel::BOSS_APPROACH_SPEED
                                        * std::min(1.0f, dist / 12.0f)
                       : Vec2{0, 0};
            m.knockback = {0, 0};
        }
        if (dist <= 2.0f) scene_walk_t = 0.0f;
    }

    world.update(dt);          // movimiento, rumbo y animacion: por eso camina de verdad
    renderer.update_shake(dt);
    camera.set_target(pos);
    camera.update(dt);

    if (scene_walk_t <= 0.0f) {
        scene_walk_t = 0.0f;
        if (player.entity != ENTITY_NONE && world.has(player.entity, Component::Movement))
            world.movements[player.entity].velocity = {0, 0};
        // La camara se ASIENTA aqui, no en la caja siguiente: en Dialogue nadie la
        // actualiza, asi que un paseo cortado dejaria el recorte a medio deslizar.
        camera.room_bounds = camera.bounds_goal;
        camera.set_target(scene_walk_to);
        camera.position = scene_walk_to;
        camera.update(0.0f);
        // Y mira a donde dice la escena. `system_animation` lo dejo con el rumbo del ultimo
        // paso (llegando de lado, o de espaldas), y la caja siguiente es la del encuentro:
        // tiene que estar de cara a quien habla, igual que lo deja `start_scene`.
        if (player.entity != ENTITY_NONE && world.has(player.entity, Component::Transform)) {
            const SceneDef def = get_scene_def(scene_id);
            world.transforms[player.entity].facing = def.player_facing;
            const AnimID idle = facing_anim(AnimID::IdleDown, def.player_facing);
            if (world.has(player.entity, Component::Animation))
                play_anim(world.animations[player.entity], idle, true);
            if (world.has(player.entity, Component::Sprite)) {
                auto& sp = world.sprites[player.entity];
                sp.src_rect.x = 0.0f;
                sp.src_rect.y = static_cast<f32>(get_anim_def(idle).row * SHEET_FRAME_SIZE);
                sp.flip_x = false;
            }
        }
        SDL_Log("paseo de escena terminado");
        scene_next();
    }
}

// =============================================================================
// Room state: clear detection, boss defeat, tile interactions
// =============================================================================
void Engine::reset_boss_bar() {
    boss_group_max = 0.0f;
    boss_bar_hp = 0.0f;
    boss_bar_max = 0.0f;
    boss_bar_heads = 0;
    boss_bar_invulnerable = false;
    boss_bar_name = nullptr;
    boss_group_alive_last = 0;
}

void Engine::update_boss_bar() {
    f32 hp = 0.0f, head_max = 0.0f;
    int heads = 0;
    bool head_alive = false, invul = false;
    for (u32 e = 0; e < world.entity_count; e++) {
        if (!world.is_alive(e) || !world.has(e, Component::EnemyAI) || !world.has(e, Component::Health)) continue;
        const auto& ai = world.enemy_ais[e];
        if (!ai.is_boss && !ai.boss_group) continue;
        const auto& h = world.healths[e];
        if (!h.is_alive()) continue;
        hp += h.current;
        heads++;
        if (ai.is_boss) { head_alive = true; head_max = h.max; invul = h.invulnerable; }
    }
    boss_bar_hp = hp;
    boss_bar_heads = heads;
    boss_bar_invulnerable = invul;
    boss_bar_max = (boss_group_max > 0.0f) ? boss_group_max : head_max;
    // Log para los guiones: una linea por baja del grupo (carmelo_check)
    if (boss_group_max > 0.0f) {
        if (heads < boss_group_alive_last)
            SDL_Log("Boss group: %d alive (head %s)", heads, head_alive ? "alive" : "down");
        boss_group_alive_last = heads;
    }
}

void Engine::update_room_state() {
    Room& room = current_room_data;
    update_boss_bar();

    if (room.type == RoomType::Zone) {
        // UNA limpieza por frame: `killall` puede vaciar varias celdas a la vez y
        // open_item_choice no puede abrirse dos veces en un frame.
        for (int i = 0; i < room.cell_count && i < floor_map.room_count; i++) {
            CellState& cs = room.cells[i];
            const auto& node = floor_map.rooms[i];
            if (!is_combat_room(node.type) || cs.cleared || !cs.active) continue;
            if (world.count_living_enemies(i) > 0) continue;
            cs.cleared = true;
            cs.active = false;
            player.rooms_cleared++;
            tally.cells++;
            // Pan de Cada Dia: vencer a un grupo da de comer
            if (player.clear_heal > 0.0f && player.entity != ENTITY_NONE &&
                world.has(player.entity, Component::Health)) {
                auto& ph = world.healths[player.entity];
                const f32 before = ph.current;
                ph.heal(player.clear_heal);
                if (ph.current - before >= 1.0f && world.popups.size() < 24)
                    world.popups.push_back({world.transforms[player.entity].position, ph.current - before,
                                            0.0f, false, ECSWorld::DamagePopup::Heal});
            }

            if (node.type == RoomType::Boss) {
                room.arena_sealed = false;
                room.place_exit();
                boss_entity = ENTITY_NONE;
                reset_boss_bar();
                camera.set_room({0, 0, room.world_w(), room.world_h()}, false);
                show_banner(tr("JEFE DERROTADO", "BOSS DEFEATED"), tr("Cruza el arco dorado para continuar", "Cross the golden arch to go on"), 4.0f);
                audio.play_sfx(SfxID::LevelUp);
            } else if (!player.took_damage_this_room) {
                // Ya no hay salas (zona continua): lo que se vence es un GRUPO de enemigos,
                // y un "Sala despejada" en mitad del campo abierto no significaba nada. Solo
                // se avisa cuando hay premio, y el aviso dice el premio.
                faith.on_room_no_damage();
                show_message(tr("Sin un rasguño  +2 Fe", "Not a scratch  +2 Faith"), 2.0f);
                audio.play_sfx(SfxID::FaithGain, 0.6f);
                award(AchievementID::SinRasguno);
            }
            // Despejarla con el celo apagado tiene su propio merito
            if (faith.current < Config::FAITH_LEVEL_1) award(AchievementID::BajoLaRetama);
            if (node.type == RoomType::Elite) {
                // Elite reward: a small offering pile at the cell hub
                Vec2 c = room.tile_to_world(cs.hub_tx, cs.hub_ty);
                spawn_pickup(world, c + Vec2{-12, 0}, ItemComponent::Pickup::Heart, 15.0f);
                spawn_pickup(world, c + Vec2{12, 0}, ItemComponent::Pickup::FaithOrb, 8.0f);
                spawn_pickup(world, c + Vec2{0, 12}, ItemComponent::Pickup::Heart, 15.0f);
                // ...y un consumible para la alforja: la elite es donde se va a buscarlos
                spawn_consumable_at(c + Vec2{0, -12}, randf());
            }
            SDL_Log("Cell %d cleared (%s)", i, room_type_name(node.type));

            // Auto-save on every cleared cell (GDD 4.4: mobile expects it, and it makes
            // "Continuar" land on the cell the player actually reached)
            populate_save_data();
            save.save();
            refresh_continue_state();

            // Core loop step 4: choose an upgrade. Elite/Boss always, normal cells 1 in 5 —
            // one card per cell would hand out all items well before the first chapter ends.
            if (node.type == RoomType::Elite)      open_item_choice(tr("ELITE DERROTADA", "ELITE DEFEATED"));
            else if (node.type == RoomType::Boss)  open_item_choice(tr("BOTÍN DEL JEFE", "BOSS LOOT"));
            else if (randi_range(0, 4) == 0)       open_item_choice(tr("ELIGE UNA BENDICIÓN", "CHOOSE A BLESSING"));
            break;
        }
    }

    // Tile interactions at the player's feet
    if (player.entity == ENTITY_NONE || !world.has(player.entity, Component::Transform)) return;
    Vec2 feet = world.transforms[player.entity].position + Vec2{0, 6};
    // floor(), no division entera: en (-16, 0) la division trunca a 0 y el pie contaria
    // como columna/fila 0 (misma convencion que Room::is_walkable_world)
    int tx = static_cast<int>(std::floor(feet.x / Config::TILE_SIZE));
    int ty = static_cast<int>(std::floor(feet.y / Config::TILE_SIZE));
    if (tx < 0 || tx >= room.tiles_w || ty < 0 || ty >= room.tiles_h) return;
    const int ci = room.cell_of_tile(tx, ty);
    if (ci < 0) return;
    CellState& cs = room.cells[ci];

    // Chapter exit: stepping anywhere on the arch patch (3x3 around the Exit tile)
    if (room.exit_open) {
        Vec2 exit_pos = room.tile_to_world(room.exit_tx, room.exit_ty);
        Vec2 center = world.transforms[player.entity].position;
        if (std::abs(center.x - exit_pos.x) < 20.0f && std::abs(feet.y - exit_pos.y) < 20.0f) {
            // En el tutorial el arco es SU salida, nunca la del capitulo: sin esta guarda,
            // caminar sobre el en cualquiera de los ultimos pasos lanzaba el epilogo narrado
            // del capitulo 1 y advance_chapter() con el tutorial todavia puesto.
            if (tutorial.active) {
                if (tutorial.current().goal == TutorialGoal::CrossExit) tutorial.exit_reached = true;
                return;
            }
            // Cierre narrado del capitulo antes de pasar al siguiente
            story.begin();
            story_chapter_outro(story, state.current_chapter);
            play_story(StoryThen::AdvanceChapter);
            return;
        }
    }

    TileType t = room.tiles[ty][tx];
    switch (t) {
        case TileType::AltarGod:
            if (!cs.altar_used) {
                cs.altar_used = true;
                faith.on_pray();
                if (world.has(player.entity, Component::Health)) world.healths[player.entity].heal(10.0f);
                // Praying is where new miracles are learned (GDD: miracles come from God)
                open_miracle_choice(tr("UN NUEVO MILAGRO", "A NEW MIRACLE"));
            }
            break;
        case TileType::Chest:
            if (!cs.chest_opened) {
                cs.chest_opened = true;
                room.tiles[ty][tx] = TileType::Path;
                if (world.has(player.entity, Component::Health)) world.healths[player.entity].heal(15.0f);
                faith.modify(5.0f);
                // El cofre trae tambien algo para la alforja, junto al cofre
                if (!tutorial.active) spawn_consumable_at(room.tile_to_world(tx, ty) + Vec2{0, 14}, randf());
                // En el tutorial la carta se explica MIENTRAS esta abierta: el cartel de
                // despues llegaba con la carta ya cerrada y nada que hacer.
                open_item_choice(tutorial.active ? tutorial_choice_title()
                                                 : tr("EL COFRE SE ABRE", "THE CHEST OPENS"));
            }
            break;
        case TileType::Event:
            if (!cs.event_used && cs.event_id >= 0) open_dialogue(cs.event_id);
            break;
        case TileType::Doorway:
            // Coyote time: al salir de un interior se aparece SOBRE la puerta de fuera, asi
            // que sin la gracia se volveria a entrar en el acto y en bucle.
            if (room_change_grace > 0.0f) break;
            // En el tutorial la puerta lleva a la sala SIGUIENTE de las suyas: `use_doorway`
            // consultaria un `floor_map` que aqui no describe este mapa.
            if (tutorial.active) {
                if (tutorial.waiting_door) {
                    audio.play_sfx(SfxID::DoorOpen, 0.9f);
                    tutorial_enter_room(tutorial.current().room);
                    tutorial_setup_step();
                }
                break;
            }
            use_doorway();
            break;
        default:
            break;
    }
}

// =============================================================================
// Puertas: entrar a un interior y volver de el (GDD2.0 fase 3)
// =============================================================================
// El estado de la sala de fuera NO se pierde, y eso es todo el gate del track: la sala se
// guarda en `room_cache` antes de irse, igual que en una transicion normal, asi que los
// enemigos muertos siguen muertos y el cofre abierto sigue abierto al volver.
void Engine::use_doorway() {
    const auto& node = floor_map.rooms[floor_map.current_room];
    const int target = (node.type == RoomType::Interior) ? node.interior_of : node.interior_room;
    if (target < 0 || target >= floor_map.room_count) return;

    if (node.type == RoomType::Interior) {
        floor_map.room_cache[1] = current_room_data;   // el interior conserva su estado
        // Volver: a la puerta de la zona y un tile MAS ABAJO, para no caer encima del
        // propio umbral (coordenadas de zona: la puerta se coso con su celda).
        const Room& out = floor_map.room_cache[0];
        spawn_tx = out.has_doorway ? out.door_tx : -1;
        spawn_ty = out.has_doorway ? std::min(out.tiles_h - 2, out.door_ty + 1) : -1;
    } else {
        // Entrar: delante de la puerta del interior, que esta siempre en el muro sur.
        spawn_tx = Config::SCREEN_TILES_X / 2;
        spawn_ty = Config::SCREEN_TILES_Y - 2;
    }
    audio.play_sfx(SfxID::DoorOpen, 0.9f);
    load_room(target);
}

// =============================================================================
// Encuentros biblicos (game/events.h): sala Event, dialogo y decision
// =============================================================================
void Engine::open_dialogue(int event_id) {
    EventDef def = get_dialogue_def(event_id);
    if (!def.speaker || !def.speaker[0]) return;
    dialogue = DialogueState{};
    dialogue.event_id = event_id;
    dialogue.chapter = event_chapter(event_id);
    // Se marca al ABRIRLO, no al resolverlo: el relato ya lo has leido, y quien cierre el
    // juego a mitad de la caja no deberia perderlo del Codice. Las cajas de escena
    // (game/scenes.h) no son relatos: quedan fuera de la mascara.
    if (event_id < Ev::COUNT) codex_see_event(save.data, event_id);
    load_event_plate(event_id);
    dialogue.auto_timer = autoplay.active ? 1.0f : 0.0f;
    audio.play_sfx(SfxID::Notification, 0.7f);
    change_scene(GameScene::Dialogue);
}

// La estampa del relato (tools/estampas, una por id de `Ev::`). Las cajas de escena
// (Sc::LINE_BASE en adelante) no llevan: son dialogos, no relatos. Sin el PNG la caja se
// pinta como siempre (draw_dialogue mira `plate`).
void Engine::load_event_plate(int event_id) {
    const int want = (event_id >= 0 && event_id < Ev::COUNT) ? event_id : -1;
    if (want == plate_event) return;
    char path[64];
    if (plate_event >= 0) {
        std::snprintf(path, sizeof(path), "sprites/estampas/ev_%02d.png", plate_event);
        renderer.unload_texture(path);
    }
    plate_texture = TEXTURE_NONE;
    plate_event = want;
    if (want < 0) return;
    std::snprintf(path, sizeof(path), "sprites/estampas/ev_%02d.png", want);
    plate_texture = renderer.load_texture(path, /*smooth=*/true);
    if (plate_texture == TEXTURE_NONE) SDL_Log("Estampa ausente: %s", path);
    else                               SDL_Log("Estampa del relato %d: %s", want, path);
}

void Engine::update_dialogue(f32 dt) {
    dialogue.t += dt;
    EventDef def = get_dialogue_def(dialogue.event_id);
    int opt_count = def.options[1].label ? 2 : 1;

    bool confirm = input.is_action_just_pressed(Action::Confirm) ||
                   input.is_action_just_pressed(Action::Attack);
    if (dialogue.auto_timer > 0.0f) {
        dialogue.auto_timer -= dt;
        if (dialogue.auto_timer <= 0.0f) { confirm = true; dialogue.auto_timer = 0.8f; }
    }

    // Una escena guionizada se salta ENTERA: con ESC (que dispara Cancel y Pause a la vez)
    // o con el boton tactil de SALTAR, que es la unica salida en un telefono por gestos.
    // Se atiende ANTES que todo lo demas para que valga tambien durante el paseo.
    if (scene_step >= 0) {
        Vec2 tp;
        const bool tapped_skip = input.touch_ui && input.peek_tap(tp) &&
                                 skip_button_rect().contains(tp);
        if (tapped_skip) input.take_tap(tp);
        if (tapped_skip || input.is_action_just_pressed(Action::Cancel) ||
            input.is_action_just_pressed(Action::Pause)) {
            scene_fade = 1.0f;   // que se VEA que el salto ha entrado
            finish_scene(true);
            return;
        }
    }

    // El paseo de la apertura: el motor lleva al profeta y no hay caja que atender. Se
    // salta TRAMO A TRAMO, como el resto: saltarlo cierra la escena, no la cadena entera
    // de la apertura (detras sigue viniendo la intro del capitulo).
    if (scene_walk_t > 0.0f) { update_scene_walk(dt); return; }

    if (dialogue.choosing && !dialogue.resolved) {
        if (input.is_action_just_pressed(Action::MoveUp))
            dialogue.selected = (dialogue.selected - 1 + opt_count) % opt_count;
        if (input.is_action_just_pressed(Action::MoveDown))
            dialogue.selected = (dialogue.selected + 1) % opt_count;
    }

    // Toque. Con decision en pantalla hacen falta DOS toques (el primero elige, el segundo
    // decide): una decision del GDD no puede perderse por un dedo mal puesto. Sin decision,
    // tocar en cualquier sitio pasa de linea.
    Vec2 tp;
    if (input.take_tap(tp)) {
        if (dialogue.choosing && !dialogue.resolved) {
            for (int i = 0; i < opt_count && i < 2; i++) {
                Rect r = dialogue_option_rect(i);
                if (r.w <= 0.0f || !r.contains(tp)) continue;
                if (i == dialogue.selected) confirm = true;
                else { dialogue.selected = i; audio.play_sfx(SfxID::MenuSelect, 0.6f); }
                break;
            }
        } else {
            confirm = true;
        }
    }

    if (!confirm) return;

    if (!dialogue.choosing) {
        // Avanzar por las lineas del personaje hasta agotarlas
        dialogue.line++;
        if (dialogue.line < 3 && def.lines[dialogue.line]) return;
        // Agotadas: a decidir, o —sin decision (options[0].label nulo)— directo al cierre
        if (def.options[0].label) {
            dialogue.choosing = true;
            dialogue.selected = 0;
            return;
        }
    } else if (!dialogue.resolved) {
        apply_event_choice(dialogue.event_id, dialogue.selected);
        dialogue.result = def.options[dialogue.selected].result;
        dialogue.resolved = true;
        return;
    }
    // Las cadenas encadenan: al cerrar una parada se abre la siguiente. No tocan la sala
    // porque no estan pasando en ninguna (las escenas, porque la suya es un escenario que
    // finish_scene desmonta; el dialogo del jefe, porque no cambia nada de la arena).
    if (scene_step >= 0) { scene_next(); return; }
    if (ascent_step >= 0) { ascent_next(); return; }
    if (epilogue_step >= 0) { epilogue_next(); return; }

    // El encuentro no se repite: la hoguera se apaga y la CELDA que lo cuenta queda marcada
    // (la del relato, no la que pisa el profeta: el comando `event` abre cualquiera)
    for (int i = 0; i < current_room_data.cell_count; i++)
        if (current_room_data.cells[i].event_id >= 0 && current_room_data.cells[i].event_id == dialogue.event_id)
            current_room_data.cells[i].event_used = true;
    if (dialogue.event_id >= 0 && dialogue.event_id < 64) run_events_done |= 1ull << dialogue.event_id;
    Vec2 feet = (player.entity != ENTITY_NONE && world.has(player.entity, Component::Transform))
                ? world.transforms[player.entity].position + Vec2{0, 6} : Vec2{0, 0};
    int tx = static_cast<int>(std::floor(feet.x / Config::TILE_SIZE));
    int ty = static_cast<int>(std::floor(feet.y / Config::TILE_SIZE));
    if (tx >= 0 && tx < current_room_data.tiles_w && ty >= 0 && ty < current_room_data.tiles_h &&
        current_room_data.tiles[ty][tx] == TileType::Event)
        current_room_data.tiles[ty][tx] = TileType::Path;
    change_scene(GameScene::Playing);
}

// Las etiquetas de consecuencia de una opcion ("+18 Fe", "-12 HP", "Vida llena"...). Salen
// de la MISMA tabla que aplica apply_event_choice (get_event_effect, game/events.h), asi que
// lo que se lee antes de decidir es exactamente lo que pasa al decidir. Orden: la fe primero
// (es el recurso del juego y la pregunta de cada relato), luego el cuerpo, luego lo demas.
int Engine::build_event_chips(int event_id, int option, EffectChip* out, int max) const {
    const EventEffect fx = get_event_effect(event_id, option);
    const Color GOLD = {250, 214, 110, 255}, BAD = {238, 118, 98, 255};
    const Color GOOD = {146, 226, 132, 255}, ITEM = {150, 200, 250, 255};
    int n = 0;
    auto add = [&](Color c, const char* fmt, auto... args) {
        if (n >= max) return;
        snprintf(out[n].text, sizeof(out[n].text), fmt, args...);
        out[n].color = c;
        n++;
    };
    if (fx.faith > 0.0f) add(GOLD, tr("+%.0f Fe", "+%.0f Faith"), fx.faith);
    if (fx.faith < 0.0f) add(BAD,  tr("%.0f Fe", "%.0f Faith"), fx.faith);
    if (fx.hurt > 0.0f)  add(BAD,  "-%.0f HP", fx.hurt);
    if (fx.heal)         add(GOOD, "%s", tr("Vida llena", "Full health"));
    if (fx.max_hp > 0.0f) add(GOOD, tr("+%.0f vida máx.", "+%.0f max HP"), fx.max_hp);
    if (fx.max_hp < 0.0f) add(BAD,  tr("%.0f vida máx.", "%.0f max HP"), fx.max_hp);
    if (fx.damage > 0.0f) add(GOOD, tr("+%.0f daño", "+%.0f damage"), fx.damage);
    if (fx.damage < 0.0f) add(BAD,  tr("%.0f daño", "%.0f damage"), fx.damage);
    if (fx.boss_hp < 1.0f)
        add(GOOD, tr("Jefe -%.0f%% vida", "Boss -%.0f%% HP"), (1.0f - fx.boss_hp) * 100.0f);
    if (fx.item >= 0) {
        const ItemDef* d = item_registry.get(static_cast<u32>(fx.item));
        if (d) add(ITEM, "%s", d->name);
    }
    switch (fx.special) {
        case EventSpecial::SecondSlot:     add(GOOD, "%s", tr("2º milagro", "2nd miracle slot")); break;
        case EventSpecial::LoseConsumable: add(BAD,  "%s", tr("Pierdes el consumible", "Lose your consumable")); break;
        case EventSpecial::Revive:         add(GOOD, "%s", tr("Una muerte perdonada", "One death forgiven")); break;
        case EventSpecial::HeavenlyHost:   add(GOOD, "%s", tr("Ejército Celestial", "Heavenly Host")); break;
        case EventSpecial::AscentFollow:   add(GOOD, "%s", tr("Herencia entera", "Full inheritance")); break;
        case EventSpecial::AscentStay:     add(BAD,  "%s", tr("-10% de herencia", "-10% inheritance")); break;
        case EventSpecial::AscentDouble:   add(GOOD, "%s", tr("Milagro heredado", "Inherited miracle")); break;
        case EventSpecial::AscentLess:     add(BAD,  "%s", tr("Sin milagro heredado", "No inherited miracle")); break;
        default: break;   // el arco de Joas cambia el final, no la partida: no se etiqueta
    }
    return n;
}

// El texto vive en events.h, y desde el 2026-09-24 tambien las CIFRAS (get_event_effect):
// aqui solo se aplican, con los sistemas que ya existen.
void Engine::apply_event_choice(int event_id, int option) {
    const EventEffect fx = get_event_effect(event_id, option);
    HealthComponent* hp = (player.entity != ENTITY_NONE && world.has(player.entity, Component::Health))
                 ? &world.healths[player.entity] : nullptr;

    // El cuerpo, en este orden: el golpe, la cura, el techo (una cura seguida de +vida
    // maxima deja la vida llena con el techo nuevo)
    if (hp && fx.hurt > 0.0f) hp->current = std::max(1.0f, hp->current - fx.hurt);   // un encuentro nunca mata
    if (hp && fx.heal) hp->current = hp->max;
    if (hp && fx.max_hp != 0.0f) {
        hp->max = std::max(20.0f, hp->max + fx.max_hp);
        hp->current = std::min(hp->current, hp->max);
        if (fx.max_hp > 0.0f) hp->current += fx.max_hp;
    }
    // Con suelo: hay encuentros (los sirios de Dotan, el rey de Moab) cuyo coste es RESTAR
    // dano, y un cayado que no hace nada convertiria la eleccion piadosa en una partida rota
    if (fx.damage != 0.0f && player.entity != ENTITY_NONE && world.has(player.entity, Component::Combat)) {
        auto& c = world.combats[player.entity];
        c.damage = std::max(4.0f, c.damage + fx.damage);
    }
    if (fx.faith != 0.0f) faith.modify(fx.faith);
    // Un DECIDIR es actuar: la fe recien ganada no empieza a gotear por inactividad (-0,5/s a
    // los 10 s sin atacar) nada mas cerrar la caja
    faith.reset_inactivity();
    // min, no asignacion: Abdias (0,75) no debe deshacer el altar del Carmelo (0,5)
    if (fx.boss_hp < 1.0f) boss_hp_scale = std::min(boss_hp_scale, fx.boss_hp);
    if (fx.item >= 0) {
        if (const ItemDef* d = item_registry.get(static_cast<u32>(fx.item))) {
            player.grant_item(world, faith, *d);
            codex_see_item(save.data, static_cast<u32>(fx.item));
            show_message(d->name, 2.5f);
        }
    }
    switch (fx.special) {
        case EventSpecial::SecondSlot:
            player.active_slot_count = std::max(player.active_slot_count, 2);
            break;
        case EventSpecial::LoseConsumable:
            // Sale tambien de la fila de objetos, como cuando se gasta la ultima carga
            // Con la alforja: se pierde el consumible ELEGIDO, entero
            player.belt_fix_selection();
            player.belt[player.belt_sel] = Player::BeltSlot{};
            player.belt_fix_selection();
            break;
        case EventSpecial::Revive:
            has_revive = true;
            break;
        case EventSpecial::HeavenlyHost:
            player.miracle_slots[player.active_slot_count - 1].equip(MiracleID::EjercitoCelestial);
            break;
        // El Ascenso: no se aplica, se APUNTA. Lo que cambia es la HERENCIA, y se reparte al
        // final del viaje en `finish_ascension`
        case EventSpecial::AscentFollow: ascent_follows++;      break;
        case EventSpecial::AscentDouble: ascent_double = true;  break;
        case EventSpecial::AscentLess:   ascent_double = false; break;
        case EventSpecial::JoashFull:    joash_full = true;     break;
        case EventSpecial::JoashThrice:  joash_full = false;    break;
        default: break;
    }

    // El log dice lo que cambio con las mismas palabras que la caja (gate de encuentros)
    EffectChip chips[8];
    const int nc = build_event_chips(event_id, option, chips, 8);
    std::string summary;
    for (int i = 0; i < nc; i++) { summary += i ? " | " : ""; summary += chips[i].text; }
    SDL_Log("Event choice %d opcion %d: %s", event_id, option,
            nc ? summary.c_str() : "(sin efecto en la partida)");

    audio.play_sfx(fx.faith > 0.0f ? SfxID::FaithGain
                 : fx.faith < 0.0f ? SfxID::FaithLoss : SfxID::MenuConfirm, 0.9f);
    populate_save_data();
    save.save();
}

// =============================================================================
// "Elige una bendicion": core loop step 4 (GDD 1.4)
// =============================================================================
void Engine::open_item_choice(const char* title) {
    std::vector<u32> ids = item_registry.roll_choices(player.items, 3, save.data.items_unlocked);
    // Se loguea LO QUE SE OFRECE, no solo lo que se toma: es lo unico que permite
    // comprobar desde un guion que un objeto bloqueado no ha entrado en el sorteo.
    if (ids.empty()) { SDL_Log("Item choice: (sin candidatos)"); return; }
    {
        std::string offered;
        for (u32 id : ids) {
            const ItemDef* d = item_registry.get(id);
            offered += offered.empty() ? "" : " / ";
            offered += d ? d->name : "?";
        }
        // El TITULO va en el log porque es lo unico que distingue de donde sale la carta, y
        // en el tutorial es contenido: la explicacion de "eliges una de tres" se da ahi,
        // con la carta abierta. Fotografiarla no vale de gate: bajo autoplay se toma sola
        // a los 0,7 s y el `shot` de un guion llega tarde.
        SDL_Log("Item choice [%s]: %s", title ? title : "-", offered.c_str());
    }
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

// Tecla de cada slot de milagro: la usan la carta de eleccion Y el aviso de equipado, para
// que no puedan volver a divergir (el aviso mandaba al slot 2 a la tecla [E]).
static const char* SLOT_KEYS[Config::MAX_MIRACLE_SLOTS] = {"Q", "E", "F"};

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
            ? std::string(tr("SLOT ", "SLOT ")) + SLOT_KEYS[std::max(0, std::min(Config::MAX_MIRACLE_SLOTS - 1, target))] + tr(" LIBRE", " FREE")
            : std::string(tr("SUSTITUYE A ", "REPLACES ")) + get_miracle_def(slot.miracle).name;
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

    // Echar Suertes: tres cartas nuevas (sin repetir las que habia, si hay de donde sacar)
    bool reroll = !choice.miracles && rerolls_left > 0 && input.is_action_just_pressed(Action::Consumable);
    Vec2 tp;
    const bool tapped = input.take_tap(tp);
    if (tapped && !choice.miracles && rerolls_left > 0 && choice_reroll_rect.w > 0.0f &&
        choice_reroll_rect.contains(tp)) reroll = true;
    if (reroll) {
        std::vector<u32> owned = player.items;
        for (int i = 0; i < choice.count; i++) owned.push_back(choice.ids[i]);
        std::vector<u32> ids = item_registry.roll_choices(owned, 3, save.data.items_unlocked);
        if (ids.empty()) ids = item_registry.roll_choices(player.items, 3, save.data.items_unlocked);
        if (!ids.empty()) {
            rerolls_left--;
            choice.count = static_cast<int>(ids.size());
            for (int i = 0; i < choice.count; i++) choice.ids[i] = ids[i];
            choice.selected = 0;
            std::string offered;
            for (u32 id : ids) {
                const ItemDef* d = item_registry.get(id);
                offered += offered.empty() ? "" : " / ";
                offered += d ? d->name : "?";
            }
            SDL_Log("Echar suertes (quedan %d): %s", rerolls_left, offered.c_str());
            audio.play_sfx(SfxID::Notification, 0.9f);
            renderer.shake_camera(Config::Feel::SHAKE_HIT);
            if (autoplay.active) choice.auto_timer = 0.7f;
        }
        return;
    }

    // Toque: dos toques. El primero elige la carta, el segundo la toma. Una carta es una
    // decision de partida entera; tomarla con un dedo resbalado no puede pasar.
    if (tapped) {
        for (int i = 0; i < choice.count; i++) {
            if (!choice_card_rect(i, choice.count).contains(tp)) continue;
            if (i == choice.selected) take = true;
            else { choice.selected = i; audio.play_sfx(SfxID::MenuSelect, 0.6f); }
            break;
        }
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
            codex_see_miracle(save.data, id);
            MiracleDef d = get_miracle_def(id);
            SDL_Log("Miracle equipped in slot %d: %s (tier %d, %.0f faith)", target, d.name, d.tier, d.faith_cost);
            snprintf(buf, sizeof(buf), tr("%s equipado en [%s]", "%s equipped on [%s]"), d.name,
                     SLOT_KEYS[std::max(0, std::min(Config::MAX_MIRACLE_SLOTS - 1, target))]);
            show_message(buf, 3.0f);
            audio.play_sfx(SfxID::MenuConfirm, 0.9f);
        } else {
            const ItemDef* def = item_registry.get(choice.ids[choice.selected]);
            if (def) {
                player.grant_item(world, faith, *def);
                codex_see_item(save.data, def->id);
                SDL_Log("Item taken: %s (%s)", def->name, def->description);
                // Las armas traen tecla: sin decirlo, el jugador recoge la Honda y no la usa jamas
                if (def->category == ItemCategory::Weapon)
                    snprintf(buf, sizeof(buf), "%s", input.touch_ui
                             ? tr("Arma nueva: botón ARMA para usarla", "New weapon: WEAPON button to use it")
                             : tr("Arma nueva: [I] o clic derecho para usarla", "New weapon: [I] or right click to use it"));
                else if (def->category == ItemCategory::Relic &&
                         player.relic_replaced != UINT32_MAX &&
                         player.relic_replaced != def->id) {
                    // Solo hay un slot: hay que DECIR que la otra se ha ido, o el jugador
                    // ve caer un numero suyo y no sabe por que.
                    const ItemDef* old = item_registry.get(player.relic_replaced);
                    snprintf(buf, sizeof(buf), tr("%s sustituye a %s", "%s replaces %s"),
                             def->name, old ? old->name : "?");
                } else
                    buf[0] = '\0';
                // La ficha de "objeto obtenido": icono, nombre, que hace y como se usa. El
                // aviso de abajo queda para lo que hay que DECIR (tecla, reliquia que sale).
                announce_item(*def, buf[0] ? buf : nullptr);
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
// SaveData -> el espejo del menu. El menu no conoce el guardado ni Input: solo pinta.
void Engine::load_settings_mirror() {
    menus.settings_music_vol   = save.data.music_volume;
    menus.settings_sfx_vol     = save.data.sfx_volume;
    menus.settings_aim_mode    = save.data.aim_assist;
    menus.settings_sensitivity = save.data.sensitivity;
    menus.settings_rumble      = (save.data.settings_flags & SETTING_RUMBLE) != 0;
    menus.settings_rstick      = (save.data.settings_flags & SETTING_TOUCH_RSTICK) != 0;
    menus.settings_language    = save.data.language;
    menus.settings_skin        = static_cast<int>(save.data.skin_equipped);
    menus.settings_skins_mask  = save.data.skins_unlocked;
    // La fila del stick derecho SOLO existe en tactil: en teclado seria ruido, y con
    // --touch tiene que estar para poder probarla.
    menus.settings_show_rstick = input.touch_ui;
}

// El espejo del menu -> SaveData -> los subsistemas. Guarda con save(false): tocar un
// ajuste NO re-sella la fecha, asi que no le reinicia a nadie la caducidad de 48 h.
void Engine::commit_settings_from_mirror() {
    save.data.music_volume = menus.settings_music_vol;
    save.data.sfx_volume   = menus.settings_sfx_vol;
    save.data.aim_assist   = menus.settings_aim_mode;
    save.data.sensitivity  = menus.settings_sensitivity;
    save.data.language     = menus.settings_language;
    save.data.skin_equipped = static_cast<u32>(std::max(0, menus.settings_skin));
    if (menus.settings_rumble) save.data.settings_flags |=  SETTING_RUMBLE;
    else                       save.data.settings_flags &= ~SETTING_RUMBLE;
    if (menus.settings_rstick) save.data.settings_flags |=  SETTING_TOUCH_RSTICK;
    else                       save.data.settings_flags &= ~SETTING_TOUCH_RSTICK;
    apply_settings_from_save();
    save.save(false);
}

void Engine::update_settings() {
    menus.update(input);  // navegacion arriba/abajo

    const bool left    = input.is_action_just_pressed(Action::MoveLeft);
    const bool right   = input.is_action_just_pressed(Action::MoveRight);
    const bool confirm = input.is_action_just_pressed(Action::Confirm);
    bool changed = false;

    // Por ID de fila, no por indice: la fila del stick derecho solo existe en tactil.
    const int row = menus.settings_row_at(menus.selected_option);

    switch (row) {
        case MenuSystem::ROW_MUSIC:
        case MenuSystem::ROW_SFX: {
            f32* vol = (row == MenuSystem::ROW_MUSIC) ? &menus.settings_music_vol
                                                      : &menus.settings_sfx_vol;
            if (left)  { *vol = std::max(0.0f, *vol - 0.1f); changed = true; }
            if (right) { *vol = std::min(1.0f, *vol + 0.1f); changed = true; }
            break;
        }
        case MenuSystem::ROW_AIM:   // OFF -> Asistido -> ON, y el ciclo da la vuelta
            if (left)  { menus.settings_aim_mode = (menus.settings_aim_mode + 2) % 3; changed = true; }
            if (right || confirm) { menus.settings_aim_mode = (menus.settings_aim_mode + 1) % 3; changed = true; }
            break;
        case MenuSystem::ROW_SENS:
            if (left)  { menus.settings_sensitivity = std::max(1,  menus.settings_sensitivity - 1); changed = true; }
            if (right) { menus.settings_sensitivity = std::min(10, menus.settings_sensitivity + 1); changed = true; }
            break;
        case MenuSystem::ROW_RUMBLE:
            if (left || right || confirm) { menus.settings_rumble = !menus.settings_rumble; changed = true; }
            break;
        case MenuSystem::ROW_RSTICK:
            if (left || right || confirm) { menus.settings_rstick = !menus.settings_rstick; changed = true; }
            break;
        case MenuSystem::ROW_SKIN: {
            // Solo cicla entre las DESBLOQUEADAS: pasar por una que no tienes y verla
            // rebotar seria peor que no ensenarla.
            if (!left && !right && !confirm) break;
            int dir = left ? -1 : 1;
            int k = menus.settings_skin;
            for (int tries = 0; tries < static_cast<int>(STAFF_SKIN_COUNT); tries++) {
                k = (k + dir + static_cast<int>(STAFF_SKIN_COUNT)) % static_cast<int>(STAFF_SKIN_COUNT);
                if (menus.settings_skins_mask & (1u << k)) break;
            }
            menus.settings_skin = k;
            changed = true;
            break;
        }
        case MenuSystem::ROW_LANG:
            if (left || right || confirm) {
                int n = static_cast<int>(Language::COUNT);
                menus.settings_language = (menus.settings_language + (left ? n - 1 : 1)) % n;
                changed = true;
            }
            break;
        default:
            break;
    }

    if (changed) {
        commit_settings_from_mirror();
        audio.play_sfx(SfxID::MenuSelect, 0.7f);
        // Que se OIGA lo que se esta tocando: mover el volumen de efectos sin oir un
        // efecto es ajustar a ciegas. Y la vibracion se prueba al encenderla.
        if (row == MenuSystem::ROW_RUMBLE && menus.settings_rumble)
            input.rumble(Config::Feel::RUMBLE_HIT, 120);
    }

    // "Volver" o Cancelar -> vuelve a donde se entro
    if ((confirm && row == MenuSystem::ROW_BACK) ||
        input.is_action_just_pressed(Action::Cancel)) {
        commit_settings_from_mirror();
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
        else if (c.op == "goto")  { ss >> c.arg; }   // indice, o el TIPO de sala ("event", "boss"...)
        else if (c.op == "bot")   { ss >> c.num; }
        else if (c.op == "walk")  { ss >> c.num >> c.num2; }
        else if (c.op == "walks") { ss >> c.num >> c.num2; }   // walk relativo al origen de la celda de INICIO
        else if (c.op == "walkarena") { ss >> c.num; }          // walk a `d` unidades del vano (+ = dentro)
        else if (c.op == "intel") { }                          // loguea el rastro del jefe
        else if (c.op == "bosshp") { ss >> c.num >> c.num2; }  // fraccion de vida del jefe [0 todos, 1 cabeza, 2 sequito]
        else if (c.op == "seed")  { ss >> c.num; }
        else if (c.op == "chapter") { ss >> c.num; }
        else if (c.op == "event")   { ss >> c.num >> c.num2; }   // capitulo + indice en su pozo
        else if (c.op == "cine")  { ss >> c.num; }              // estampa del prologo (0..7)
        else if (c.op == "chintro") { ss >> c.num; }            // intro cinematica del capitulo n
        else if (c.op == "asccine") { ss >> c.num; }            // cinematica del Ascenso (escena n)
        else if (c.op == "audiencia") { }                      // = escena 0 (la audiencia), parada
        else if (c.op == "tutorial") { ss >> c.num; }          // lanza el tutorial (num = paso inicial)
        else if (c.op == "tutorialask") { }                    // abre la pregunta de Nueva Partida
        else if (c.op == "escena") { ss >> c.num >> c.num2; }  // Scene::* + 1 = avanza sola
        else if (c.op == "spawn") { ss >> c.num >> c.num2; }    // EnemyType + cuantos
        else if (c.op == "give")  { ss >> c.num; }              // id de objeto (items.h)
        else if (c.op == "learn") { ss >> c.num >> c.num2; }    // id de milagro + slot
        else if (c.op == "faith") { ss >> c.num; }              // fijar la fe actual
        else if (c.op == "fx")    { ss >> c.num >> c.num2 >> c.num3; }  // FxID + tamano + duracion
        else if (c.op == "agesave") { ss >> c.num; }            // envejecer el guardado N horas
        else if (c.op == "unlocks") { ss >> c.num; }            // fijar SaveData::unlocks
        else if (c.op == "savedump") { }                       // volcar el guardado al log
        else if (c.op == "savenow")  { }                       // guardar la partida AHORA (como al limpiar una celda)
        else if (c.op == "lang")    { ss >> c.num; }            // 0 = ES, 1 = EN
        else if (c.op == "langmenu") { }                       // abrir el selector de idioma
        else if (c.op == "defaults") { }                       // ajustes de fabrica
        else if (c.op == "aimmode") { ss >> c.num; }           // 0 OFF / 1 Asistido / 2 ON
        else if (c.op == "rstick")  { ss >> c.num; }           // 0/1: segundo joystick tactil
        else if (c.op == "meta")    { ss >> c.num >> c.num2; }  // comprar una mejora de El Santuario [rango]
        else if (c.op == "offerings") { ss >> c.num; }          // fijar el banco de ofrendas
        else if (c.op == "dropcons")  { ss >> c.num; }          // soltar un consumible delante
        else if (c.op == "metareset") { }                      // volver a bloquear TODA la meta
        else if (c.op == "pit")     { }                        // colocarse al borde de un hueco
        else if (c.op == "door")    { }                        // plantarse sobre la puerta de la sala
        else if (c.op == "collect") { }                        // atraer las recogidas de la sala
        // Toque sintetico: el raton NO genera eventos SDL_EVENT_FINGER_*, asi que sin
        // esto los controles tactiles solo se podian probar en un telefono.
        else if (c.op == "touch")   { ss >> c.num >> c.num2 >> c.num3; }   // x y [dedo]
        else if (c.op == "drag")    { ss >> c.num >> c.num2 >> c.num3; }   // x y [dedo]
        else if (c.op == "untouch") { ss >> c.num >> c.num2 >> c.num3; }   // x y [dedo]
        // Variantes en PIXELES DE VENTANA: la unica prueba de extremo a extremo de que el
        // dibujo y el hit-test caen en el mismo sitio (ver tactil_ancho_check.txt).
        else if (c.op == "touchpx")   { ss >> c.num >> c.num2 >> c.num3; } // px py [dedo]
        else if (c.op == "dragpx")    { ss >> c.num >> c.num2 >> c.num3; } // px py [dedo]
        else if (c.op == "untouchpx") { ss >> c.num >> c.num2 >> c.num3; } // px py [dedo]
        else if (c.op == "say")   { std::getline(ss, c.arg); }
        else if (c.op == "stats") { std::getline(ss, c.arg); }
        else if (c.op == "feel")  { std::getline(ss, c.arg); }   // etiqueta del tramo medido
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
    if (n == "m3")      return Action::Miracle3;
    if (n == "weapon")  return Action::AttackSecondary;
    if (n == "confirm") return Action::Confirm;
    if (n == "cancel")  return Action::Cancel;
    if (n == "pause")   return Action::Pause;
    if (n == "map")     return Action::Map;
    if (n == "use")     return Action::Consumable;
    if (n == "pray")    return Action::Pray;
    if (n == "belt1")   return Action::Belt1;
    if (n == "belt2")   return Action::Belt2;
    if (n == "belt3")   return Action::Belt3;
    if (n == "belt4")   return Action::Belt4;
    if (n == "beltnext") return Action::BeltNext;
    if (n == "beltprev") return Action::BeltPrev;
    return Action::COUNT;
}

void Engine::update_autoplay(f32 dt) {
    Autoplay& ap = autoplay;
    // Actions the script may drive: reset each tick, then re-assert what is held
    static const Action driven[] = {Action::MoveUp, Action::MoveDown, Action::MoveLeft, Action::MoveRight,
                                    Action::Attack, Action::Dash, Action::Miracle1, Action::Miracle2,
                                    Action::Miracle3,
                                    Action::Confirm, Action::Cancel, Action::Pause, Action::Map, Action::Consumable,
                                    Action::AttackSecondary, Action::Pray,
                                    Action::Belt1, Action::Belt2, Action::Belt3, Action::Belt4,
                                    Action::BeltNext, Action::BeltPrev};
    for (Action a : driven) input.actions[static_cast<u32>(a)] = false;

    if (ap.timer > 0.0f) {
        ap.timer -= dt;
        if (ap.holding) input.actions[ap.hold_action] = true;
    } else {
        // Lo que se estaba manteniendo sobrevive a un `shot`: `hold map 0.6` + `shot` es la
        // unica forma de fotografiar el mapa grande, y sin esto la tecla se soltaba justo
        // antes de la captura.
        const bool was_holding = ap.holding;
        const u32  was_action  = ap.hold_action;
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
            if (c.op == "shot") {
                pending_shot_name = c.arg;
                screenshot_requested = true;
                ap.timer = Config::FIXED_DT;
                if (was_holding) { ap.holding = true; ap.hold_action = was_action;
                                   input.actions[was_action] = true; }
                break;
            }
            if (c.op == "goto") {
                // Por INDICE o por TIPO. El tipo es lo que hace que un guion sobreviva a un
                // cambio del generador: el reparto de salas se mueve en cuanto cambia una
                // linea de FloorMap::generate, y un `goto 11` clavado deja de apuntar a la
                // sala que el guion queria comprobar sin decir ni pio.
                int target = -1;
                if (!c.arg.empty() && (isdigit(static_cast<unsigned char>(c.arg[0])) || c.arg[0] == '-')) {
                    target = std::atoi(c.arg.c_str());
                } else {
                    RoomType want = RoomType::Normal;
                    if      (c.arg == "event")    want = RoomType::Event;
                    else if (c.arg == "treasure") want = RoomType::Treasure;
                    else if (c.arg == "altar")    want = RoomType::Altar;
                    else if (c.arg == "elite")    want = RoomType::Elite;
                    else if (c.arg == "boss")     want = RoomType::Boss;
                    else if (c.arg == "start")    want = RoomType::Start;
                    else if (c.arg == "rest")     want = RoomType::Rest;
                    for (int i = 0; i < floor_map.room_count; i++)
                        if (floor_map.rooms[i].type == want) { target = i; break; }
                    if (target < 0) SDL_Log("Autoplay: goto %s -> no hay ninguna en este mapa", c.arg.c_str());
                }
                if (target >= 0) load_room(target);
                continue;
            }
            if (c.op == "bot")  { ap.bot_timer = c.num; ap.timer = c.num; break; }
            // Con el mundo fijo, `seed` es un override de COMPATIBILIDAD: fija la semilla de
            // todos los capitulos de la sesion (pisa Config::CHAPTER_SEEDS) y re-siembra el
            // generador. Un guion que quiera el mapa REAL del juego simplemente no lo usa.
            if (c.op == "seed") { forced_seed = static_cast<u32>(c.num); srand(static_cast<unsigned>(c.num)); continue; }
            if (c.op == "chapter") {   // jump to a chapter (debug: biome / boss checks)
                if (state.scene == GameScene::Playing && player.entity != ENTITY_NONE) {
                    int ch = std::max(1, std::min(static_cast<int>(c.num), Config::CHAPTERS_ACT_I + Config::CHAPTERS_ACT_II));
                    start_chapter(ch, ChapterAnnounce::Card);   // nunca la intro: los guiones no cambian
                }
                continue;
            }
            // Fuerza la intro cinematica de un capitulo y la deja PARADA (como `cine`),
            // para fotografiarla. Es el unico camino bajo autoplay: las entradas
            // naturales la suprimen para no comerse los comandos de los guiones.
            // Se sale con `press cancel` (ESC la salta y cae en Playing).
            if (c.op == "chintro") {
                if (state.scene == GameScene::Playing && player.entity != ENTITY_NONE) {
                    int ch = std::max(1, std::min(static_cast<int>(c.num), Config::CHAPTERS_ACT_I + Config::CHAPTERS_ACT_II));
                    if (!cinematic_chapter_ready(ch))
                        SDL_Log("chintro %d: sin estampa (se vera cielo y texto)", ch);
                    start_chapter(ch, ChapterAnnounce::None);
                    start_chapter_intro(ch);
                    cinematic.auto_timer = 0.0f;   // quieta hasta el siguiente comando
                }
                continue;
            }
            // `tutorial [paso]`: lo lanza desde donde sea y, con el numero, salta DIRECTO a
            // ese paso (1-based) con su sala ya montada. Sin el salto, fotografiar la sala 3
            // exigiria jugarse doce pasos, y la pregunta de Nueva Partida esta suprimida bajo
            // autoplay a proposito (los guiones de siempre esperan entrar a jugar). Vuelve al
            // TITULO al acabar, para que el guion siga mandando en vez de caer en una partida.
            // La pregunta de Nueva Partida esta suprimida bajo autoplay (30 guiones esperan
            // entrar a jugar con un `press confirm`), asi que este es el unico camino para
            // fotografiarla y para comprobar sus dos botones.
            if (c.op == "tutorialask") {
                if (state.scene == GameScene::MainMenu) {
                    menus.current = MenuScreen::TutorialAsk;
                    menus.selected_option = 0;
                }
                continue;
            }
            if (c.op == "tutorial") {
                start_tutorial(TutorialThen::Title);
                const int want = std::max(0, std::min(static_cast<int>(c.num) - 1, TUTORIAL_STEP_COUNT - 1));
                if (want > 0) {
                    tutorial.step = want;
                    tutorial_enter_room(tutorial.current().room);
                    tutorial_setup_step();
                    tutorial.prev_props = 0;   // se recalcula en el primer probe util
                    tutorial.prev_props = tutorial_count_props(current_room_data);
                }
                SDL_Log("Tutorial (autoplay): paso %d/%d, sala %d",
                        tutorial.step + 1, TUTORIAL_STEP_COUNT, tutorial.room + 1);
                continue;
            }
            // Fuerza una escena guionizada (game/scenes.h): `escena <id> [1]`, o `audiencia`
            // (= escena 0). Se deja PARADA como chintro —`press confirm` pasa de pagina y
            // `press cancel` la salta entera— salvo con el 1, que la hace avanzar sola. Una
            // apertura al cerrarse lanza la intro del capitulo aunque haya autoplay (avanza
            // sola), para que el guion compruebe el ORDEN real: escena -> intro -> Playing.
            // Un dialogo de jefe carga antes la arena con el jefe de pie, y al cerrarse
            // suelta la pelea.
            if (c.op == "audiencia" || c.op == "escena") {
                if (state.scene == GameScene::Playing && player.entity != ENTITY_NONE) {
                    const int id = (c.op == "audiencia") ? Scene::AUDIENCIA
                                 : std::max(0, std::min(static_cast<int>(c.num), Scene::COUNT - 1));
                    const SceneDef def = get_scene_def(id);
                    // El profeta del acto: una escena del Acto II con Elias de pie (o al
                    // reves) mentiria en la captura. Solo aqui, bajo el comando: en el
                    // juego real el Ascenso ya ha hecho el cambio.
                    const CharacterType want = (def.chapter > Config::CHAPTERS_ACT_I)
                                             ? CharacterType::Eliseo : CharacterType::Elias;
                    if (player.character != want) {
                        if (player.entity != ENTITY_NONE) world.destroy(player.entity);
                        world.gap_crosser = ENTITY_NONE;
                        player = Player{};
                        if (want == CharacterType::Elias) player.init_as_elias(world, character_textures[static_cast<u32>(CharacterType::Elias)]);
                        else                              player.init_as_eliseo(world, character_textures[static_cast<u32>(CharacterType::Eliseo)]);
                        apply_meta_upgrades();
                        SDL_Log("escena %d: el profeta pasa a ser %s", id, want == CharacterType::Elias ? "Elias" : "Eliseo");
                    }
                    start_chapter(def.chapter, ChapterAnnounce::None);
                    if (def.stage == StageKind::Live) {
                        load_room(floor_map.boss_room);      // al nudo de la celda B...
                        enter_arena();                       // ...y dentro del recinto, con el jefe de pie
                        pending_scene = -1;                  // se fuerza aqui mismo
                    }
                    scene_manual = (c.op == "audiencia") || c.num2 == 0;
                    start_scene(id);
                    // En el juego real quien ASIENTA la camara sobre la arena es el paseo de
                    // entrada (update_boss_approach), que bajo el comando no corre: la escena
                    // del jefe se fotografiaba desde el nudo de FUERA del recinto, sin jefe ni
                    // profeta en cuadro (visto el 2026-10-03 en escenas_check, es11..es20).
                    if (def.stage == StageKind::Live && player.entity != ENTITY_NONE &&
                        world.has(player.entity, Component::Transform)) {
                        const Vec2 pp = world.transforms[player.entity].position;
                        camera.room_bounds = camera.bounds_goal;
                        camera.set_target(pp);
                        camera.position = pp;
                        camera.update(0.0f);
                    }
                }
                continue;
            }
            // Fuerza la cinematica del Ascenso en la escena n (0..5) y la deja PARADA,
            // como chintro. SOLO mira: cine_then = Play, asi que salirse de ella no
            // reparte ninguna herencia — el camino real es `ascend` y las 4 paradas.
            if (c.op == "asccine") {
                if (state.scene == GameScene::Playing && player.entity != ENTITY_NONE) {
                    if (!cinematic_ascension_ready())
                        SDL_Log("asccine: sin estampas (se vera cielo y texto)");
                    // Con la rama que tenga la sesion (true de fabrica; la cambia ASC_JORDAN).
                    cinematic_ascension(cinematic, ascent_double);
                    cinematic.load_art(renderer);
                    cinematic.active = true;
                    cinematic.clock = 0.0f;
                    cinematic.auto_timer = 0.0f;   // quieta hasta el siguiente comando
                    cinematic.go_to(static_cast<int>(c.num), renderer);
                    cine_then = CineThen::Play;
                    change_scene(GameScene::Intro);
                    SDL_Log("Cinematica del Ascenso (asccine, escena %d)", cinematic.scene);
                }
                continue;
            }
            // Depuracion: abrir el encuentro de un capitulo sin caminar hasta su sala. La
            // sala Event la sortea el generador, asi que sin esto verificar los diez
            // encuentros depende de que el mapa los ponga a mano.
            //   event <capitulo> [indice en el pozo, 0 por defecto]
            if (c.op == "event") {
                if (state.scene == GameScene::Playing) {
                    int ch = std::max(1, std::min(static_cast<int>(c.num),
                                                  Config::CHAPTERS_ACT_I + Config::CHAPTERS_ACT_II));
                    EventPool pool = get_chapter_events(ch);
                    int idx = std::max(0, std::min(static_cast<int>(c.num2), pool.count - 1));
                    int id = pool.first + idx;
                    open_dialogue(id);
                    SDL_Log("event %d.%d (id %d de %d) -> \"%s\" (retrato %d, pnj %d)",
                            ch, idx, id, pool.count, get_event_def(id).speaker,
                            get_event_def(id).portrait, get_event_def(id).npc);
                }
                continue;
            }
            // Depuracion: disparar la Ascension sin tener que rematar al jefe del cap. 5
            // y cruzar el arco de salida. Es el unico camino al Manto de Elias.
            // Depuracion: retrasar el sello del guardado N horas para probar la caducidad
            // de 48 h sin esperar dos dias. No re-sella el fichero (save(false)).
            if (c.op == "agesave") {
                u64 back = static_cast<u64>(std::max(0.0f, c.num) * 3600.0f);
                if (save.data.save_timestamp > back) save.data.save_timestamp -= back;
                else                                 save.data.save_timestamp = 1;
                save.save(false);
                refresh_continue_state();
                SDL_Log("agesave: run aged %.0f h (expired=%d)", c.num, save.is_run_expired() ? 1 : 0);
                continue;
            }
            // Depuracion: cerrar el capitulo sin rematar al jefe ni cruzar el arco. En el
            // cap. 10 es lo que dispara la victoria (y con ella el epilogo, si toca).
            if (c.op == "advance") {
                if (state.scene == GameScene::Playing) advance_chapter();
                continue;
            }
            // Depuracion: volver a bloquear toda la meta-progresion comprable. Sin esto,
            // scripts/meta_check.txt solo servia la PRIMERA vez: las compras persisten
            // entre partidas (esa es justamente su gracia) y en la segunda vuelta ya
            // estaba todo desbloqueado. Misma razon que `defaults` para los ajustes.
            if (c.op == "metareset") {
                for (auto& u : progression.upgrades) { u.unlocked = false; u.rank = 0; u.cost = u.next_cost(); }
                save.data.items_unlocked = 0;
                save.data.upgrades_bought = 0;
                for (int i = 0; i < SAVE_MAX_UPGRADES; i++) save.data.upgrade_ranks[i] = 0;
                save.data.tips_seen = 0;
                save.data.skins_unlocked = 1;   // la 0 es la de siempre y no se compra
                save.data.skin_equipped  = 0;
                // Los LOGROS tambien son meta y tambien hay que poder rebobinarlos: sin
                // esto, scripts/logros_check.txt solo servia con el fichero de guardado
                // recien borrado a mano (y `delete_save` NO los borra, por diseno).
                achievements.unlocked = 0;
                save.data.achievements = 0;
                save.data.miracles_cast = 0;
                save.data.total_kills = 0;
                save.save(false);
                SDL_Log("meta reset: upgrades, item unlocks and achievements cleared");
                continue;
            }
            // Depuracion: comprar una mejora de El Santuario sin pasar por las ofrendas
            // `meta <id> [rango]`: sin rango, el maximo de esa mejora.
            if (c.op == "meta") {
                u32 id = static_cast<u32>(std::max(0.0f, c.num));
                for (auto& u : progression.upgrades) {
                    if (u.id != id) continue;
                    const int r = c.num2 > 0.5f ? static_cast<int>(c.num2) : u.max_rank;
                    u.rank = std::max(0, std::min(r, u.max_rank));
                    u.unlocked = u.rank > 0;
                    u.cost = u.next_cost();
                }
                sync_progression_to_save();
                save.save(false);
                // En plena partida, los dones que se gastan se rellenan YA (en el juego real
                // se rellenan al empezar intento / capitulo): un guion que compra a mitad de
                // partida tiene que poder probarlos sin reiniciar.
                if (state.scene == GameScene::Playing || state.scene == GameScene::Paused) {
                    const int r = progression.rank_of(id);
                    if (id == Meta::ALTAR)   prayers_left = std::max(prayers_left, meta_altar_prayers(r));
                    if (id == Meta::SUERTES) rerolls_left = std::max(rerolls_left, meta_rerolls(r));
                    if (id == Meta::ALIENTO) revives_left = std::max(revives_left, meta_revives(r));
                    if (id == Meta::ALFORJA) player.belt_cap = std::min(Player::BELT_MAX, meta_belt_slots(r));
                }
                SDL_Log("meta upgrade %u unlocked (rango %d)", id, progression.rank_of(id));
                continue;
            }
            // `offerings <n>`: fija el banco de ofrendas (para probar compras sin jugar)
            if (c.op == "offerings") {
                progression.offerings = static_cast<int>(std::max(0.0f, c.num));
                sync_progression_to_save();
                save.save(false);
                SDL_Log("offerings = %d", progression.offerings);
                continue;
            }
            // `dropcons <id>`: suelta un consumible delante del profeta (la alforja)
            if (c.op == "dropcons") {
                if (player.entity != ENTITY_NONE) {
                    const Vec2 at = world.transforms[player.entity].position + Vec2{0, 22};
                    spawn_pickup(world, at, ItemComponent::Pickup::Consumable, c.num);
                    SDL_Log("dropcons %d at (%.0f,%.0f)", static_cast<int>(c.num), at.x, at.y);
                }
                continue;
            }
            // Depuracion: encender o apagar el segundo joystick virtual sin pasar por
            // Opciones (su fila solo existe en tactil, y la navegacion por indice de un
            // guion no puede contar con que este ahi).
            if (c.op == "rstick") {
                if (c.num > 0.5f) save.data.settings_flags |=  SETTING_TOUCH_RSTICK;
                else              save.data.settings_flags &= ~SETTING_TOUCH_RSTICK;
                apply_settings_from_save();
                SDL_Log("rstick = %d", input.touch_right_stick ? 1 : 0);
                continue;
            }
            // Depuracion: fijar el modo de auto-mira sin pasar por Opciones. Lo pide
            // scripts/mira_check.txt, que mide los tres modos seguidos en la misma partida.
            if (c.op == "aimmode") {
                int m = static_cast<int>(std::max(0.0f, c.num));
                if (m > 2) m = 2;
                save.data.aim_assist = m;
                apply_settings_from_save();
                SDL_Log("aimmode = %d", m);
                continue;
            }
            // Depuracion: devolver los AJUSTES a fabrica. Existe porque las filas de
            // Opciones que ciclan (auto-mira, idioma, vibracion) no tienen un valor
            // absoluto al que llevarlas con `press`: sin esto, un guion que las toca da
            // un resultado distinto en cada vuelta segun como quedaran en la anterior.
            if (c.op == "defaults") {
                SaveData fresh{};
                save.data.music_volume   = fresh.music_volume;
                save.data.sfx_volume     = fresh.sfx_volume;
                save.data.aim_assist     = fresh.aim_assist;
                save.data.sensitivity    = fresh.sensitivity;
                save.data.language       = fresh.language;
                save.data.settings_flags = fresh.settings_flags;
                apply_settings_from_save();
                load_settings_mirror();
                save.save(false);
                SDL_Log("settings reset to factory defaults");
                continue;
            }
            // Depuracion: cambiar de idioma en caliente (0 = ES, 1 = EN). El texto se
            // pide con `tr()` cuando se dibuja, asi que el cambio se ve en el frame
            // siguiente sin recargar nada.
            if (c.op == "lang") {
                int l = static_cast<int>(std::max(0.0f, c.num));
                if (l >= static_cast<int>(Language::COUNT)) l = 0;
                set_language(static_cast<Language>(l));
                SDL_Log("lang = %d (%s)", l, I18N::language_name(I18N::current));
                continue;
            }
            // Depuracion: volver a abrir el selector de idioma del primer arranque sin
            // borrar el guardado. Solo asi se puede fotografiar esa pantalla desde un
            // guion (en una partida ya jugada el bit SETTING_LANG_CHOSEN esta puesto).
            if (c.op == "langmenu") {
                // Olvida la respuesta: ademas de abrirlo ahora, el selector vuelve a salir
                // en el siguiente arranque. Es lo que hace que se pueda probar dos veces
                // seguidas sin borrar el guardado.
                save.data.settings_flags &= ~SETTING_LANG_CHOSEN;
                save.save(false);
                if (state.scene == GameScene::MainMenu) {
                    menus.current = MenuScreen::Language;
                    menus.selected_option = (save.data.language == 1) ? 1 : 0;
                }
                SDL_Log("langmenu (escena %d)", static_cast<int>(state.scene));
                continue;
            }
            // Depuracion: volcar el guardado al log. Es lo unico que permite comprobar
            // desde un guion que una migracion de version NO ha perdido nada: la meta
            // (ofrendas, mejoras, codice, logros, desbloqueos, ajustes) no se ve entera
            // en ninguna pantalla del juego.
            // Guardar la partida en este instante, por el mismo camino que el autoguardado de
            // una celda limpia: lo que no se puede comprobar sin esperar a limpiar una celda.
            if (c.op == "savenow") {
                if (state.scene == GameScene::Playing || state.scene == GameScene::Paused) {
                    populate_save_data();
                    save.save();
                    SDL_Log("savenow: partida guardada");
                }
                continue;
            }
            if (c.op == "savedump") {
                const SaveData& d = save.data;
                SDL_Log("SAVEDUMP v%u run=%d act=%d cap=%d sala=%d hp=%.0f fe=%.0f objetos=%d",
                        SAVE_VERSION, d.run_active ? 1 : 0, d.current_act, d.current_chapter,
                        d.current_room, d.hp, d.faith, d.item_count);
                SDL_Log("SAVEDUMP meta ofrendas=%d mejoras=0x%X partidas=%d ganadas=%d "
                        "codice=%X/%X/%X/%X relatos=0x%llX unlocks=0x%X logros=0x%X "
                        "milagros=0x%X bajas=%d",
                        d.offerings, d.upgrades_bought, d.total_runs, d.runs_completed,
                        d.codex_enemies, d.codex_items, d.codex_miracles, d.codex_chapters,
                        static_cast<unsigned long long>(d.codex_events),
                        d.unlocks, d.achievements, d.miracles_cast, d.total_kills);
                {
                    char ranks[96]; int off = 0;
                    for (int i = 0; i < static_cast<int>(Meta::COUNT) && off < static_cast<int>(sizeof(ranks)) - 4; i++)
                        off += snprintf(ranks + off, sizeof(ranks) - off, "%d", d.upgrade_ranks[i]);
                    SDL_Log("SAVEDUMP rangos=%s avisos=0x%X alforja=%u:%u/%u:%u/%u:%u/%u:%u dones=%d/%d/%d",
                            ranks, d.tips_seen, d.belt_ids[0], d.belt_charges[0], d.belt_ids[1], d.belt_charges[1],
                            d.belt_ids[2], d.belt_charges[2], d.belt_ids[3], d.belt_charges[3],
                            d.revives_left, d.rerolls_left, d.prayers_left);
                }
                SDL_Log("SAVEDUMP ajustes musica=%.2f efectos=%.2f mira=%d sens=%d idioma=%d "
                        "banderas=0x%X items_desbloq=0x%X skins=0x%X/%u derechos=0x%X",
                        d.music_volume, d.sfx_volume, d.aim_assist, d.sensitivity, d.language,
                        d.settings_flags, d.items_unlocked, d.skins_unlocked, d.skin_equipped,
                        d.entitlements);
                // v9: estado de sala. `salas` lista indice:flags de las que guardan algo;
                // `celdas` = total de celdas alteradas. Es el gate de guardado_check.
                {
                    char salas[160]; int off = 0, celdas = 0;
                    for (int i = 0; i < SAVE_MAX_ROOMS; i++) {
                        if (d.room_flags[i] && off < static_cast<int>(sizeof(salas)) - 8)
                            off += snprintf(salas + off, sizeof(salas) - off, " %d:%02x", i, d.room_flags[i]);
                        for (int b = 0; b < SAVE_ROOM_BYTES; b++)
                            for (u8 v = d.room_open[i][b]; v; v >>= 1) celdas += v & 1;
                    }
                    SDL_Log("SAVEDUMP salas%s celdas_abiertas=%d", off ? salas : " -", celdas);
                }
                continue;
            }
            // Depuracion: volver a bloquear el epilogo para poder probar las dos ramas
            if (c.op == "unlocks") {
                save.data.unlocks = static_cast<u32>(std::max(0.0f, c.num));
                SDL_Log("unlocks = %u", save.data.unlocks);
                continue;
            }
            // Toque sintetico en coordenadas de juego (320x180). `dedo` (tercer numero,
            // 0 por defecto) permite dos dedos a la vez para el gesto de pausa.
            if (c.op == "touch" || c.op == "drag" || c.op == "untouch" ||
                c.op == "touchpx" || c.op == "dragpx" || c.op == "untouchpx") {
                bool px = (c.op.size() > 2 && c.op.compare(c.op.size() - 2, 2, "px") == 0);
                SDL_Event ev{};
                ev.type = (c.op == "touch" || c.op == "touchpx") ? SDL_EVENT_FINGER_DOWN
                        : (c.op == "drag"  || c.op == "dragpx")  ? SDL_EVENT_FINGER_MOTION
                                                                 : SDL_EVENT_FINGER_UP;
                ev.tfinger.fingerID = static_cast<SDL_FingerID>(1 + static_cast<int>(c.num3));
                if (px) {
                    // Pixeles de ventana: el guion no sabe nada del letterbox, y por eso
                    // esta variante SI detecta un desfase entre donde se dibuja y donde
                    // responde. Las otras tres aplican la inversa exacta y nunca lo verian.
                    ev.tfinger.x = c.num  / static_cast<f32>(renderer.window_w);
                    ev.tfinger.y = c.num2 / static_cast<f32>(renderer.window_h);
                } else {
                    // Unidades de juego -> ventana: la inversa de lo que hace process_touch.
                    Rect vp = input.touch_vp;
                    ev.tfinger.x = vp.x + c.num  / static_cast<f32>(Config::GAME_WIDTH)  * vp.w;
                    ev.tfinger.y = vp.y + c.num2 / static_cast<f32>(Config::GAME_HEIGHT) * vp.h;
                }
                input.process_event(ev);
                SDL_Log("touch %s (%.0f,%.0f) finger %d", c.op.c_str(), c.num, c.num2,
                        1 + static_cast<int>(c.num3));
                continue;
            }
            if (c.op == "ascend") {
                if (state.current_act == Act::Act_I && player.entity != ENTITY_NONE) trigger_ascension();
                continue;
            }
            // Depuracion: colocarse al borde de un HUECO y mirando hacia el. El Carro de
            // Fuego es lo unico que los cruza, y las coordenadas de un hueco cambian con la
            // semilla y con cualquier linea de `decorate`: sin esto, comprobar el cruce
            // dependia de que el mapa pusiera uno donde el guion lo esperaba.
            if (c.op == "pit") {
                if (state.scene == GameScene::Playing && player.entity != ENTITY_NONE) {
                    // El hueco MAS CERCANO al profeta: la zona entera tiene varios y el
                    // primero por filas podia estar a tres celdas de distancia.
                    const Room& rm = current_room_data;
                    const Vec2 me = world.transforms[player.entity].position;
                    int bx = -1, by = -1;
                    f32 best = 1e12f;
                    for (int y = 1; y < rm.tiles_h - 1; y++) {
                        for (int x = 2; x < rm.tiles_w - 1; x++) {
                            if (rm.tiles[y][x] != TileType::Pit) continue;
                            if (!rm.is_tile_walkable(x - 1, y)) continue;
                            const f32 d = rm.tile_to_world(x - 1, y).distance_to(me);
                            if (d < best) { best = d; bx = x; by = y; }
                        }
                    }
                    if (bx >= 0) {
                        Vec2 at = rm.tile_to_world(bx - 1, by);
                        world.transforms[player.entity].position = at - Vec2{0, 6};
                        player.last_move_dir = {1, 0};
                        camera.position = at;
                        camera.set_target(at);
                        SDL_Log("pit -> profeta en (%.0f,%.0f), hueco en tile (%d,%d)", at.x, at.y, bx, by);
                    } else {
                        SDL_Log("pit -> este mapa no tiene ningun hueco");
                    }
                }
                continue;
            }
            // Depuracion: plantarse SOBRE la puerta de la sala. La puerta se coloca al azar
            // sobre la senda, asi que sin esto entrar y salir de un interior desde un guion
            // dependeria de acertar sus coordenadas.
            if (c.op == "door") {
                if (state.scene == GameScene::Playing && player.entity != ENTITY_NONE) {
                    const Room& rm = current_room_data;
                    if (!rm.has_doorway) {
                        SDL_Log("door -> esta sala no tiene puerta");
                    } else {
                        // AL LADO, no encima: pisarla la dispara, y asi no se puede
                        // fotografiar la puerta antes de cruzarla. Se prefiere el tile de
                        // ABAJO (en un interior la puerta esta en el muro sur y abajo no hay
                        // nada, asi que ahi cae el de arriba).
                        // Y DENTRO de la sala: `is_tile_walkable` deja pasar fuera de la
                        // rejilla (lo necesita `try_room_transition`), asi que sin el limite
                        // la puerta del muro sur de un interior dejaba al profeta en el
                        // exterior de la sala, donde no hay nada que pisar.
                        bool below = (rm.door_ty + 1 < rm.tiles_h - 1) &&
                                     rm.is_tile_walkable(rm.door_tx, rm.door_ty + 1);
                        int ay = below ? rm.door_ty + 1 : rm.door_ty - 1;
                        Vec2 at = rm.tile_to_world(rm.door_tx, ay);
                        world.transforms[player.entity].position = at - Vec2{0, 6};
                        camera.position = at;
                        camera.set_target(at);
                        SDL_Log("door -> puerta en tile (%d,%d); profeta %s de ella (entrar: %s)",
                                rm.door_tx, rm.door_ty, below ? "debajo" : "encima",
                                below ? "hold up" : "hold down");
                    }
                }
                continue;
            }
            // Depuracion: atraer al profeta TODAS las recogidas de la sala. Un hallazgo cae
            // donde estaba el secreto, y un guion no sabe donde fue: sin esto, comprobar que
            // la ofrenda llega a la meta-progresion dependia de que el bot pasara por encima.
            if (c.op == "collect") {
                if (player.entity != ENTITY_NONE && world.has(player.entity, Component::Transform)) {
                    Vec2 at = world.transforms[player.entity].position;
                    int n = 0;
                    for (u32 e = 0; e < world.entity_count; e++) {
                        if (!world.is_alive(e) || !world.has(e, Component::Item)) continue;
                        if (world.items[e].pickup == ItemComponent::Pickup::None) continue;
                        world.transforms[e].position = at;
                        n++;
                    }
                    SDL_Log("collect -> %d recogidas atraidas", n);
                }
                continue;
            }
            if (c.op == "killall") {
                // Lista primero, matanza despues: si no, los hijos que nacen al morir un
                // enemigo (Agua Corrupta) caen dentro del mismo barrido y nunca se ven.
                Entity list[MAX_ENTITIES];
                int n = 0;
                for (u32 e = 0; e < world.entity_count; e++) {
                    if (!world.is_alive(e) || !is_hostile(world, e) || !world.has(e, Component::Health)) continue;
                    if (!world.healths[e].is_alive()) continue;
                    list[n++] = static_cast<Entity>(e);
                }
                for (int i = 0; i < n; i++) {
                    // Atraviesa lo intocable (2026-10-03): desde que los jefes tienen fases
                    // en las que no se les toca —el sumo sacerdote con su corro, el Espiritu
                    // bajo el agua, el Sacerdote con sus idolos— un `killall` en el momento
                    // malo dejaba al jefe vivo y la arena sin cerrar. Un prop que haya caido
                    // con su jefe ya no tiene Health y se salta.
                    if (!world.has(list[i], Component::Health)) continue;
                    world.healths[list[i]].invulnerable = false;
                    world.healths[list[i]].iframes_timer = 0.0f;
                    if (apply_damage(world, ENTITY_NONE, list[i], 100000.0f))
                        on_enemy_killed(world, faith, list[i], combat_events);
                }
                continue;
            }
            if (c.op == "walk") { ap.walk_target = {c.num, c.num2}; ap.timer = 6.0f; ap.walking = true; break; }
            // Mapa grande (2026-09-24): la celda de inicio y la del jefe cambian de sitio por
            // capitulo, asi que un `walk` en coordenadas absolutas se pudre con cada cambio de
            // CHAPTER_LAYOUT. `walks` va relativo al origen de la celda de INICIO (lo que los
            // guiones clavaban en ella) y `walkarena d` a `d` unidades del centro del vano,
            // hacia dentro si es positivo (en North, hacia arriba; en West/South, hacia la
            // izquierda; en East, hacia la derecha) — valga el lado que valga.
            if (c.op == "walks") {
                Vec2 o = {0, 0};
                for (int i = 0; i < floor_map.room_count; i++)
                    if (floor_map.rooms[i].type == RoomType::Start)
                        o = {static_cast<f32>(floor_map.rooms[i].grid_x * Config::ROOM_W_UNITS),
                             static_cast<f32>(floor_map.rooms[i].grid_y * Config::ROOM_H_UNITS)};
                ap.walk_target = o + Vec2{c.num, c.num2}; ap.timer = 6.0f; ap.walking = true; break;
            }
            if (c.op == "walkarena") {
                const Room* z = zone_ptr();
                if (!z || !z->has_arena) { SDL_Log("walkarena -> no hay arena"); continue; }
                Vec2 in = {0, -1};
                if (z->arena_side == ArenaSide::West || z->arena_side == ArenaSide::South) in = {-1, 0};
                if (z->arena_side == ArenaSide::East) in = {1, 0};
                ap.walk_target = z->arena_gap_center_world() + in * c.num;
                SDL_Log("walkarena %.0f -> (%.0f,%.0f)", c.num, ap.walk_target.x, ap.walk_target.y);
                ap.timer = 6.0f; ap.walking = true; break;
            }
            if (c.op == "intel") {
                const Room* z = zone_ptr();
                if (z) {
                    const BossIntel bi = get_boss_intel(*z, floor_map, player.map_revealed);
                    SDL_Log("INTEL nivel=%d pistas=%d visto=%d celda_jefe=%d", bi.level, bi.clues, bi.seen ? 1 : 0, z->boss_cell);
                }
                continue;
            }
            // `bosshp <fraccion> [quien]`: fija la vida del jefe (y/o de su sequito: el corro, Jezabel)
            // a una fraccion de su maximo, para saltar a una FASE sin jugarse la pelea entera.
            // quien: 0 todos (por defecto), 1 solo la cabeza, 2 solo el sequito. Con 0 los MATA
            // por el camino normal (apply_damage + on_enemy_killed), aunque esten intocables.
            if (c.op == "bosshp") {
                const f32 frac = std::max(0.0f, std::min(1.0f, c.num));
                const int who = static_cast<int>(c.num2);
                int touched = 0;
                for (u32 e = 0; e < world.entity_count; e++) {
                    if (!world.is_alive(e) || !world.has(e, Component::EnemyAI) || !world.has(e, Component::Health)) continue;
                    const auto& bai = world.enemy_ais[e];
                    auto& bh = world.healths[e];
                    if (!bh.is_alive()) continue;
                    if (!(bai.is_boss && who != 2) && !(bai.boss_group && who != 1)) continue;
                    if (frac <= 0.0f) {
                        bh.invulnerable = false;
                        bh.iframes_timer = 0.0f;
                        if (apply_damage(world, ENTITY_NONE, e, 1.0e6f))
                            on_enemy_killed(world, faith, e, combat_events);
                    } else {
                        bh.current = std::max(1.0f, bh.max * frac);
                    }
                    touched++;
                }
                SDL_Log("Autoplay: bosshp %.2f (quien %d) -> %d", frac, who, touched);
                continue;
            }
            if (c.op == "god")  { god_mode = !god_mode; continue; }
            // `entrada`: deja pasar la ENTRADA del jefe (paseo + dialogo), que el autoplay
            // suprime por defecto. Va antes del `walk` que mete al profeta en la arena.
            if (c.op == "entrada") { autoplay_boss_talk = true; SDL_Log("Autoplay: entrada del jefe activada"); continue; }
            // Depuracion: sembrar enemigos de un tipo concreto alrededor del jugador.
            // Sin esto, comprobar una conducta del Acto II depende de que el capitulo
            // la sortee (los cuatro tipos se reparten al azar por sala).
            if (c.op == "spawn") {
                u32 t = static_cast<u32>(std::max(0.0f, c.num));
                int n = std::max(1, static_cast<int>(c.num2));
                if (t < static_cast<u32>(EnemyType::COUNT) && player.entity != ENTITY_NONE &&
                    world.has(player.entity, Component::Transform)) {
                    Vec2 p = world.transforms[player.entity].position;
                    for (int i = 0; i < n; i++) {
                        Vec2 at = p + Vec2{static_cast<f32>((i % 4) - 1) * 20.0f,
                                           static_cast<f32>((i / 4) - 1) * 20.0f - 34.0f};
                        Entity se = spawn_enemy(world, static_cast<EnemyType>(t), at, world.enemy_textures[t],
                                                state.current_chapter);
                        // Cuenta en la celda actual: si no, la celda no se limpiaria nunca
                        if (se != ENTITY_NONE && current_room_data.type == RoomType::Zone)
                            world.enemy_ais[se].home_cell = floor_map.current_room;
                    }
                    SDL_Log("Autoplay: spawn %d x %s", n, get_enemy_def(static_cast<EnemyType>(t)).name);
                }
                continue;
            }
            // Depuracion: soltar UN efecto de la hoja propia (game/combat.h FxID) delante
            // del profeta y nada mas. En plena pelea no hay forma de decir que efecto es
            // cual, y sin esto el track de VFX no se puede verificar de verdad.
            //   fx <id> [tamano_unidades] [duracion_s]
            if (c.op == "fx") {
                u32 id = static_cast<u32>(std::max(0.0f, c.num));
                f32 size = (c.num2 > 0.0f) ? c.num2 : 48.0f;
                f32 dur  = (c.num3 > 0.0f) ? c.num3 : 0.6f;
                if (id < static_cast<u32>(FxID::COUNT) && player.entity != ENTITY_NONE &&
                    world.has(player.entity, Component::Transform)) {
                    Vec2 at = world.transforms[player.entity].position + Vec2{0, -34.0f};
                    Entity fe = spawn_fx(world, at, static_cast<FxID>(id),
                                         {255, 255, 255, 245}, size, dur, 11);
                    SDL_Log("fx %u en (%.0f,%.0f) tam %.0f dur %.2f -> %s", id, at.x, at.y,
                            size, dur, fe == ENTITY_NONE ? "SIN HOJA" : "ok");
                }
                continue;
            }
            // Depuracion: montar el estado que hace falta para mirar un efecto concreto
            if (c.op == "give") {
                const ItemDef* d = item_registry.get(static_cast<u32>(c.num));
                if (d) { player.grant_item(world, faith, *d); codex_see_item(save.data, d->id); }
                continue;
            }
            if (c.op == "learn") {
                int slot = std::max(0, std::min(Config::MAX_MIRACLE_SLOTS - 1, static_cast<int>(c.num2)));
                u32 mid = static_cast<u32>(c.num);
                if (mid < static_cast<u32>(MiracleID::COUNT)) {
                    player.miracle_slots[slot].equip(static_cast<MiracleID>(mid));
                    player.active_slot_count = std::max(player.active_slot_count, slot + 1);
                }
                continue;
            }
            if (c.op == "cards")   { open_item_choice(tr("ELIGE UNA BENDICIÓN", "CHOOSE A BLESSING")); continue; }
            if (c.op == "mcards")  { open_miracle_choice(tr("ORAS EN EL ALTAR", "YOU PRAY AT THE ALTAR")); continue; }
            // El prologo no se lanza solo con --script (los guiones esperan el menu al
            // arrancar), asi que para fotografiarlo hay que pedirlo. `cine <n>` salta
            // directo a una estampa concreta.
            if (c.op == "intro")   { start_intro(); continue; }
            if (c.op == "cine") {
                if (state.scene != GameScene::Intro) start_intro();
                cinematic.go_to(static_cast<int>(c.num), renderer);
                cinematic.auto_timer = 0.0f;   // se queda quieta hasta el siguiente comando
                continue;
            }
            if (c.op == "faith") {
                faith.current = std::max(0.0f, std::min(faith.max, c.num));
                faith.modify(0.0f);   // recalcula el nivel
                continue;
            }
            // Sonda de TACTO (track 1 del plan de game feel). Imprime el TRAMO desde la
            // ultima marca, no el acumulado: con `feel start` ... `feel end` alrededor de
            // una pelea sale el TTK y los golpes encajados de ESA pelea, que es lo unico
            // con lo que se puede decir si un ajuste ha mejorado algo o solo se ve bonito.
            if (c.op == "feel") {
                f32 dt_s   = state.run_time - feel_mark.t;
                int dk     = run_kills - feel_mark.kills;
                int dh     = run_hits_taken - feel_mark.hits;
                int ds     = player.shots_fired - feel_mark.shots;
                f32 d_out  = world.dmg_dealt - feel_mark.out;
                f32 d_in   = world.dmg_taken - feel_mark.in_;
                SDL_Log("FEEL %s | dt %.2fs | bajas %d | TTK %.2fs | golpes recibidos %d | "
                        "tiros %d | dano hecho %.0f (%.1f/s) | dano recibido %.0f",
                        c.arg.empty() ? "-" : c.arg.c_str(), dt_s, dk,
                        dk > 0 ? dt_s / static_cast<f32>(dk) : 0.0f, dh, ds,
                        d_out, dt_s > 0.01f ? d_out / dt_s : 0.0f, d_in);
                feel_mark = {state.run_time, run_kills, run_hits_taken,
                             player.shots_fired, world.dmg_dealt, world.dmg_taken};
                continue;
            }
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
                // Segunda linea: el equipo. Sin esto, comprobar la Ascension (manto,
                // milagro heredado, tercer slot) obligaba a leer capturas a ojo.
                std::string loadout;
                for (int s = 0; s < player.active_slot_count && s < Config::MAX_MIRACLE_SLOTS; s++) {
                    MiracleID m = player.miracle_slots[s].miracle;
                    loadout += (s ? " / " : "");
                    loadout += (m == MiracleID::COUNT) ? "(vacio)" : get_miracle_def(m).name;
                }
                std::string owned;
                for (u32 id : player.items) {
                    const ItemDef* d = item_registry.get(id);
                    if (d) { owned += owned.empty() ? "" : ", "; owned += d->name; }
                }
                // La reliquia y sus DOS efectos van en la linea: son lo unico que no se
                // deduce de la lista de objetos, y sin ellos no hay forma de comprobar
                // desde un guion que la sustitucion ha deshecho lo de la anterior.
                const ItemDef* relic = (player.relic_slot != UINT32_MAX)
                                     ? item_registry.get(player.relic_slot) : nullptr;
                SDL_Log("LOADOUT %s | %s | slots %d [%s] | recarga x%.2f | reliquia %s "
                        "(suelo fe %.0f, milagros x%.2f) | fe %.0f | objetos: %s",
                        c.arg.empty() ? "-" : c.arg.c_str(),
                        player.character == CharacterType::Elias ? "Elias" : "Eliseo",
                        player.active_slot_count, loadout.c_str(), player.miracle_haste,
                        relic ? relic->name : "-", faith.floor_value, player.miracle_power,
                        faith.current, owned.empty() ? "-" : owned.c_str());
                // Tercera linea (2026-10-03): la alforja, los dones del Santuario que quedan y
                // las cifras que lo comprado cambia. Es el gate de santuario_check / alforja_check.
                std::string belt;
                for (int i = 0; i < player.belt_cap && i < Player::BELT_MAX; i++) {
                    const auto& b = player.belt[i];
                    const ItemDef* d = b.empty() ? nullptr : item_registry.get(b.id);
                    char cell[64];
                    snprintf(cell, sizeof(cell), "%s%s%s x%u", i ? " / " : "", (i == player.belt_sel && d) ? "*" : "",
                             d ? d->name : "-", static_cast<unsigned>(b.empty() ? 0 : b.charges));
                    belt += cell;
                }
                f32 dmg = 0.0f, spd = 0.0f;
                if (player.entity != ENTITY_NONE && world.has(player.entity, Component::Combat))   dmg = world.combats[player.entity].damage;
                if (player.entity != ENTITY_NONE && world.has(player.entity, Component::Movement)) spd = world.movements[player.entity].speed;
                SDL_Log("GIFTS %s | alforja %d [%s] | aliento %d | suertes %d | oraciones %d | dano %.0f | vel %.0f | "
                        "escudo %d | ofrendas %d",
                        c.arg.empty() ? "-" : c.arg.c_str(), player.belt_cap, belt.c_str(), revives_left,
                        rerolls_left, prayers_left, dmg, spd, player.shield_up ? 1 : 0, progression.offerings);
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
            if (player.active_slot_count > 2 && player.miracle_slots[2].is_ready() &&
                best < 90.0f && (static_cast<int>(world.time * 10) % 13 == 0))
                input.actions[static_cast<u32>(Action::Miracle3)] = true;
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
