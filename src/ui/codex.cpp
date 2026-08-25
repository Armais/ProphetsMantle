// =============================================================================
// MANTLE - Códice
// =============================================================================

#include "core/i18n.h"
#include "ui/codex.h"
#include "game/rooms.h"
#include <algorithm>
#include <cstdio>

static constexpr int CHAPTER_COUNT = Config::CHAPTERS_ACT_I + Config::CHAPTERS_ACT_II;
static constexpr int ENEMY_COUNT   = static_cast<int>(EnemyType::COUNT);
static constexpr int ACH_COUNT     = static_cast<int>(AchievementID::COUNT);
static constexpr int VISIBLE_ROWS  = 8;

int CodexScreen::entry_count(const SaveData& save, const ItemRegistry& registry) const {
    (void)save;
    switch (tab) {
        case CodexTab::Chapters: return CHAPTER_COUNT;
        case CodexTab::Bestiary: return ENEMY_COUNT;
        case CodexTab::Items:    return std::max(1, static_cast<int>(registry.items.size()));
        case CodexTab::Achievements: return ACH_COUNT;
        default:                 return static_cast<int>(MiracleID::COUNT);
    }
}

bool CodexScreen::update(Input& input, const SaveData& save, const ItemRegistry& registry) {
    const int n = entry_count(save, registry);

    if (input.is_action_just_pressed(Action::MoveLeft)) {
        tab = static_cast<CodexTab>((static_cast<int>(tab) + static_cast<int>(CodexTab::COUNT) - 1) %
                                    static_cast<int>(CodexTab::COUNT));
        selected = 0; scroll = 0;
    }
    if (input.is_action_just_pressed(Action::MoveRight)) {
        tab = static_cast<CodexTab>((static_cast<int>(tab) + 1) % static_cast<int>(CodexTab::COUNT));
        selected = 0; scroll = 0;
    }
    if (input.is_action_just_pressed(Action::MoveUp))   selected = (selected - 1 + n) % n;
    if (input.is_action_just_pressed(Action::MoveDown)) selected = (selected + 1) % n;

    // La lista sigue al cursor sin saltar de golpe
    if (selected < scroll) scroll = selected;
    if (selected >= scroll + VISIBLE_ROWS) scroll = selected - VISIBLE_ROWS + 1;
    scroll = std::max(0, std::min(scroll, std::max(0, n - VISIBLE_ROWS)));

    return input.is_action_just_pressed(Action::Cancel) ||
           input.is_action_just_pressed(Action::Confirm);
}

