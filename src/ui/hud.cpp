// =============================================================================
// MANTLE - HUD Implementation
// =============================================================================

#include "ui/hud.h"
#include "core/i18n.h"
#include "core/config.h"
#include "game/lore.h"
#include <cstdio>
#include <cstring>
#include <cmath>

// Layout (game units): icon badge on the left, glossy bar with a dark frame, value on the right
static constexpr f32 BAR_X = 20.0f;
static constexpr f32 BAR_W = 74.0f;
static const Color FRAME_DARK  = {22, 16, 26, 255};
static const Color PANEL_BG    = {18, 14, 24, 150};
static const Color TEXT_LIGHT  = {245, 238, 220, 255};

// Dark soft disc (reuses the shadow blob) + a tileset icon on top: a "badge"
static void draw_badge(Renderer& renderer, u32 tileset, TileSprite icon, f32 cx, f32 cy, f32 size) {
    if (renderer.shadow_tex != TEXTURE_NONE)
        renderer.draw_sprite_rect(renderer.shadow_tex, {0, 0, 64, 32}, {cx - size * 0.75f, cy - size * 0.75f, size * 1.5f, size * 1.5f}, {0, 0, 0, 170});
    if (tileset != TEXTURE_NONE)
        renderer.draw_sprite_rect(tileset, get_tile_src(icon), {cx - size * 0.5f, cy - size * 0.5f, size, size});
}

// Framed bar: thick dark frame, dark trough, fill with a lighter top band (gloss) and a
// thin highlight line. `ratio` 0..1.
static void draw_bar(Renderer& renderer, Rect r, f32 ratio, Color fill, f32 frame = 1.0f) {
    ratio = std::max(0.0f, std::min(1.0f, ratio));
    renderer.draw_rect({r.x - frame, r.y - frame, r.w + 2 * frame, r.h + 2 * frame}, FRAME_DARK);
    renderer.draw_rect(r, {48, 42, 56, 255});
    if (ratio > 0.0f) {
        f32 w = r.w * ratio;
        Color dark = {static_cast<u8>(fill.r * 0.72f), static_cast<u8>(fill.g * 0.72f), static_cast<u8>(fill.b * 0.72f), 255};
        Color lite = {static_cast<u8>(std::min(255.0f, fill.r * 1.25f + 20)), static_cast<u8>(std::min(255.0f, fill.g * 1.25f + 20)),
                      static_cast<u8>(std::min(255.0f, fill.b * 1.25f + 20)), 255};
        renderer.draw_rect({r.x, r.y, w, r.h}, dark);
        renderer.draw_rect({r.x, r.y, w, r.h * 0.55f}, fill);
        renderer.draw_rect({r.x, r.y, w, std::max(0.5f, r.h * 0.22f)}, lite);
    }
    // subtle top highlight over the whole trough
    renderer.draw_rect({r.x, r.y, r.w, 0.5f}, {255, 255, 255, 40});
}

// Greedy word wrap into fixed-size lines; returns how many lines were produced.
static int wrap_text(const Renderer& renderer, const char* text, f32 max_w, f32 scale,
                     char out[][40], int max_lines) {
    int lines = 0;
    const char* p = text;
    char cur[40] = {0};
    while (*p && lines < max_lines) {
        const char* space = std::strchr(p, ' ');
        size_t wlen = space ? static_cast<size_t>(space - p) : std::strlen(p);
        char word[40];
        if (wlen >= sizeof(word)) wlen = sizeof(word) - 1;
        std::memcpy(word, p, wlen);
        word[wlen] = 0;

        char candidate[80];
        snprintf(candidate, sizeof(candidate), "%s%s%s", cur, cur[0] ? " " : "", word);
        if (cur[0] && renderer.measure_text(candidate, scale) > max_w) {
            snprintf(out[lines++], 40, "%s", cur);
            snprintf(cur, sizeof(cur), "%s", word);
        } else {
            snprintf(cur, sizeof(cur), "%s", candidate);
        }
        if (!space) break;
        p = space + 1;
    }
    if (cur[0] && lines < max_lines) snprintf(out[lines++], 40, "%s", cur);
    return lines;
}

static Color rarity_color(ItemRarity r) { return item_rarity_color(r); }   // game/items.h

static const char* rarity_name(ItemRarity r) {
    switch (r) {
        case ItemRarity::Common:    return tr("COMÚN", "COMMON");
        case ItemRarity::Uncommon:  return tr("POCO COMÚN", "UNCOMMON");
        case ItemRarity::Rare:      return tr("RARO", "RARE");
        default:                    return tr("LEGENDARIO", "LEGENDARY");
    }
}

static TileSprite miracle_icon(MiracleID id);   // defined with the miracle slots below

static TileSprite category_icon(ItemCategory c) {
    switch (c) {
        case ItemCategory::Weapon:     return TileSprite::Arrow;
        case ItemCategory::Relic:      return TileSprite::Star;
        case ItemCategory::Consumable: return TileSprite::Orb;
        default:                       return TileSprite::Heart;   // passives / heredables
    }
}

// Reparto de las cartas. Lo usan el DIBUJO y el hit-test tactil: la carta que se toca es
// la que se ve, sin dos copias de la formula. Alto generoso (la carta crece al elegirse).
constexpr f32 CARD_W = 92.0f, CARD_H = 78.0f, CARD_GAP = 8.0f, CARD_TOP = 38.0f;
Rect choice_card_rect(int i, int count) {
    if (count <= 0) return {0, 0, 0, 0};
    const f32 W = static_cast<f32>(Config::GAME_WIDTH);
    const f32 x0 = (W - (CARD_W * count + CARD_GAP * (count - 1))) * 0.5f;
    return {x0 + i * (CARD_W + CARD_GAP) - 2.0f, CARD_TOP - 5.0f, CARD_W + 4.0f, CARD_H + 10.0f};
}

