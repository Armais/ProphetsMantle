// =============================================================================
// MANTLE - Pantallas de historia
// =============================================================================

#include "core/i18n.h"
#include "ui/story.h"
#include "game/rooms.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

// Alto de linea por tipo (unidades de juego)
static f32 line_height(StorySystem::LineKind k) {
    switch (k) {
        case StorySystem::Verse:    return 9.0f;
        case StorySystem::Body:     return 8.0f;
        default:                    return 10.0f;   // Footer
    }
}

static f32 line_scale(StorySystem::LineKind k) {
    switch (k) {
        case StorySystem::Verse:    return 0.9f;
        case StorySystem::Body:     return 0.75f;
        default:                    return 0.85f;
    }
}

// =============================================================================
// Construccion
// =============================================================================
void StorySystem::begin() {
    page_count = 0;
    page = 0;
    timer = 0.0f;
    clock = 0.0f;
    line_count = 0;
    active = false;   // Engine::play_story lo activa cuando las paginas ya estan encoladas
    for (auto& p : pages) p = StoryPage{};
}

void StorySystem::add(const StoryPage& p) {
    if (page_count < MAX_PAGES) pages[page_count++] = p;
}

void StorySystem::layout(const Renderer& renderer) {
    line_count = 0;
    if (page >= page_count) return;
    const StoryPage& p = pages[page];

    auto push = [&](const char* text, LineKind kind, f32 max_w) {
        if (!text || !*text || line_count >= MAX_LINES) return;
        std::string buf[MAX_LINES];
        int n = renderer.wrap_text(text, max_w, line_scale(kind), buf, MAX_LINES - line_count);
        for (int i = 0; i < n && line_count < MAX_LINES; i++) {
            wrapped[line_count] = buf[i];
            kinds[line_count] = kind;
            line_count++;
        }
    };

    if (p.verse.text) {
        // Comillas angulares: la cita se lee como cita aunque el jugador no vea la referencia
        std::string quoted = std::string("«") + p.verse.text + "»";
        push(quoted.c_str(), Verse, 252.0f);
    }
    for (const char* b : p.body) push(b, Body, 282.0f);
    push(p.footer, Footer, 282.0f);
}

// =============================================================================
// Update
// =============================================================================
bool StorySystem::update(Input& input, f32 dt, const Renderer& renderer) {
    if (!active) return true;
    if (line_count == 0) layout(renderer);
    timer += dt;
    clock += dt;

    bool advance = input.is_action_just_pressed(Action::Confirm) ||
                   input.is_action_just_pressed(Action::Attack);
    bool skip_all = input.is_action_just_pressed(Action::Cancel) ||
                    input.is_action_just_pressed(Action::Pause);

    // Los playtests con --script no pueden quedarse clavados en la historia
    if (auto_timer > 0.0f) {
        auto_timer -= dt;
        if (auto_timer <= 0.0f) { advance = true; auto_timer = 1.0f; }
    }

    if (skip_all) {
        active = false;
        return true;
    }
    if (advance) {
        if (!all_revealed()) {
            // Primer ENTER: mostrar la pagina entera de golpe
            timer = reveal_end();
            return false;
        }
        page++;
        if (page >= page_count) {
            active = false;
            return true;
        }
        timer = 0.0f;
        layout(renderer);
    }
    return false;
}

