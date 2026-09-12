#pragma once

// =============================================================================
// MANTLE - 2D Renderer (SDL3 based)
// =============================================================================
//
// All drawing code works in game units (320x180). Frames are rendered to a
// 1280x720 target (Config::RENDER_SCALE px per unit, applied with
// SDL_SetRenderScale) so sprites/text are smooth and sub-unit positions are
// real pixels; the target is then letterboxed into the window.
//
// =============================================================================

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <vector>
#include <string>
#include <unordered_map>
#include "core/types.h"
#include "core/config.h"

// =============================================================================
// Texture Registry
// =============================================================================
struct TextureEntry {
    SDL_Texture* texture = nullptr;
    int width  = 0;
    int height = 0;
};

// =============================================================================
// Deferred draw command (world space). Sorted by sort_y for 3/4-view depth:
// props and characters are drawn back-to-front by their ground contact line.
// =============================================================================
struct DrawCmd {
    f32   sort_y     = 0.0f;
    u32   texture_id = TEXTURE_NONE;   // TEXTURE_NONE = filled rect (placeholder)
    Rect  src        = {0, 0, 0, 0};
    Rect  dst        = {0, 0, 0, 0};   // World-space destination
    Color tint       = Color::white();
    f32   rotation   = 0.0f;           // Degrees, around the dst centre
    bool  flip_x     = false;
    bool  outline    = false;          // Placeholder rect: draw a light outline too
};

// =============================================================================
// Baked charset. Printable ASCII (32..126) plus the Latin-1 / punctuation codepoints
// Spanish needs: the game text quotes Reina-Valera, so tildes and enye are not optional.
// Strings in the sources are UTF-8 (MSVC needs /utf-8, see CMakeLists.txt).
// =============================================================================
constexpr int GLYPH_ASCII_COUNT = 95;   // 32..126

inline constexpr u32 GLYPH_EXTRA[] = {
    0x00A1, 0x00BF,                                           // ¡ ¿
    0x00C1, 0x00C9, 0x00CD, 0x00D3, 0x00DA, 0x00DC, 0x00D1,   // Á É Í Ó Ú Ü Ñ
    0x00E1, 0x00E9, 0x00ED, 0x00F3, 0x00FA, 0x00FC, 0x00F1,   // á é í ó ú ü ñ
    0x00AB, 0x00BB, 0x00AA, 0x00BA, 0x00B7,                   // « » ª º ·
    0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2026,           // — ‘ ’ “ ” …
};
constexpr int GLYPH_EXTRA_COUNT = static_cast<int>(sizeof(GLYPH_EXTRA) / sizeof(GLYPH_EXTRA[0]));
constexpr int GLYPH_COUNT       = GLYPH_ASCII_COUNT + GLYPH_EXTRA_COUNT;

// Codepoint -> slot in FontAtlas::glyphs (-1 = not part of the baked charset)
inline int glyph_index(u32 cp) {
    if (cp >= 32 && cp <= 126) return static_cast<int>(cp) - 32;
    for (int i = 0; i < GLYPH_EXTRA_COUNT; i++) {
        if (GLYPH_EXTRA[i] == cp) return GLYPH_ASCII_COUNT + i;
    }
    return -1;
}

// Slot -> codepoint (the order bake_atlas walks)
inline u32 glyph_codepoint(int slot) {
    return (slot < GLYPH_ASCII_COUNT) ? static_cast<u32>(slot + 32)
                                      : GLYPH_EXTRA[slot - GLYPH_ASCII_COUNT];
}

