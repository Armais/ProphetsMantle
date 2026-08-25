#include "ui/cinematic.h"
#include "core/i18n.h"
#include "game/lore.h"
#include "game/rooms.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

// =============================================================================
// Helpers
// =============================================================================
namespace {

f32 clamp01(f32 v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

f32 smoothstep(f32 v) { v = clamp01(v); return v * v * (3.0f - 2.0f * v); }

Color mix(Color a, Color b, f32 k) {
    k = clamp01(k);
    return {static_cast<u8>(a.r + (b.r - a.r) * k),
            static_cast<u8>(a.g + (b.g - a.g) * k),
            static_cast<u8>(a.b + (b.b - a.b) * k),
            static_cast<u8>(a.a + (b.a - a.a) * k)};
}

Color with_alpha(Color c, f32 a) {
    c.a = static_cast<u8>(clamp01(a) * 255.0f);
    return c;
}

constexpr f32 W = static_cast<f32>(Config::GAME_WIDTH);
constexpr f32 H = static_cast<f32>(Config::GAME_HEIGHT);

// Franja de texto: donde empieza el degradado oscuro del pie
constexpr f32 TEXT_TOP    = 126.0f;
constexpr f32 TEXT_LINE_H = 9.5f;
constexpr f32 TEXT_WRAP_W = 284.0f;

}  // namespace

// =============================================================================
// Construccion
// =============================================================================
void CinematicSystem::begin() {
    scene_count = 0;
    scene = 0;
    t = 0.0f;
    clock = 0.0f;
    wrap_count = 0;
    active = false;
}

void CinematicSystem::add(const CineScene& s) {
    if (scene_count >= MAX_SCENES) return;
    scenes[scene_count++] = s;
}

void CinematicSystem::load_art(Renderer& r) {
    for (int i = 0; i < scene_count; i++) {
        CineScene& s = scenes[i];
        if (!s.art) continue;
        for (int l = 0; l < s.layer_count; l++) {
            char path[128];
            std::snprintf(path, sizeof(path), "%s_L%d.png", s.art, l);
            s.layers[l].tex = r.load_texture(path, true);
            if (s.layers[l].tex == TEXTURE_NONE) {
                SDL_Log("Cinematic: falta la capa %s", path);
            }
        }
    }
    art_loaded = true;
}

// =============================================================================
// Texto
// =============================================================================
void CinematicSystem::layout(const Renderer& r) {
    wrap_count = 0;
    if (scene_count == 0) return;
    const CineScene& s = scenes[scene];
    for (int i = 0; i < 3 && s.lines[i]; i++) {
        std::string tmp[MAX_LINES];
        int n = r.wrap_text(s.lines[i], TEXT_WRAP_W, TEXT_SCALE, tmp, MAX_LINES - wrap_count);
        for (int k = 0; k < n && wrap_count < MAX_LINES; k++) {
            wrapped[wrap_count] = tmp[k];
            wrap_src[wrap_count] = i;
            wrap_count++;
        }
    }
}

void CinematicSystem::go_to(int index, const Renderer& r) {
    scene = std::max(0, std::min(index, scene_count - 1));
    t = 0.0f;
    layout(r);
}

// =============================================================================
// Update
// =============================================================================
bool CinematicSystem::update(Input& in, f32 dt, const Renderer& r) {
    if (!active || scene_count == 0) {
        active = false;
        return true;
    }
    t += dt;
    clock += dt;

    // ESC dispara Cancel Y Pause a la vez (input.cpp), asi que los dos significan lo mismo.
    const bool skip_all = in.is_action_just_pressed(Action::Cancel) ||
                          in.is_action_just_pressed(Action::Pause);
    // Attack tiene que valer para avanzar: en tactil, tocar la mitad derecha solo produce eso.
    bool advance = in.is_action_just_pressed(Action::Confirm) ||
                   in.is_action_just_pressed(Action::Attack);

    if (auto_timer > 0.0f && t >= auto_timer) advance = true;   // autoplay

    if (skip_all) {
        active = false;
        return true;
    }
    if (advance || t >= scenes[scene].duration) {
        if (scene + 1 >= scene_count) {
            active = false;
            return true;
        }
        go_to(scene + 1, r);
    }
    return false;
}

// =============================================================================
// Render
// =============================================================================
void CinematicSystem::render(Renderer& r) const {
    if (scene_count == 0) return;
    const CineScene& s = scenes[scene];

    // La camara puede venir desplazada de una partida anterior (entrada "Prologo" del menu)
    // y draw_rect/draw_sprite_rect pasan por world_to_screen.
    r.camera_offset = {0.0f, 0.0f};
    r.camera_shake  = {0.0f, 0.0f};

    const f32 p = (s.duration > 0.0f) ? clamp01(t / s.duration) : 0.0f;
    const f32 e = smoothstep(p);
    const f32 pan  = s.pan_from + (s.pan_to - s.pan_from) * e;
    const f32 zoom = s.zoom_from + (s.zoom_to - s.zoom_from) * e;

    // --- cielo: degradado vertical. 90 bandas = 2 unidades cada una (8 px del target)
    r.clear(s.sky_top);
    const int BANDS = 90;
    for (int i = 0; i < BANDS; i++) {
        const f32 k = static_cast<f32>(i) / (BANDS - 1);
        r.draw_rect({0.0f, k * H, W, H / BANDS + 1.0f}, mix(s.sky_top, s.sky_bottom, k));
    }

    // --- temblor local. NO se usa renderer.shake_camera: moveria tambien el cielo y la
    // franja de texto, y asomarian los bordes de la pantalla.
    f32 shake = 0.0f;
    if ((s.fx & CineFx_Shake) && t >= s.fx_at && t < s.fx_at + 0.55f) {
        shake = (0.55f - (t - s.fx_at)) * 7.0f;
    }
    const f32 sx = shake > 0.0f ? std::sin(clock * 71.0f) * shake : 0.0f;
    const f32 sy = shake > 0.0f ? std::sin(clock * 53.0f) * shake * 0.5f : 0.0f;

    // --- capas. El haz de luz va DETRAS de la capa media: pintado encima lavaba al
    // profeta y lo dejaba de color hueso.
    const f32 pw = CINE_PANEL_W * zoom;
    const f32 ph = CINE_PANEL_H * zoom;
    for (int i = 0; i < s.layer_count; i++) {
        if (i == 1 && (s.fx & CineFx_Shaft)) {
            const f32 g = smoothstep((t - s.fx_at) / 1.8f);
            const f32 pulse = 0.86f + 0.14f * std::sin(clock * 2.1f);
            const f32 cx = W * 0.5f + pan * 0.9f;
            for (int b = 0; b < 22; b++) {
                const f32 k = static_cast<f32>(b) / 21.0f;
                const f32 half = (4.0f + k * 20.0f) * (0.55f + 0.45f * g);
                r.draw_rect({cx - half, 0.0f, half * 2.0f, 132.0f},
                            with_alpha(Color{255, 246, 212, 255}, (0.040f - k * 0.0016f) * g * pulse));
            }
        }
        const CineLayer& L = s.layers[i];
        if (L.tex == TEXTURE_NONE) continue;
        const f32 ox = (W - pw) * 0.5f + pan * L.parallax + sx * (0.4f + 0.6f * L.parallax);
        const f32 oy = (H - ph) * 0.5f + sy;
        r.draw_sprite_rect(L.tex, {0.0f, 0.0f, 1600.0f, 720.0f}, {ox, oy, pw, ph}, L.tint);
    }

    // --- efectos por delante
    if (s.fx & CineFx_Shaft) {
        // El charco de luz a sus pies: eso si va encima, es donde la luz le alcanza.
        const f32 g = smoothstep((t - s.fx_at) / 1.8f);
        const f32 pulse = 0.86f + 0.14f * std::sin(clock * 2.1f);
        r.draw_sprite_rect(r.shadow_tex, {0.0f, 0.0f, 64.0f, 32.0f},
                           {W * 0.5f - 44.0f + pan * 0.9f, 104.0f, 88.0f, 30.0f},
                           with_alpha(Color{255, 240, 200, 255}, 0.30f * g * pulse));
    }
    if (s.fx & CineFx_Torches) {
        const f32 flick = 0.5f + 0.5f * std::sin(clock * 9.3f) * std::sin(clock * 3.7f);
        r.draw_rect({0.0f, 0.0f, W, H}, with_alpha(Color{255, 150, 60, 255}, 0.05f + 0.035f * flick));
    }
    if (s.fx & CineFx_Embers) {
        for (int i = 0; i < 26; i++) {
            const f32 seed = static_cast<f32>(i) * 12.9898f;
            const f32 fx = std::fmod(std::sin(seed) * 43758.5453f, 1.0f);
            const f32 sp = 0.5f + std::fmod(std::sin(seed * 1.7f) * 1237.0f, 1.0f) * 0.9f;
            const f32 x = std::fabs(fx) * W;
            const f32 y = H - std::fmod(clock * 14.0f * sp + std::fabs(fx) * H, H + 20.0f);
            const f32 a = 0.30f + 0.35f * std::sin(clock * 3.0f + seed);
            r.draw_rect({x + std::sin(clock * 1.6f + seed) * 3.0f, y, 1.2f, 1.2f},
                        with_alpha(Color{255, 186, 96, 255}, a));
        }
    }
    if (s.fx & CineFx_Flash) {
        const f32 d = t - s.fx_at;
        if (d >= 0.0f && d < 0.75f) {
            const f32 a = (d < 0.09f) ? (d / 0.09f) : (1.0f - (d - 0.09f) / 0.66f);
            r.draw_rect({0.0f, 0.0f, W, H}, with_alpha(Color{255, 252, 240, 255}, clamp01(a) * 0.92f));
        }
    }

    // --- vinieta
    r.draw_sprite_rect(r.vignette_tex, {0.0f, 0.0f, 160.0f, 90.0f},
                       {-8.0f, -8.0f, W + 16.0f, H + 16.0f}, {0, 0, 0, 150});

    // --- titulo (escena 8)
    if (s.fx & CineFx_Title) {
        // Arriba del todo: el profeta ocupa el centro del encuadre y el titulo le caia
        // justo sobre la cara.
        const f32 g = smoothstep((t - s.fx_at) / 1.4f);
        const f32 y = 14.0f - (1.0f - g) * 5.0f;
        r.draw_text_centered("MANTLE", y, with_alpha(Color{247, 226, 168, 255}, g), 3.4f);
        if (t > s.fx_at + 0.9f) {
            const f32 g2 = smoothstep((t - s.fx_at - 0.9f) / 1.2f);
            r.draw_text_centered(tr("El Manto del Profeta", "The Prophet's Mantle"), y + 33.0f,
                                 with_alpha(Color{226, 206, 178, 255}, g2 * 0.95f), 1.0f);
        }
    }

    // --- franja y texto
    if (wrap_count > 0) {
        // 54 bandas = 1 unidad por banda (4 px del target). Con 26 se veian las franjas.
        const int TB = 54;
        for (int i = 0; i < TB; i++) {
            const f32 k = static_cast<f32>(i) / (TB - 1);
            r.draw_rect({0.0f, TEXT_TOP + k * (H - TEXT_TOP), W, (H - TEXT_TOP) / TB + 1.0f},
                        with_alpha(Color::black(), 0.06f + k * 0.62f));
        }
        const f32 block_h = wrap_count * TEXT_LINE_H;
        const f32 y0 = H - 16.0f - block_h;
        for (int i = 0; i < wrap_count; i++) {
            const f32 at = s.line_at[wrap_src[i]] + (i - wrap_src[i]) * 0.18f;
            const f32 g = smoothstep((t - at) / LINE_FADE);
            if (g <= 0.0f) continue;
            r.draw_text_centered(wrapped[i].c_str(), y0 + i * TEXT_LINE_H + (1.0f - g) * 3.0f,
                                 with_alpha(Color{243, 234, 216, 255}, g), TEXT_SCALE);
        }
    }

    // --- aviso de teclas (discreto, parpadeante)
    if (t > 1.6f) {
        const f32 a = 0.30f + 0.16f * std::sin(clock * 2.4f);
        r.draw_text(tr("[ENTER] seguir    [ESC] saltar", "[ENTER] continue    [ESC] skip"), 8.0f, H - 9.0f,
                    with_alpha(Color{214, 204, 190, 255}, a), 0.6f);
    }

    // --- fundido de entrada y salida de cada escena
    f32 veil = 1.0f - smoothstep(t / FADE);
    if (s.duration > 0.0f) {
        veil = std::max(veil, smoothstep((t - (s.duration - FADE)) / FADE));
    }
    if (veil > 0.001f) {
        r.draw_rect({0.0f, 0.0f, W, H}, with_alpha(Color::black(), veil));
    }
}

// =============================================================================
// El prologo
// =============================================================================
void cinematic_prologue(CinematicSystem& c) {
    c.begin();

    // Paletas de cielo por escena (arriba, abajo)
    struct Setup {
        const char* art;
        f32 duration;
        f32 pan_from, pan_to;
        f32 zoom_from, zoom_to;
        Color sky_top, sky_bottom;
        f32 par_far, par_near;
        u8  fx;
        f32 fx_at;
    };

    static const Setup SETUPS[8] = {
        // 1. LA TIERRA PARTIDA — anochecer, paneo largo a la derecha
        {"sprites/cine/01_tierra",  11.0f, -14.0f, 14.0f, 1.00f, 1.05f,
         {28, 24, 52, 255}, {122, 92, 104, 255}, 0.30f, 1.55f, CineFx_None, 0.0f},
        // 2. ACAB — interior, empuje lento hacia el trono
        {"sprites/cine/02_acab",     9.0f,   3.0f, -3.0f, 1.00f, 1.10f,
         {20, 16, 22, 255}, {52, 42, 44, 255},  0.35f, 1.40f, CineFx_None, 0.0f},
        // 3. JEZABEL — el cortejo cruza, paneo lateral
        {"sprites/cine/03_jezabel", 10.5f,  12.0f, -12.0f, 1.02f, 1.06f,
         {58, 34, 74, 255}, {150, 118, 132, 255}, 0.30f, 1.50f, CineFx_None, 0.0f},
        // 4. LA CACERIA — noche, camara casi quieta, antorchas
        {"sprites/cine/04_caceria", 10.0f,  -5.0f,  5.0f, 1.03f, 1.08f,
         {12, 10, 26, 255}, {40, 32, 52, 255},  0.32f, 1.45f, CineFx_Torches, 0.0f},
        // 5. UN HOMBRE DE GALAAD — el haz de luz baja sobre el
        {"sprites/cine/05_galaad",  12.0f,   2.0f, -2.0f, 1.00f, 1.09f,
         {26, 24, 46, 255}, {96, 92, 124, 255}, 0.28f, 1.50f, CineFx_Shaft, 2.8f},
        // 6. DELANTE DE QUIEN ESTOY — el cielo se cierra: fogonazo, trueno y viraje a ocre
        // Paneo de izquierda a derecha: la escena arranca sobre Elias y acaba en el rey
        {"sprites/cine/06_trono",   11.5f,   6.0f, -4.0f, 1.02f, 1.09f,
         {22, 17, 22, 255}, {58, 44, 42, 255},  0.35f, 1.40f,
         static_cast<u8>(CineFx_Flash | CineFx_Shake), 7.6f},
        // 7. EL ARROYO DE QUERIT — amanece seco, paneo suave
        {"sprites/cine/07_querit",  11.0f,  -9.0f,  9.0f, 1.00f, 1.06f,
         {84, 74, 96, 255}, {186, 150, 108, 255}, 0.30f, 1.50f, CineFx_None, 0.0f},
        // 8. TITULO — cielo de fuego y pavesas
        {"sprites/cine/08_titulo",   9.5f,   1.5f, -1.5f, 1.02f, 1.07f,
         {96, 32, 26, 255}, {224, 118, 44, 255}, 0.30f, 1.45f,
         static_cast<u8>(CineFx_Embers | CineFx_Title), 0.9f},
    };

    for (int i = 0; i < 8; i++) {
        const Setup& u = SETUPS[i];
        CineScene s;
        s.art = u.art;
        s.layer_count = CINE_LAYERS;
        s.layers[0].parallax = u.par_far;
        s.layers[1].parallax = 1.0f;
        s.layers[2].parallax = u.par_near;
        s.duration  = u.duration;
        s.pan_from  = u.pan_from;
        s.pan_to    = u.pan_to;
        s.zoom_from = u.zoom_from;
        s.zoom_to   = u.zoom_to;
        s.sky_top    = u.sky_top;
        s.sky_bottom = u.sky_bottom;
        s.fx    = u.fx;
        s.fx_at = u.fx_at;

        // La escena 8 es solo el titulo: no lleva narracion
        if (i < 7) {
            const PrologueScene ps = get_prologue_scene(i);
            for (int k = 0; k < 3; k++) s.lines[k] = ps.lines[k];
        }
        // Las frases entran escalonadas dentro de la duracion de la escena
        s.line_at[0] = 1.0f;
        s.line_at[1] = 1.0f + (u.duration - 3.2f) * 0.30f;
        s.line_at[2] = 1.0f + (u.duration - 3.2f) * 0.58f;

        c.add(s);
    }
}