// =============================================================================
// Render
// =============================================================================
void StorySystem::render(Renderer& renderer) const {
    const f32 W = static_cast<f32>(Config::GAME_WIDTH);
    const f32 H = static_cast<f32>(Config::GAME_HEIGHT);
    if (page >= page_count) return;
    const StoryPage& p = pages[page];

    // Fondo: pergamino nocturno (calido arriba, negro abajo) + vineta
    renderer.clear({8, 6, 10, 255});
    constexpr int BANDS = 30;
    for (int i = 0; i < BANDS; i++) {
        f32 k = 1.0f - static_cast<f32>(i) / BANDS;
        renderer.draw_rect({0, H * i / BANDS, W, H / BANDS + 0.5f},
                           {static_cast<u8>(46 * k), static_cast<u8>(26 * k), static_cast<u8>(20 * k), 255});
    }
    if (renderer.vignette_tex != TEXTURE_NONE)
        renderer.draw_sprite_rect(renderer.vignette_tex, {0, 0, 160, 90}, {-8, -8, W + 16, H + 16}, {0, 0, 0, 170});

    auto alpha_at = [&](f32 delay) -> f32 {
        return std::max(0.0f, std::min(1.0f, (timer - delay) / LINE_FADE));
    };
    auto fade = [](Color c, f32 a) {
        return Color{c.r, c.g, c.b, static_cast<u8>(c.a * a)};
    };

    // Titulo + subtitulo
    f32 ta = alpha_at(0.0f);
    if (!p.title.empty() && ta > 0.0f) {
        f32 tw = renderer.measure_text(p.title.c_str(), 2.0f);
        renderer.draw_text(p.title.c_str(), (W - tw) * 0.5f + 1.0f, 20.0f + 1.0f, fade({0, 0, 0, 170}, ta), 2.0f);
        renderer.draw_text(p.title.c_str(), (W - tw) * 0.5f, 20.0f, fade(p.accent, ta), 2.0f);
    }
    f32 sa = alpha_at(0.12f);
    if (!p.subtitle.empty() && sa > 0.0f)
        renderer.draw_text_centered(p.subtitle.c_str(), 42.0f, fade({198, 178, 146, 235}, sa), 1.0f);

    // Filete bajo la cabecera
    f32 ra = alpha_at(0.2f);
    if (ra > 0.0f) renderer.draw_rect({W * 0.5f - 46.0f, 53.0f, 92.0f, 1.0f}, fade({150, 120, 80, 190}, ra));

    // La cita y el cuerpo se separan con un respiro: sin la referencia debajo quedarian pegados
    auto gap_after = [&](int i) -> f32 {
        return (kinds[i] == Verse && i + 1 < line_count && kinds[i + 1] != Verse) ? 4.0f : 0.0f;
    };

    // Bloque de texto centrado verticalmente entre la cabecera y el pie
    f32 total = 0.0f;
    for (int i = 0; i < line_count; i++) total += line_height(kinds[i]) + gap_after(i);
    const f32 top = 60.0f, bottom = 158.0f;
    f32 y = top + std::max(0.0f, (bottom - top - total) * 0.5f);

    for (int i = 0; i < line_count; i++) {
        f32 a = alpha_at(LINE_DELAY0 + i * LINE_STAGGER);
        if (a > 0.0f) {
            Color c;
            switch (kinds[i]) {
                case Verse:    c = {240, 226, 196, 255}; break;
                case Footer:   c = p.accent; break;
                default:       c = {206, 198, 186, 245}; break;
            }
            // Las lineas suben un poco al entrar
            renderer.draw_text_centered(wrapped[i].c_str(), y + (1.0f - a) * 3.0f, fade(c, a), line_scale(kinds[i]));
        }
        y += line_height(kinds[i]) + gap_after(i);
    }

    // Pie: solo cuando ya se ha revelado todo
    if (all_revealed()) {
        f32 blink = 0.55f + 0.45f * std::sin(clock * 3.4f);
        const char* prompt = (page + 1 < page_count) ? tr("[ENTER] seguir leyendo", "[ENTER] keep reading") : tr("[ENTER] continuar", "[ENTER] continue");
        renderer.draw_text_centered(prompt, 166.0f, {210, 190, 150, static_cast<u8>(200 * blink)}, 0.8f);
        if (page_count > 1) {
            char pages_lbl[24];
            snprintf(pages_lbl, sizeof(pages_lbl), "%d / %d", page + 1, page_count);
            renderer.draw_text_centered(pages_lbl, 175.0f, {120, 105, 85, 180}, 0.7f);
        }
    }
}