void draw_choice_cards(Renderer& renderer, const char* title, const ChoiceCard* cards,
                       int count, int selected, u32 tileset) {
    const f32 W = static_cast<f32>(Config::GAME_WIDTH);
    const f32 H = static_cast<f32>(Config::GAME_HEIGHT);
    renderer.draw_rect({0, 0, W, H}, {10, 8, 16, 175});

    renderer.draw_text_centered(title, 12, {245, 205, 90, 255}, 1.3f);

    constexpr f32 GAP = CARD_GAP, TOP = CARD_TOP;
    const f32 x0 = (W - (CARD_W * count + GAP * (count - 1))) * 0.5f;
    // Everything inside a card centres on the card, not on the screen
    auto centred = [&renderer](const char* s, f32 cx, f32 y, Color c, f32 sc) {
        renderer.draw_text(s, cx - renderer.measure_text(s, sc) * 0.5f, y, c, sc);
    };

    for (int i = 0; i < count; i++) {
        const ChoiceCard& card = cards[i];
        if (!card.name[0]) continue;
        bool on = (i == selected);
        f32 x = x0 + i * (CARD_W + GAP);
        f32 y = TOP - (on ? 3.0f : 0.0f);
        f32 h = CARD_H + (on ? 6.0f : 0.0f);
        Color rc = card.accent;

        // Bevelled frame: rarity-tinted border for the selected card, dark otherwise
        Color border = on ? Color{245, 205, 90, 255} : Color{60, 52, 66, 235};
        renderer.draw_rect({x - 2, y - 2, CARD_W + 4, h + 4}, border);
        renderer.draw_rect({x, y, CARD_W, h}, {26, 21, 33, 245});
        renderer.draw_rect({x, y, CARD_W, 1.0f}, {255, 255, 255, 35});
        // Rarity strip along the top of the card
        renderer.draw_rect({x, y + 1.0f, CARD_W, 1.5f}, rc);

        // Icon on a soft disc
        f32 cx = x + CARD_W * 0.5f;
        draw_badge(renderer, tileset, card.icon, cx, y + 17.0f, 22.0f);

        // Se mide todo antes de pintar y el bloque se centra bajo el icono: al quitar la
        // referencia biblica del pie, la carta se quedaba con un tercio vacio abajo.
        char lines[2][40];
        int n = wrap_text(renderer, card.name, CARD_W - 8.0f, 0.8f, lines, 2);
        char dl[4][40];
        int dn = wrap_text(renderer, card.desc, CARD_W - 8.0f, 0.62f, dl, 4);

        f32 content_h = n * 8.0f + 3.0f + dn * 6.5f + 2.0f
                      + (card.tag[0]   ? 7.0f : 0.0f)
                      + (card.extra[0] ? 7.0f : 0.0f);
        const f32 body_top = y + 29.0f;               // justo debajo del icono
        f32 ty = body_top + std::max(0.0f, (y + h - 5.0f - body_top - content_h) * 0.5f);

        for (int l = 0; l < n; l++)
            centred(lines[l], cx, ty + l * 8.0f, on ? TEXT_LIGHT : Color{215, 208, 195, 255}, 0.8f);
        ty += n * 8.0f + 3.0f;

        if (card.tag[0]) { centred(card.tag, cx, ty, rc, 0.55f); ty += 7.0f; }
        if (card.extra[0]) { centred(card.extra, cx, ty, {230, 170, 90, 255}, 0.55f); ty += 7.0f; }

        for (int l = 0; l < dn; l++)
            centred(dl[l], cx, ty + 2.0f + l * 6.5f, {180, 172, 160, 255}, 0.62f);
    }

    renderer.draw_text_centered(tr("[A/D] elegir     [ENTER] tomar", "[A/D] choose     [ENTER] take"), H - 14.0f, {170, 160, 150, 220}, 0.7f);
}

// =============================================================================
// Encuentro biblico: caja de dialogo sobre el mundo congelado
// =============================================================================
// Rects de las dos opciones, apuntados al pintar. Solo hay un dialogo en pantalla a la
// vez, asi que un solo juego basta; y es la unica forma de que el hit-test caiga donde
// esta el texto, porque el alto de la caja lo decide el ajuste de lineas.
static Rect g_dialogue_opt[2] = {};
Rect dialogue_option_rect(int i) {
    return (i >= 0 && i < 2) ? g_dialogue_opt[i] : Rect{0, 0, 0, 0};
}

void draw_dialogue(Renderer& renderer, const DialogueView& view, u32 tileset, f32 time,
                   u32 portraits) {
    const f32 W = static_cast<f32>(Config::GAME_WIDTH);
    const f32 H = static_cast<f32>(Config::GAME_HEIGHT);
    renderer.draw_rect({0, 0, W, H}, {8, 6, 12, 130});

    // Caja: alta cuando hay opciones que listar, baja cuando solo hay texto
    const f32 box_h = view.option_count > 0 ? 78.0f : 54.0f;
    const f32 bx = 12.0f, by = H - box_h - 10.0f, bw = W - 24.0f;

    renderer.draw_rect({bx - 2, by - 2, bw + 4, box_h + 4}, {180, 150, 95, 240});
    renderer.draw_rect({bx, by, bw, box_h}, {24, 19, 28, 248});
    renderer.draw_rect({bx, by, bw, 1.0f}, {255, 255, 255, 40});

    // Retrato de quien habla, si lo tiene. La celda mide 24 unidades (96 px) y se dibuja a
    // PORTRAIT_W: por encima de ~34 se ve blanda, y el Codice ya la estira a 34.
    // Sin retrato (o sin hoja) TODO lo de abajo cae en las coordenadas de siempre.
    const f32 PORTRAIT_W = 32.0f;
    const bool has_portrait = view.portrait >= 0 && view.portrait < Portrait::COUNT &&
                              portraits != TEXTURE_NONE;
    f32 text_x = bx + 8.0f;
    f32 text_w = bw - 16.0f;
    if (has_portrait) {
        const f32 pxx = bx + 5.0f, pyy = by + 5.0f;
        renderer.draw_rect({pxx - 1.0f, pyy - 1.0f, PORTRAIT_W + 2.0f, PORTRAIT_W + 2.0f},
                           {150, 122, 80, 200});
        renderer.draw_rect({pxx, pyy, PORTRAIT_W, PORTRAIT_W}, {14, 11, 18, 240});
        renderer.draw_sprite_rect(portraits, Portrait::src(view.portrait),
                                  {pxx, pyy, PORTRAIT_W, PORTRAIT_W}, Color::white());
        renderer.draw_rect({pxx, pyy, PORTRAIT_W, 0.7f}, {255, 240, 200, 90});
        text_x = pxx + PORTRAIT_W + 7.0f;
        text_w = bx + bw - 8.0f - text_x;
    }

    // Nombre de quien habla, sobre una placa que muerde el borde superior. Va alineado con
    // el texto, no con el borde: el nombre y sus palabras se leen en la misma columna.
    if (view.speaker && view.speaker[0]) {
        f32 nw = renderer.measure_text(view.speaker, 0.85f);
        f32 nx = has_portrait ? text_x - 5.0f : bx + 5.0f;
        renderer.draw_rect({nx, by - 7.0f, nw + 10.0f, 12.0f}, {180, 150, 95, 245});
        renderer.draw_rect({nx + 1.0f, by - 6.0f, nw + 8.0f, 10.0f}, {40, 31, 24, 250});
        renderer.draw_text(view.speaker, nx + 5.0f, by - 5.0f, {245, 218, 160, 255}, 0.85f);
    }
    // Cuerpo del dialogo. Con retrato la columna es ~35 unidades mas estrecha, asi que se
    // le concede una CUARTA linea: `wrap_text` trunca en silencio lo que no le cabe, y la
    // caja tiene hueco de sobra en vertical (4 lineas + 2 opciones = 70 de 78).
    std::string lines[4];
    const int max_lines = has_portrait ? 4 : 3;
    int n = renderer.wrap_text(view.body, text_w, 0.8f, lines, max_lines);
    for (int i = 0; i < n; i++)
        renderer.draw_text(lines[i].c_str(), text_x, by + 8.0f + i * 9.0f, {224, 216, 204, 250}, 0.8f);

    // Opciones
    g_dialogue_opt[0] = g_dialogue_opt[1] = {0, 0, 0, 0};
    if (view.option_count > 0) {
        f32 oy = by + 8.0f + n * 9.0f + 4.0f;
        for (int i = 0; i < view.option_count && i < 2; i++) {
            bool on = (i == view.selected);
            g_dialogue_opt[i] = {text_x - 3.0f, oy + i * 11.0f - 2.0f,
                                 (bx + bw - 5.0f) - (text_x - 3.0f), 12.0f};
            Color c = on ? Color{250, 215, 130, 255} : Color{160, 152, 142, 235};
            // El borde derecho es el de siempre (bx+bw-5): sin retrato la banda tiene que
            // quedar exactamente donde estaba antes de que existiera este campo.
            if (on) renderer.draw_rect({text_x - 3.0f, oy + i * 11.0f - 1.0f,
                                        (bx + bw - 5.0f) - (text_x - 3.0f), 10.0f}, {70, 56, 34, 200});
            char buf[96];
            snprintf(buf, sizeof(buf), "%s%s", on ? "> " : "  ", view.options[i]);
            renderer.draw_text(buf, text_x, oy + i * 11.0f, c, 0.8f);
            // Coste a la derecha, en rojo: la decision tiene que doler antes de tomarla
            if (view.costs[i] && view.costs[i][0]) {
                f32 cw = renderer.measure_text(view.costs[i], 0.7f);
                renderer.draw_text(view.costs[i], bx + bw - cw - 8.0f, oy + i * 11.0f + 1.0f,
                                   on ? Color{240, 130, 110, 255} : Color{150, 90, 80, 220}, 0.7f);
            }
        }
        renderer.draw_text_centered(view.touch
                                        ? tr("Toca una opción; vuelve a tocarla para decidir",
                                             "Tap an option; tap it again to decide")
                                        : tr("[W/S] elegir     [ENTER] decidir", "[W/S] choose     [ENTER] decide"),
                                    H - 6.0f, {160, 150, 140, 200}, 0.65f);
    } else {
        f32 blink = 0.55f + 0.45f * std::sin(time * 3.4f);
        const char* prompt = view.touch ? tr("toca para seguir", "tap to continue")
                           : view.more_lines ? tr("[ENTER] seguir", "[ENTER] continue")
                                             : tr("[ENTER] continuar", "[ENTER] continue");
        renderer.draw_text_centered(prompt, H - 6.0f, {200, 185, 160, static_cast<u8>(210 * blink)}, 0.65f);
    }
    (void)tileset;
}

