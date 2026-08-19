// =============================================================================
// MANTLE - 2D Renderer Implementation
// =============================================================================

#include "render/renderer.h"
#include "platform/platform.h"
#include <SDL3_ttf/SDL_ttf.h>
#include <algorithm>
#include <cmath>
#include <cstring>

bool Renderer::init(SDL_Renderer* renderer, SDL_Texture* render_target) {
    sdl = renderer;
    target = render_target;

    // Get initial window size
    SDL_Window* win = SDL_GetRenderWindow(sdl);
    if (win) {
        SDL_GetWindowSize(win, &window_w, &window_h);
    }

    calculate_viewport();

    // Procedural helper textures (soft shadow blob, screen vignette)
    shadow_tex   = create_radial_texture("@shadow",   64, 32, 1.0f, 0.0f, 1.6f);
    vignette_tex = create_radial_texture("@vignette", 160, 90, 0.0f, 1.0f, 2.5f);

    // UI font (falls back to the debug bitmap font if missing)
    has_font = load_font("fonts/LilitaOne-Regular.ttf");

    SDL_Log("Renderer initialized: viewport %.0fx%.0f at (%.0f, %.0f), font %s",
            viewport.w, viewport.h, viewport.x, viewport.y, has_font ? "TTF" : "debug");
    return true;
}

// =============================================================================
// Font atlas (SDL3_ttf -> one fill atlas + one outline atlas per size)
// =============================================================================
bool Renderer::load_font(const char* path) {
    if (!TTF_Init()) {
        SDL_Log("TTF_Init failed: %s", SDL_GetError());
        return false;
    }
    std::string full = std::string(get_asset_path()) + path;
    // Small: 8 game units per line at scale 1 = 32 px on the 4x target. Large: 2x for titles.
    const f32 px_small = Renderer::TEXT_LINE_UNITS * Config::RENDER_SCALE;          // 32
    bool ok_s = bake_atlas(font_small, full.c_str(), px_small * 0.80f, 2);
    bool ok_l = bake_atlas(font_large, full.c_str(), px_small * 2.0f * 0.80f, 3);
    if (!ok_s) SDL_Log("Font '%s' not available, using debug text", path);
    return ok_s && ok_l;
}

