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
#include "render/camera.h"
#include "input/input.h"
#include "audio/audio.h"
#include "game/faith.h"
#include "game/player.h"
#include "game/enemies.h"
#include "game/combat.h"
#include "game/hazards.h"
#include "game/rooms.h"
#include "game/items.h"
#include "game/events.h"
#include "game/tutorial.h"
#include "ui/menus.h"
#include "ui/hud.h"
#include "ui/story.h"
#include "ui/cinematic.h"
#include "ui/codex.h"
#include "core/i18n.h"
#include "systems/save.h"
#include "systems/progression.h"
#include "systems/achievements.h"

// =============================================================================
// Game State
// =============================================================================
enum class GameScene : u8 {
    Intro,       // Prologo cinematico "El Cielo Cerrado" (ui/cinematic.h)
    MainMenu,
    Playing,
    Paused,
    Choice,      // "Elige una bendicion": 3 item cards over the frozen world
    Story,       // Prologo / carta de capitulo / ascension / epilogo (ui/story.h)
    Dialogue,    // Encuentro biblico de una sala Event (game/events.h)
    Codex,       // El codice: lo descubierto hasta ahora (ui/codex.h)
    Sanctuary,   // El Santuario: gastar ofrendas en mejoras (systems/progression.h)
    GameOver,
    Victory
};

// Que anuncia start_chapter al abrir un capitulo. `Cinematic` es la intro animada de la
// PRIMERA entrada natural (nueva partida, pasar de capitulo, Ascension); el reintento y
// el Continuar usan `Card` — repetir la intro en cada muerte cansaria. Es un enum class
// a proposito: un bool no convierte, asi que cada llamante se revisa al compilar.
enum class ChapterAnnounce : u8 { None, Card, Cinematic };

// A donde va GameScene::Intro al terminar: el prologo vuelve al menu, la intro de
// capitulo entra a jugar (el mundo ya esta cargado detras, como con la carta) y la
// cinematica del Ascenso reparte la herencia (finish_ascension abre el capitulo 6).
enum class CineThen : u8 { MainMenu, Play, FinishAscension };

// Pending "choose 1 of 3" offer (GDD core loop step 4)
struct ItemChoice {
    u32 ids[3]  = {UINT32_MAX, UINT32_MAX, UINT32_MAX};
    int count    = 0;
    int selected = 0;
    f32 auto_timer = 0.0f;   // > 0: autoplay picks for us after this delay
    bool miracles = false;   // true: ids are MiracleIDs, not item ids
    std::string extras[3];   // per-card note ("Sustituye a ...")
    const char* title = tr("ELIGE UNA BENDICIÓN", "CHOOSE A BLESSING");
};

// Encuentro biblico en curso (GameScene::Dialogue). El texto sale de get_event_def().
struct DialogueState {
    int  event_id = -1;      // id de `namespace Ev` (game/events.h), no el capitulo
    int  chapter  = 0;
    int  line     = 0;       // Linea de dialogo que se esta leyendo
    int  selected = 0;
    bool choosing = false;   // Ya se leyo todo: se muestran las opciones
    bool resolved = false;   // Ya se eligio: se muestra el resultado
    const char* result = ""; // Texto de la opcion elegida
    f32  auto_timer = 0.0f;  // > 0 (autoplay): decide solo
    // El Ascenso se juega sobre NEGRO y no sobre el mundo: pasa fuera de una sala, y
    // dejar detras la arena del jefe del capitulo 5 seria mentir sobre donde estas.
    bool dark = false;
    f32  t = 0.0f;           // segundos desde que se abrio la caja (entrada animada de la estampa)
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
//   shot <name> | goto <room_index> | god | quit | say <text> | escena <id> [1]
// Actions: up down left right attack dash m1 m2 confirm cancel pause map
// =============================================================================
struct AutoplayCmd {
    std::string op;
    std::string arg;
    f32 num = 0.0f;
    f32 num2 = 0.0f;
    f32 num3 = 0.0f;
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
    StorySystem story;
    CinematicSystem cinematic;
    CodexScreen codex;
    ProgressionSystem progression;
    AchievementSystem achievements;
    HUD         hud;
    GameScene   codex_return = GameScene::MainMenu;   // Desde donde se abrio el codice
    int         sanctuary_sel = 0;
    // Ventana visible del panel. Con 12 mejoras ya no caben todas en 180 unidades de alto
    // (a 14,5 de paso son 174 y el titulo se come 47), asi que el panel SCROLLEA, igual
    // que la lista del Codice. Poner dos columnas se descarto: las descripciones son
    // frases enteras y a 150 unidades de ancho se truncarian en silencio.
    int         sanctuary_scroll = 0;
    // Zonas tactiles de El Santuario, apuntadas al PINTAR. Antes update_sanctuary
    // reescribia a mano las mismas cuentas que render_sanctuary (y=48, paso 14,5, "Volver"
    // en 150): era el unico sitio del juego que rompia la regla "el rect sale de la misma
    // formula que pinta", y en cuanto el paso cambio el hit-test habria apuntado a filas
    // que no estan ahi, en silencio.
    Rect        sanctuary_row_rects[8] = {};
    int         sanctuary_row_first = 0;   // mejora que ocupa el hueco 0
    int         sanctuary_row_count = 0;
    Rect        sanctuary_back_rect = {0, 0, 0, 0};
    // Dedo apoyado en el boton de SALTAR de la caja de dialogo. Va por un campo y no
    // por una variable local porque la vista se compone dentro de una lambda.
    bool        dialogue_skip_pressed = false;
    // Velo negro corto al SALTAR un tramo de escena. Sin el, saltar la apertura del
    // capitulo dejaba en pantalla la intro cinematica —otra estampa estatica con el mismo
    // boton de SALTAR en el mismo pixel— y no habia forma de distinguirlo de "el boton no
    // ha hecho nada". Es un fundido de ENTRADA, no de salida: el tramo cambia en el acto y
    // lo que se aclara es el nuevo, asi que no añade ni un milisegundo de espera.
    f32         scene_fade = 0.0f;
    static constexpr f32 SCENE_FADE_TIME = 0.22f;
    // Filas visibles del panel. En tactil la fila mide 20 unidades para que se pueda tocar
    // (lleva nombre y descripcion, no baja de ahi) y por eso caben cinco en vez de siete.
    int sanctuary_rows() const { return input.touch_ui ? 5 : 7; }
    Player      player;
    SaveSystem  save;
    ItemRegistry item_registry;
    ItemChoice   choice;
    DialogueState dialogue;

