// =============================================================================
// MANTLE - 2D Renderer Implementation
// =============================================================================

#include "render/renderer.h"
#include "platform/platform.h"
#include <SDL3_ttf/SDL_ttf.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>

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
    // Cell width: widest advance (+ outline + margin); 16 columns, rows to fit the charset
    int max_adv = 1;
    for (int i = 0; i < GLYPH_COUNT; i++) {
        int minx, maxx, miny, maxy, adv;
        if (TTF_GetGlyphMetrics(font, glyph_codepoint(i), &minx, &maxx, &miny, &maxy, &adv))
            max_adv = std::max(max_adv, std::max(adv, maxx - std::min(minx, 0)));
    }
    const int cell_w = max_adv + 2 * outline_px + 6;
    const int cols = 16, rows = (GLYPH_COUNT + cols - 1) / cols;
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

    int missing = 0;
    for (int i = 0; i < GLYPH_COUNT; i++) {
        Glyph& g = atlas.glyphs[i];
        g = Glyph{};
        const Uint32 cp = glyph_codepoint(i);
        int minx = 0, maxx = 0, miny = 0, maxy = 0, adv = 0;
        // A font without this codepoint leaves advance at 0, and glyph_for() then skips it
        // instead of leaving a blank gap in the middle of a word.
        if (!TTF_GetGlyphMetrics(font, cp, &minx, &maxx, &miny, &maxy, &adv)) {
            missing++;
            continue;
        }
        g.advance = static_cast<f32>(adv);
        g.xoff = static_cast<f32>(std::min(minx, 0));
        const int cx = (i % cols) * cell_w + outline_px + 1;
        const int cy = (i / cols) * cell_h + outline_px + 1;

        // Fill glyph
        TTF_SetFontOutline(font, 0);
        SDL_Surface* gs = TTF_RenderGlyph_Blended(font, cp, white);
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
            SDL_Surface* os = TTF_RenderGlyph_Blended(font, cp, white);
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
    SDL_Log("Font atlas %.0fpt: line %d px, cell %dx%d, outline %d, %d/%d glyphs (%d missing)",
            ptsize, line_h, cell_w, cell_h, outline_px, GLYPH_COUNT - missing, GLYPH_COUNT, missing);
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

    // Everything the player looks at is LINEAR: the art is smooth "3D figurine", not pixel art.
    // smooth=false exists for ATLASES ONLY (the tileset), where CLAMP is texture-wide and abutting
    // cells would bleed into each other under linear filtering. It is a sampling fix, not a style.
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

void Renderer::unload_texture(const char* path) {
    if (!path) return;
    auto it = texture_map.find(path);
    if (it == texture_map.end()) return;
    TextureEntry& entry = textures[it->second];
    if (entry.texture) {
        SDL_DestroyTexture(entry.texture);
        entry.texture = nullptr;   // el id sigue siendo valido: los draws lo saltan
    }
    SDL_Log("Unloaded texture [%d]: %s", it->second, path);
    texture_map.erase(it);
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
}

// Separado de end_frame para que entre los dos quepa la pasada de superposicion en
// vertical (begin_overlay/end_overlay), que dibuja en las bandas negras.
void Renderer::present() {
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
    // Normalmente se lee el objetivo interno (1280x720), que es lo que esperan todos los
    // guiones. En VERTICAL se lee la VENTANA entera: si no, la captura se quedaria con el
    // area de juego y sin las bandas negras, que es justamente donde estan los controles
    // que se quieren comprobar. Hay que llamarla ANTES de present().
    SDL_Texture* prev = SDL_GetRenderTarget(sdl);
    SDL_SetRenderTarget(sdl, is_portrait() ? nullptr : target);
    SDL_Surface* surf = SDL_RenderReadPixels(sdl, nullptr);
    SDL_SetRenderTarget(sdl, prev);
    if (!surf) {
        SDL_Log("Screenshot failed: %s", SDL_GetError());
        return false;
    }
    bool ok = false;
    if (scale > 1) {
        // Debug zoom (scale > 1): NEAREST so a texel stays a square and can be inspected.
        // The game itself is never drawn this way -- see load_texture.
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

static inline const Glyph* glyph_for(const FontAtlas& fa, u32 cp) {
    const int i = glyph_index(cp);
    if (i < 0) return nullptr;                          // outside the baked charset
    const Glyph& g = fa.glyphs[i];
    return (g.advance > 0.0f) ? &g : nullptr;           // codepoint the font does not provide
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
            for (const char* p = text; *p; ) {
                const Glyph* g = glyph_for(fa, utf8_next(p));
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
    if (!has_font) {
        // Debug font: 8x8 per codepoint, not per byte (a tilde is 2 bytes in UTF-8)
        int n = 0;
        for (const char* p = text; *p; ) { utf8_next(p); n++; }
        return static_cast<f32>(n) * 8.0f * scale;
    }
    const FontAtlas& fa = (scale > 1.25f) ? font_large : font_small;
    const f32 k = (TEXT_LINE_UNITS * scale) / fa.px;
    f32 w = 0.0f;
    for (const char* p = text; *p; ) {
        const Glyph* g = glyph_for(fa, utf8_next(p));
        if (g) w += g->advance * k;
    }
    return w;
}

// Greedy word wrap. Splits on spaces only: a single word longer than `max_w` overflows
// rather than being cut mid-codepoint.
int Renderer::wrap_text(const char* text, f32 max_w, f32 scale, std::string* out, int max_lines) const {
    if (!text || !out || max_lines <= 0) return 0;
    int lines = 0;
    std::string cur;
    const char* p = text;
    while (*p && lines < max_lines) {
        const char* space = std::strchr(p, ' ');
        std::string word = space ? std::string(p, static_cast<size_t>(space - p)) : std::string(p);
        if (!word.empty()) {
            std::string candidate = cur.empty() ? word : cur + " " + word;
            if (!cur.empty() && measure_text(candidate.c_str(), scale) > max_w) {
                out[lines++] = cur;
                cur = word;
            } else {
                cur = candidate;
            }
        }
        if (!space) { p += word.size(); break; }   // p al final: no queda nada por colocar
        p = space + 1;
    }
    bool dropped = false;
    if (!cur.empty()) {
        if (lines < max_lines) out[lines++] = cur;
        else                   dropped = true;
    }
    // El truncado era SILENCIOSO, y es el unico fallo de texto que no se ve venir: el ingles
    // no siempre es mas corto y la caja de dialogo con retrato pierde ~35 unidades de ancho.
    // Con esta linea un guion de regresion puede exigir CERO "TEXT TRUNCATED" en el log, en
    // vez de comparar setenta capturas a ojo. Una vez por texto (y con techo), o en una caja
    // que se redibuja cada frame saldrian sesenta lineas por segundo.
    if (dropped || *p) {
        static std::set<const void*> reported;
        if (reported.size() < 96 && reported.insert(static_cast<const void*>(text)).second)
            SDL_Log("TEXT TRUNCATED (%d lineas de %.0f u): \"%s\"", max_lines, max_w, text);
    }
    return lines;
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

void Renderer::shake_camera(f32 intensity, f32 duration, f32 frequency) {
    // Lo que queda de la sacudida en curso, ya con su envolvente aplicada. Una sacudida
    // pequena que llega detras de una grande NO la corta: gana la que se sienta mas.
    f32 live = 0.0f;
    if (shake_timer > 0.0f && shake_duration > 0.0f) {
        f32 k = shake_timer / shake_duration;
        live = shake_intensity * k * k;
    }
    if (intensity < live) return;
    shake_intensity = intensity;
    shake_timer     = duration;
    shake_duration  = duration;
    shake_freq      = frequency;
    shake_phase     = randf() * 6.2831853f;   // dos golpes seguidos no repiten el trazo
}

void Renderer::update_shake(f32 dt) {
    if (shake_timer <= 0.0f || shake_duration <= 0.0f) {
        shake_timer = 0.0f;
        camera_shake = {0, 0};
        return;
    }
    shake_timer -= dt;
    if (shake_timer <= 0.0f) { shake_timer = 0.0f; camera_shake = {0, 0}; return; }

    shake_phase += shake_freq * dt;
    // Envolvente cuadratica: la sacudida se APAGA en vez de cortarse de golpe (el
    // decaimiento anterior era por frame, asi que a 30 fps duraba el doble).
    f32 k   = shake_timer / shake_duration;
    f32 mag = shake_intensity * k * k;
    // Dos senos inconmensurables: un trazo continuo en vez del ruido blanco por frame,
    // que a 320x180 se leia como parpadeo del bordillo, no como un golpe.
    camera_shake = {
        std::sin(shake_phase)               * mag,
        std::sin(shake_phase * 1.37f + 1.1f) * mag
    };
}

Vec2 Renderer::world_to_screen(Vec2 world_pos) const {
    return {
        world_pos.x - camera_offset.x + camera_shake.x + ui_offset.x,
        world_pos.y - camera_offset.y + camera_shake.y + ui_offset.y
    };
}

void Renderer::begin_overlay() {
    SDL_SetRenderTarget(sdl, nullptr);
    f32 s = (viewport.w > 0.0f) ? viewport.w / static_cast<f32>(Config::GAME_WIDTH) : 1.0f;
    SDL_SetRenderScale(sdl, s, s);
    // El origen se lleva a la esquina del area de juego SUMANDOLO a las coordenadas y no
    // con SDL_SetRenderViewport: en SDL3 el viewport y la escala interactuan de forma que
    // es facil equivocarse, y esto es una suma.
    ui_offset = {viewport.x / s, viewport.y / s};
    overlay_active = true;
    camera_offset = {0, 0};
    camera_shake  = {0, 0};
}

void Renderer::end_overlay() {
    overlay_active = false;
    ui_offset = {0, 0};
    SDL_SetRenderScale(sdl, 1.0f, 1.0f);
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