bool Renderer::bake_atlas(FontAtlas& atlas, const char* full_path, f32 ptsize, int outline_px) {
    TTF_Font* font = TTF_OpenFont(full_path, ptsize);
    if (!font) {
        SDL_Log("TTF_OpenFont('%s') failed: %s", full_path, SDL_GetError());
        return false;
    }
    TTF_SetFontHinting(font, TTF_HINTING_LIGHT);
    const int line_h = TTF_GetFontHeight(font);
    const int cell_h = line_h + 2 * outline_px + 2;
    // Cell width: widest advance (+ outline + margin); 16 columns x 6 rows
    int max_adv = 1;
    for (int c = 32; c < 127; c++) {
        int minx, maxx, miny, maxy, adv;
        if (TTF_GetGlyphMetrics(font, static_cast<Uint32>(c), &minx, &maxx, &miny, &maxy, &adv))
            max_adv = std::max(max_adv, std::max(adv, maxx - std::min(minx, 0)));
    }
    const int cell_w = max_adv + 2 * outline_px + 6;
    const int cols = 16, rows = 6;
    SDL_Surface* fill = SDL_CreateSurface(cols * cell_w, rows * cell_h, SDL_PIXELFORMAT_RGBA32);
    SDL_Surface* outl = SDL_CreateSurface(cols * cell_w, rows * cell_h, SDL_PIXELFORMAT_RGBA32);
    if (!fill || !outl) {
        if (fill) SDL_DestroySurface(fill);
        if (outl) SDL_DestroySurface(outl);
        TTF_CloseFont(font);
        return false;
    }
    SDL_FillSurfaceRect(fill, nullptr, 0);
    SDL_FillSurfaceRect(outl, nullptr, 0);
    const SDL_Color white = {255, 255, 255, 255};

    for (int c = 32; c < 127; c++) {
        Glyph& g = atlas.glyphs[c - 32];
        int minx = 0, maxx = 0, miny = 0, maxy = 0, adv = 0;
        TTF_GetGlyphMetrics(font, static_cast<Uint32>(c), &minx, &maxx, &miny, &maxy, &adv);
        g.advance = static_cast<f32>(adv);
        g.xoff = static_cast<f32>(std::min(minx, 0));
        const int i = c - 32;
        const int cx = (i % cols) * cell_w + outline_px + 1;
        const int cy = (i / cols) * cell_h + outline_px + 1;
        g.src = {0, 0, 0, 0};
        g.src_out = {0, 0, 0, 0};

        // Fill glyph
        TTF_SetFontOutline(font, 0);
        SDL_Surface* gs = TTF_RenderGlyph_Blended(font, static_cast<Uint32>(c), white);
        if (gs) {
            SDL_Rect dst = {cx, cy, gs->w, gs->h};
            SDL_SetSurfaceBlendMode(gs, SDL_BLENDMODE_NONE);
            SDL_BlitSurface(gs, nullptr, fill, &dst);
            g.src = {static_cast<f32>(cx), static_cast<f32>(cy), static_cast<f32>(gs->w), static_cast<f32>(gs->h)};
            SDL_DestroySurface(gs);
        }
        // Outline glyph (bigger by outline_px on every side)
        if (outline_px > 0) {
            TTF_SetFontOutline(font, outline_px);
            SDL_Surface* os = TTF_RenderGlyph_Blended(font, static_cast<Uint32>(c), white);
            if (os) {
                SDL_Rect dst = {cx - outline_px, cy - outline_px, os->w, os->h};
                SDL_SetSurfaceBlendMode(os, SDL_BLENDMODE_NONE);
                SDL_BlitSurface(os, nullptr, outl, &dst);
                g.src_out = {static_cast<f32>(cx - outline_px), static_cast<f32>(cy - outline_px),
                             static_cast<f32>(os->w), static_cast<f32>(os->h)};
                SDL_DestroySurface(os);
            }
            TTF_SetFontOutline(font, 0);
        }
    }
    TTF_CloseFont(font);

    auto upload = [&](SDL_Surface* s, const char* name) -> u32 {
        SDL_Texture* tex = SDL_CreateTextureFromSurface(sdl, s);
        SDL_DestroySurface(s);
        if (!tex) return TEXTURE_NONE;
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_LINEAR);
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        TextureEntry entry;
        entry.texture = tex;
        entry.width = cols * cell_w;
        entry.height = rows * cell_h;
        u32 id = static_cast<u32>(textures.size());
        textures.push_back(entry);
        texture_map[std::string(name) + std::to_string(static_cast<int>(ptsize))] = id;
        return id;
    };
    atlas.texture     = upload(fill, "@font_fill_");
    atlas.texture_out = upload(outl, "@font_outline_");
    atlas.px = static_cast<f32>(line_h);
    atlas.outline = static_cast<f32>(outline_px);
    atlas.valid = (atlas.texture != TEXTURE_NONE);
    SDL_Log("Font atlas %.0fpt: line %d px, cell %dx%d, outline %d", ptsize, line_h, cell_w, cell_h, outline_px);
    return atlas.valid;
}

// =============================================================================
// Procedural textures
// =============================================================================
// White RGB, alpha = smooth elliptical gradient from inner_a (centre) to outer_a (edge).
// Tint with draw_sprite_rect() to colour it (shadows: black, alpha-mod for strength).
u32 Renderer::create_radial_texture(const char* name, int w, int h, f32 inner_a, f32 outer_a, f32 power) {
    SDL_Surface* surf = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
    if (!surf) {
        SDL_Log("create_radial_texture '%s' failed: %s", name, SDL_GetError());
        return TEXTURE_NONE;
    }
    u8* pixels = static_cast<u8*>(surf->pixels);
    const f32 cx = (w - 1) * 0.5f, cy = (h - 1) * 0.5f;
    for (int y = 0; y < h; y++) {
        u32* row = reinterpret_cast<u32*>(pixels + y * surf->pitch);
        for (int x = 0; x < w; x++) {
            f32 dx = (x - cx) / (w * 0.5f);
            f32 dy = (y - cy) / (h * 0.5f);
            f32 d = std::min(1.0f, std::sqrt(dx * dx + dy * dy));
            // smoothstep then power: soft core, fading rim
            f32 s = d * d * (3.0f - 2.0f * d);
            s = std::pow(s, power);
            f32 a = inner_a + (outer_a - inner_a) * s;
            u8 a8 = static_cast<u8>(std::round(std::max(0.0f, std::min(1.0f, a)) * 255.0f));
            row[x] = SDL_MapRGBA(SDL_GetPixelFormatDetails(surf->format), nullptr, 255, 255, 255, a8);
        }
    }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(sdl, surf);
    SDL_DestroySurface(surf);
    if (!tex) {
        SDL_Log("create_radial_texture '%s' failed: %s", name, SDL_GetError());
        return TEXTURE_NONE;
    }
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_LINEAR);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

    TextureEntry entry;
    entry.texture = tex;
    entry.width = w;
    entry.height = h;
    u32 id = static_cast<u32>(textures.size());
    textures.push_back(entry);
    texture_map[name] = id;
    return id;
}