    // Efectos de run que fijan los encuentros biblicos (game/events.h)
    f32  boss_hp_scale = 1.0f;   // < 1: el jefe del capitulo entra debilitado
    bool has_revive    = false;  // El aposento sunamita: una muerte perdonada
    // "El Reloj de Acaz" (item 20, 2 Reyes 20:11): perdona una muerte POR CAPITULO. Es un
    // permiso aparte y no un `has_revive = true` al empezar el capitulo, porque asi no
    // depende de en que orden se rehaga el equipo (concesion, reintento, carga, Ascension)
    // ni pisa el perdon del encuentro sunamita: se pueden tener los dos.
    bool relic_revive_ready = false;

    // --- Dones de El Santuario en curso (2026-10-03) ----------------------------------
    // Lo que el jugador COMPRO tiene que verse funcionar: cada don lleva su contador, el HUD
    // lo pinta y el log dice cuando salta. Van al guardado (v12) para que salir al menu y
    // Continuar no los rellene.
    int  revives_left = 0;          // Aliento de Vida: levantarse, por INTENTO
    int  rerolls_left = 0;          // Echar Suertes: re-sorteos, por CAPITULO
    int  prayers_left = 0;          // Altar Portatil: oraciones sin altar, por CAPITULO
    bool primicias_pending = false; // la carta de Primicias espera a que se juegue
    // Lo ganado en el INTENTO (desde el ultimo cobro): lo que paga award_offerings. Antes la
    // paga se calculaba con `run_kills`, que NO se reiniciaba al reintentar: cada derrota
    // volvia a cobrar las bajas de todas las anteriores.
    struct RunTally { int kills = 0, cells = 0, bosses = 0, found = 0; };
    RunTally tally;
    // El desglose de la ultima paga, para la pantalla de derrota / victoria. Lo escribe
    // award_offerings y lo pinta render_run_summary, con la cuenta subiendo.
    struct OfferingLine { char label[48]; int value; };
    struct OfferingSummary {
        OfferingLine lines[7];
        int  count = 0;
        int  total = 0;          // lo que entra ahora (sin lo ya ingresado de los hallazgos)
        int  bank  = 0;          // ofrendas que quedan en el banco tras cobrar
        bool victory = false;
        f32  t = 0.0f;           // segundos desde que se abrio: la cuenta sube
    };
    OfferingSummary summary;
    // El contador de ofrendas del HUD: salta y dice "+N" al ingresar
    f32  offering_pop = 0.0f;
    int  offering_pop_amount = 0;