// Decodes one UTF-8 codepoint and advances `p` past it. Malformed bytes are consumed one at
// a time and reported as 0 so callers just skip them.
inline u32 utf8_next(const char*& p) {
    const unsigned char c0 = static_cast<unsigned char>(*p);
    if (c0 < 0x80) { p++; return c0; }
    auto cont = [](const char* q) { return (static_cast<unsigned char>(*q) & 0xC0) == 0x80; };
    auto bits = [](const char* q) { return static_cast<u32>(static_cast<unsigned char>(*q) & 0x3F); };
    if ((c0 & 0xE0) == 0xC0 && cont(p + 1)) {
        u32 cp = ((c0 & 0x1Fu) << 6) | bits(p + 1);
        p += 2; return cp;
    }
    if ((c0 & 0xF0) == 0xE0 && cont(p + 1) && cont(p + 2)) {
        u32 cp = ((c0 & 0x0Fu) << 12) | (bits(p + 1) << 6) | bits(p + 2);
        p += 3; return cp;
    }
    if ((c0 & 0xF8) == 0xF0 && cont(p + 1) && cont(p + 2) && cont(p + 3)) {
        u32 cp = ((c0 & 0x07u) << 18) | (bits(p + 1) << 12) | (bits(p + 2) << 6) | bits(p + 3);
        p += 4; return cp;
    }
    p++;
    return 0;
}

// =============================================================================
// Bitmap font baked from a TTF at init (SDL3_ttf is only used while baking).
// Glyphs are drawn as quads from one atlas texture: no per-frame allocations.
// =============================================================================
struct Glyph {
    Rect src     = {0, 0, 0, 0};   // Fill atlas texels
    Rect src_out = {0, 0, 0, 0};   // Outline atlas texels (same glyph, bigger by `outline` px each side)
    f32  advance = 0.0f;           // Pen advance in atlas pixels
    f32  xoff    = 0.0f;           // Left bearing correction (atlas pixels, <= 0)
};

struct FontAtlas {
    u32   texture     = TEXTURE_NONE;   // Fill glyphs (white)
    u32   texture_out = TEXTURE_NONE;   // Outline glyphs (white)
    Glyph glyphs[GLYPH_COUNT];          // Indexed by glyph_index(codepoint)
    f32   px      = 0.0f;               // Line height in atlas pixels
    f32   outline = 0.0f;               // Outline thickness in atlas pixels
    bool  valid   = false;
};

// =============================================================================
// Renderer
// =============================================================================
struct Renderer {
    SDL_Renderer* sdl      = nullptr;
    SDL_Texture*  target   = nullptr;  // (GAME_WIDTH*RENDER_SCALE) x (GAME_HEIGHT*RENDER_SCALE) render target

    // Texture storage
    std::vector<TextureEntry> textures;
    std::unordered_map<std::string, u32> texture_map;  // path -> index

    // Procedural helper textures (created in init)
    u32 shadow_tex   = TEXTURE_NONE;   // 64x32 soft elliptical blob (white, alpha 1 -> 0)
    u32 vignette_tex = TEXTURE_NONE;   // 160x90 radial gradient (alpha 0 centre -> 1 edge)
    // Panel redondeado BLANCO para servir en 9 trozos (draw_panel). Es lo que da botones y
    // marcos con esquina redonda y bisel sin meter un PNG en assets/: la regla es que
    // assets/ no crece si se puede evitar, y esto se genera en init como shadow/vignette.
    u32 panel_tex    = TEXTURE_NONE;   // PANEL_TEX_SIZE^2, esquina = PANEL_TEX_SIZE*PANEL_CORNER_FRAC
    static constexpr int PANEL_TEX_SIZE   = 48;
    static constexpr f32 PANEL_CORNER_FRAC = 14.0f / 48.0f;

    // Text: TTF atlases at two sizes (small for scale <= 1.25, large for titles). If the
    // font is missing, draw_text falls back to SDL_RenderDebugText (8x8 bitmap font).
    static constexpr f32 TEXT_LINE_UNITS = 8.0f;   // Line height in game units at scale 1 (matches the debug font)
    FontAtlas font_small, font_large;
    bool has_font = false;
    bool load_font(const char* path);
    bool bake_atlas(FontAtlas& atlas, const char* full_path, f32 ptsize, int outline_px);