void Renderer::shutdown() {
    for (auto& entry : textures) {
        if (entry.texture) {
            SDL_DestroyTexture(entry.texture);
        }
    }
    textures.clear();
    texture_map.clear();
    if (TTF_WasInit()) TTF_Quit();
}

// =============================================================================
// Texture Management
// =============================================================================
u32 Renderer::load_texture(const char* path, bool smooth) {
    if (!path) return TEXTURE_NONE;

    // Check cache
    auto it = texture_map.find(path);
    if (it != texture_map.end()) {
        return it->second;
    }

    // Build full path
    std::string full_path = std::string(get_asset_path()) + path;

    SDL_Texture* tex = IMG_LoadTexture(sdl, full_path.c_str());
    if (!tex) {
        SDL_Log("Failed to load texture '%s': %s", full_path.c_str(), SDL_GetError());
        return TEXTURE_NONE;
    }

    SDL_SetTextureScaleMode(tex, smooth ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);

    TextureEntry entry;
    entry.texture = tex;
    float tex_w = 0.0f, tex_h = 0.0f;
    SDL_GetTextureSize(tex, &tex_w, &tex_h);
    entry.width = static_cast<int>(tex_w);
    entry.height = static_cast<int>(tex_h);

    u32 id = static_cast<u32>(textures.size());
    textures.push_back(entry);
    texture_map[path] = id;

    SDL_Log("Loaded texture [%d]: %s (%dx%d)", id, path, entry.width, entry.height);
    return id;
}

TextureEntry* Renderer::get_texture(u32 id) {
    if (id < textures.size()) return &textures[id];
    return nullptr;
}

// =============================================================================
// Frame Lifecycle
// =============================================================================
void Renderer::begin_frame() {
    // Render to the internal target in game units: the render scale is per render target
    // in SDL3 (view state), so it is set here every frame (no-op if unchanged) and the
    // window view keeps scale 1 for end_frame.
    SDL_SetRenderTarget(sdl, target);
    const f32 rs = static_cast<f32>(Config::RENDER_SCALE);
    SDL_SetRenderScale(sdl, rs, rs);
}

void Renderer::end_frame() {
    // Switch back to window
    SDL_SetRenderTarget(sdl, nullptr);

    // Clear window (black bars for letterboxing)
    SDL_SetRenderDrawColor(sdl, 0, 0, 0, 255);
    SDL_RenderClear(sdl);

    // Draw the target scaled to the viewport (linear)
    SDL_RenderTexture(sdl, target, nullptr, &viewport);

    SDL_RenderPresent(sdl);
}

// =============================================================================
// Drawing
// =============================================================================
void Renderer::clear(Color color) {
    SDL_SetRenderDrawColor(sdl, color.r, color.g, color.b, color.a);
    SDL_RenderClear(sdl);
}

void Renderer::draw_sprite(u32 texture_id, Rect src, Vec2 position,
                           Vec2 scale, f32 rotation, bool flip_x, Color tint) {
    if (texture_id >= textures.size()) return;

    TextureEntry& tex = textures[texture_id];
    if (!tex.texture) return;

    // Apply camera offset
    Vec2 screen_pos = world_to_screen(position);

    // src is in texels (ASSET_SCALE per game unit), dst in game units
    const f32 inv = 1.0f / static_cast<f32>(Config::ASSET_SCALE);
    SDL_FRect src_rect = {src.x, src.y, src.w, src.h};
    SDL_FRect dst_rect = {
        screen_pos.x - (src.w * inv * scale.x * 0.5f),
        screen_pos.y - (src.h * inv * scale.y * 0.5f),
        src.w * inv * scale.x,
        src.h * inv * scale.y
    };

    SDL_SetTextureColorMod(tex.texture, tint.r, tint.g, tint.b);
    SDL_SetTextureAlphaMod(tex.texture, tint.a);

    SDL_FlipMode flip = flip_x ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
    SDL_RenderTextureRotated(sdl, tex.texture, &src_rect, &dst_rect,
                             static_cast<double>(rotation), nullptr, flip);
}