// =============================================================================
// Constructores de secuencias
// =============================================================================
void story_prologue(StorySystem& s) {
    ChapterLore l = get_prologue_lore();
    StoryPage p;
    p.title = "MANTLE";
    p.subtitle = tr("Israel, siglo IX a.C.", "Israel, 9th century BC");
    p.verse = l.verse;
    for (int i = 0; i < 4; i++) p.body[i] = l.lines[i];
    p.footer = tr("Su nombre es Elías.", "His name is Elijah.");
    s.add(p);
}

void story_chapter_card(StorySystem& s, int chapter) {
    ChapterDef def = get_chapter_def(chapter);
    ChapterLore l = get_chapter_lore(chapter);
    char title[32];
    snprintf(title, sizeof(title), tr("CAPÍTULO %d", "CHAPTER %d"), chapter);

    StoryPage p;
    p.title = title;
    p.subtitle = def.name;
    p.verse = l.verse;
    for (int i = 0; i < 4; i++) p.body[i] = l.lines[i];
    p.footer = l.objective;
    // Acto II en azul/verde, Acto I en oro (paleta del GDD). Los capitulos que abren
    // acto llevan el acto en el subtitulo: son los dos unicos saltos de protagonista.
    if (chapter == 1) {
        p.subtitle = std::string(tr("ACTO I  ·  ", "ACT I  ·  ")) + def.name;
    } else if (chapter == Config::CHAPTERS_ACT_I + 1) {
        p.subtitle = std::string(tr("ACTO II  ·  ", "ACT II  ·  ")) + def.name;
    }
    if (chapter > Config::CHAPTERS_ACT_I) p.accent = {150, 205, 225, 255};
    s.add(p);
}

void story_chapter_outro(StorySystem& s, int chapter) {
    ChapterDef def = get_chapter_def(chapter);
    ChapterLore l = get_chapter_lore(chapter);

    StoryPage p;
    p.title = tr("ESTÁ ESCRITO", "IT IS WRITTEN");
    p.subtitle = def.name;
    p.verse = {nullptr, l.outro};
    p.footer = nullptr;
    if (chapter > Config::CHAPTERS_ACT_I) p.accent = {150, 205, 225, 255};
    s.add(p);
}

void story_ascension(StorySystem& s) {
    for (int page = 0; page < 3; page++) {
        ChapterLore l = get_ascension_lore(page);
        StoryPage p;
        p.title = (page == 0) ? tr("EL ASCENSO", "THE ASCENT") : "";
        p.subtitle = (page == 0) ? tr("El torbellino del Jordán", "The whirlwind of the Jordan") : "";
        p.verse = l.verse;
        for (int i = 0; i < 4; i++) p.body[i] = l.lines[i];
        if (page == 2) p.footer = tr("Heredas la mitad de su fe, su manto y dos milagros.", "You inherit half his faith, his mantle and two miracles.");
        p.accent = {245, 220, 160, 255};
        s.add(p);
    }
}

void story_epilogue(StorySystem& s, bool joash_full) {
    ChapterLore l = get_epilogue_lore();
    StoryPage p;
    p.title = tr("EL LEGADO", "THE LEGACY");
    p.subtitle = tr("Israel ha visto la mano de Dios", "Israel has seen the hand of God");
    p.verse = l.verse;
    for (int i = 0; i < 4; i++) p.body[i] = l.lines[i];
    p.footer = joash_full
        ? tr("Golpeaste la tierra hasta el final, y Siria fue consumida.",
             "You smote the ground to the end, and Syria was consumed.")
        : tr("Te detuviste a la tercera. Siria fue herida tres veces, y volvio.",
             "You stayed at the third. Syria was smitten thrice, and came again.");
    p.accent = {225, 220, 245, 255};
    s.add(p);
}
