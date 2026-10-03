#include "game/ambience.h"

#include "game/rooms.h"
#include "render/renderer.h"

#include <SDL3/SDL.h>
#include <cmath>
#include <vector>

// =============================================================================
// Texturas (se crean una vez)
// =============================================================================

namespace {

constexpr int CLOUD_PX   = 256;      // textura periodica de nubes
constexpr f32 CLOUD_TILE = 640.0f;   // lo que ocupa en el mundo (2,5 unidades por texel)
constexpr int LIGHT_W = 128, LIGHT_H = 72;

u32 g_cloud     = TEXTURE_NONE;
u32 g_light     = TEXTURE_NONE;   // ADD: luz calida desde arriba a la izquierda
u32 g_shade     = TEXTURE_NONE;   // BLEND: penumbra en la esquina contraria
u32 g_mote      = TEXTURE_NONE;   // BLEND: polvo, ceniza
u32 g_mote_glow = TEXTURE_NONE;   // ADD: vilanos, pavesas, polvo en la luz

u32 hash_u32(u32 x) {
    x ^= x >> 16; x *= 0x7feb352du;
    x ^= x >> 15; x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}
f32 hash01(u32 x) { return static_cast<f32>(hash_u32(x) & 0xFFFFFFu) / 16777216.0f; }

f32 smooth01(f32 t) { return t * t * (3.0f - 2.0f * t); }
f32 smoothstep(f32 a, f32 b, f32 x) {
    f32 t = (x - a) / (b - a);
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return smooth01(t);
}

// Ruido de valor PERIODICO (rejilla de `period` celdas que se cierra sobre si misma): es lo
// que deja repetir la textura en el mundo sin costura.
f32 periodic_noise(f32 u, f32 v, int period, u32 seed) {
    f32 x = u * period, y = v * period;
    int xi = static_cast<int>(std::floor(x)), yi = static_cast<int>(std::floor(y));
    f32 fx = smooth01(x - xi), fy = smooth01(y - yi);
    auto lat = [&](int i, int j) {
        i = ((i % period) + period) % period;
        j = ((j % period) + period) % period;
        return hash01(seed ^ (static_cast<u32>(i) * 73856093u) ^ (static_cast<u32>(j) * 19349663u));
    };
    f32 a = lat(xi, yi), b = lat(xi + 1, yi), c = lat(xi, yi + 1), d = lat(xi + 1, yi + 1);
    return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
}

u32 make_cloud_texture(Renderer& r) {
    std::vector<u8> px(static_cast<size_t>(CLOUD_PX) * CLOUD_PX * 4);
    for (int y = 0; y < CLOUD_PX; y++) {
        for (int x = 0; x < CLOUD_PX; x++) {
            f32 u = (x + 0.5f) / CLOUD_PX, v = (y + 0.5f) / CLOUD_PX;
            f32 n = 0.0f, amp = 0.5f, norm = 0.0f;
            int period = 3;
            for (int o = 0; o < 5; o++) {
                n += amp * periodic_noise(u, v, period, 0x51C0u + o * 977u);
                norm += amp;
                amp *= 0.5f;
                period *= 2;
            }
            n /= norm;
            // Nubes sueltas con el borde blando: la mitad del cielo queda limpia.
            f32 a = smoothstep(0.50f, 0.68f, n);
            a *= 0.80f + 0.20f * smoothstep(0.60f, 0.80f, n);
            u8* p = &px[(static_cast<size_t>(y) * CLOUD_PX + x) * 4];
            p[0] = p[1] = p[2] = 255;
            p[3] = static_cast<u8>(a * 255.0f + 0.5f);
        }
    }
    return r.create_texture_from_rgba("amb_cloud", CLOUD_PX, CLOUD_PX, px.data(), true);
}

// Degradado de pantalla: `corner` 0 = arriba-izquierda (luz), 1 = abajo-derecha (penumbra).
u32 make_corner_texture(Renderer& r, const char* name, bool light) {
    std::vector<u8> px(static_cast<size_t>(LIGHT_W) * LIGHT_H * 4);
    for (int y = 0; y < LIGHT_H; y++) {
        for (int x = 0; x < LIGHT_W; x++) {
            f32 u = (x + 0.5f) / LIGHT_W, v = (y + 0.5f) / LIGHT_H;
            f32 a;
            if (light) {
                // Foco fuera de pantalla, arriba a la izquierda: cae en diagonal.
                f32 dx = (u + 0.15f) * 1.0f, dy = (v + 0.25f) * 1.35f;
                f32 d = std::sqrt(dx * dx + dy * dy) / 1.55f;
                a = 1.0f - smoothstep(0.0f, 1.0f, d);
                a = a * a;
            } else {
                f32 d = (u * 0.55f + v * 0.45f);
                a = smoothstep(0.45f, 1.05f, d);
            }
            u8* p = &px[(static_cast<size_t>(y) * LIGHT_W + x) * 4];
            p[0] = p[1] = p[2] = 255;
            p[3] = static_cast<u8>(a * 255.0f + 0.5f);
        }
    }
    return r.create_texture_from_rgba(name, LIGHT_W, LIGHT_H, px.data(), true);
}

void set_blend(Renderer& r, u32 id, SDL_BlendMode mode) {
    if (TextureEntry* t = r.get_texture(id); t && t->texture) SDL_SetTextureBlendMode(t->texture, mode);
}

// =============================================================================
// Ambiente por capitulo
// =============================================================================

enum class Mote : u8 { None, Dust, Pollen, Ash, Ember, RoomDust };

struct Amb {
    bool  clouds;
    Color cloud;        // color y alfa de la sombra
    Vec2  wind;         // unidades/s (nubes y motas)
    Color light;        // ADD; alfa = fuerza
    Color shade;        // BLEND
    Mote  mote;
    int   motes;
    Color mote_col;
};

Amb get_amb(int chapter, bool interior) {
    if (interior)
        return {false, {}, {0.6f, -0.3f}, {255, 210, 150, 26}, {14, 10, 20, 70},
                Mote::RoomDust, 18, {255, 220, 160, 90}};
    switch (chapter) {
        case 1:  return {true,  {22, 18, 40, 62},  {7.0f, 2.2f},  {255, 214, 160, 34}, {30, 24, 60, 40},
                         Mote::Dust,   26, {236, 214, 170, 80}};
        case 2:  return {true,  {24, 20, 40, 56},  {8.0f, 1.5f},  {255, 220, 170, 36}, {34, 26, 56, 36},
                         Mote::Dust,   30, {240, 220, 180, 80}};
        case 3:  return {true,  {30, 16, 30, 58},  {5.0f, 2.8f},  {255, 176, 110, 44}, {42, 18, 54, 50},
                         Mote::Ember,  26, {255, 140, 60, 230}};
        case 4:  return {true,  {8, 8, 24, 70},    {13.0f, 3.0f}, {170, 180, 255, 24}, {8, 8, 26, 66},
                         Mote::Dust,   34, {190, 185, 200, 70}};
        case 5:  return {true,  {20, 34, 34, 56},  {5.0f, 1.6f},  {255, 232, 170, 40}, {20, 30, 50, 34},
                         Mote::Pollen, 26, {255, 250, 210, 190}};
        case 6:  return {true,  {16, 20, 22, 76},  {4.0f, 2.0f},  {220, 200, 170, 20}, {18, 26, 26, 56},
                         Mote::Ash,    30, {150, 148, 145, 120}};
        case 7:  return {true,  {24, 20, 40, 56},  {7.0f, 2.0f},  {255, 218, 165, 34}, {30, 24, 56, 38},
                         Mote::Dust,   24, {236, 214, 175, 75}};
        case 8:  return {true,  {22, 22, 40, 52},  {6.0f, 1.4f},  {255, 226, 180, 34}, {26, 24, 50, 34},
                         Mote::Pollen, 16, {255, 245, 220, 160}};
        case 9:  return {true,  {16, 30, 40, 54},  {5.5f, 2.0f},  {255, 236, 180, 38}, {16, 30, 52, 34},
                         Mote::Pollen, 24, {245, 255, 230, 180}};
        case 10: return {true,  {14, 14, 20, 80},  {4.5f, 2.6f},  {210, 190, 165, 18}, {16, 16, 24, 60},
                         Mote::Ash,    34, {140, 136, 132, 120}};
        default: return {true,  {22, 18, 40, 60},  {7.0f, 2.0f},  {255, 214, 160, 32}, {30, 24, 60, 40},
                         Mote::Dust,   24, {236, 214, 170, 80}};
    }
}

bool interior_of(const Room& room) { return room.type == RoomType::Interior; }

} // namespace

