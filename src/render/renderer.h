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
    Glyph glyphs[96];                   // ASCII 32..126
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

    // Camera
    Vec2 camera_offset = {0, 0};
    Vec2 camera_shake  = {0, 0};
    f32  shake_intensity = 0.0f;
    f32  shake_timer     = 0.0f;

    // Init / Shutdown
    bool init(SDL_Renderer* renderer, SDL_Texture* render_target);
    void shutdown();

    // Texture management. smooth = linear filtering (sprites); false = nearest (tile atlas:
    // cells abut each other and linear sampling would bleed across cell edges)
    u32 load_texture(const char* path, bool smooth = true);
    u32 create_radial_texture(const char* name, int w, int h, f32 inner_a, f32 outer_a, f32 power);
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
    // Text: (x, y) = top-left in game units, line height = TEXT_LINE_UNITS * scale
    void draw_text(const char* text, f32 x, f32 y, Color color, f32 scale = 1.0f);
    void draw_text_centered(const char* text, f32 y, Color color, f32 scale = 1.0f);
    f32  measure_text(const char* text, f32 scale = 1.0f) const;   // Width in game units
    void draw_sorted(std::vector<DrawCmd>& cmds);   // Sorts by sort_y, then draws

    // Screenshot of the internal render target (PNG, 1280x720 at scale 1)
    bool save_screenshot(const char* path, int scale = 1);

    // Camera
    void set_camera(Vec2 position);
    void shake_camera(f32 intensity, f32 duration);
    void update_shake(f32 dt);
    Vec2 world_to_screen(Vec2 world_pos) const;
    Vec2 window_to_game(Vec2 window_pos) const;   // Mouse -> 320x180 game coordinates

    // Resize handling
    void handle_resize(int new_w, int new_h);

private:
    void calculate_viewport();
};