    // Viewport / scaling
    int window_w = Config::WINDOW_WIDTH;
    int window_h = Config::WINDOW_HEIGHT;
    SDL_FRect viewport = {};  // Where the game renders within the window (letterbox)

    // Desplazamiento extra que se suma a TODO lo que se dibuja (world_to_screen). Vale
    // cero salvo durante la pasada de superposicion en vertical, ver begin_overlay().
    Vec2 ui_offset = {0, 0};
    bool overlay_active = false;

    // Camera
    Vec2 camera_offset = {0, 0};
    Vec2 camera_shake  = {0, 0};
    f32  shake_intensity = 0.0f;
    f32  shake_timer     = 0.0f;
    f32  shake_duration  = 0.0f;   // Para la envolvente: la sacudida se apaga, no se corta
    f32  shake_freq      = 45.0f;  // rad/s (grave = estallido, agudo = golpe seco)
    f32  shake_phase     = 0.0f;

    // Init / Shutdown
    bool init(SDL_Renderer* renderer, SDL_Texture* render_target);
    void shutdown();

    // Texture management. smooth = linear filtering (sprites); false = nearest (tile atlas:
    // cells abut each other and linear sampling would bleed across cell edges)
    u32 load_texture(const char* path, bool smooth = true);
    // Libera una textura cargada (intros de capitulo: se ven una vez por partida y
    // dejarlas en la cache acumula VRAM en movil). El id queda como hueco estable en
    // `textures` (puntero a nullptr: los draws ya lo toleran); recargar el path crea
    // una entrada nueva. Path desconocido = no-op.
    void unload_texture(const char* path);
    u32 create_radial_texture(const char* name, int w, int h, f32 inner_a, f32 outer_a, f32 power);
    // Textura desde un buffer RGBA en CPU. Si `name` ya existe se REEMPLAZA el contenido y
    // se devuelve el MISMO id: el mapa del capitulo se regenera cada vez que cambia la
    // niebla y con una entrada nueva por regeneracion se irian acumulando texturas.
    u32 create_texture_from_rgba(const char* name, int w, int h, const u8* rgba, bool smooth = true);
    u32 create_panel_texture(const char* name, int size, int radius);
    TextureEntry* get_texture(u32 id);

    // Frame lifecycle
    void begin_frame();
    void end_frame();

    // Drawing primitives
    void clear(Color color);
    void draw_sprite(u32 texture_id, Rect src, Vec2 position,
                     Vec2 scale = {1, 1}, f32 rotation = 0.0f,
                     bool flip_x = false, Color tint = Color::white());
    void draw_sprite_rect(u32 texture_id, Rect src, Rect dst_world,
                          Color tint = Color::white(), bool flip_x = false, f32 rotation = 0.0f);
    void draw_rect(Rect rect, Color color, bool filled = true);
    void draw_rect_outline(Rect rect, Color color);
    // Panel/boton redondeado en 9 trozos a partir de `panel_tex`. El tinte MULTIPLICA, asi
    // que el bisel (claro arriba, oscuro abajo) y el contorno sobreviven a cualquier color.
    // `corner` en unidades de juego; se acota a la mitad del lado menor.
    void draw_panel(Rect dst, Color tint, f32 corner = 5.0f);
    // Text: (x, y) = top-left in game units, line height = TEXT_LINE_UNITS * scale
    void draw_text(const char* text, f32 x, f32 y, Color color, f32 scale = 1.0f);
    void draw_text_centered(const char* text, f32 y, Color color, f32 scale = 1.0f);
    f32  measure_text(const char* text, f32 scale = 1.0f) const;   // Width in game units
    // Greedy word wrap into `out` (caller-owned, `max_lines` entries). Returns the line count.
    // Wrap once when the text changes, not every frame: it allocates.
    int  wrap_text(const char* text, f32 max_w, f32 scale, std::string* out, int max_lines) const;
    void draw_sorted(std::vector<DrawCmd>& cmds);   // Sorts by sort_y, then draws

    // Screenshot of the internal render target (PNG, 1280x720 at scale 1)
    bool save_screenshot(const char* path, int scale = 1);