void Renderer::draw_sprite_rect(u32 texture_id, Rect src, Rect dst_world, Color tint, bool flip_x, f32 rotation) {
    if (texture_id >= textures.size()) return;
    TextureEntry& tex = textures[texture_id];
    if (!tex.texture) return;

    Vec2 sp = world_to_screen({dst_world.x, dst_world.y});
    SDL_FRect src_rect = {src.x, src.y, src.w, src.h};
    SDL_FRect dst_rect = {sp.x, sp.y, dst_world.w, dst_world.h};

    SDL_SetTextureColorMod(tex.texture, tint.r, tint.g, tint.b);
    SDL_SetTextureAlphaMod(tex.texture, tint.a);
    SDL_RenderTextureRotated(sdl, tex.texture, &src_rect, &dst_rect, static_cast<double>(rotation), nullptr,
                             flip_x ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}

void Renderer::draw_sorted(std::vector<DrawCmd>& cmds) {
    std::stable_sort(cmds.begin(), cmds.end(),
                     [](const DrawCmd& a, const DrawCmd& b) { return a.sort_y < b.sort_y; });
    for (const DrawCmd& c : cmds) {
        if (c.texture_id != TEXTURE_NONE && c.texture_id < textures.size() && textures[c.texture_id].texture) {
            draw_sprite_rect(c.texture_id, c.src, c.dst, c.tint, c.flip_x, c.rotation);
        } else {
            draw_rect(c.dst, c.tint);
            if (c.outline) draw_rect_outline(c.dst, {255, 255, 255, 180});
        }
    }
}

bool Renderer::save_screenshot(const char* path, int scale) {
    // Read back the render target (full pixel size, e.g. 1280x720); we rebind explicitly here.
    SDL_Texture* prev = SDL_GetRenderTarget(sdl);
    SDL_SetRenderTarget(sdl, target);
    SDL_Surface* surf = SDL_RenderReadPixels(sdl, nullptr);
    SDL_SetRenderTarget(sdl, prev);
    if (!surf) {
        SDL_Log("Screenshot failed: %s", SDL_GetError());
        return false;
    }
    bool ok = false;
    if (scale > 1) {
        SDL_Surface* big = SDL_ScaleSurface(surf, surf->w * scale, surf->h * scale, SDL_SCALEMODE_NEAREST);
        if (big) {
            ok = IMG_SavePNG(big, path);
            SDL_DestroySurface(big);
        }
    } else {
        ok = IMG_SavePNG(surf, path);
    }
    SDL_DestroySurface(surf);
    if (!ok) SDL_Log("Screenshot save failed '%s': %s", path, SDL_GetError());
    else     SDL_Log("Screenshot saved: %s", path);
    return ok;
}

void Renderer::draw_rect(Rect rect, Color color, bool filled) {
    Vec2 screen_pos = world_to_screen({rect.x, rect.y});
    SDL_FRect r = {screen_pos.x, screen_pos.y, rect.w, rect.h};

    SDL_SetRenderDrawColor(sdl, color.r, color.g, color.b, color.a);
    if (filled) {
        SDL_RenderFillRect(sdl, &r);
    } else {
        SDL_RenderRect(sdl, &r);
    }
}

void Renderer::draw_rect_outline(Rect rect, Color color) {
    draw_rect(rect, color, false);
}

static inline const Glyph* glyph_for(const FontAtlas& fa, unsigned char ch) {
    if (ch < 32 || ch > 126) return nullptr;   // non-ASCII bytes are skipped
    return &fa.glyphs[ch - 32];
}

void Renderer::draw_text(const char* text, f32 x, f32 y, Color color, f32 scale) {
    if (!text) return;
    if (has_font) {
        const FontAtlas& fa = (scale > 1.25f) ? font_large : font_small;
        const f32 k = (TEXT_LINE_UNITS * scale) / fa.px;   // game units per atlas pixel
        const Color dark = {22, 16, 26, color.a};
        // Pass 0: dark outline behind everything, pass 1: fill
        for (int pass = 0; pass < 2; pass++) {
            f32 pen = x;
            for (const char* p = text; *p; p++) {
                const Glyph* g = glyph_for(fa, static_cast<unsigned char>(*p));
                if (!g) continue;
                if (pass == 0 && g->src_out.w > 0.0f) {
                    draw_sprite_rect(fa.texture_out, g->src_out,
                                     {pen + (g->xoff - fa.outline) * k, y - fa.outline * k,
                                      g->src_out.w * k, g->src_out.h * k}, dark);
                } else if (pass == 1 && g->src.w > 0.0f) {
                    draw_sprite_rect(fa.texture, g->src,
                                     {pen + g->xoff * k, y, g->src.w * k, g->src.h * k}, color);
                }
                pen += g->advance * k;
            }
        }
        return;
    }

    // Fallback: SDL debug font, 8x8 glyphs in game units. The render scale is RENDER_SCALE
    // (target view); fold the text scale into it and restore RENDER_SCALE afterwards (NOT 1).
    Vec2 sp = world_to_screen({x, y});
    SDL_SetRenderDrawColor(sdl, color.r, color.g, color.b, color.a);
    const f32 rs = static_cast<f32>(Config::RENDER_SCALE);
    if (scale != 1.0f) {
        SDL_SetRenderScale(sdl, rs * scale, rs * scale);
        SDL_RenderDebugText(sdl, sp.x / scale, sp.y / scale, text);
        SDL_SetRenderScale(sdl, rs, rs);
    } else {
        SDL_RenderDebugText(sdl, sp.x, sp.y, text);
    }
}

f32 Renderer::measure_text(const char* text, f32 scale) const {
    if (!text) return 0.0f;
    if (!has_font) return static_cast<f32>(std::strlen(text)) * 8.0f * scale;
    const FontAtlas& fa = (scale > 1.25f) ? font_large : font_small;
    const f32 k = (TEXT_LINE_UNITS * scale) / fa.px;
    f32 w = 0.0f;
    for (const char* p = text; *p; p++) {
        const Glyph* g = glyph_for(fa, static_cast<unsigned char>(*p));
        if (g) w += g->advance * k;
    }
    return w;
}

void Renderer::draw_text_centered(const char* text, f32 y, Color color, f32 scale) {
    f32 text_w = measure_text(text, scale);
    f32 x = (Config::GAME_WIDTH - text_w) * 0.5f;
    draw_text(text, x, y, color, scale);
}

// =============================================================================
// Camera
// =============================================================================
void Renderer::set_camera(Vec2 position) {
    // Center camera on position
    camera_offset = {
        position.x - Config::GAME_WIDTH * 0.5f,
        position.y - Config::GAME_HEIGHT * 0.5f
    };
}

void Renderer::shake_camera(f32 intensity, f32 duration) {
    shake_intensity = intensity;
    shake_timer = duration;
}

void Renderer::update_shake(f32 dt) {
    if (shake_timer > 0.0f) {
        shake_timer -= dt;
        camera_shake = {
            randf_range(-shake_intensity, shake_intensity),
            randf_range(-shake_intensity, shake_intensity)
        };
        // Decay
        shake_intensity *= 0.9f;
    } else {
        camera_shake = {0, 0};
    }
}

Vec2 Renderer::world_to_screen(Vec2 world_pos) const {
    return {
        world_pos.x - camera_offset.x + camera_shake.x,
        world_pos.y - camera_offset.y + camera_shake.y
    };
}

Vec2 Renderer::window_to_game(Vec2 window_pos) const {
    if (viewport.w <= 0.0f || viewport.h <= 0.0f) return window_pos;
    return {
        (window_pos.x - viewport.x) / viewport.w * Config::GAME_WIDTH,
        (window_pos.y - viewport.y) / viewport.h * Config::GAME_HEIGHT
    };
}

// =============================================================================
// Viewport Calculation (Letterboxing)
// =============================================================================
void Renderer::handle_resize(int new_w, int new_h) {
    window_w = new_w;
    window_h = new_h;
    calculate_viewport();
}

void Renderer::calculate_viewport() {
    // Scale to fill window while preserving 16:9 aspect ratio
    float scale_x = static_cast<float>(window_w) / Config::GAME_WIDTH;
    float scale_y = static_cast<float>(window_h) / Config::GAME_HEIGHT;
    float scale = std::max(1.0f, std::min(scale_x, scale_y));

    float scaled_w = Config::GAME_WIDTH  * scale;
    float scaled_h = Config::GAME_HEIGHT * scale;

    viewport = {
        (static_cast<float>(window_w) - scaled_w) * 0.5f,
        (static_cast<float>(window_h) - scaled_h) * 0.5f,
        scaled_w,
        scaled_h
    };
}