    // ---- Destreza (Config::Skill, 2026-11-10) -----------------------------------------
    // CELO: la racha de golpes y bajas encadenados sin que te toquen. Solo corre en Playing
    // (un dialogo o una carta la congelan, no la enfrian) y no viaja entre capitulos.
    struct Zeal {
        int  count = 0;          // golpes + bajas de la racha en curso
        f32  timer = 0.0f;       // lo que queda de ventana antes de que se cierre
        int  tier  = 0;          // 0..Config::Skill::ZEAL_TIERS
        f32  pop   = 0.0f;       // latido del contador al sumar (1 -> 0)
        f32  tier_flash = 0.0f;  // destello del rotulo al SUBIR de grado (1 -> 0)
        // El rotulo de cierre: "+N ofrendas" si se enfrio limpia, "Racha perdida" si te
        // tocaron. `end_payout` -1 = rota.
        f32  end_t = 0.0f;
        int  end_count = 0;
        int  end_payout = 0;
    };
    Zeal zeal;
    int  run_best_zeal = 0;          // la mejor racha del intento (stats / log)
    int  run_perfect_dodges = 0;     // esquivas perfectas del intento (stats / log)
    int  run_crits = 0;              // criticos del intento (stats / log)
    f32  slowmo_t = 0.0f;            // camara lenta de la esquiva perfecta (s reales)
    bool dodge_latched = false;      // una esquiva perfecta por dash, no una por frame
    void zeal_add(int hits, int kills);
    void zeal_close(bool broken);    // se enfria (paga) o se rompe (no paga)
    void zeal_reset();               // capitulo nuevo / reintento / tutorial: a cero sin rotulo
    void on_perfect_dodge();
    void update_skill(f32 dt);       // lo de arriba, una vez por paso de Playing
    f32  belt_full_cd = 0.0f;       // aviso "alforja llena" (uno cada 3 s como mucho)
    // La ficha de "objeto obtenido" del HUD (HUD::draw_pickup_card)
    int         pickup_icon = -1;
    std::string pickup_name, pickup_desc, pickup_hint;
    Color       pickup_color = {245, 205, 90, 255};
    f32         pickup_t = 0.0f;
    void announce_item(const struct ItemDef& d, const char* hint);
    // A donde vuelve El Santuario: al titulo o a la pantalla de derrota (desde ahi se va a
    // gastar lo que se acaba de ganar y se reintenta ya con la mejora puesta).
    GameScene sanctuary_return = GameScene::MainMenu;
    int  sanctuary_tab = 0;         // MetaCategory
    f32  sanctuary_flash = 0.0f;    // destello de compra
    u32  sanctuary_flash_id = 0xFFFFFFFFu;
    Rect sanctuary_tab_rects[8] = {};
    Rect sanctuary_buy_rect = {0, 0, 0, 0};
    Rect choice_reroll_rect = {0, 0, 0, 0};   // placa de Echar Suertes en la carta
    // Fila de la pausa que abre el Equipo (pantalla propia sobre la pausa)
    bool equipment_open = false;
    int  equipment_scroll = 0;

    // Room / Map
    FloorMap  floor_map;
    Room      current_room_data;
    // La camara del mundo exterior. `Camera` existia desde el primer commit y NO la llamaba
    // nadie: con la sala midiendo exactamente una pantalla no habia nada que seguir. Ahora
    // una sala exterior son 2x2 pantallas y esto es lo que las hace recorribles. En una sala
    // de una pantalla — la arena del jefe, y los interiores de la fase siguiente — su
    // recorte contra los limites la deja clavada, o sea el comportamiento de siempre.
    Camera    camera;
    Entity    boss_entity = ENTITY_NONE;
    // Barra del jefe (ui/hud.h HUDContext). Con el jefe en GRUPO (cap. 3, el corro de
    // Baal: game/enemies.cpp spawn_boss_group) la vida es la suma de la cabeza y de los
    // miembros (`EnemyAIComponent::boss_group`), el maximo se fija al nacer el grupo y la
    // barra sigue mientras quede alguien aunque la cabeza (`boss_entity`) haya caido.
    f32         boss_group_max = 0.0f;         // > 0: jefe en grupo
    f32         boss_bar_hp    = 0.0f;
    f32         boss_bar_max   = 0.0f;
    int         boss_bar_heads = 0;
    bool        boss_bar_invulnerable = false;
    const char* boss_bar_name  = nullptr;
    int         boss_group_alive_last = 0;     // para el log "Boss group: N alive"

