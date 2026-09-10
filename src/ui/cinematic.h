#pragma once

// =============================================================================
// MANTLE - Prologo cinematico "El Cielo Cerrado"
// =============================================================================
//
// Lo que se ve al arrancar el juego, antes del menu. Ocho estampas renderizadas en
// Blender (tools/blender/mantle_sprites/cine.py), cada una partida en tres capas
// transparentes que se mueven a distinta velocidad:
//
//     L0 lejos   siluetas del fondo      parallax ~0.35
//     L1 medio   la escena de verdad     parallax  1.00
//     L2 cerca   encuadre en sombra      parallax ~1.55
//
// El CIELO no viene de Blender: se pinta aqui como degradado vertical. A 320x180 el
// bandeado no se ve, no gasta textura, y se puede tenir por escena (asi se cierra el
// cielo sobre Israel en la escena 6).
//
// Los paneles miden 1600x720 px = 400x180 unidades a ASSET_SCALE 4, o sea 80 unidades
// de holgura sobre las 320 de pantalla para el paneo.
//
// NO es una extension de StorySystem: aquella es una pila de paginas de texto que
// avanza con ENTER, sin reloj ni imagen. Esta va por tiempo.
//
// =============================================================================

#include <string>
#include "render/renderer.h"
#include "input/input.h"

// Ancho/alto de un panel en unidades de juego (1600x720 px / ASSET_SCALE)
constexpr f32 CINE_PANEL_W = 400.0f;
constexpr f32 CINE_PANEL_H = 180.0f;
constexpr int CINE_LAYERS  = 3;

// Efectos que se pintan por encima de las capas (bitmask)
enum CineFx : u8 {
    CineFx_None    = 0,
    CineFx_Shaft   = 1 << 0,   // El haz de luz que baja sobre Elias (escena 5)
    CineFx_Flash   = 1 << 1,   // Fogonazo blanco: "ni rocio ni lluvia" (escena 6)
    CineFx_Shake   = 1 << 2,   // Temblor (acompana al fogonazo)
    CineFx_Embers  = 1 << 3,   // Pavesas subiendo (escena 8)
    CineFx_Torches = 1 << 4,   // Parpadeo calido de las antorchas (escena 4)
    CineFx_Title   = 1 << 5,   // El titulo del juego
};

struct CineLayer {
    u32   tex      = TEXTURE_NONE;
    f32   parallax = 1.0f;
    Color tint     = Color::white();
};

struct CineScene {
    const char* art = nullptr;         // Base: "sprites/cine/01_tierra" (+ "_L0.png")
    CineLayer   layers[CINE_LAYERS];
    int         layer_count = 0;

    f32   duration  = 10.0f;
    f32   pan_from  = 0.0f, pan_to  = 0.0f;   // Unidades de juego (la capa cerca lo multiplica)
    f32   zoom_from = 1.0f, zoom_to = 1.05f;

    Color sky_top    = {26, 22, 44, 255};
    Color sky_bottom = {84, 66, 92, 255};

    const char* lines[3]  = {nullptr, nullptr, nullptr};
    f32         line_at[3] = {0.7f, 3.4f, 6.1f};

    u8  fx    = CineFx_None;
    f32 fx_at = 0.0f;

    // Rotulo bajo CineFx_Title. Vacio = el titulo del juego (prologo); con texto, el
    // rotulo del capitulo ("CAPÍTULO 3" / "El Monte Carmelo") en el color de su acto.
    std::string title;
    std::string subtitle;
    Color       title_color = {247, 226, 168, 255};
};

struct CinematicSystem {
    static constexpr int MAX_SCENES = 8;
    static constexpr int MAX_LINES  = 8;    // Las 3 frases pueden partirse al envolver
    static constexpr f32 FADE       = 0.65f;
    static constexpr f32 LINE_FADE  = 0.55f;
    static constexpr f32 TEXT_SCALE = 0.85f;

    CineScene scenes[MAX_SCENES];
    int  scene_count = 0;
    int  scene       = 0;
    f32  t           = 0.0f;    // Tiempo dentro de la escena actual
    f32  clock       = 0.0f;    // Reloj propio: el mundo no existe todavia
    bool active      = false;
    bool art_loaded  = false;
    f32  auto_timer  = 0.0f;    // > 0 (autoplay): pasa de escena solo

    // Cache de la escena actual (se rellena en layout: wrap_text asigna memoria)
    std::string wrapped[MAX_LINES];
    int         wrap_src[MAX_LINES] = {};   // Frase original de la que sale cada linea
    int         wrap_count = 0;

    void begin();
    void add(const CineScene& s);
    void load_art(Renderer& r);                  // Carga las texturas de todas las capas
    void unload_art(Renderer& r);                // Libera las texturas (intros de capitulo)
    void layout(const Renderer& r);              // Envuelve el texto de la escena actual
    void go_to(int index, const Renderer& r);
    bool update(Input& in, f32 dt, const Renderer& r);   // true = terminada
    void render(Renderer& r) const;

    const CineScene& current() const { return scenes[scene]; }
};

// Monta las 8 escenas del prologo desde las tablas de game/lore.h
void cinematic_prologue(CinematicSystem& c);

// Intro de capitulo: DOS escenas sobre la MISMA estampa (versiculo + narracion + objetivo
// de get_chapter_lore). El arte se entrega por fases: sin estampa todavia
// (cinematic_chapter_ready false), el Engine cae a la carta de texto de siempre.
void cinematic_chapter(CinematicSystem& c, int chapter);
bool cinematic_chapter_ready(int chapter);

// El Ascenso (2 Reyes 2:8-14): SEIS escenas sobre CUATRO estampas — el Jordan abierto,
// el carro de fuego, el torbellino y el manto caido. El texto sale integro de
// get_ascension_lore (las mismas tres paginas que leia StorySystem); cuando la
// cinematica se ve, esas paginas NO se leen despues (precedente del prologo: no se
// repite lo recien visto). Sin estampas entregadas, el Engine cae al texto de siempre.
// `asked_double` = Engine::ascent_double: si Eliseo no pidio la doble porcion, ni la
// peticion ni la herencia pueden decir que la pidio (2 Reyes 2:10).
void cinematic_ascension(CinematicSystem& c, bool asked_double = true);
bool cinematic_ascension_ready();