void CodexScreen::render(Renderer& renderer, const SaveData& save,
                         const ItemRegistry& registry, u32 tileset, u32 portraits) const {
    (void)tileset;
    const f32 W = static_cast<f32>(Config::GAME_WIDTH);
    const f32 H = static_cast<f32>(Config::GAME_HEIGHT);
    renderer.clear({12, 10, 16, 255});
    if (renderer.vignette_tex != TEXTURE_NONE)
        renderer.draw_sprite_rect(renderer.vignette_tex, {0, 0, 160, 90}, {-8, -8, W + 16, H + 16}, {0, 0, 0, 150});

    renderer.draw_text_centered(tr("EL CÓDICE", "THE CODEX"), 10, {240, 215, 150, 255}, 1.4f);

    // Pestañas
    const char* tabs[] = {tr("CAPÍTULOS", "CHAPTERS"), tr("BESTIARIO", "BESTIARY"), tr("MILAGROS", "MIRACLES"), tr("OBJETOS", "ITEMS"), tr("LOGROS", "ACHIEVEMENTS")};
    // Con cinco pestañas el margen se queda corto: se arranca más a la izquierda y se
    // aprieta la separación para que la última no se salga por los 320 de ancho.
    f32 tx = 8.0f;
    for (int i = 0; i < static_cast<int>(CodexTab::COUNT); i++) {
        bool on = (i == static_cast<int>(tab));
        f32 tw = renderer.measure_text(tabs[i], 0.8f) + 12.0f;
        if (on) renderer.draw_rect({tx, 26.0f, tw, 12.0f}, {70, 56, 34, 235});
        renderer.draw_rect({tx, on ? 37.0f : 37.5f, tw, on ? 1.5f : 0.5f}, on ? Color{245, 205, 90, 255} : Color{80, 72, 64, 200});
        renderer.draw_text(tabs[i], tx + 6.0f, 28.0f, on ? Color{250, 225, 165, 255} : Color{140, 132, 122, 230}, 0.8f);
        tx += tw + 3.0f;
    }

    // Entrada seleccionada: nombre, si se conoce, y su pasaje
    const int n = entry_count(save, registry);
    f32 y = 46.0f;
    int shown = 0;
    for (int i = scroll; i < n && shown < VISIBLE_ROWS; i++, shown++) {
        bool known = false;
        const char* name = "???";
        char buf[64];
        Color known_color = {206, 196, 182, 240};

        switch (tab) {
            case CodexTab::Chapters: {
                known = codex_knows_chapter(save, i + 1);
                if (known) {
                    snprintf(buf, sizeof(buf), "%2d. %s", i + 1, get_chapter_def(i + 1).name);
                    name = buf;
                } else {
                    snprintf(buf, sizeof(buf), "%2d. ??????", i + 1);
                    name = buf;
                }
                break;
            }
            case CodexTab::Bestiary: {
                EnemyType t = static_cast<EnemyType>(i);
                known = codex_knows_enemy(save, t);
                name = known ? get_enemy_def(t).name : "??????";
                break;
            }
            case CodexTab::Items: {
                const ItemDef* d = (i < static_cast<int>(registry.items.size())) ? &registry.items[i] : nullptr;
                known = d && codex_knows_item(save, d->id);
                name = known ? d->name : "??????";
                if (d) known_color = item_rarity_color(d->rarity);   // la rareza se lee en la fila
                break;
            }
            case CodexTab::Achievements: {
                AchievementID a = static_cast<AchievementID>(i);
                known = knows_achievement(save, a);
                // Un logro bloqueado SÍ enseña su nombre: es lo que da algo que perseguir.
                // Lo que se oculta es el "???" de las otras pestañas, no esto.
                // Marcador en ASCII puro a proposito: el atlas de glifos se hornea de
                // la fuente al arrancar y los circulos de dibujo (U+25CF/U+25CB) no
                // estan, asi que salian como un hueco en blanco.
                snprintf(buf, sizeof(buf), "%s %s", known ? "[*]" : "[ ]",
                         get_achievement_def(a).name);
                name = buf;
                known_color = known ? Color{245, 205, 90, 255} : Color{125, 118, 110, 220};
                known = true;   // la fila siempre se lee; el color dice si está conseguido
                break;
            }
            default: {
                MiracleID m = static_cast<MiracleID>(i);
                known = codex_knows_miracle(save, m);
                name = known ? get_miracle_def(m).name : "??????";
                break;
            }
        }

        bool on = (i == selected);
        if (on) renderer.draw_rect({14.0f, y - 1.0f, W - 28.0f, 10.0f}, {58, 46, 30, 220});
        Color c = !known ? Color{95, 90, 84, 200}
                         : (on && tab != CodexTab::Items ? Color{250, 228, 170, 255} : known_color);
        renderer.draw_text(on ? "> " : "  ", 16.0f, y, c, 0.8f);
        renderer.draw_text(name, 26.0f, y, c, 0.8f);
        y += 10.0f;
    }

    // Panel inferior con el pasaje de la entrada seleccionada
    const f32 py = 128.0f;
    renderer.draw_rect({12.0f, py, W - 24.0f, 40.0f}, {22, 18, 26, 240});
    renderer.draw_rect({12.0f, py, W - 24.0f, 1.0f}, {150, 122, 80, 200});

    Scripture s = {"", nullptr};
    bool known = false;
    const char* effect = nullptr;   // solo objetos: que hace, encima del pasaje
    switch (tab) {
        case CodexTab::Chapters:
            known = codex_knows_chapter(save, selected + 1);
            if (known) s = get_chapter_lore(selected + 1).verse;
            break;
        case CodexTab::Bestiary: {
            EnemyType t = static_cast<EnemyType>(selected);
            known = codex_knows_enemy(save, t);
            if (known) s = get_enemy_lore(t);
            break;
        }
        case CodexTab::Items: {
            const ItemDef* d = (selected < static_cast<int>(registry.items.size())) ? &registry.items[selected] : nullptr;
            known = d && codex_knows_item(save, d->id);
            if (known) { s = get_item_scripture(d->id); effect = d->description; }
            break;
        }
        case CodexTab::Achievements: {
            AchievementID a = static_cast<AchievementID>(selected);
            AchievementDef d = get_achievement_def(a);
            known = true;
            effect = d.name;
            // El "pasaje" de un logro es cómo se consigue: sin eso, la mitad de la
            // lista serían nombres bonitos que nadie sabe cómo perseguir.
            // Scripture es {ref, text} — el orden importa: la pantalla dibuja `text` y
            // NUNCA `ref` (regla del proyecto: las referencias biblicas no salen en
            // pantalla). Poner la cadena en el primer hueco la hacia invisible.
            s = {nullptr, knows_achievement(save, a) ? tr("Conseguido.", "Unlocked.") : d.description};
            break;
        }
        default: {
            MiracleID m = static_cast<MiracleID>(selected);
            known = codex_knows_miracle(save, m);
            if (known) s = get_miracle_scripture(m);
            break;
        }
    }

    // Retrato del bicho seleccionado, a la izquierda del panel. Es lo que convierte una
    // lista de nombres en un bestiario: el texto se corre a su derecha para dejarle sitio.
    f32 text_x = 20.0f;
    f32 text_w = W - 40.0f;
    if (tab == CodexTab::Bestiary && known && portraits != TEXTURE_NONE &&
        selected < static_cast<int>(EnemyType::COUNT)) {
        const f32 pw = 34.0f;
        renderer.draw_rect({15.0f, py + 3.0f, pw, pw}, {14, 11, 18, 235});
        renderer.draw_sprite_rect(portraits, Portrait::src(selected),
                                  {15.0f, py + 3.0f, pw, pw}, Color::white());
        renderer.draw_rect({15.0f, py + 3.0f, pw, 0.7f}, {150, 122, 80, 170});
        text_x = 15.0f + pw + 6.0f;
        text_w = W - text_x - 16.0f;
    }

    if (!known) {
        renderer.draw_text_centered(tr("Aún no has visto esto.", "You have not seen this yet."), py + 15.0f, {120, 112, 104, 220}, 0.75f);
    } else {
        f32 ty = py + 5.0f;
        int room = 4;
        if (effect) {
            renderer.draw_text(effect, text_x, ty, {245, 205, 90, 240}, 0.72f);
            ty += 9.0f;
            room = 3;
        }
        std::string lines[4];
        int ln = renderer.wrap_text(s.text ? s.text : "", text_w, 0.72f, lines, room);
        for (int i = 0; i < ln; i++)
            renderer.draw_text(lines[i].c_str(), text_x, ty + i * 8.0f, {226, 216, 200, 245}, 0.72f);
    }

    // Contador de descubrimiento: las cuatro pestañas, en el mismo orden en que salen
    int found = 0, total = 0;
    for (int i = 0; i < CHAPTER_COUNT; i++) { total++; if (codex_knows_chapter(save, i + 1)) found++; }
    for (int i = 0; i < ENEMY_COUNT; i++)   { total++; if (codex_knows_enemy(save, static_cast<EnemyType>(i))) found++; }
    for (int i = 0; i < static_cast<int>(MiracleID::COUNT); i++) { total++; if (codex_knows_miracle(save, static_cast<MiracleID>(i))) found++; }
    for (const auto& it : registry.items)   { total++; if (codex_knows_item(save, it.id)) found++; }
    char count_buf[48];
    if (tab == CodexTab::Achievements) {
        int got = 0;
        for (int i = 0; i < ACH_COUNT; i++)
            if (knows_achievement(save, static_cast<AchievementID>(i))) got++;
        snprintf(count_buf, sizeof(count_buf), tr("Logros  %d / %d", "Achievements  %d / %d"), got, ACH_COUNT);
    } else {
        snprintf(count_buf, sizeof(count_buf), tr("Descubierto  %d / %d", "Discovered  %d / %d"), found, total);
    }
    // Arriba a la derecha: abajo solo cabe una linea y ahi va la ayuda de teclas
    // (el contador y la ayuda se pisaban con el panel del pasaje).
    renderer.draw_text(count_buf, W - 14.0f - renderer.measure_text(count_buf, 0.7f), 12.0f,
                       {170, 150, 115, 235}, 0.7f);

    renderer.draw_text_centered(tr("[A/D] pestaña   [W/S] mover   [ESC] volver", "[A/D] tab   [W/S] move   [ESC] back"), 171.0f,
                                {150, 140, 130, 200}, 0.65f);
}