void draw_item_choice(Renderer& renderer, const char* title, const ItemDef* const defs[3],
                      int count, int selected, u32 tileset, bool act_one,
                      const char* held_relic) {
    ChoiceCard cards[3];
    char swap[3][40];
    for (int i = 0; i < count && i < 3; i++) {
        if (!defs[i]) continue;
        cards[i].name   = defs[i]->name;
        cards[i].tag    = rarity_name(defs[i]->rarity);
        // La herencia solo significa algo mientras juegas con Elias: siendo ya Eliseo,
        // "pasa a Eliseo" no tiene a quien pasar
        cards[i].extra  = (defs[i]->heredable && act_one) ? tr("PASA A ELISEO", "PASSES TO ELISHA") : "";
        // Solo hay un slot de reliquia: tomar esta deja la otra. Se dice en la carta.
        if (defs[i]->category == ItemCategory::Relic && held_relic && *held_relic) {
            snprintf(swap[i], sizeof(swap[i]), tr("SUSTITUYE A %s", "REPLACES %s"), held_relic);
            cards[i].extra = swap[i];
        }
        cards[i].desc   = defs[i]->description;
        cards[i].accent = rarity_color(defs[i]->rarity);
        cards[i].icon   = category_icon(defs[i]->category);
    }
    draw_choice_cards(renderer, title, cards, count, selected, tileset);
}

void draw_miracle_choice(Renderer& renderer, const char* title, const MiracleID* ids, int count,
                         int selected, u32 tileset, const char* const* extras) {
    static const Color TIER_COLORS[5] = {{200, 195, 185, 255}, {200, 195, 185, 255},
                                         {120, 210, 120, 255}, {110, 175, 245, 255},
                                         {245, 205, 90, 255}};
    ChoiceCard cards[3];
    char tags[3][28];
    for (int i = 0; i < count && i < 3; i++) {
        MiracleDef d = get_miracle_def(ids[i]);
        snprintf(tags[i], sizeof(tags[i]), tr("TIER %d  ·  %.0f FE", "TIER %d  ·  %.0f FAITH"), d.tier, d.faith_cost);
        cards[i].name   = d.name;
        cards[i].tag    = tags[i];
        cards[i].extra  = (extras && extras[i]) ? extras[i] : "";
        cards[i].desc   = d.description;
        cards[i].accent = TIER_COLORS[d.tier < 0 ? 0 : (d.tier > 4 ? 4 : d.tier)];
        cards[i].icon   = miracle_icon(ids[i]);
    }
    draw_choice_cards(renderer, title, cards, count, selected, tileset);
}

void HUD::render(Renderer& renderer, const Player& player,
                 const ECSWorld& world, const FaithSystem& faith, const HUDContext& ctx) {
    if (!visible) return;

    // Translucent panel behind the vitals (top-left)
    renderer.draw_rect({2, 2, BAR_X + BAR_W + 30, 28}, PANEL_BG);
    renderer.draw_rect({2, 30, BAR_X + BAR_W + 30, 0.75f}, {255, 240, 200, 40});

    if (player.entity != ENTITY_NONE && world.is_alive(player.entity)) {
        const auto& health = world.healths[player.entity];
        draw_health_bar(renderer, health.current, health.max, ctx);
    }

    draw_faith_bar(renderer, faith, ctx);
    draw_inventory(renderer, player, ctx);
    // En VERTICAL las celdas de milagro se han ido a la banda negra con el resto de los
    // botones, y ahi no llega el objetivo de 320x180: las pinta la pasada de superposicion
    // (Engine::render), igual que dash y consumible.
    const bool slots_in_band = ctx.input && ctx.input->touch_ui && ctx.input->touch_band >= 62.0f;
    if (!slots_in_band) draw_miracle_slots(renderer, player, ctx, faith);
    draw_minimap(renderer, ctx, ctx.show_map);
    if (!ctx.show_map) draw_objective(renderer, ctx);
    draw_boss_bar(renderer, ctx);
    draw_message(renderer, ctx);
    draw_banner(renderer, ctx);
    draw_touch_controls(renderer, player, ctx);
}

void HUD::draw_health_bar(Renderer& renderer, f32 current, f32 max, const HUDContext& ctx) {
    f32 ratio = max > 0.0f ? current / max : 0.0f;
    const f32 y = 5.0f, h = 7.0f;

    draw_badge(renderer, ctx.tileset, TileSprite::Heart, 11.0f, y + h * 0.5f, 12.0f);

    Color hp_color = ratio > 0.5f ? Color{52, 200, 96, 255} :
                     ratio > 0.25f ? Color{240, 190, 40, 255} :
                                     Color{215, 60, 45, 255};
    draw_bar(renderer, {BAR_X, y, BAR_W, h}, ratio, hp_color);

    char buf[16];
    snprintf(buf, sizeof(buf), "%.0f", current);
    renderer.draw_text(buf, BAR_X + BAR_W + 4, y - 1.5f, TEXT_LIGHT, 1.0f);
}