    // Camera
    void set_camera(Vec2 position);
    // Pasada de MUNDO: entre estas dos, todo lo que se dibuje se desplaza con la camara.
    // Hace falta acotarla porque `world_to_screen` la aplica a TODO — `draw_rect`,
    // `draw_text` y por tanto el HUD entero —, asi que sin cerrarla el HUD se iria con el
    // profeta. Mismo patron que begin_overlay/end_overlay. La SACUDIDA se deja fuera a
    // proposito: sigue moviendo tambien el HUD, como hasta ahora, y el tacto ya medido no
    // se toca.
    void begin_world(Vec2 camera_center) { set_camera(camera_center); }
    void end_world() { camera_offset = {0, 0}; }
    // Una sacudida NO pisa a otra mas fuerte que siga viva: antes, el golpe siguiente
    // mataba la sacudida del jefe recien muerto. Ver Config::Feel::Shake para las curvas.
    void shake_camera(f32 intensity, f32 duration, f32 frequency = 45.0f);
    void shake_camera(const Config::Feel::Shake& s) { shake_camera(s.mag, s.dur, s.freq); }
    void update_shake(f32 dt);
    Vec2 world_to_screen(Vec2 world_pos) const;
    Vec2 window_to_game(Vec2 window_pos) const;   // Mouse -> 320x180 game coordinates

    // El viewport en FRACCIONES de ventana (0..1). Es lo que necesita el tactil: los
    // eventos de dedo de SDL vienen normalizados a la ventana entera, no al area de
    // juego, asi que sin esto un dedo en el borde del mundo se lee dentro del pilar negro.
    // Mismo sistema de coordenadas que `window_to_game`, expresado sin pixeles.
    Rect viewport_norm() const {
        f32 ww = static_cast<f32>(window_w), wh = static_cast<f32>(window_h);
        if (ww <= 0.0f || wh <= 0.0f || viewport.w <= 0.0f || viewport.h <= 0.0f)
            return {0.0f, 0.0f, 1.0f, 1.0f};
        return {viewport.x / ww, viewport.y / wh, viewport.w / ww, viewport.h / wh};
    }

    // Resize handling
    void handle_resize(int new_w, int new_h);

private:
    void calculate_viewport();
public:
    // --- Vertical (GDD 4.5: "vertical opcional", 1080x1920) --------------------
    // El letterbox ya centra CUALQUIER proporcion de ventana, asi que en vertical el area
    // de juego se ve entera sin tocar nada: lo que cambia es DONDE caben los controles.
    // El area de juego sigue siendo 320x180 a proposito — recortar el mundo visible
    // cambiaria el juego, no la presentacion.
    bool is_portrait() const { return window_h > window_w; }

    // Unidades de juego de banda negra que quedan por DEBAJO del area de juego. 0 en
    // apaisado. Es lo que decide si los controles tactiles caben fuera del mundo.
    f32 bottom_band_units() const {
        if (viewport.w <= 0.0f) return 0.0f;
        f32 s = viewport.w / static_cast<f32>(Config::GAME_WIDTH);
        if (s <= 0.0f) return 0.0f;
        return (static_cast<f32>(window_h) - viewport.y - viewport.h) / s;
    }

    // Pasada de superposicion: dibuja SOBRE la ventana entera en vez de dentro del
    // objetivo de 320x180, para poder pintar en las bandas negras. Las coordenadas siguen
    // siendo unidades de juego y el origen sigue siendo la esquina del area de juego, asi
    // que un boton en y=190 cae 10 unidades por debajo del mundo — que es exactamente lo
    // que devuelve `window_to_game` para un dedo apoyado ahi. Los dos lados usan el mismo
    // sistema de coordenadas y por eso el hit-test no necesita saber nada de esto.
    void present();          // SDL_RenderPresent; va DESPUES de la superposicion
    void begin_overlay();
    void end_overlay();
    bool in_overlay() const { return overlay_active; }
private:
};