    // Sprite sheets (renderer texture ids, TEXTURE_NONE if missing -> placeholders)
    u32 character_textures[2] = {TEXTURE_NONE, TEXTURE_NONE};            // by CharacterType
    u32 enemy_textures[static_cast<u32>(EnemyType::COUNT)] = {};         // by EnemyType
    u32 tileset_texture = TEXTURE_NONE;
    u32 vfx_texture     = TEXTURE_NONE;   // assets/sprites/vfx.png (ver spawn_fx)
    u32 icons_texture   = TEXTURE_NONE;   // assets/sprites/icons.png (render/sprites.h, namespace Icon)
    u32 chariot_texture = TEXTURE_NONE;   // assets/sprites/chariot.png (Carro de Fuego visible)
    u32 portrait_texture = TEXTURE_NONE;  // assets/sprites/portraits.png (Bestiario del Códice)
    // La ESTAMPA del relato abierto (assets/sprites/estampas/ev_NN.png, tools/estampas). Se carga
    // al abrir el relato y se descarga al abrir el siguiente: solo hay una viva, 1,2 MB de VRAM.
    u32 plate_texture = TEXTURE_NONE;
    int plate_event   = -1;
    u32 npc_texture = TEXTURE_NONE;       // assets/sprites/npcs.png (quien espera en la sala Event)
    // Hojas de Acab y Jezabel (896x1920, como las de los profetas): solo las usan las escenas
    // guionizadas (start_scene), donde estan de pie en el escenario.
    u32 acab_texture    = TEXTURE_NONE;
    u32 jezabel_texture = TEXTURE_NONE;
    // Hoja propia del jefe del capitulo (rooms.h get_boss_sheet), cargada al empezar el
    // capitulo. Sin el fichero, la del enemigo base y `own` a false (bosses.cpp lo dibuja
    // entonces como siempre, a 1,6). Ver load_boss_sheets.
    BossSheets boss_sheets;
    void load_boss_sheets(int chapter);
    // Texturas procedurales de los telegrafos de suelo (game/hazards.h)
    HazardArt hazard_art;
    // La oscuridad de La Muerte (cap. 8): 0..1 suavizado, y el radio de la luz que deja
    // alrededor del profeta. La pide el jefe cada frame (ECSWorld::darkness_req).
    f32 darkness_level  = 0.0f;
    f32 darkness_radius = 76.0f;
    void render_darkness();

    // Frame draw list (reused)
    std::vector<DrawCmd> draw_list;

    // HUD feedback
    std::string banner_text, subbanner_text, message_text;
    f32 banner_timer = 0.0f;
    f32 message_timer = 0.0f;
    f32 room_fade = 0.0f;        // 1 -> 0 fade-in after a room/chapter load (black veil under the HUD)
    // Bandas de cine (0..1) en las escenas guionizadas y en la entrada del jefe: entran al
    // abrirse la escena y salen al volver a jugar. Lo que dice "esto es una escena" sin texto.
    f32 scene_bars = 0.0f;
    f32 world_flash = 0.0f;      // Fogonazo blanco de milagro (Player::flash_request)
    u32 chapter_seed = 0;        // RNG seed the current chapter map was generated with (saved)
    u32 pending_seed = 0;        // != 0: use this seed for the next start_chapter (loading a save)
    // Mundo fijo (2026-08-27): la semilla normal es Config::CHAPTER_SEEDS[cap-1]. El comando
    // `seed` del autoplay deja aqui un override PEGAJOSO (vale para todos los capitulos de la
    // sesion): existe solo como compatibilidad de guiones, el juego real no lo toca nunca.
    u32 forced_seed = 0;
    // Contador de re-siembras del rand() global tras capture_room_states (que lo deja con la
    // semilla de la ultima sala regenerada). Efimero por sesion, no va al guardado: la
    // reproducibilidad de los guiones viene de que la SECUENCIA de guardados es determinista.
    u32 rng_epoch = 0;
    // Estado de sala guardado (v9): continue_run pone el primero justo antes de start_chapter,
    // y este lo consume en el segundo, que es el que load_room consulta al generar cada sala.
    // Un capitulo nuevo o un reintento lo apagan solos: arrancan con las salas frescas.
    bool restore_room_state = false;
    bool apply_saved_rooms  = false;
    CombatEvents combat_events;
    int  run_kills = 0;
    int  run_hits_taken = 0;   // Golpes encajados en la partida (sonda de tacto `feel`)
    // Margen sin transiciones justo despues de cargar una sala. Sin el, un paso en
    // diagonal por una esquina podia encadenar dos cambios de sala en dos frames.
    f32  room_change_grace = 0.0f;
    // Marca de la ultima medida `feel`: el comando imprime el TRAMO, no el acumulado.
    struct FeelMark { f32 t = 0.0f; int kills = 0, hits = 0, shots = 0; f32 out = 0.0f, in_ = 0.0f; };
    FeelMark feel_mark;
    bool god_mode = false;
    // Reglas de movil en escritorio (--touch): hoy solo la caducidad de la partida a las
    // 48 h, que en PC no se aplica pero hay que poder probar.
    bool mobile_rules = false;
    // Pista unica "[Q] milagro": el milagro es el corazon del juego y nadie lo pulsa
    // si nadie se lo dice. Cuenta atras mientras se juega el cap. 1.
    f32  miracle_hint_timer = 5.0f;
    bool miracle_hint_shown = false;

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
    // Tamano de ventana forzado (0 = el que decida la plataforma). Lo usa `--window WxH`,
    // que existe para poder probar la orientacion VERTICAL del GDD 4.5 en un escritorio:
    // sin el, comprobar el reparto de bandas exigia un telefono.
    int forced_win_w = 0, forced_win_h = 0;
    // `--dpi N`: finge una pantalla de N puntos por pulgada para que el tamaño de los
    // controles tactiles por DPI se pueda probar en un escritorio (0 = el del aparato; en
    // escritorio, sin el flag, la escala es 1: el movil de referencia).
    f32 forced_dpi = 0.0f;