void HUD::draw_faith_bar(Renderer& renderer, const FaithSystem& faith, const HUDContext& ctx) {
    f32 ratio = faith.get_ratio();
    const f32 y = 16.0f, h = 5.0f;

    draw_badge(renderer, ctx.tileset, TileSprite::Orb, 11.0f, y + h * 0.5f, 11.0f);

    // Faith level color
    Color faith_color = {180, 180, 100, 255};
    switch (faith.level) {
        case FaithLevel::BajoLaRetama:  faith_color = {120, 70, 70, 255}; break;
        case FaithLevel::EnElDesierto:  faith_color = {175, 130, 85, 255}; break;
        case FaithLevel::FeVacilante:   faith_color = {200, 195, 105, 255}; break;
        case FaithLevel::SiervoFiel:    faith_color = {100, 185, 230, 255}; break;
        case FaithLevel::ProfetaUngido: faith_color = {185, 140, 255, 255}; break;
        case FaithLevel::FuegoDelCielo: faith_color = {255, 200, 50, 255}; break;
    }
    draw_bar(renderer, {BAR_X, y, BAR_W, h}, ratio, faith_color);

    // Muescas en los umbrales que desbloquean tier de milagro (25 / 50 / 75). Encendidas
    // las ya alcanzadas: asi se ve de un vistazo cuanta fe falta para el siguiente milagro.
    for (int t = 2; t <= 4; t++) {
        f32 need = faith_required_for_tier(t);
        f32 mx = BAR_X + BAR_W * (need / faith.max);
        bool reached = faith.current >= need;
        renderer.draw_rect({mx - 0.5f, y - 1.0f, 1.0f, h + 2.0f},
                           reached ? Color{255, 240, 190, 235} : Color{70, 66, 78, 220});
    }

    // Percentage + level name
    char buf[48];
    snprintf(buf, sizeof(buf), "%.0f%%", faith.current);
    renderer.draw_text(buf, BAR_X + BAR_W + 4, y - 2.0f, TEXT_LIGHT, 0.85f);
    renderer.draw_text(faith.get_level_name(), BAR_X, y + h + 1.0f, faith_color, 0.65f);
}

// Small icon row under the vitals: one cell per item taken this run. The held consumable
// gets a highlighted frame and its key, since it is the only one the player can trigger.
// Icono por arma: la piedra de la Honda y la vara florecida no son lo mismo. Y NO puede
// ser la flecha generica de la categoria: en tactil el boton ARMA queda pegado al de DASH,
// que ya es una flecha, y los dos salian identicos.
static TileSprite weapon_icon(u32 weapon_slot) {
    return (weapon_slot == 1) ? TileSprite::Grass : TileSprite::Rock;
}

void HUD::draw_inventory(Renderer& renderer, const Player& player, const HUDContext& ctx) {
    if (ctx.tileset == TEXTURE_NONE || !ctx.registry) return;
    const bool touch = (ctx.input && ctx.input->touch_ui);

    // Arma equipada: celda propia con su tecla y el barrido de recarga, porque es lo
    // unico del inventario que se dispara aparte del consumible.
    constexpr f32 WS = 12.0f, WY = 33.0f;
    if (player.weapon_slot != UINT32_MAX) {
        bool ready = player.secondary_cd <= 0.0f;
        TileSprite wicon = weapon_icon(player.weapon_slot);
        renderer.draw_rect({2.0f, WY - 1.0f, WS + 2, WS + 2}, ready ? Color{245, 205, 90, 235} : Color{90, 80, 70, 220});
        renderer.draw_rect({3.0f, WY, WS, WS}, {22, 16, 26, 230});
        renderer.draw_sprite_rect(ctx.tileset, get_tile_src(wicon), {3.0f, WY, WS, WS},
                                  ready ? Color::white() : Color{120, 115, 110, 200});
        if (!ready) {
            f32 k = std::min(1.0f, player.secondary_cd / 0.9f);
            renderer.draw_rect({3.0f, WY, WS, WS * k}, {10, 8, 14, 170});
        }
        // Tecla en placa, como la del consumible: es la unica pista de que el arma se dispara.
        // En tactil no se pinta: ahi el arma tiene su propio boton (ARMA) y una chapa con
        // una tecla de PC solo puede confundir.
        f32 kx = 3.0f + WS + 2.0f;
        if (!touch) {
            if (renderer.shadow_tex != TEXTURE_NONE)
                renderer.draw_sprite_rect(renderer.shadow_tex, {0, 0, 64, 32}, {kx - 1.0f, WY + 1.0f, 10, 10}, {0, 0, 0, 180});
            renderer.draw_text("I", kx + 2.0f, WY + 2.0f, ready ? Color{250, 220, 130, 245} : Color{120, 110, 100, 200}, 0.7f);
        }
        // El nombre del arma, la primera vez que se lleva: sin esto la celda es un icono mudo
        if (ctx.registry) {
            const ItemDef* wd = ctx.registry->get(player.weapon_slot);
            if (wd) renderer.draw_text(wd->name, kx + 10.0f, WY + 2.5f,
                                       ready ? Color{200, 180, 140, 200} : Color{120, 110, 100, 170}, 0.55f);
        }
    }

    if (player.items.empty()) return;
    constexpr f32 S = 9.0f, Y = 47.0f;
    f32 x = 3.0f;
    int shown = 0;
    for (u32 id : player.items) {
        // Tope de la fila: con el catalogo en 22 objetos una run larga pasa de 12. A 9
        // unidades por celda + 2 de hueco, 20 celdas ocupan 220 de las 320 de ancho y no
        // llegan al minimapa.
        if (shown >= 20) break;
        const ItemDef* def = ctx.registry->get(id);
        if (!def) continue;
        bool held  = (id == player.consumable_slot);
        bool relic = (id == player.relic_slot);
        // Frame carries the rarity colour; the icon stays untinted so it reads at 9 units
        renderer.draw_rect({x - 1, Y - 1, S + 2, S + 2}, held ? Color{245, 205, 90, 230} : rarity_color(def->rarity));
        renderer.draw_rect({x, Y, S, S}, {22, 16, 26, 220});
        renderer.draw_sprite_rect(ctx.tileset, get_tile_src(category_icon(def->category)), {x, Y, S, S});
        // Cargas del consumible (Aceite Inagotable): sin el numero, "+2 cargas" es invisible
        if (held && player.consumable_charges > 1) {
            char n[4];
            snprintf(n, sizeof(n), "%u", static_cast<unsigned>(player.consumable_charges));
            renderer.draw_text(n, x + S - 3.0f, Y + S - 5.0f, {255, 235, 180, 250}, 0.6f);
        }
        // La reliquia que llevas puesta: solo cabe una (GDD 7.3), y sin marca la fila de
        // objetos no distingue "la que me hace efecto" de "una legendaria mas".
        if (relic) {
            renderer.draw_rect({x - 2, Y - 2, S + 4, S + 4}, {0, 0, 0, 0});
            renderer.draw_rect({x - 2, Y - 2, S + 4, 1.0f}, {255, 235, 180, 235});
            renderer.draw_rect({x - 2, Y + S + 1.0f, S + 4, 1.0f}, {255, 235, 180, 235});
            renderer.draw_rect({x - 2, Y - 2, 1.0f, S + 4}, {255, 235, 180, 235});
            renderer.draw_rect({x + S + 1.0f, Y - 2, 1.0f, S + 4}, {255, 235, 180, 235});
        }
        x += S + 2.0f;
        shown++;
    }
    // En tactil el consumible se usa con el boton USAR, no con la R
    if (player.consumable_slot != UINT32_MAX && !(ctx.input && ctx.input->touch_ui))
        renderer.draw_text("R", x + 1.0f, Y + 1.5f, {245, 205, 90, 230}, 0.6f);
}