// =============================================================================
// API
// =============================================================================

void ambience_init(Renderer& renderer) {
    g_cloud = make_cloud_texture(renderer);
    g_light = make_corner_texture(renderer, "amb_light", true);
    g_shade = make_corner_texture(renderer, "amb_shade", false);
    g_mote      = renderer.create_radial_texture("amb_mote", 16, 16, 1.0f, 0.0f, 0.8f);
    g_mote_glow = renderer.create_radial_texture("amb_mote_glow", 16, 16, 1.0f, 0.0f, 1.2f);
    set_blend(renderer, g_light, SDL_BLENDMODE_ADD);
    set_blend(renderer, g_mote_glow, SDL_BLENDMODE_ADD);
    SDL_Log("Ambiente: texturas %s", (g_cloud != TEXTURE_NONE && g_light != TEXTURE_NONE) ? "listas" : "NO disponibles");
}

u32 ambience_cloud_texture() { return g_cloud; }

void ambience_draw_world(Renderer& renderer, const Room& room, int chapter, Rect view, f32 time) {
    if (g_cloud == TEXTURE_NONE || interior_of(room)) return;
    const Amb amb = get_amb(chapter, false);
    if (!amb.clouds) return;

    // Las nubes van un poco mas despacio que las motas: estan altas y la sombra se arrastra.
    const f32 ox = std::fmod(amb.wind.x * 0.8f * time, CLOUD_TILE);
    const f32 oy = std::fmod(amb.wind.y * 0.8f * time, CLOUD_TILE);
    const f32 x0 = std::floor((view.x - ox) / CLOUD_TILE) * CLOUD_TILE + ox;
    const f32 y0 = std::floor((view.y - oy) / CLOUD_TILE) * CLOUD_TILE + oy;
    // Las juntas van AJUSTADAS a la rejilla de pixeles del objetivo (RENDER_SCALE por unidad):
    // con el borde en fraccion de pixel las dos baldosas rasterizan la misma fila y la sombra
    // se suma dos veces — una raya oscura de lado a lado. Y sin solape, por la misma razon.
    const Vec2 off = {renderer.camera_offset.x - renderer.camera_shake.x - renderer.ui_offset.x,
                      renderer.camera_offset.y - renderer.camera_shake.y - renderer.ui_offset.y};
    auto snap = [](f32 v, f32 o) {
        const f32 k = static_cast<f32>(Config::RENDER_SCALE);
        return std::round((v - o) * k) / k + o;
    };
    for (f32 y = y0; y < view.y + view.h; y += CLOUD_TILE) {
        const f32 ya = snap(y, off.y), yb = snap(y + CLOUD_TILE, off.y);
        for (f32 x = x0; x < view.x + view.w; x += CLOUD_TILE) {
            const f32 xa = snap(x, off.x), xb = snap(x + CLOUD_TILE, off.x);
            renderer.draw_sprite_rect(g_cloud, {0, 0, static_cast<f32>(CLOUD_PX), static_cast<f32>(CLOUD_PX)},
                                      {xa, ya, xb - xa, yb - ya}, amb.cloud);
        }
    }
}