    // Controles tactiles: zona segura de la ventana (notch, gestos del sistema, pilares) y
    // escala por el tamaño FISICO de la pantalla. Se recalcula al crear la ventana, al
    // redimensionarla y cuando el sistema cambia la zona segura.
    void refresh_touch_area();
    f32  touch_dpi_scale() const;
    // Vista previa de los controles en Opciones: al tocar tamaño, opacidad o zurdo, los
    // botones se pintan un momento encima de la pantalla con el ajuste nuevo.
    static constexpr f32 TOUCH_PREVIEW_TIME = 1.6f;
    f32  touch_preview_t = 0.0f;
    // "Ahorro de bateria" (SETTING_BATTERY_SAVE): el bucle pinta un paso de cada dos.
    bool battery_saver = false;
    // Ritmo de frames con reglas de movil (Engine::run): pasos de update desde el ultimo
    // frame pintado, y "pinta ya aunque no haya paso" (ventana recreada o redimensionada).
    int  steps_since_render = 0;
    bool force_render = true;
    // Sonda `fpsmark` de los guiones
    u64  fps_mark_time = 0;
    u32  fps_mark_frames = 0;
    // Atras en el titulo (movil): la primera vez avisa durante estos segundos, la segunda sale.
    f32  exit_confirm_t = 0.0f;

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
    void start_chapter(int chapter, ChapterAnnounce announce = ChapterAnnounce::Card);
    void advance_chapter();
    void retry_chapter();      // Game over -> same chapter again
    void trigger_ascension();  // Elias -> Eliseo transition
    void finish_ascension();   // Confirm on the ascension screen

    // Historia (ui/story.h): abre GameScene::Story y ejecuta `then` al terminar
    void play_story(StoryThen then);
    void finish_story();
    void finish_story_action(StoryThen then);

    // Prologo cinematico (ui/cinematic.h). Lo arranca main.cpp cuando NO hay --script:
    // el guion de autoplay se carga despues de init(), asi que dentro de init el flag
    // todavia es false y los guiones de scripts/ esperan el menu al arrancar.
    void start_intro();
    // El prologo va DETRAS del selector de idioma del primer arranque: es la primera
    // narracion del juego y contarla en un idioma que el jugador no ha elegido seria
    // justo lo que el selector viene a evitar. Lo pone main.cpp (que es quien decide si
    // hay prologo) y lo consume el propio selector al contestarse.
    bool intro_after_language = false;
    // Intro cinematica del capitulo (mismas escenas GameScene::Intro que el prologo,
    // pero al terminar entra a jugar). Solo la dispara la primera entrada natural.
    void start_chapter_intro(int chapter);
    // La cinematica del Ascenso (el carro de fuego): corre tras la cuarta parada, SIN
    // mundo debajo (las paradas van sobre negro), y al terminar reparte la herencia.
    void start_ascension_intro();
    CineThen cine_then = CineThen::MainMenu;

    // Room loading
    void load_room(int room_index);
    // Tile en el que debe aparecer el profeta al cargar la sala (-1 = el sitio de siempre,
    // segun `entry_direction`). Lo usan las puertas: al entrar y al salir de un interior no
    // se llega por un borde, se llega POR LA PUERTA.
    int spawn_tx = -1, spawn_ty = -1;
    void use_doorway();                         // entrar al interior o volver de el

    // --- El Ascenso (2 Reyes 2:1-14) ---------------------------------------------
    // Cuatro paradas SEGUIDAS con decision, en vez de las cinco lineas de texto que
    // habia. `ascent_step` es -1 fuera del Ascenso; 0..3 durante el viaje.
    int  ascent_step    = -1;
    int  ascent_follows = 0;      // cuantas veces le seguiste (0..3)
    bool ascent_double  = true;   // pediste la doble porcion
    void ascent_next();           // siguiente parada, o el carro de fuego

    // --- El epilogo "El Legado" (2 Reyes 9; 13:14-21) -------------------------------
    // Mismo mecanismo que el Ascenso: TRES escenas seguidas y luego la pagina de cierre.
    // Cierra los dos arcos de los profetas y nada mas (decision del usuario, 2026-08-30:
    // todo relato va anclado a Elias o Eliseo). `joash_full` recuerda si el rey golpeo
    // hasta el final, y con eso cambian las ultimas palabras del juego.
    int  epilogue_step = -1;
    bool joash_full    = true;
    void epilogue_next();