// =============================================================================
// Controles tactiles (GDD 4.2)
// =============================================================================
// Solo con `Input::touch_ui` (movil, pantalla tactil detectada o --touch). En
// escritorio no se dibuja nada y el HUD queda exactamente igual que antes. Poca
// opacidad: el pulgar tiene que ver donde pulsa sin que los botones tapen el juego.
// Los tres botones de milagro NO se dibujan aqui: son las propias celdas del HUD
// (mismos rects que Input::init_touch_layout).
// En VERTICAL esto NO se llama desde HUD::render sino desde Engine::render, ya despues
// del letterbox y dentro de Renderer::begin_overlay(), para poder pintar en la banda negra
// de abajo. La funcion es la misma: los rects vienen de Input::init_touch_layout, que es
// quien decide si estan dentro del mundo o en la banda.
void HUD::draw_touch_controls(Renderer& renderer, const Player& player, const HUDContext& ctx) {
    const Input* in = ctx.input;
    if (!in || !in->touch_ui || ctx.tileset == TEXTURE_NONE) return;
    // Si los botones se han ido a la banda, los pinta la pasada de superposicion: aqui se
    // dibujarian recortados por el borde del area de juego.
    if (in->touch_band >= 62.0f && !renderer.in_overlay()) return;

    // --- Joystick de movimiento: la base nace donde bajo el dedo ---
    if (in->joystick_move.active) {
        Vec2 o = in->joystick_move.origin;
        Vec2 k = o + in->joystick_move.get_axis() * 13.0f;
        renderer.draw_rect({o.x - 17, o.y - 17, 34, 34}, {255, 245, 220, 60});
        renderer.draw_rect({o.x - 15, o.y - 15, 30, 30}, {20, 16, 26, 110});
        renderer.draw_rect({k.x - 7, k.y - 7, 14, 14}, {250, 230, 190, 205});
    }

    // --- Apuntado manual (hold + drag en la mitad derecha) ---
    // Se dibuja el origen, un rastro de puntos y la mira: sin el rastro no se distingue
    // de un tap y no se sabe hacia donde se esta disparando.
    if (in->joystick_aim.active) {
        Vec2 o = in->joystick_aim.origin;
        Vec2 dir = in->joystick_aim.get_axis();
        renderer.draw_rect({o.x - 9, o.y - 9, 18, 18}, {245, 205, 90, 70});
        renderer.draw_rect({o.x - 7, o.y - 7, 14, 14}, {20, 16, 26, 90});
        for (int d = 1; d <= 4; d++) {
            Vec2 p = o + dir * (7.0f * d);
            renderer.draw_rect({p.x - 1.5f, p.y - 1.5f, 3, 3},
                               {250, 225, 160, static_cast<u8>(210 - d * 30)});
        }
        Vec2 t = o + dir * 34.0f;
        renderer.draw_rect({t.x - 5, t.y - 1, 10, 2}, {255, 235, 175, 235});
        renderer.draw_rect({t.x - 1, t.y - 5, 2, 10}, {255, 235, 175, 235});
    }

    // --- Fila de accion: DASH, USAR, ARMA, ORAR (los que apliquen) ---
    static const int action_btns[4] = {TOUCH_BTN_DASH, TOUCH_BTN_CONS,
                                       TOUCH_BTN_WEAPON, TOUCH_BTN_PRAY};
    for (int k = 0; k < 4; k++) {
        const TouchButton& b = in->touch_buttons[action_btns[k]];
        if (!b.enabled) continue;   // `enabled` manda en el dibujo Y en el hit-test
        bool cons = (b.action == Action::Consumable);
        bool weap = (b.action == Action::AttackSecondary);
        bool pray = (b.action == Action::Pray);

        renderer.draw_rect({b.rect.x - 1, b.rect.y - 1, b.rect.w + 2, b.rect.h + 2},
                           b.down ? Color{250, 225, 160, 225} : Color{200, 180, 140, 120});
        renderer.draw_rect(b.rect, {20, 16, 26, static_cast<u8>(b.down ? 200 : 145)});

        TileSprite icon = TileSprite::Arrow;
        if (cons && ctx.registry) {
            const ItemDef* d = ctx.registry->get(player.consumable_slot);
            if (d) icon = category_icon(d->category);
        } else if (weap) {
            icon = weapon_icon(player.weapon_slot);
        } else if (pray) {
            icon = TileSprite::AltarGod;
        }
        renderer.draw_sprite_rect(ctx.tileset, get_tile_src(icon),
                                  {b.rect.x + 3, b.rect.y + 3, b.rect.w - 6, b.rect.h - 6},
                                  b.down ? Color::white() : Color{230, 225, 215, 200});
        if (cons && player.consumable_charges > 1) {
            char n[4];
            snprintf(n, sizeof(n), "%u", static_cast<unsigned>(player.consumable_charges));
            renderer.draw_text(n, b.rect.right() - 5.0f, b.rect.bottom() - 7.0f, {255, 235, 180, 250}, 0.65f);
        }
        // La etiqueta va ENCIMA: debajo de estos botones estan las placas de coste de las
        // celdas de milagro y los dos textos se pisaban.
        const char* label = cons ? tr("USAR", "USE")
                          : weap ? tr("ARMA", "WEAPON")
                          : pray ? tr("ORAR", "PRAY")
                                 : "DASH";
        f32 lw = renderer.measure_text(label, 0.55f);
        renderer.draw_text(label, b.rect.x + (b.rect.w - lw) * 0.5f, b.rect.y - 6.0f,
                           {225, 205, 165, 200}, 0.55f);
    }

    // --- Pausa: dos barritas, arriba al centro. Sustituye al swipe de dos dedos, que
    // chocaba con "stick + apuntar" (dos dedos en pantalla es la postura normal). ---
    {
        const TouchButton& b = in->touch_buttons[TOUCH_BTN_PAUSE];
        if (b.enabled) {
            renderer.draw_rect({b.rect.x - 1, b.rect.y - 1, b.rect.w + 2, b.rect.h + 2},
                               b.down ? Color{250, 225, 160, 225} : Color{200, 180, 140, 110});
            renderer.draw_rect(b.rect, {20, 16, 26, static_cast<u8>(b.down ? 200 : 140)});
            Color bar = b.down ? Color::white() : Color{230, 225, 215, 200};
            f32 cx = b.rect.center().x;
            renderer.draw_rect({cx - 4.0f, b.rect.y + 3.0f, 3.0f, b.rect.h - 6.0f}, bar);
            renderer.draw_rect({cx + 1.0f, b.rect.y + 3.0f, 3.0f, b.rect.h - 6.0f}, bar);
        }
    }

    // --- Ficha del milagro: mantener pulsada su celda en vez de lanzarlo (GDD) ---
    if (in->touch_info_slot >= 0 && in->touch_info_slot < Config::MAX_MIRACLE_SLOTS) {
        const MiracleSlot& s = player.miracle_slots[in->touch_info_slot];
        if (!s.is_empty()) {
            auto def = get_miracle_def(s.miracle);
            const f32 w = 152.0f, h = 34.0f;
            f32 x = (Config::GAME_WIDTH - w) * 0.5f, y = 96.0f;
            renderer.draw_rect({x - 1, y - 1, w + 2, h + 2}, {245, 205, 90, 190});
            renderer.draw_rect({x, y, w, h}, {18, 14, 24, 238});
            renderer.draw_text(def.name, x + 5, y + 3, {250, 225, 160, 250}, 0.8f);
            char sub[64];
            snprintf(sub, sizeof(sub), tr("Tier %d   %.0f de fe", "Tier %d   %.0f faith"), def.tier, def.faith_cost);
            renderer.draw_text(sub, x + 5, y + 13, {200, 185, 150, 225}, 0.6f);
            renderer.draw_text(def.description, x + 5, y + 22, {215, 210, 200, 230}, 0.6f);
        }
    }
}

