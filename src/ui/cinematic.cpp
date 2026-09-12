#include "ui/cinematic.h"
#include "core/i18n.h"
#include "ui/hud.h"          // draw_skip_button: el boton tactil de SALTAR
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
    // Cada begin() reconstruye los CineScene con tex = TEXTURE_NONE, asi que el arte
    // anterior deja de valer: sin este reset, la SEGUNDA cinematica de una sesion
    // (repetir el prologo desde el menu, o cada intro de capitulo) salia sin capas.
    art_loaded = false;
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

void CinematicSystem::unload_art(Renderer& r) {
    // Las intros de capitulo se ven una vez por partida: dejar sus capas en la cache
    // acumularia ~14 MB de VRAM por capitulo a lo largo de una run completa (y el juego
    // sale a movil). El prologo NO se descarga: la opcion "Prologo" del menu lo reusa.
    for (int i = 0; i < scene_count; i++) {
        CineScene& s = scenes[i];
        if (!s.art) continue;
        for (int l = 0; l < s.layer_count; l++) {
            char path[128];
            std::snprintf(path, sizeof(path), "%s_L%d.png", s.art, l);
            r.unload_texture(path);   // tolera paths ya descargados (dos escenas, un arte)
            s.layers[l].tex = TEXTURE_NONE;
        }
    }
    art_loaded = false;
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
        active  = false;
        skipped = true;
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

    // --- titulo. Con `title` vacio es el del juego (escena 8 del prologo); con texto,
    // el rotulo del capitulo en el acento de su acto.
    if (s.fx & CineFx_Title) {
        // Arriba del todo: el profeta ocupa el centro del encuadre y el titulo le caia
        // justo sobre la cara.
        const f32 g = smoothstep((t - s.fx_at) / 1.4f);
        const f32 y = 14.0f - (1.0f - g) * 5.0f;
        if (!s.title.empty()) {
            r.draw_text_centered(s.title.c_str(), y, with_alpha(s.title_color, g), 2.0f);
            if (!s.subtitle.empty() && t > s.fx_at + 0.9f) {
                const f32 g2 = smoothstep((t - s.fx_at - 0.9f) / 1.2f);
                r.draw_text_centered(s.subtitle.c_str(), y + 19.0f,
                                     with_alpha(Color{226, 206, 178, 255}, g2 * 0.95f), 0.9f);
            }
        } else {
            r.draw_text_centered("MANTLE", y, with_alpha(Color{247, 226, 168, 255}, g), 3.4f);
            if (t > s.fx_at + 0.9f) {
                const f32 g2 = smoothstep((t - s.fx_at - 0.9f) / 1.2f);
                r.draw_text_centered(tr("El Manto del Profeta", "The Prophet's Mantle"), y + 33.0f,
                                     with_alpha(Color{226, 206, 178, 255}, g2 * 0.95f), 1.0f);
            }
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

    // --- aviso (discreto, parpadeante). En un telefono no hay ENTER ni ESC: el aviso habla
    // de dedos y el "saltar" es el boton de arriba a la derecha.
    if (t > 1.6f) {
        const f32 a = 0.30f + 0.16f * std::sin(clock * 2.4f);
        r.draw_text(touch_ui ? tr("toca para seguir", "tap to continue")
                             : tr("[ENTER] seguir    [ESC] saltar", "[ENTER] continue    [ESC] skip"),
                    8.0f, H - 9.0f, with_alpha(Color{214, 204, 190, 255}, a), 0.6f);
    }
    if (touch_ui) draw_skip_button(r, clock, skip_pressed);

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

// =============================================================================
// Intros de capitulo
// =============================================================================
// DOS escenas por capitulo: la primera (rotulo + versiculo + dos lineas) panea hacia un
// lado; la segunda (la linea final + el objetivo) deshace el paneo con algo mas de zoom.
// Por defecto las dos van sobre la MISMA estampa (`art`; las texturas se cachean por
// path, asi que la segunda escena sale gratis), pero el plano B puede llevar estampa
// propia (`art_b`): el cap. 1 la lleva, porque su plano A repite la escena 7 del prologo
// (Elias bebiendo en Querit) y el jugador veia la misma imagen tres veces seguidas. Con
// `art_b` se cargan seis capas y load_art/unload_art las tratan una a una.
// `art = nullptr` = estampa aun no entregada: el Engine cae a la carta de texto de
// siempre (cinematic_chapter_ready).
namespace {

struct ChapterCine {
    const char* art;
    const char* art_b;      // Estampa del plano B; nullptr = la misma que `art`
    Color sky_top, sky_bottom;
    f32   par_far, par_near;
    f32   pan;              // Escena A: -pan -> +pan; escena B: +pan -> -0.4*pan
    u8    fx_a, fx_b;       // El rotulo (CineFx_Title) se anade solo en la escena A
    f32   fx_at_b;          // Cuando entra el efecto de la escena B (Shaft del cap. 4)
};

const ChapterCine CHAPTER_CINE[10] = {
    // 1. EL ARROYO QUERIT — amanecer ocre sobre el canon
    // Plano B: el escondite — Elias de pie en el fondo de la garganta, los cuervos bajando
    // por la franja de cielo y dos soldados de Acab pasando de largo por el borde.
    {"sprites/cine/11_cap01_querit", "sprites/cine/31_cap01b_escondite",
     {84, 74, 96, 255}, {186, 150, 108, 255},
     0.30f, 1.50f, 10.0f, CineFx_None, CineFx_None, 0.0f},
    // 2. LA VIUDA DE SAREPTA — atardecer violeta sobre el mar fenicio
    {"sprites/cine/12_cap02_sarepta", nullptr, {64, 38, 72, 255}, {178, 122, 106, 255},
     0.30f, 1.50f, 10.0f, CineFx_None, CineFx_None, 0.0f},
    // 3. EL MONTE CARMELO — mediodia quemado; las pavesas del altar en la escena B
    {"sprites/cine/13_cap03_carmelo", nullptr, {148, 138, 122, 255}, {214, 178, 128, 255},
     0.30f, 1.50f, 11.0f, CineFx_None, CineFx_Embers, 0.0f},
    // 4. LA CUEVA DE HOREB — noche azul cerrada; el silbo apacible es un haz de luz
    {"sprites/cine/14_cap04_horeb", nullptr, {10, 12, 30, 255}, {46, 52, 86, 255},
     0.28f, 1.50f, 8.0f, CineFx_None, CineFx_Shaft, 2.0f},
    // 5. EL LLAMADO — dorado de dia sobre los campos de labor
    {"sprites/cine/15_cap05_llamado", nullptr, {96, 90, 120, 255}, {212, 168, 100, 255},
     0.30f, 1.50f, 11.0f, CineFx_None, CineFx_None, 0.0f},
    // 6. LAS AGUAS DE JERICO — verdoso enfermo
    {"sprites/cine/16_cap06_jerico", nullptr, {40, 52, 44, 255}, {124, 142, 104, 255},
     0.32f, 1.45f, 10.0f, CineFx_None, CineFx_None, 0.0f},
    // 7. EL ACEITE DE LA VIUDA — alba gris sobre la aldea pobre
    {"sprites/cine/17_cap07_aceite", nullptr, {70, 72, 84, 255}, {160, 158, 150, 255},
     0.30f, 1.50f, 10.0f, CineFx_None, CineFx_None, 0.0f},
    // 8. EL HIJO SUNAMITA — interior del aposento, luz de candil
    {"sprites/cine/18_cap08_sunamita", nullptr, {24, 20, 18, 255}, {56, 46, 38, 255},
     0.35f, 1.40f, 4.0f, CineFx_Torches, CineFx_Torches, 0.0f},
    // 9. NAAMAN EL SIRIO — dia claro sobre el Jordan
    {"sprites/cine/19_cap09_naaman", nullptr, {90, 120, 160, 255}, {192, 205, 198, 255},
     0.30f, 1.50f, 11.0f, CineFx_None, CineFx_None, 0.0f},
    // 10. EL EJERCITO CELESTIAL — noche de asedio; los carros de fuego son brasa y luz
    {"sprites/cine/20_cap10_dotan", nullptr, {16, 14, 36, 255}, {90, 54, 42, 255},
     0.32f, 1.45f, 9.0f, CineFx_Torches, static_cast<u8>(CineFx_Embers | CineFx_Shaft), 1.6f},
};

}  // namespace

bool cinematic_chapter_ready(int chapter) {
    if (chapter < 1 || chapter > 10) return false;
    return CHAPTER_CINE[chapter - 1].art != nullptr;
}

void cinematic_chapter(CinematicSystem& c, int chapter) {
    c.begin();
    if (chapter < 1 || chapter > 10) return;
    const ChapterCine& u = CHAPTER_CINE[chapter - 1];
    const ChapterDef   def  = get_chapter_def(chapter);
    const ChapterLore  lore = get_chapter_lore(chapter);

    char title[32];
    std::snprintf(title, sizeof(title), tr("CAPÍTULO %d", "CHAPTER %d"), chapter);
    // Mismo rotulo que story_chapter_card: los capitulos que abren acto llevan el acto
    // (son los dos unicos saltos de protagonista), y el Acto II va en azul.
    std::string subtitle = def.name;
    if (chapter == 1) {
        subtitle = std::string(tr("ACTO I  ·  ", "ACT I  ·  ")) + def.name;
    } else if (chapter == Config::CHAPTERS_ACT_I + 1) {
        subtitle = std::string(tr("ACTO II  ·  ", "ACT II  ·  ")) + def.name;
    }
    const Color accent = (chapter > Config::CHAPTERS_ACT_I) ? Color{150, 205, 225, 255}
                                                            : Color{247, 226, 168, 255};

    CineScene s;
    s.art = u.art;
    s.layer_count = CINE_LAYERS;
    s.layers[0].parallax = u.par_far;
    s.layers[1].parallax = 1.0f;
    s.layers[2].parallax = u.par_near;
    s.sky_top    = u.sky_top;
    s.sky_bottom = u.sky_bottom;

    // Escena A: rotulo + versiculo + las dos primeras lineas. La referencia del pasaje
    // (lore.verse.ref) NO se pinta: regla del proyecto.
    s.duration  = 10.5f;
    s.pan_from  = -u.pan;
    s.pan_to    = u.pan;
    s.zoom_from = 1.00f;
    s.zoom_to   = 1.06f;
    s.fx    = static_cast<u8>(u.fx_a | CineFx_Title);
    s.fx_at = 0.6f;
    s.title       = title;
    s.subtitle    = subtitle;
    s.title_color = accent;
    s.lines[0] = lore.verse.text;
    s.lines[1] = lore.lines[0];
    s.lines[2] = lore.lines[1];
    s.line_at[0] = 1.0f;
    s.line_at[1] = 1.0f + (s.duration - 3.2f) * 0.30f;
    s.line_at[2] = 1.0f + (s.duration - 3.2f) * 0.58f;
    c.add(s);

    // Escena B: deshace el paneo (sobre su propia estampa si el capitulo la tiene); la
    // narracion final y el objetivo.
    CineScene b;
    b.art = u.art_b ? u.art_b : u.art;
    b.layer_count = CINE_LAYERS;
    b.layers[0].parallax = u.par_far;
    b.layers[1].parallax = 1.0f;
    b.layers[2].parallax = u.par_near;
    b.sky_top    = u.sky_top;
    b.sky_bottom = u.sky_bottom;
    b.duration  = 9.0f;
    b.pan_from  = u.pan;
    b.pan_to    = -u.pan * 0.4f;
    b.zoom_from = 1.04f;
    b.zoom_to   = 1.10f;
    b.fx    = u.fx_b;
    b.fx_at = u.fx_at_b;
    // Compactar: layout() corta en el primer null, y el objetivo va SIEMPRE el ultimo
    int k = 0;
    for (int i = 2; i < 4 && k < 2; i++) {
        if (lore.lines[i]) b.lines[k++] = lore.lines[i];
    }
    b.lines[k] = lore.objective;
    b.line_at[0] = 1.0f;
    b.line_at[1] = 1.0f + (b.duration - 3.2f) * 0.30f;
    b.line_at[2] = 1.0f + (b.duration - 3.2f) * 0.58f;
    c.add(b);
}

// =============================================================================
// El Ascenso (2 Reyes 2:8-14)
// =============================================================================
// La escena que da nombre al juego, asi que es la mas trabajada: seis escenas sobre
// cuatro estampas (Jordan abierto A/B, carro de fuego, torbellino, manto caido A/B).
// El texto es el de get_ascension_lore (con la rama de `asked_double`: sin doble porcion
// la peticion es otra) mas la linea de la herencia: no se escribio una palabra nueva
// aqui, igual que en las intros de capitulo.
namespace {

// nullptr = estampa no entregada (mismo interruptor que CHAPTER_CINE)
const char* ASC_ART[4] = {
    "sprites/cine/21_asc01_jordan",
    "sprites/cine/22_asc02_carro",
    "sprites/cine/23_asc03_torbellino",
    "sprites/cine/24_asc04_manto",
};

// Una escena del Ascenso con los valores comunes ya puestos
CineScene asc_scene(int panel, f32 duration, f32 pan_from, f32 pan_to,
                    f32 zoom_from, f32 zoom_to, Color sky_top, Color sky_bottom) {
    CineScene s;
    s.art = ASC_ART[panel];
    s.layer_count = CINE_LAYERS;
    s.layers[0].parallax = 0.30f;
    s.layers[1].parallax = 1.0f;
    s.layers[2].parallax = 1.50f;
    s.duration  = duration;
    s.pan_from  = pan_from;
    s.pan_to    = pan_to;
    s.zoom_from = zoom_from;
    s.zoom_to   = zoom_to;
    s.sky_top    = sky_top;
    s.sky_bottom = sky_bottom;
    s.line_at[0] = 1.0f;
    s.line_at[1] = 1.0f + (duration - 3.2f) * 0.30f;
    s.line_at[2] = 1.0f + (duration - 3.2f) * 0.58f;
    return s;
}

}  // namespace

bool cinematic_ascension_ready() {
    for (int i = 0; i < 4; i++)
        if (!ASC_ART[i]) return false;
    return true;
}

void cinematic_ascension(CinematicSystem& c, bool asked_double) {
    c.begin();
    const ChapterLore jordan = get_ascension_lore(0, asked_double);   // el cruce y la peticion
    const ChapterLore carro  = get_ascension_lore(1);   // el carro y el torbellino
    const ChapterLore manto  = get_ascension_lore(2);   // el manto caido
    const Color accent = {245, 220, 160, 255};          // el dorado de story_ascension

    // 1. EL JORDAN ABIERTO (A) — rotulo + el cruce en seco + la peticion
    {
        CineScene s = asc_scene(0, 11.0f, -10.0f, 10.0f, 1.00f, 1.06f,
                                {84, 80, 118, 255}, {210, 170, 116, 255});
        s.fx    = CineFx_Title;
        s.fx_at = 0.6f;
        s.title       = tr("EL ASCENSO", "THE ASCENT");
        s.subtitle    = tr("El torbellino del Jordán", "The whirlwind of the Jordan");
        s.title_color = accent;
        s.lines[0] = jordan.lines[0];        // cruzaron el Jordan en seco
        s.lines[1] = jordan.verse.text;      // «Pide lo que quieras...»
        s.lines[2] = jordan.lines[1];        // «...una doble porcion de tu espiritu»
        c.add(s);
    }
    // 2. EL JORDAN ABIERTO (B) — el silencio antes del carro: una sola frase, mas cerca
    {
        CineScene s = asc_scene(0, 7.5f, 10.0f, -4.0f, 1.05f, 1.12f,
                                {84, 80, 118, 255}, {210, 170, 116, 255});
        s.lines[0] = jordan.lines[2];        // «Cosa dificil has pedido...»
        c.add(s);
    }
    // 3. EL CARRO DE FUEGO — el cielo arde; el fogonazo entra CON el versiculo
    {
        CineScene s = asc_scene(1, 11.5f, 4.0f, -4.0f, 1.00f, 1.12f,
                                {70, 18, 20, 255}, {235, 140, 52, 255});
        s.fx    = static_cast<u8>(CineFx_Flash | CineFx_Shake | CineFx_Embers);
        s.lines[0] = carro.lines[0];         // «...he aqui un carro de fuego.»
        s.lines[1] = carro.verse.text;       // «Un carro de fuego con caballos de fuego...»
        s.fx_at = s.line_at[1];
        c.add(s);
    }
    // 4. EL TORBELLINO — Elias sube; el haz de luz baja sobre el remolino
    {
        CineScene s = asc_scene(2, 11.0f, -6.0f, 6.0f, 1.02f, 1.08f,
                                {30, 20, 48, 255}, {190, 96, 44, 255});
        s.fx    = static_cast<u8>(CineFx_Shaft | CineFx_Embers);
        s.fx_at = 1.2f;
        s.lines[0] = carro.lines[1];         // «¡Padre mio, padre mio...!»
        s.lines[1] = carro.lines[2];         // «Y nunca mas le vio.»
        c.add(s);
    }
    // 5. EL MANTO CAIDO (A) — Eliseo lo alza y vuelve al Jordan
    {
        CineScene s = asc_scene(3, 10.5f, -9.0f, 9.0f, 1.00f, 1.06f,
                                {70, 70, 96, 255}, {188, 168, 140, 255});
        s.lines[0] = manto.lines[0];         // alzo el manto que se le habia caido
        s.lines[1] = manto.verse.text;       // «¿Donde esta Jehova, el Dios de Elias?»
        s.lines[2] = manto.lines[1];         // golpeo las aguas, y se apartaron
        c.add(s);
    }
    // 6. EL MANTO CAIDO (B) — el cierre y lo que hereda el jugador
    {
        CineScene s = asc_scene(3, 9.0f, 9.0f, -3.5f, 1.04f, 1.12f,
                                {70, 70, 96, 255}, {188, 168, 140, 255});
        s.lines[0] = manto.lines[2];         // «El manto no era suyo. Le fue prestado.»
        // La misma linea que pone el pie de story_ascension: es la unica informacion
        // de partida de toda la cinematica, y por eso va la ultima.
        s.lines[1] = get_ascension_inheritance_line(asked_double);
        c.add(s);
    }
}