    // --- Escenas guionizadas (game/scenes.h) --------------------------------------------
    // La historia contada con el motor del gameplay: un escenario (sala del bioma, figuras
    // de pie, el profeta delante, HUD apagado) y una cadena de cajas sin decision. Empezo
    // como la audiencia con Acab (cap. 1, 2026-09-09) y desde el 2026-09-10 la tienen todos
    // los capitulos (APERTURA antes de la intro cinematica) y todos los jefes (DIALOGO al
    // entrar en la arena, con el jefe ya de pie). Sin bit en el guardado: el criterio es el
    // sitio de llamada, como las intros. Bajo autoplay se suprimen y las fuerza el comando
    // `escena <id> [1]` (parada como `chintro`, o avanzando sola con el 1). ESC salta la
    // escena entera. Al cerrarse una apertura vuelve la sala real desde room_cache y arranca
    // la intro del capitulo; al cerrarse un dialogo de jefe, a pelear.
    int  scene_id      = -1;      // Scene::*; -1 fuera de toda escena
    int  scene_step    = -1;      // caja abierta dentro de la cadena
    bool scene_manual  = false;   // abierta por el comando: la caja no avanza sola
    int  pending_scene = -1;      // dialogo de jefe pedido por load_room: arranca en el
                                  // siguiente paso de Playing, no en mitad de la transicion
    u32  boss_talk_seen = 0;      // bit por capitulo: el jefe habla una vez por partida
    // Pistas del jefe (rooms.h, get_boss_intel): el nivel que el jugador ya ha visto
    // anunciado. -1 = recien cargado: se toma el nivel actual SIN aviso (un Continuar no
    // repite lo que ya se sabia). Lo pone a -1 start_chapter.
    int  boss_intel_shown = -1;

    // La ENTRADA del jefe: mientras `boss_approach_t > 0` el motor lleva al profeta hasta
    // el sitio exacto en que la escena guionizada lo coloca, con la camara adelantandose
    // hacia el jefe, y el `pending_scene` espera. El destino es el de la escena A
    // PROPOSITO: si no coincidiera, `start_scene` teletransportaria al profeta y el corte
    // seguiria ahi, solo que dos segundos mas tarde.
    // Comando `entrada` del autoplay: deja pasar el paseo + el dialogo del jefe, que bajo
    // autoplay se suprimen (los guiones cronometran las peleas). Sin esto la entrada del
    // jefe seria lo unico del juego que ningun guion puede mirar.
    bool autoplay_boss_talk = false;
    f32  boss_approach_t    = 0.0f;
    Vec2 boss_approach_to   = {0, 0};   // = tile_to_world(anchor + def.player_dx/dy)
    Vec2 boss_approach_from = {0, 0};   // solo para el avance de la camara
    Vec2 boss_approach_eye  = {0, 0};   // el jefe: hacia donde se adelanta la camara
    // Relatos del pozo RESUELTOS en esta partida (bit = id de `namespace Ev`). No se guarda:
    // al Continuar, `event_resolved_in_run` lo lee de `event_used` de la sala, que el v9 si
    // guarda y start_chapter rehidrata en room_cache. `codex_events` no vale para esto: es
    // el Codice y persiste entre partidas.
    u64  run_events_done = 0;
    bool event_resolved_in_run(int ev) const;
    Entity scene_figures[3] = {ENTITY_NONE, ENTITY_NONE, ENTITY_NONE};   // se retiran al cerrar
    // PASEO dentro de una apertura (SceneDef::walk_after): mientras `scene_walk_t > 0` no
    // hay caja en pantalla y el motor lleva al profeta andando hasta su sitio. Misma
    // maquinaria que la entrada del jefe: se empuja con velocidad, el mundo sigue vivo y
    // la camara lo sigue. `scene_walked` es lo que evita que el paseo se repita cuando
    // `scene_next` vuelve a pasar por la misma parada al terminarlo.
    f32  scene_walk_t  = 0.0f;
    Vec2 scene_walk_to = {0, 0};   // = tile_to_world(anchor + def.player_dx/dy)
    bool scene_walked  = false;
    void start_scene(int id);
    void scene_next();
    void update_scene_walk(f32 dt);
    void finish_scene(bool skipped);