static TileSprite miracle_icon(MiracleID id) {
    switch (id) {
        case MiracleID::CuervosDeQuerit:   return TileSprite::Heart;
        case MiracleID::PalabraDeSequia:   return TileSprite::Skull;
        case MiracleID::FuegoDelCielo:     return TileSprite::Fireball;
        case MiracleID::CarroDeFuego:      return TileSprite::Fireball;
        case MiracleID::AltarConsumido:    return TileSprite::Star;
        case MiracleID::SanarAguas:        return TileSprite::Orb;
        case MiracleID::AceiteInagotable:  return TileSprite::Heart;
        case MiracleID::OjosAbiertos:      return TileSprite::Star;
        case MiracleID::EjercitoCelestial: return TileSprite::Lightbolt;
        case MiracleID::DoblePorcion:      return TileSprite::Star;
        default:                           return TileSprite::Star;
    }
}

void HUD::draw_miracle_slots(Renderer& renderer, const Player& player, const HUDContext& ctx,
                             const FaithSystem& faith) {
    static const char* keys[Config::MAX_MIRACLE_SLOTS] = {"Q", "E", "F"};
    const f32 S = 22.0f;   // slot size
    // En tactil, la celda del HUD **es** el boton, asi que la posicion se toma de
    // `Input::init_touch_layout` en vez de recalcularla aqui. Antes estaban en dos sitios
    // y solo un comentario las mantenia iguales; en vertical, donde los botones bajan a la
    // banda negra, esa duplicidad separaba el dibujo del hit-test (se veia la celda dentro
    // del mundo y se pulsaba 160 unidades mas abajo).
    const Input* tin = (ctx.input && ctx.input->touch_ui) ? ctx.input : nullptr;
    for (int i = 0; i < player.active_slot_count && i < Config::MAX_MIRACLE_SLOTS; i++) {
        f32 x = static_cast<f32>(Config::GAME_WIDTH) - 28.0f - i * 27.0f;
        f32 y = static_cast<f32>(Config::GAME_HEIGHT) - 36.0f;
        if (tin && (2 + i) < TOUCH_BUTTON_COUNT) {
            x = tin->touch_buttons[2 + i].rect.x;
            y = tin->touch_buttons[2 + i].rect.y;
        }

        const auto& slot = player.miracle_slots[i];
        // Dos bloqueos distintos, y el jugador tiene que poder distinguirlos de un vistazo:
        //   lock_tier = el milagro pide un NIVEL de fe (candado + fe requerida)
        //   lock_cost = tienes el nivel pero no te llega para pagarlo (coste en rojo)
        bool lock_tier = false, lock_cost = false;
        f32 tier_need = 0.0f;
        if (!slot.is_empty()) {
            auto def = get_miracle_def(slot.miracle);
            lock_tier = def.tier > faith.get_max_miracle_tier();
            lock_cost = !lock_tier && def.faith_cost > faith.current;
            if (lock_tier) tier_need = faith_required_for_tier(def.tier);
        }
        bool locked = lock_tier || lock_cost;
        bool ready = !slot.is_empty() && slot.is_ready() && !locked;

        // Frame + inner plate (lighter top half = bevel)
        Color frame = locked ? Color{120, 50, 45, 255} : ready ? Color{210, 170, 80, 255} : Color{90, 84, 100, 255};
        renderer.draw_rect({x - 1.5f, y - 1.5f, S + 3, S + 3}, FRAME_DARK);
        renderer.draw_rect({x - 0.75f, y - 0.75f, S + 1.5f, S + 1.5f}, frame);
        Color plate = slot.is_empty() ? Color{40, 38, 46, 230} :
                      locked          ? Color{70, 36, 40, 240} :
                      ready           ? Color{62, 58, 92, 245} :
                                        Color{48, 46, 58, 240};
        renderer.draw_rect({x, y, S, S}, plate);
        renderer.draw_rect({x, y, S, S * 0.5f}, {255, 255, 255, 18});

        // Icon (on a soft dark disc)
        if (!slot.is_empty() && ctx.tileset != TEXTURE_NONE) {
            if (renderer.shadow_tex != TEXTURE_NONE)
                renderer.draw_sprite_rect(renderer.shadow_tex, {0, 0, 64, 32}, {x + 1, y + 1, S - 2, S - 2}, {0, 0, 0, 120});
            Color tint = locked ? Color{130, 120, 120, 255} : Color::white();
            renderer.draw_sprite_rect(ctx.tileset, get_tile_src(miracle_icon(slot.miracle)), {x + 3, y + 3, S - 6, S - 6}, tint);
        }

        // Candado: solo cuando falta NIVEL de fe. Un cuerpo con arco encima, en 3 rects.
        if (lock_tier) {
            renderer.draw_rect({x, y, S, S}, {20, 8, 10, 130});
            f32 lx = x + S * 0.5f - 3.0f, ly = y + S * 0.5f - 1.0f;
            renderer.draw_rect({lx - 0.75f, ly - 0.75f, 7.5f, 6.5f}, {30, 14, 16, 235});
            renderer.draw_rect({lx, ly, 6.0f, 5.0f}, {235, 165, 80, 255});
            renderer.draw_rect({lx + 1.0f, ly - 3.0f, 4.0f, 3.0f}, {235, 165, 80, 255});
            renderer.draw_rect({lx + 2.0f, ly - 2.0f, 2.0f, 2.0f}, {30, 14, 16, 235});
        }

        // Cooldown sweep (bottom-up)
        if (!slot.is_empty() && !slot.is_ready()) {
            auto def = get_miracle_def(slot.miracle);
            f32 cd_ratio = def.cooldown > 0.0f ? slot.cooldown_remaining / def.cooldown : 0.0f;
            renderer.draw_rect({x, y + S * (1 - cd_ratio), S, S * cd_ratio}, {0, 0, 0, 150});
        }
        // Ready glow line
        if (ready) renderer.draw_rect({x, y + S - 1.0f, S, 1.0f}, {255, 220, 120, 200});

        // Key hint badge under the slot. En tactil la celda ES el boton: la chapa con la
        // tecla sobra y ademas cae fuera de la pantalla cuando los botones bajan a la banda.
        f32 kx = x + S * 0.5f, ky = y + S + 5.0f;
        if (!tin) {
            if (renderer.shadow_tex != TEXTURE_NONE)
                renderer.draw_sprite_rect(renderer.shadow_tex, {0, 0, 64, 32}, {kx - 6, ky - 5, 12, 10}, {0, 0, 0, 180});
            renderer.draw_text(keys[i], kx - renderer.measure_text(keys[i], 0.7f) * 0.5f, ky - 3.0f, {235, 225, 200, 255}, 0.7f);
        }

        // Sobre el slot: normalmente el coste en fe; si falta nivel, la fe que hay que
        // alcanzar precedida de "+", que es la informacion que de verdad hace falta.
        if (!slot.is_empty()) {
            auto def = get_miracle_def(slot.miracle);
            char cost[12];
            if (lock_tier) snprintf(cost, sizeof(cost), "%.0f", tier_need);
            else           snprintf(cost, sizeof(cost), "%.0f", def.faith_cost);
            Color cc = locked ? Color{240, 120, 100, 255} : Color{160, 210, 250, 255};
            f32 tw = renderer.measure_text(cost, 0.65f);
            f32 cx0 = x + S - tw - 1.0f;
            if (ctx.tileset != TEXTURE_NONE)
                renderer.draw_sprite_rect(ctx.tileset, get_tile_src(TileSprite::Orb), {cx0 - 7.0f, y - 8.5f, 7, 7},
                                          lock_tier ? Color{240, 150, 130, 255} : Color::white());
            renderer.draw_text(cost, cx0, y - 9.0f, cc, 0.65f);
        }
    }
}