void ambience_draw_screen(Renderer& renderer, const Room& room, int chapter, Vec2 camera, f32 time) {
    const bool interior = interior_of(room);
    const Amb amb = get_amb(chapter, interior);
    const f32 W = static_cast<f32>(Config::GAME_WIDTH), H = static_cast<f32>(Config::GAME_HEIGHT);
    const Rect full = {-8, -8, W + 16, H + 16};

    if (g_shade != TEXTURE_NONE && amb.shade.a > 0)
        renderer.draw_sprite_rect(g_shade, {0, 0, LIGHT_W, LIGHT_H}, full, amb.shade);
    if (g_light != TEXTURE_NONE && amb.light.a > 0)
        renderer.draw_sprite_rect(g_light, {0, 0, LIGHT_W, LIGHT_H}, full, amb.light);

    if (amb.mote == Mote::None || g_mote == TEXTURE_NONE) return;

    // Motas ANCLADAS al mundo: cada una tiene su sitio en un lienzo algo mayor que la pantalla
    // que se repite alrededor de la camara. Asi no se van con el profeta y nunca se acaban.
    constexpr f32 M = 16.0f;
    const f32 RW = W + 2 * M, RH = H + 2 * M;
    const f32 cam_x = camera.x - W * 0.5f, cam_y = camera.y - H * 0.5f;
    const bool glow = amb.mote == Mote::Pollen || amb.mote == Mote::Ember || amb.mote == Mote::RoomDust;
    const u32 tex = glow && g_mote_glow != TEXTURE_NONE ? g_mote_glow : g_mote;

    for (int i = 0; i < amb.motes; i++) {
        const u32 s = static_cast<u32>(i) * 0x9E3779B9u + static_cast<u32>(chapter) * 0x85EBCA6Bu;
        const f32 h1 = hash01(s), h2 = hash01(s ^ 0x1234u), h3 = hash01(s ^ 0xBEEFu), h4 = hash01(s ^ 0x7777u);
        const f32 spd = 0.55f + 0.9f * h3;
        Vec2 v = amb.wind;
        f32 wob = 2.0f, size = 0.8f + 0.7f * h4, a = 1.0f;
        switch (amb.mote) {
            case Mote::Dust:     v = {amb.wind.x * 1.4f, amb.wind.y * 1.2f}; wob = 2.5f; break;
            case Mote::Pollen:   v = {3.0f, -2.2f}; wob = 7.0f; size = 0.9f + 0.8f * h4; break;
            case Mote::Ash:      v = {2.0f, 5.5f};  wob = 5.0f; size = 0.7f + 0.6f * h4; break;
            case Mote::Ember:    v = {1.5f, -9.0f}; wob = 4.0f; size = 0.6f + 0.6f * h4; break;
            case Mote::RoomDust: v = {0.5f, -0.4f}; wob = 3.0f; size = 0.5f + 0.6f * h4; break;
            default: break;
        }
        const f32 ph = h4 * 6.2831853f;
        f32 wx = h1 * RW + v.x * spd * time + wob * std::sin(time * (0.7f + h2) + ph);
        f32 wy = h2 * RH + v.y * spd * time + wob * 0.6f * std::cos(time * (0.9f + h1) + ph);
        f32 sx = std::fmod(wx - cam_x, RW); if (sx < 0) sx += RW;
        f32 sy = std::fmod(wy - cam_y, RH); if (sy < 0) sy += RH;
        sx -= M; sy -= M;

        // Parpadeo: los vilanos y las pavesas destellan, el polvo apenas.
        const f32 tw = 0.5f + 0.5f * std::sin(time * (1.3f + 2.1f * h3) + ph * 1.7f);
        a = amb.mote == Mote::Dust || amb.mote == Mote::Ash ? 0.65f + 0.35f * tw : 0.25f + 0.75f * tw * tw;
        Color c = amb.mote_col;
        c.a = static_cast<u8>(c.a * a);
        const f32 r = size * (glow ? 2.6f : 1.1f);
        renderer.draw_sprite_rect(tex, {0, 0, 16, 16}, {sx - r, sy - r, r * 2, r * 2}, c);
    }
}