    // --- El tutorial (game/tutorial.h, 2026-09-12) --------------------------------------
    // Se ofrece al pulsar NUEVA PARTIDA y se juega en su PROPIO mapa de tres salas: no vive
    // dentro del capitulo 1 ni toca el mundo fijo de ninguno. Corre sobre GameScene::Playing
    // con `tutorial.active` puesto — el bloque de Playing son 400 lineas de combate, camara,
    // milagros y HUD que no tiene sentido duplicar — y las tres salas se montan sobre
    // `current_room_data` igual que el escenario de una escena guionizada (start_scene).
    //
    // Lo que el tutorial NO puede tocar, y como se resuelve:
    //   - el GUARDADO: `tutorial_save_backup` guarda `save.data` entera al empezar y
    //     finish_tutorial la restaura. Sale mas barato que sembrar guardas por los sitios
    //     que tocan total_kills, miracles_cast y los logros mientras se juega.
    //   - las OFRENDAS: se cuentan para su paso pero no se ingresan (guarda en el bloque de
    //     hallazgos de `update`). Jugar el tutorial no regala moneda meta.
    //   - la MUERTE: la vida se sujeta en 1 en update_tutorial, antes de que la comprobacion
    //     de muerte pueda verla en cero.
    TutorialSystem tutorial;
    SaveData tutorial_save_backup;
    void start_tutorial(TutorialThen then);
    void update_tutorial(f32 dt);      // avanza el paso; lo llama el bloque de Playing
    void tutorial_enter_room(int index);
    void tutorial_probe();                              // refresca las sondas del frame
    bool tutorial_goal_done(TutorialGoal goal, f32 dt); // ¿se ha hecho lo que pedia el paso?
    void tutorial_setup_step();                         // lo que el paso nuevo pone en el mundo
    void finish_tutorial(bool completed);  // `completed` = se cerro el paso CrossExit; si no
                                           // es un SALTO y sale sin las paginas de cierre
    void tutorial_back_to_title();     // la salida de quien lo lanzo desde Controles
    void render_tutorial_card();       // el cartel de dos lineas + el boton de SALTAR

    // --- Zona continua (2026-09-10) ---------------------------------------------------
    // Un capitulo es UN mapa: las celdas de la rejilla (40x22, la unidad de generacion pura)
    // cosidas en `current_room_data` (RoomType::Zone). La camara recorre la zona entera; los
    // enemigos de una celda nacen al acercarse y se reponen al alejarse (ALTTP); la arena
    // del jefe vive dentro de la celda B, sellada mientras el jefe vive. Los interiores
    // siguen siendo una pantalla aparte (FloorMap::room_cache[1]) y el fundido queda solo
    // para la puerta. Detalle: docs/mundo-exterior.md ("Zona continua").
    void build_pristine_zone(Room& out);              // todas las celdas, cosidas (generacion pura)
    void check_zone(const Room& zone);                // log `Zone check:` (vano, cerco, nudos, puntos, puerta)
    void update_boss_intel();                         // aviso "PISTA" al subir de nivel el rastro del jefe
    void collect_chariot_draws(std::vector<DrawCmd>& out);   // las dos capas del Carro de Fuego
    void render_sky_fire();                                  // el Fuego del Cielo cayendo
    void generate_pristine_cell(int node, Room& out); // UNA celda (o el interior), en coordenadas locales
    const Room& pristine_zone();                      // copia pristina cacheada por (semilla, capitulo)
    Room* zone_ptr();                                 // la zona: current_room_data, o su cache dentro de un interior
    const Room* zone_ptr() const;
    void place_player(Vec2 pos);                      // con la guardia de "pies en solido", salto de camara y discipulo
    void enter_cell(int cell);                        // teletransporte al nudo de la celda (goto, arranque, vuelta)
    void update_cells();                              // celda actual, activacion / despawn, arena, camara
    void enter_arena();                               // el profeta pisa la arena: jefe, sellado, banner
    void start_boss_approach(int scene);              // arma el paseo de entrada hacia el jefe
    void update_boss_approach(f32 dt);                // lo corre: mundo vivo, input tomado, camara adelantada
    void spawn_cell_enemies(int cell);
    void despawn_cell(int cell);
    void dump_cell_ascii(const Room& r, int cell);    // volcado ASCII de una celda (guiones)
    void update_room_state();    // Clear detection (por celda), boss death, tile interactions
    void reset_boss_bar();
    void update_boss_bar();      // Cada frame: suma cabeza + corro y loguea las bajas del grupo
    void open_dialogue(int event_id);           // Encuentro biblico -> GameScene::Dialogue
    void load_event_plate(int event_id);        // La estampa del relato (assets/sprites/estampas)
    void update_dialogue(f32 dt);               // Avance de lineas + eleccion
    void apply_event_choice(int event_id, int option);  // Efecto de la decision
    // Etiquetas de consecuencia de una opcion, de la misma tabla que la aplica (events.h)
    int  build_event_chips(int event_id, int option, struct EffectChip* out, int max) const;
    void open_item_choice(const char* title);      // Roll 3 item offers -> GameScene::Choice
    void open_miracle_choice(const char* title);   // Roll 3 miracle offers -> GameScene::Choice
    void update_item_choice(f32 dt);            // Navigation + confirm on the choice screen