// -----------------------------------------------------------------------------
// Minimap: top-right. Each room a small cell; visited = filled, current = bright,
// boss = red with skull-ish mark, treasure/altar = gold/blue dot when visited or adjacent.
// -----------------------------------------------------------------------------
// El marco del minimapa pequeno, tal cual se dibuja mas abajo (el `else` de `big`).
// Existe para que Input pueda saber donde esta sin copiar la formula.
Rect HUD::minimap_rect(const FloorMap& fm) {
    if (fm.room_count <= 0) return {0, 0, 0, 0};
    const f32 cw = 9.0f, ch = 6.0f, gap = 2.0f;
    f32 total_w = fm.grid_w * (cw + gap) - gap;
    f32 total_h = fm.grid_h * (ch + gap) - gap;
    f32 ox = Config::GAME_WIDTH - total_w - 5.0f;
    return {ox - 2.0f, 2.0f, total_w + 4.0f, total_h + 4.0f};
}

void HUD::draw_minimap(Renderer& renderer, const HUDContext& ctx, bool big) {
    const FloorMap* fm = ctx.floor_map;
    if (!fm || fm->room_count <= 0) return;

    f32 cw = big ? 34.0f : 9.0f;
    f32 ch = big ? 18.0f : 6.0f;
    f32 gap = big ? 12.0f : 2.0f;
    f32 total_w = fm->grid_w * (cw + gap) - gap;
    f32 total_h = fm->grid_h * (ch + gap) - gap;
    f32 ox = big ? (Config::GAME_WIDTH - total_w) * 0.5f : Config::GAME_WIDTH - total_w - 5.0f;
    f32 oy = big ? (Config::GAME_HEIGHT - total_h) * 0.5f : 4.0f;

    if (big) {
        renderer.draw_rect({0, 0, static_cast<f32>(Config::GAME_WIDTH), static_cast<f32>(Config::GAME_HEIGHT)}, {0, 0, 0, 160});
        renderer.draw_text_centered(tr("MAPA DEL CAPÍTULO", "CHAPTER MAP"), oy - 16.0f, {240, 220, 160, 255});
    } else {
        renderer.draw_rect({ox - 2, oy - 2, total_w + 4, total_h + 4}, {0, 0, 0, 110});
    }

    int cur = fm->current_room;
    // Dentro de un INTERIOR se marca la sala de FUERA: es donde el jugador esta de verdad
    // en la rejilla. Dejar el mapa sin ninguna sala marcada es peor que aproximar.
    if (cur >= 0 && cur < fm->room_count && fm->rooms[cur].interior_of >= 0)
        cur = fm->rooms[cur].interior_of;
    bool blink = (static_cast<int>(ctx.time * 3.0f) % 2) == 0;

    for (int i = 0; i < fm->room_count; i++) {
        const auto& node = fm->rooms[i];
        // Los interiores NO salen en el mapa: no estan en la rejilla (grid -1) y pintarlos
        // en una celda inventada seria justo lo que el gate del track prohibe — que el
        // mapa mienta. Su sala de fuera si sale, y es la que hay que buscar.
        if (node.interior_of >= 0) continue;
        const Room* z = ctx.zone;   // el estado vive en las celdas de la zona
        bool visited = z && i < z->cell_count && z->cells[i].visited;
        // "Y vio el monte lleno": Ojos Abiertos vale por haber estado en la sala de al lado
        bool adjacent = ctx.map_revealed;
        for (int c = 0; c < 4; c++) if (fm->rooms[cur].connections[c] == i) adjacent = true;
        bool known = visited || adjacent || i == cur || node.type == RoomType::Boss || node.type == RoomType::Start;
        if (!known && !big) continue;

        f32 x = ox + node.grid_x * (cw + gap);
        f32 y = oy + node.grid_y * (ch + gap);

        Color fill = {70, 65, 60, 200};
        if (visited) fill = {150, 135, 110, 230};
        if (node.type == RoomType::Boss) fill = visited ? Color{200, 60, 50, 240} : Color{130, 40, 35, 220};
        if (i == cur) fill = blink ? Color{250, 235, 190, 255} : Color{230, 200, 140, 255};
        if (!known) fill = {40, 38, 36, 160};

        renderer.draw_rect({x, y, cw, ch}, fill);
        renderer.draw_rect_outline({x, y, cw, ch}, {20, 18, 16, 220});

        // Room-type marks
        Color mark = {0, 0, 0, 0};
        if (node.type == RoomType::Treasure && (visited || adjacent)) mark = {255, 210, 60, 255};
        if (node.type == RoomType::Altar    && (visited || adjacent)) mark = {120, 200, 255, 255};
        if (node.type == RoomType::Elite    && (visited || adjacent)) mark = {200, 120, 255, 255};
        if (node.type == RoomType::Event    && (visited || adjacent)) mark = {255, 226, 150, 255};
        if (mark.a) renderer.draw_rect({x + cw * 0.5f - 1.5f, y + ch * 0.5f - 1.5f, 3, 3}, mark);
        if (node.type == RoomType::Boss) {
            // small skull-ish cross
            renderer.draw_rect({x + cw * 0.5f - 0.5f, y + 1, 1, ch - 2}, {255, 240, 240, 255});
            renderer.draw_rect({x + 1, y + ch * 0.5f - 0.5f, cw - 2, 1}, {255, 240, 240, 255});
        }
        if (visited && z->cells[i].cleared && node.type != RoomType::Boss && big) {
            renderer.draw_text("ok", x + 2, y + 4, {40, 30, 20, 255}, 0.75f);
        }
        if (big) {
            const char* label = nullptr;
            switch (node.type) {
                case RoomType::Start:    label = tr("INICIO", "START"); break;
                case RoomType::Boss:     label = tr("JEFE", "BOSS"); break;
                case RoomType::Treasure: label = (visited || adjacent) ? tr("TESORO", "TREASURE") : nullptr; break;
                case RoomType::Altar:    label = (visited || adjacent) ? "ALTAR" : nullptr; break;
                case RoomType::Elite:    label = (visited || adjacent) ? tr("ELITE", "ELITE") : nullptr; break;
                case RoomType::Event:    label = (visited || adjacent) ? tr("RELATO", "STORY") : nullptr; break;
                default: break;
            }
            if (label) renderer.draw_text(label, x + 1, y + ch + 1, {220, 210, 190, 255}, 0.6f);
        }
    }

    if (big) {
        // EL OBJETIVO DEL CAPITULO. `ChapterLore::objective` existia desde el primer commit y
        // no lo pintaba nadie: el mapa grande solo sabia decir "ve al rojo". Es lo que
        // convierte el mapa en una respuesta a "¿que hago aqui?" (GDD2.0 fase 4).
        if (ctx.room) {
            const char* obj = get_chapter_lore(ctx.room->chapter).objective;
            if (obj && obj[0])
                renderer.draw_text_centered(obj, oy + total_h + 10.0f, {248, 216, 130, 255}, 0.85f);
        }
        renderer.draw_text_centered(tr("Llega al JEFE (rojo) y derrótalo para abrir la salida", "Reach the BOSS (red) and beat it to open the exit"), oy + total_h + 20.0f, {200, 200, 200, 255}, 0.75f);
        renderer.draw_text_centered(tr("Suelta TAB para volver", "Release TAB to go back"), oy + total_h + 29.0f, {160, 160, 160, 255}, 0.75f);
    }
}

