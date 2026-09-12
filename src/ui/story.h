#pragma once

// =============================================================================
// MANTLE - Pantallas de historia (prologo, capitulo, ascension, epilogo)
// =============================================================================
//
// Una pila de paginas de texto sobre fondo oscuro. Las lineas entran escalonadas
// (fade + deslizamiento) en vez de con maquina de escribir: no hay que cortar
// cadenas UTF-8 a mitad de codepoint y se lee mejor para una cita larga.
//
// El texto se envuelve UNA vez por pagina (layout) y se cachea en `wrapped`.
//
// =============================================================================

#include <string>
#include "render/renderer.h"
#include "input/input.h"
#include "game/lore.h"

// Que hacer cuando la secuencia termina (lo resuelve Engine::finish_story)
enum class StoryThen : u8 {
    Play,              // Prologo / carta de capitulo -> jugar
    AdvanceChapter,    // Outro -> siguiente capitulo / ascension / victoria
    FinishAscension,   // El manto cae -> convertirse en Eliseo
    VictoryMenu        // Epilogo -> pantalla final
};

struct StoryPage {
    // title/subtitle son std::string porque llevan el numero de capitulo formateado;
    // el resto apunta a los literales estaticos de lore.h.
    std::string title;                 // "CAPÍTULO 1", "EL ASCENSO"
    std::string subtitle;              // "El Arroyo Querit"
    Scripture   verse    = {nullptr, nullptr};
    const char* body[4]  = {nullptr, nullptr, nullptr, nullptr};
    const char* footer   = nullptr;    // Objetivo del capitulo
    Color       accent   = {235, 205, 130, 255};
};

struct StorySystem {
    static constexpr int MAX_PAGES = 4;
    static constexpr int MAX_LINES = 16;

    // Ritmo de entrada: la cabecera entra primero, luego una linea cada LINE_STAGGER
    static constexpr f32 LINE_DELAY0  = 0.30f;   // Retardo de la primera linea de texto
    static constexpr f32 LINE_STAGGER = 0.28f;   // Retardo entre lineas
    static constexpr f32 LINE_FADE    = 0.45f;   // Duracion del fundido de cada linea

    StoryPage pages[MAX_PAGES];
    int  page_count = 0;
    int  page       = 0;
    f32  timer      = 0.0f;      // Tiempo dentro de la pagina actual (entrada escalonada)
    f32  clock      = 0.0f;      // Reloj propio: el mundo esta congelado durante la historia
    bool active     = false;
    f32  auto_timer = 0.0f;     // > 0 (autoplay): pasa de pagina solo
    // Lo escribe el Engine antes de renderizar (igual que CinematicSystem::touch_ui): en
    // tactil el pie habla de dedos y se pinta el boton de SALTAR.
    bool touch_ui   = false;
    StoryThen then  = StoryThen::Play;

    // Cache de la pagina actual (se rellena en layout)
    enum LineKind : u8 { Verse, Body, Footer };
    std::string wrapped[MAX_LINES];
    LineKind    kinds[MAX_LINES] = {};
    int         line_count = 0;

    void begin();                                          // Limpia las paginas
    void add(const StoryPage& p);                          // Encola una pagina (hasta MAX_PAGES)
    void layout(const Renderer& renderer);                 // Envuelve la pagina actual
    bool update(Input& input, f32 dt, const Renderer& renderer);  // true = secuencia terminada
    void render(Renderer& renderer) const;

    f32  reveal_end() const { return LINE_DELAY0 + line_count * LINE_STAGGER + LINE_FADE; }
    bool all_revealed() const { return timer >= reveal_end(); }
};

// Construye las paginas a partir de las tablas de lore.h
void story_prologue(StorySystem& s);
void story_chapter_card(StorySystem& s, int chapter);
void story_chapter_outro(StorySystem& s, int chapter);
// `asked_double` = Engine::ascent_double (la cuarta parada): cambia la peticion de la
// pagina 0 y la linea de la herencia (sin doble porcion no hay milagro heredado).
void story_ascension(StorySystem& s, bool asked_double = true);
// `joash_full` = el rey golpeo la tierra hasta que le mandaron parar (2 Reyes 13:19).
// Es lo unico que cambia las ultimas palabras del juego, y en ese pasaje es
// exactamente lo que estaba en juego.
void story_epilogue(StorySystem& s, bool joash_full = true);