    // Feedback
    // Exclamacion del profeta (GDD 6.4). Elige la toma segun quien juegue y NO deja que
    // dos voces se pisen: `voice_cooldown` es lo unico que separa "el profeta se queja"
    // de "el profeta no calla". Sin el, con i-frames de 1 s se oye un gemido por segundo.
    enum class Voice { Esfuerzo, Dolor, Milagro, Jefe };
    void play_voice(Voice v, f32 volume = 1.0f);
    f32 voice_cooldown = 0.0f;

    void show_banner(const char* title, const char* sub, f32 seconds = 3.0f);
    void show_message(const char* text, f32 seconds = 2.0f);
    void update_music();

    // Meta-progresion (systems/progression.h) y Santuario
    void sync_progression_from_save();
    // Vuelve a traducir lo que guarda un `tr()` de larga vida (objetos, mejoras de El
    // Santuario y el rotulo del jefe en curso). Lo llaman los DOS caminos por los que el
    // idioma puede cambiar. Detalle y por que en engine.cpp.
    void relocalize();   // save.data -> progression (arranque)
    void sync_progression_to_save();     // progression -> save.data
    // Mejoras compradas -> SaveData::items_unlocked. Idempotente: se llama al comprar y al
    // arrancar, y NUNCA quita bits (un objeto desbloqueado no se vuelve a bloquear solo).
    void apply_meta_unlocks();

    // Ajustes del jugador (GDD 4.2). Viven en SaveData (v7) porque son meta: sobreviven a
    // delete_save y al reinicio. `apply_settings_from_save` los vuelca a los subsistemas
    // que los usan (Audio, Input, i18n) y es lo unico que los aplica; el menu de Opciones
    // escribe en save.data y vuelve a llamarla.
    void apply_settings_from_save();
    void set_language(Language l);       // cambia el idioma activo y lo guarda
    void load_settings_mirror();         // SaveData -> MenuSystem::settings_*
    void commit_settings_from_mirror();  // MenuSystem::settings_* -> SaveData -> subsistemas

    // Logros (systems/achievements.h). `award` es el UNICO camino: desbloquea, avisa en
    // pantalla, suena y escribe el guardado. Las llamadas estan sembradas donde ocurre
    // cada cosa y muchas se repiten cada frame; AchievementSystem::unlock las filtra.
    void award(AchievementID id);
    void check_achievements();           // los que se comprueban por estado, no por evento
    void apply_meta_upgrades();          // Aplica lo comprado al empezar un run
    void award_offerings(bool victory);  // Ofrendas ganadas al terminar la partida
    void update_sanctuary();
    void render_sanctuary();
    void open_sanctuary(GameScene back_to);
    // Los consumibles caidos (secretos, elites, cofres, jefes): elige cual y lo planta
    void spawn_consumable_at(Vec2 pos, f32 roll);
    // La alforja: meter lo recogido, usar, cambiar de hueco. Una vez por frame en Playing.
    void update_belt();
    void use_belt_slot(int slot);
    // Dones de El Santuario que se rellenan: por intento (revives) y por capitulo
    void refill_attempt_gifts();
    void refill_chapter_gifts();
    // La primera vez que pasa algo, un cartel que lo explica (SaveData::tips_seen)
    void show_tip_once(u32 bit, const char* title, const char* body, f32 seconds = 5.0f);
    void bank_offerings(int amount, bool from_pickup);   // ingresa, guarda y anima el HUD
    // Pantalla de derrota / victoria: el desglose de ofrendas y lo que ya se puede comprar
    void render_run_summary(f32 top);
    // Equipo (pausa): todos los objetos, la alforja, las bendiciones y las cifras
    void update_equipment();
    void render_equipment();

    // Save / Settings
    void populate_save_data();
    void save_after_defeat();   // La derrota rebobina la partida al inicio del capitulo, no la borra
    // Mapas fijos + estado de sala persistido (v9):
    void capture_room_states();                  // vuelca flags + celdas alteradas por CELDA a save.data
    void apply_saved_cell_state(int cell, Room& zone);          // re-aplica lo guardado sobre una celda de la zona
    void apply_saved_interior_state(int node, Room& interior);  // idem sobre el interior recien generado
    void continue_run();
    // Caducidad movil: recalcula si "Continuar" sigue siendo continuable o ya es una
    // partida abandonada, y cobra las ofrendas de esta ultima.
    void refresh_continue_state();
    void claim_abandoned_run();
    void update_settings();

    // Debug
    void update_autoplay(f32 dt);
    void take_screenshot(const char* name);
};