// Objective hint under the minimap: direction to the boss room
void HUD::draw_objective(Renderer& renderer, const HUDContext& ctx) {
    const FloorMap* fm = ctx.floor_map;
    if (!fm || fm->room_count <= 0) return;
    const auto& cur = fm->rooms[fm->current_room];
    const auto& boss = fm->rooms[fm->boss_room];
    int dx = boss.grid_x - cur.grid_x;
    int dy = boss.grid_y - cur.grid_y;

    char buf[48];
    if (dx == 0 && dy == 0) {
        bool cleared = ctx.zone && ctx.zone->boss_cell >= 0 && ctx.zone->cells[ctx.zone->boss_cell].cleared;
        // "%s" y no el texto como formato: clang de Android compila con -Wformat-security
        // como ERROR, y un literal traducible nunca deberia hacer de cadena de formato.
        snprintf(buf, sizeof(buf), "%s", cleared ? tr("Salida abierta", "Exit open") : tr("!! JEFE !!", "!! BOSS !!"));
    } else {
        // e.g. "Jefe: 2N 1E"
        char part1[12] = "", part2[12] = "";
        if (dy != 0) snprintf(part1, sizeof(part1), "%d%c", std::abs(dy), dy < 0 ? 'N' : 'S');
        // La inicial del punto cardinal tambien se traduce: en ingles Oeste es West.
        if (dx != 0) snprintf(part2, sizeof(part2), "%s%d%c", dy != 0 ? " " : "", std::abs(dx),
                              dx < 0 ? tr("O", "W")[0] : 'E');
        snprintf(buf, sizeof(buf), tr("Jefe: %s%s", "Boss: %s%s"), part1, part2);
    }
    f32 tw = renderer.measure_text(buf, 0.75f);
    f32 y = 4.0f + fm->grid_h * 8.0f + 3.0f;
    renderer.draw_text(buf, Config::GAME_WIDTH - tw - 5.0f, y, {230, 215, 170, 255}, 0.75f);
    // En tactil el minimapa se TOCA para abrir el mapa grande: anunciar una tecla que el
    // telefono no tiene es la misma clase de error que las chapas Q/E/F.
    const char* maph = (ctx.input && ctx.input->touch_ui) ? tr("Toca: mapa", "Tap: map")
                                                          : tr("TAB: mapa", "TAB: map");
    renderer.draw_text(maph, Config::GAME_WIDTH - renderer.measure_text(maph, 0.6f) - 5.0f, y + 8.0f, {150, 150, 150, 255}, 0.6f);
}

void HUD::draw_boss_bar(Renderer& renderer, const HUDContext& ctx) {
    if (ctx.boss_hp_max <= 0.0f || ctx.boss_hp <= 0.0f || ctx.boss_heads <= 0) return;
    f32 w = 160.0f, x = (Config::GAME_WIDTH - w) * 0.5f, y = Config::GAME_HEIGHT - 14.0f;
    const char* name = ctx.boss_name ? ctx.boss_name : tr("JEFE", "BOSS");
    // Name plate + framed red bar with a skull badge on the left
    f32 nw = renderer.measure_text(name, 0.8f);
    renderer.draw_rect({(Config::GAME_WIDTH - nw) * 0.5f - 6, y - 11.0f, nw + 12, 9.0f}, {0, 0, 0, 120});
    renderer.draw_text_centered(name, y - 11.0f, {255, 225, 185, 255}, 0.8f);
    // Un jefe intocable (La Muerte del cap. 8) tiene que decirlo, o parece un bug
    Color bar_c = ctx.boss_invulnerable ? Color{110, 100, 120, 255} : Color{215, 55, 45, 255};
    // Las reposiciones del corro pueden pasar del maximo fijado al nacer: se recorta
    draw_bar(renderer, {x, y, w, 6}, std::min(1.0f, ctx.boss_hp / ctx.boss_hp_max), bar_c, 1.25f);
    draw_badge(renderer, ctx.tileset, TileSprite::Skull, x - 6.0f, y + 3.0f, 11.0f);
    if (ctx.boss_invulnerable)
        renderer.draw_text_centered(tr("INTOCABLE — acércate", "UNTOUCHABLE — get closer"), y - 19.0f, {215, 190, 230, 235}, 0.7f);
    // Jefe en GRUPO: una marca por cabeza viva bajo la barra, para ver caer al corro
    if (ctx.boss_group) {
        const f32 mw = 6.0f, gap = 3.0f;
        const f32 total = ctx.boss_heads * mw + (ctx.boss_heads - 1) * gap;
        f32 mx = (Config::GAME_WIDTH - total) * 0.5f;
        for (int i = 0; i < ctx.boss_heads; i++, mx += mw + gap) {
            renderer.draw_rect({mx - 1.0f, y + 8.0f, mw + 2.0f, 4.0f}, {0, 0, 0, 140});
            renderer.draw_rect({mx, y + 9.0f, mw, 2.0f}, {235, 120, 110, 255});
        }
    }
}

void HUD::draw_banner(Renderer& renderer, const HUDContext& ctx) {
    if (!ctx.banner || ctx.banner_t <= 0.0f) return;
    f32 a = std::min(1.0f, ctx.banner_t / 0.6f);      // fade out over the last 0.6s
    u8 alpha = static_cast<u8>(255 * a);
    f32 y = 66.0f;
    renderer.draw_rect({0, y - 6, static_cast<f32>(Config::GAME_WIDTH), 34}, {0, 0, 0, static_cast<u8>(150 * a)});
    renderer.draw_rect({0, y - 6, static_cast<f32>(Config::GAME_WIDTH), 1}, {230, 190, 110, alpha});
    renderer.draw_rect({0, y + 27, static_cast<f32>(Config::GAME_WIDTH), 1}, {230, 190, 110, alpha});
    renderer.draw_text_centered(ctx.banner, y, {250, 225, 160, alpha}, 1.5f);
    if (ctx.subbanner) renderer.draw_text_centered(ctx.subbanner, y + 16, {220, 220, 220, alpha}, 0.9f);
}

void HUD::draw_message(Renderer& renderer, const HUDContext& ctx) {
    if (!ctx.message || ctx.message_t <= 0.0f) return;
    f32 a = std::min(1.0f, ctx.message_t / 0.4f);
    u8 alpha = static_cast<u8>(255 * a);
    f32 scale = 1.0f;
    f32 tw = renderer.measure_text(ctx.message, scale);
    if (tw > 300.0f) { scale = 0.75f; tw = renderer.measure_text(ctx.message, scale); }
    f32 y = Config::GAME_HEIGHT - 44.0f;
    renderer.draw_rect({(Config::GAME_WIDTH - tw) * 0.5f - 4, y - 2, tw + 8, 12}, {0, 0, 0, static_cast<u8>(140 * a)});
    renderer.draw_text_centered(ctx.message, y, {255, 240, 200, alpha}, scale);
}
