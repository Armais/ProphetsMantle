#pragma once

// =============================================================================
// MANTLE - HUD (In-Game Overlay) (GDD 4.2)
// =============================================================================
//
// Layout:
//   Top-left:     HP / FE bars + faith level name
//   Top-right:    Minimap (rooms visited, current, boss marked) + objective hint
//   Bottom-right: Miracle slots
//   Bottom-center: Boss health bar (boss rooms) / transient messages
//   Center:       Chapter banner
//
// =============================================================================

#include "render/renderer.h"
#include "render/sprites.h"   // namespace Portrait
#include "game/faith.h"
#include "game/player.h"
#include "game/rooms.h"
#include "game/items.h"
#include "game/lore.h"
#include "core/ecs.h"
#include "input/input.h"   // TouchButton: en movil el HUD es tambien el mando

struct HUDContext {
    const FloorMap* floor_map = nullptr;
    const Room*     room      = nullptr;   // la activa: la zona, o el interior
    // La ZONA del capitulo (Engine::zone_ptr): el minimapa lee visited/cleared de sus
    // celdas. Dentro de un interior sigue siendo la zona de fuera; null fuera de partida.
    const Room*     zone      = nullptr;
    // Barra del jefe. La rellena el Engine (update_boss_bar): con un jefe solo es su vida;
    // con el jefe en GRUPO (cap. 3, el corro de Baal) es la suma de la cabeza y del corro,
    // el maximo se fija al nacer el grupo y la barra SIGUE mientras quede alguien aunque
    // la cabeza haya caido. `boss` sigue siendo la cabeza (auto-mira, escena Live).
    Entity          boss      = ENTITY_NONE;
    f32             boss_hp   = 0.0f;
    f32             boss_hp_max = 0.0f;        // 0 = sin barra
    int             boss_heads = 0;            // vivos (cabeza + miembros)
    bool            boss_group = false;        // pinta una marca por cabeza viva bajo la barra
    bool            boss_invulnerable = false;
    const char*     boss_name = nullptr;
    int             chapter   = 1;
    const char*     banner    = nullptr;   // Big centred title (chapter start)
    const char*     subbanner = nullptr;   // Smaller line under the banner
    f32             banner_t  = 0.0f;      // Seconds remaining
    const char*     message   = nullptr;   // Small transient message
    f32             message_t = 0.0f;
    f32             time      = 0.0f;
    // Donde esta el profeta, en coordenadas de ZONA. El mapa grande ya no pinta celdas
    // sino el terreno, asi que la marca del jugador es un punto, no una casilla encendida.
    // `player_in_zone` es false dentro de un interior (ahi la posicion es del interior y
    // no significa nada en el mapa del capitulo): entonces se marca su celda de fuera.
    Vec2            player_pos = {0, 0};
    bool            player_in_zone = false;
    bool            show_map  = false;     // Tab held: big map
    // Se esta jugando el TUTORIAL (game/tutorial.h). Lo que pinta el minimapa entonces es el
    // capitulo 1, que es el que se va a jugar al salir, y NO la sala del tutorial: la linea
    // de objetivo lo dice en vez de dar una direccion de jefe que aqui no significa nada.
    bool            tutorial  = false;
    bool            map_revealed = false;  // "Ojos Abiertos": todo el capitulo a la vista
    u32             tileset   = TEXTURE_NONE;
    const ItemRegistry* registry = nullptr;   // Needed to draw the inventory icons
    // Controles tactiles: null o `touch_ui` apagado = no se dibuja ninguno y en
    // escritorio el HUD queda exactamente igual que antes.
    const Input*        input    = nullptr;
};

// Choice overlay (GameScene::Choice): up to 3 cards drawn over the frozen world.
// Items and miracles both feed the same card renderer.
struct ChoiceCard {
    const char* name  = "";
    const char* tag   = "";      // rarity / tier line, in `accent`
    const char* extra = "";      // "HEREDABLE", "Sustituye a X"... (may be empty)
    const char* desc  = "";
    Color accent      = {200, 195, 185, 255};
    TileSprite icon   = TileSprite::Star;
};

// `touch`: la linea de ayuda del pie habla de dedos en vez de teclas.
void draw_choice_cards(Renderer& renderer, const char* title, const ChoiceCard* cards,
                       int count, int selected, u32 tileset, bool touch = false);
// Rect de la carta i, en unidades de juego. Lo usan el DIBUJO y el hit-test tactil: una
// sola formula, para que tocar una carta no dependa de recordar el reparto.
Rect choice_card_rect(int i, int count);
// `defs[i]` may be null if an offer could not be rolled.
// `act_one`: en el Acto II ya no hay a quien heredar, asi que la etiqueta se calla.
// `held_relic`: nombre de la reliquia que se lleva puesta, o nullptr. Las reliquias van a
// UN SOLO slot (GDD 7.3), asi que la carta tiene que decir a cual sustituye: sin eso, el
// jugador toma la segunda sin saber que pierde la primera.
void draw_item_choice(Renderer& renderer, const char* title, const ItemDef* const defs[3],
                      int count, int selected, u32 tileset, bool act_one = true,
                      const char* held_relic = nullptr, bool touch = false);
// `extras[i]` is an optional line per card ("Sustituye a Cuervos de Querit").
void draw_miracle_choice(Renderer& renderer, const char* title, const MiracleID* ids, int count,
                         int selected, u32 tileset, const char* const* extras,
                         bool touch = false);

// Encuentro biblico (GameScene::Dialogue): caja inferior sobre el mundo congelado.
// `body` es el texto ya elegido por el Engine (linea de dialogo o resultado).
// Con `option_count` > 0 se pintan las opciones; `selected` marca la activa.
struct DialogueView {
    const char* speaker = "";
    int  portrait = Portrait::NONE;             // indice en portraits.png, -1 = sin cara
    const char* body    = "";
    const char* options[2] = {nullptr, nullptr};
    const char* costs[2]   = {nullptr, nullptr};
    int  option_count = 0;
    int  selected     = 0;
    bool more_lines   = false;                  // Quedan lineas: el pie dice "seguir"
    bool touch        = false;                  // pie con gestos en vez de teclas
    // ¿Se puede saltar TODA la escena desde aqui? Solo una escena guionizada
    // (Engine::scene_step >= 0). Un encuentro del pozo no se salta, y si el pie dijera
    // que si, el boton (o el [ESC]) mentiria.
    bool skippable    = false;
    bool skip_pressed = false;   // dedo apoyado en SALTAR: el boton se enciende
};
void draw_dialogue(Renderer& renderer, const DialogueView& view, u32 tileset, f32 time,
                   u32 portraits = TEXTURE_NONE);

// =============================================================================
// Boton de SALTAR (solo tactil)
// =============================================================================
// Un telefono por gestos no tiene boton atras, y el toque en estas pantallas siempre
// significaba "seguir": no habia forma de saltarse el prologo, una intro de capitulo ni
// una apertura guionizada. Lo pintan CinematicSystem::render, StorySystem::render y
// draw_dialogue; lo atiende el Engine (Intro / Story / Dialogue). Rect y dibujo salen de
// la MISMA formula, que es el contrato de todo control tactil del proyecto, y NO se mueve
// entre pantallas: el mismo sitio siempre, como la fila de accion del HUD.
Rect skip_button_rect();
// El del tutorial: mismo boton, otro sitio (el de siempre pisa el minimapa, y en el
// tutorial SI hay HUD debajo). Ver el comentario en hud.cpp.
Rect tutorial_skip_rect();
void draw_skip_button(Renderer& renderer, f32 time, bool pressed = false);
void draw_skip_button_at(Renderer& renderer, Rect r, f32 time, bool pressed = false);
// Idem para las opciones del encuentro (y del Ascenso, que usa la misma pantalla). El
// alto de la caja depende de cuantas lineas ocupe el texto, asi que el rect no se puede
// recalcular a ciegas: lo apunta draw_dialogue y esto lo devuelve. Rect vacio si esa
// opcion no se ha pintado.
Rect dialogue_option_rect(int i);

struct HUD {
    bool visible = true;

    void render(Renderer& renderer, const Player& player,
                const ECSWorld& world, const FaithSystem& faith, const HUDContext& ctx);

    // Publica a proposito: en VERTICAL la llama Engine::render por separado, despues del
    // letterbox, para pintar los botones en la banda negra (Renderer::begin_overlay).
    void draw_touch_controls(Renderer& renderer, const Player& player, const HUDContext& ctx);
    // Idem: en vertical las celdas de milagro se pintan en la banda, fuera del mundo.
    void draw_miracle_slots(Renderer& renderer, const Player& player, const HUDContext& ctx,
                            const FaithSystem& faith);
    // Rect del minimapa pequeno. Publico porque el Engine se lo pasa a Input para que
    // tocarlo abra el mapa grande: dibujo y hit-test salen de la MISMA formula.
    static Rect minimap_rect(const FloorMap& fm);

private:
    void draw_health_bar(Renderer& renderer, f32 current, f32 max, const HUDContext& ctx);
    void draw_faith_bar(Renderer& renderer, const FaithSystem& faith, const HUDContext& ctx);
    void draw_minimap(Renderer& renderer, const HUDContext& ctx, bool big);
    void draw_boss_bar(Renderer& renderer, const HUDContext& ctx);
    void draw_banner(Renderer& renderer, const HUDContext& ctx);
    void draw_message(Renderer& renderer, const HUDContext& ctx);
    void draw_objective(Renderer& renderer, const HUDContext& ctx);
    void draw_inventory(Renderer& renderer, const Player& player, const HUDContext& ctx);
};
