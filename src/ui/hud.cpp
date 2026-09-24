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
#include <vector>

// Layout (game units): icon badge on the left, glossy bar with a dark frame, value on the right
static constexpr f32 BAR_X = 20.0f;
static constexpr f32 BAR_W = 74.0f;
static const Color FRAME_DARK  = {22, 16, 26, 255};
static const Color PANEL_BG    = {18, 14, 24, 150};
static const Color TEXT_LIGHT  = {245, 238, 220, 255};

// Dark soft disc (reuses the shadow blob) + a tileset icon on top: a "badge"
static u32 g_icon_sheet = TEXTURE_NONE;
void set_icon_sheet(u32 tex) { g_icon_sheet = tex; }

bool draw_icon(Renderer& renderer, int idx, Rect dst, Color tint) {
    if (g_icon_sheet == TEXTURE_NONE || idx < 0) return false;
    renderer.draw_sprite_rect(g_icon_sheet, Icon::src(idx), dst, tint);
    return true;
}

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
                       int count, int selected, u32 tileset, bool touch) {
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

        // Icon on a soft disc. El propio del objeto / milagro si hay hoja (un poco mayor: es
        // un dibujo, no un simbolo), y si no el generico del tileset.
        f32 cx = x + CARD_W * 0.5f;
        if (card.icon_idx >= 0 && g_icon_sheet != TEXTURE_NONE) {
            const f32 is = 25.0f, cy = y + 17.0f;
            if (renderer.shadow_tex != TEXTURE_NONE)
                renderer.draw_sprite_rect(renderer.shadow_tex, {0, 0, 64, 32}, {cx - is * 0.7f, cy - is * 0.6f, is * 1.4f, is * 1.2f}, {0, 0, 0, 170});
            draw_icon(renderer, card.icon_idx, {cx - is * 0.5f, cy - is * 0.5f, is, is});
        } else {
            draw_badge(renderer, tileset, card.icon, cx, y + 17.0f, 22.0f);
        }

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

    // En tactil la carta se toma con DOS toques (el primero elige), igual que una opcion
    // del encuentro: una carta tomada por error cuesta la partida.
    renderer.draw_text_centered(touch ? tr("toca una carta; vuelve a tocarla para tomarla",
                                           "tap a card; tap it again to take it")
                                      : tr("[A/D] elegir     [ENTER] tomar", "[A/D] choose     [ENTER] take"),
                                H - 14.0f, {170, 160, 150, 220}, 0.7f);
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

// Etiquetas de consecuencia en fila, a partir de `x` (chips_width las mide). `dim` apaga las
// de la opcion no seleccionada: se leen, pero la que manda es la que vas a tomar.
static constexpr f32 CHIP_SCALE = 0.7f, CHIP_GAP = 6.0f;
static f32 chips_width(Renderer& renderer, const EffectChip* chips, int count) {
    f32 w = 0.0f;
    for (int i = 0; i < count; i++) w += (i ? CHIP_GAP : 0.0f) + renderer.measure_text(chips[i].text, CHIP_SCALE);
    return w;
}
static void draw_chips(Renderer& renderer, const EffectChip* chips, int count, f32 x, f32 y, bool dim) {
    for (int i = 0; i < count; i++) {
        Color c = chips[i].color;
        if (dim) { c.r = static_cast<u8>(c.r * 0.72f); c.g = static_cast<u8>(c.g * 0.72f);
                   c.b = static_cast<u8>(c.b * 0.72f); c.a = 215; }
        renderer.draw_text(chips[i].text, x, y, c, CHIP_SCALE);
        x += renderer.measure_text(chips[i].text, CHIP_SCALE) + CHIP_GAP;
    }
}

void draw_dialogue(Renderer& renderer, const DialogueView& view, u32 tileset, f32 time,
                   u32 portraits) {
    const f32 W = static_cast<f32>(Config::GAME_WIDTH);
    const f32 H = static_cast<f32>(Config::GAME_HEIGHT);
    renderer.draw_rect({0, 0, W, H}, {8, 6, 12, 130});

    const f32 bx = 12.0f, bw = W - 24.0f;
    // Retrato de quien habla, si lo tiene. La celda mide 24 unidades (96 px) y se dibuja a
    // PORTRAIT_W: por encima de ~34 se ve blanda, y el Codice ya la estira a 34.
    // Sin retrato (o sin hoja) TODO lo de abajo cae en las coordenadas de siempre.
    const f32 PORTRAIT_W = 32.0f;
    const bool has_portrait = view.portrait >= 0 && view.portrait < Portrait::COUNT &&
                              portraits != TEXTURE_NONE;
    const f32 text_x = has_portrait ? bx + 5.0f + PORTRAIT_W + 7.0f : bx + 8.0f;
    const f32 text_w = has_portrait ? bx + bw - 8.0f - text_x : bw - 16.0f;

    // Cuerpo del dialogo. Con retrato la columna es ~35 unidades mas estrecha, asi que se
    // le concede una CUARTA linea: `wrap_text` trunca en silencio lo que no le cabe.
    std::string lines[4];
    const int max_lines = has_portrait ? 4 : 3;
    const int n = renderer.wrap_text(view.body, text_w, 0.8f, lines, max_lines);

    // Opciones: la etiqueta a la izquierda y lo que CAMBIA a la derecha, en la misma fila si
    // cabe; si no, en una segunda fila debajo. Se mide ANTES de pintar la caja, porque es lo
    // que decide su alto.
    constexpr f32 OPT_H = 11.0f, CHIP_ROW_H = 7.5f;
    const f32 opt_right = bx + bw - 8.0f;
    bool chips_below[2] = {false, false};
    f32 opt_h[2] = {OPT_H, OPT_H};
    for (int i = 0; i < view.option_count && i < 2; i++) {
        if (view.chip_count[i] <= 0) continue;
        char buf[96];
        snprintf(buf, sizeof(buf), "> %s", view.options[i]);
        const f32 lw = renderer.measure_text(buf, 0.8f);
        const f32 cw = chips_width(renderer, view.chips[i], view.chip_count[i]);
        if (text_x + lw + 10.0f + cw > opt_right) { chips_below[i] = true; opt_h[i] = OPT_H + CHIP_ROW_H; }
    }
    // Tras decidir: lo que ha pasado, en una o dos filas bajo el texto
    int result_rows = 0;
    int result_split = view.result_chip_count;   // cuantas van en la primera fila
    if (view.option_count == 0 && view.result_chip_count > 0) {
        result_rows = 1;
        if (chips_width(renderer, view.result_chips, view.result_chip_count) > text_w) {
            result_rows = 2;
            result_split = 0;
            f32 w = 0.0f;
            while (result_split < view.result_chip_count) {
                w += (result_split ? CHIP_GAP : 0.0f) +
                     renderer.measure_text(view.result_chips[result_split].text, CHIP_SCALE);
                if (w > text_w) break;
                result_split++;
            }
        }
    }

    // Caja: el alto de siempre de la de texto (54) y lo que pidan las opciones y sus
    // consecuencias por encima (crece hacia arriba: el pie se queda donde estaba). Antes la de
    // opciones tenia 78 FIJOS, y con una sola linea de contexto le sobraba un tercio vacio.
    f32 box_h = 54.0f;
    if (view.option_count > 0) {
        f32 need = 8.0f + n * 9.0f + 4.0f + 4.0f;
        for (int i = 0; i < view.option_count && i < 2; i++) need += opt_h[i];
        box_h = std::max(box_h, need);
    } else if (result_rows > 0) {
        box_h = std::max(box_h, 8.0f + n * 9.0f + 3.0f + result_rows * 8.0f + 5.0f);
    }
    const f32 by = H - box_h - 10.0f;

    renderer.draw_rect({bx - 2, by - 2, bw + 4, box_h + 4}, {180, 150, 95, 240});
    renderer.draw_rect({bx, by, bw, box_h}, {24, 19, 28, 248});
    renderer.draw_rect({bx, by, bw, 1.0f}, {255, 255, 255, 40});

    if (has_portrait) {
        const f32 pxx = bx + 5.0f, pyy = by + 5.0f;
        renderer.draw_rect({pxx - 1.0f, pyy - 1.0f, PORTRAIT_W + 2.0f, PORTRAIT_W + 2.0f},
                           {150, 122, 80, 200});
        renderer.draw_rect({pxx, pyy, PORTRAIT_W, PORTRAIT_W}, {14, 11, 18, 240});
        renderer.draw_sprite_rect(portraits, Portrait::src(view.portrait),
                                  {pxx, pyy, PORTRAIT_W, PORTRAIT_W}, Color::white());
        renderer.draw_rect({pxx, pyy, PORTRAIT_W, 0.7f}, {255, 240, 200, 90});
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
    for (int i = 0; i < n; i++)
        renderer.draw_text(lines[i].c_str(), text_x, by + 8.0f + i * 9.0f, {224, 216, 204, 250}, 0.8f);

    // Resuelto: lo que ha cambiado, bajo el texto del resultado
    if (result_rows > 0) {
        const f32 ry = by + 8.0f + n * 9.0f + 3.0f;
        draw_chips(renderer, view.result_chips, result_split, text_x, ry, false);
        if (result_rows > 1)
            draw_chips(renderer, view.result_chips + result_split, view.result_chip_count - result_split,
                       text_x, ry + 8.0f, false);
    }

    // Opciones
    g_dialogue_opt[0] = g_dialogue_opt[1] = {0, 0, 0, 0};
    if (view.option_count > 0) {
        f32 oy = by + 8.0f + n * 9.0f + 4.0f;
        for (int i = 0; i < view.option_count && i < 2; i++) {
            bool on = (i == view.selected);
            g_dialogue_opt[i] = {text_x - 3.0f, oy - 2.0f,
                                 (bx + bw - 5.0f) - (text_x - 3.0f), opt_h[i] + 1.0f};
            Color c = on ? Color{250, 215, 130, 255} : Color{160, 152, 142, 235};
            // El borde derecho es el de siempre (bx+bw-5): sin retrato la banda tiene que
            // quedar exactamente donde estaba antes de que existiera este campo.
            if (on) renderer.draw_rect({text_x - 3.0f, oy - 1.0f,
                                        (bx + bw - 5.0f) - (text_x - 3.0f), opt_h[i] - 1.0f}, {70, 56, 34, 200});
            char buf[96];
            snprintf(buf, sizeof(buf), "%s%s", on ? "> " : "  ", view.options[i]);
            renderer.draw_text(buf, text_x, oy, c, 0.8f);
            // Lo que cambia: a la derecha en la misma fila, o debajo, sangrado bajo la etiqueta.
            // Las dos opciones lo ensenan SIEMPRE: la decision se toma sabiendo lo que cuesta
            // y lo que da cada camino, no descubriendolo despues.
            if (view.chip_count[i] > 0) {
                const f32 cw = chips_width(renderer, view.chips[i], view.chip_count[i]);
                if (chips_below[i]) draw_chips(renderer, view.chips[i], view.chip_count[i],
                                               text_x + 9.0f, oy + OPT_H - 1.5f, !on);
                else                draw_chips(renderer, view.chips[i], view.chip_count[i],
                                               opt_right - cw, oy + 1.0f, !on);
            }
            oy += opt_h[i];
        }
        renderer.draw_text_centered(view.touch
                                        ? tr("Toca una opción; vuelve a tocarla para decidir",
                                             "Tap an option; tap it again to decide")
                                        : tr("[W/S] elegir     [ENTER] decidir", "[W/S] choose     [ENTER] decide"),
                                    H - 6.0f, {160, 150, 140, 200}, 0.65f);
    } else {
        f32 blink = 0.55f + 0.45f * std::sin(time * 3.4f);
        // Una escena guionizada SIEMPRE se podia saltar con ESC, pero el pie no lo decia:
        // nadie lo sabia. En tactil el aviso no hace falta porque ahi esta el boton.
        const char* prompt =
              view.touch    ? tr("toca para seguir", "tap to continue")
            : view.skippable ? (view.more_lines
                                  ? tr("[ENTER] seguir    [ESC] saltar", "[ENTER] continue    [ESC] skip")
                                  : tr("[ENTER] continuar    [ESC] saltar", "[ENTER] continue    [ESC] skip"))
            : view.more_lines ? tr("[ENTER] seguir", "[ENTER] continue")
                              : tr("[ENTER] continuar", "[ENTER] continue");
        renderer.draw_text_centered(prompt, H - 6.0f, {200, 185, 160, static_cast<u8>(210 * blink)}, 0.65f);
    }
    if (view.touch && view.skippable) draw_skip_button(renderer, time, view.skip_pressed);
    (void)tileset;
}

// =============================================================================
// Boton de SALTAR (solo tactil)
// =============================================================================
// Arriba a la derecha, lejos del pie donde viven el aviso de "seguir" y las opciones: en
// estas pantallas un toque en CUALQUIER otro sitio avanza, asi que el boton tiene que caer
// donde nadie toca por costumbre. 52x14 unidades son ~9 mm en un telefono, que es el
// minimo que se acierta sin mirar.
Rect skip_button_rect() { return {262.0f, 4.0f, 52.0f, 14.0f}; }

// El del TUTORIAL va en otro sitio, y es la unica excepcion a "el boton de SALTAR no se
// mueve": alli hay HUD debajo y el rect de siempre PISA el minimapa (`HUD::minimap_rect`
// empieza en x=271), asi que el dedo abria el mapa grande en vez de saltar. Este hueco —
// entre el panel de vitales, que acaba hacia x=145, y el minimapa — no lo ocupa nada.
Rect tutorial_skip_rect() { return {196.0f, 4.0f, 52.0f, 14.0f}; }

void draw_skip_button(Renderer& renderer, f32 time, bool pressed) {
    draw_skip_button_at(renderer, skip_button_rect(), time, pressed);
}

void draw_skip_button_at(Renderer& renderer, Rect r, f32 time, bool pressed) {
    const f32 k = 0.72f + 0.28f * std::sin(time * 2.6f);   // respira: se ve que es un boton
    const char* label = tr("SALTAR >>", "SKIP >>");
    renderer.draw_panel({r.x, r.y + 1.4f, r.w, r.h}, {0, 0, 0, 120}, 4.0f);
    // Mientras el dedo esta encima el boton se enciende, igual que los de menu. Sin esto el
    // toque no daba NINGUNA señal —el tap se resuelve al soltar— y como el tramo siguiente
    // es otra pantalla estatica con este mismo boton en el mismo pixel, saltar parecia no
    // hacer nada. Que se vea que el boton responde es la mitad del arreglo.
    renderer.draw_panel({r.x - 1.0f, r.y - 1.0f, r.w + 2.0f, r.h + 2.0f},
                        pressed ? Color{255, 214, 120, 235}
                                : Color{214, 176, 96, static_cast<u8>(110.0f + 90.0f * k)}, 4.8f);
    renderer.draw_panel(r, pressed ? Color{188, 132, 38, 255} : Color{40, 33, 30, 238}, 4.0f);
    const f32 sc = 0.62f;
    const f32 lw = renderer.measure_text(label, sc);
    renderer.draw_text(label, r.x + (r.w - lw) * 0.5f,
                       r.y + (r.h - Renderer::TEXT_LINE_UNITS * sc) * 0.5f,
                       pressed ? Color{255, 249, 234, 255} : Color{248, 228, 178, 255}, sc);
}

void draw_item_choice(Renderer& renderer, const char* title, const ItemDef* const defs[3],
                      int count, int selected, u32 tileset, bool act_one,
                      const char* held_relic, bool touch) {
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
        cards[i].icon_idx = Icon::item(defs[i]->id);
    }
    draw_choice_cards(renderer, title, cards, count, selected, tileset, touch);
}

void draw_miracle_choice(Renderer& renderer, const char* title, const MiracleID* ids, int count,
                         int selected, u32 tileset, const char* const* extras, bool touch) {
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
        cards[i].icon_idx = Icon::miracle(static_cast<u32>(ids[i]));
    }
    draw_choice_cards(renderer, title, cards, count, selected, tileset, touch);
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
    if (!ctx.show_map) {
        draw_minimap(renderer, ctx, false);
        draw_objective(renderer, ctx);
    }
    draw_boss_bar(renderer, ctx);
    draw_message(renderer, ctx);
    draw_banner(renderer, ctx);
    draw_touch_controls(renderer, player, ctx);
    // El mapa grande va EL ULTIMO: ahora ocupa casi toda la pantalla y con el orden de
    // antes el panel de vitales le cortaba la esquina y un mensaje de aviso se le plantaba
    // encima del terreno. Es una superposicion a pantalla completa, como el Codice.
    if (ctx.show_map) draw_minimap(renderer, ctx, true);
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
        if (!draw_icon(renderer, Icon::item(player.weapon_slot), {3.0f, WY, WS, WS},
                       ready ? Color::white() : Color{120, 115, 110, 200}))
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
        if (!draw_icon(renderer, Icon::item(id), {x - 0.5f, Y - 0.5f, S + 1.0f, S + 1.0f}))
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
        // Redondos y mas transparentes que antes: en cuadrado se leian como una caja de
        // dialogo puesta encima del juego, y tapaban demasiado.
        renderer.draw_disc(o, 16.0f, {20, 16, 26, 60});
        renderer.draw_ring(o, 17.0f, {255, 245, 220, 90});
        renderer.draw_disc(k, 7.5f, {250, 230, 190, 170});
    }

    // --- Apuntado manual (hold + drag en la mitad derecha) ---
    // Se dibuja el origen, un rastro de puntos y la mira: sin el rastro no se distingue
    // de un tap y no se sabe hacia donde se esta disparando.
    if (in->joystick_aim.active) {
        Vec2 o = in->joystick_aim.origin;
        Vec2 dir = in->joystick_aim.get_axis();
        renderer.draw_disc(o, 8.5f, {20, 16, 26, 50});
        renderer.draw_ring(o, 9.0f, {245, 205, 90, 58});
        for (int d = 1; d <= 4; d++) {
            Vec2 p = o + dir * (7.0f * d);
            renderer.draw_disc(p, 1.5f, {250, 225, 160, static_cast<u8>(175 - d * 25)});
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
        int own = Icon::NONE;   // icono propio del objeto (icons.png), si lo hay
        if (cons && ctx.registry) {
            const ItemDef* d = ctx.registry->get(player.consumable_slot);
            if (d) icon = category_icon(d->category);
            own = Icon::item(player.consumable_slot);
        } else if (weap) {
            icon = weapon_icon(player.weapon_slot);
            own = Icon::item(player.weapon_slot);
        } else if (pray) {
            icon = TileSprite::AltarGod;
        }
        const Rect ir = {b.rect.x + 3, b.rect.y + 3, b.rect.w - 6, b.rect.h - 6};
        const Color it = b.down ? Color::white() : Color{230, 225, 215, 200};
        if (!draw_icon(renderer, own, ir, it))
            renderer.draw_sprite_rect(ctx.tileset, get_tile_src(icon), ir, it);
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
            // `enabled` manda en el DIBUJO y en el hit-test, el mismo contrato que la fila
            // de accion. Durante la entrada del jefe el tactil se apaga entero, y una celda
            // latiendo que no responde al dedo es peor que no pintar nada.
            if (!tin->touch_buttons[2 + i].enabled) continue;
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

        // Latido: la celda que se puede lanzar AHORA respira en oro. Es la unica senal que
        // separa "puedes lanzarlo" de "esta ahi", y en tactil la unica que dice que la celda
        // ES un boton (ahi no hay chapa de tecla que lo diga). Lo bloqueado NO late: un
        // boton que late y no hace nada miente. Y RESPIRA, no parpadea: a 0,7 Hz el ojo lo
        // lee como brillo, no como ruido, que es donde se estrello en su dia el destello
        // de impacto.
        f32 beat = ready ? 0.5f + 0.5f * std::sin(ctx.time * 4.2f) : 0.0f;
        if (ready) {
            // Aureola por fuera del marco: la parte de dentro la tapa la celda, asi que
            // esto es un anillo aunque se dibuje como un rect (cuatro rects menos).
            renderer.draw_rect({x - 3.5f, y - 3.5f, S + 7, S + 7},
                               {255, 205, 90, static_cast<u8>(28 + 92 * beat)});
        }

        // Frame + inner plate (lighter top half = bevel)
        Color frame = locked ? Color{120, 50, 45, 255}
                    : ready  ? Color{static_cast<u8>(210 + 45 * beat),
                                     static_cast<u8>(170 + 55 * beat),
                                     static_cast<u8>(80 + 60 * beat), 255}
                             : Color{90, 84, 100, 255};
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
            if (!draw_icon(renderer, Icon::miracle(static_cast<u32>(slot.miracle)), {x + 2, y + 2, S - 4, S - 4}, tint))
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
        } else if (!slot.is_empty()) {
            // Al quitar la chapa Q/E/F en la pasada de Android la celda se quedo MUDA, y
            // el usuario probo el APK y no supo como lanzar un milagro. DASH, USAR, ARMA y
            // ORAR llevan etiqueta; estas tambien, en el mismo hueco donde iba la tecla.
            // El nombre corto es la PRIMERA palabra —"Fuego" contra "Cuervos" es lo que
            // distingue una celda de otra— y solo si CABE: `draw_text` no recorta, se sale
            // por encima de la celda vecina. Se mide, no se estima (`wrap_text` trunca en
            // silencio y esa trampa ya costo un rato en este proyecto).
            const char* full = get_miracle_def(slot.miracle).name;
            char shortname[24];
            size_t n = 0;
            while (full[n] && full[n] != ' ' && n + 1 < sizeof(shortname)) { shortname[n] = full[n]; n++; }
            shortname[n] = '\0';

            const f32 budget = S + 4.0f;      // 22 de celda + los 2 de aire a cada lado
            f32 ls = 0.55f, lw = renderer.measure_text(shortname, ls);
            if (lw > budget) { ls = 0.45f; lw = renderer.measure_text(shortname, ls); }
            const char* label = shortname;
            if (lw > budget) {
                // No cabe ni encogido: una etiqueta generica, y solo en la primera celda.
                // Repetir "MILAGRO" tres veces no anade nada y empasta la fila.
                if (i != 0) label = nullptr;
                else { label = tr("MILAGRO", "MIRACLE"); ls = 0.5f; lw = renderer.measure_text(label, ls); }
            }
            if (label)
                renderer.draw_text(label, x + (S - lw) * 0.5f, ky - 3.0f,
                                   ready ? Color{250, 225, 165, 235} : Color{205, 190, 160, 175}, ls);
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
// Con el mapa grande (6x4 / 6x5 celdas, 2026-09-24) crece a 12x7 unidades por celda: con
// las 9x6 de antes cada celda se quedaba en diez unidades y las chinchetas de los puntos
// de interes se pisaban. 6 celdas = 72 de ancho: empieza en x = 239 y la fila de objetos
// (hasta x = 223) sigue sin llegar.
Rect HUD::minimap_rect(const FloorMap& fm) {
    if (fm.room_count <= 0) return {0, 0, 0, 0};
    const f32 cw = 12.0f, ch = 7.0f;
    f32 total_w = fm.grid_w * cw;
    f32 total_h = fm.grid_h * ch;
    f32 ox = Config::GAME_WIDTH - total_w - 5.0f;
    return {ox - 2.0f, 2.0f, total_w + 4.0f, total_h + 4.0f};
}

// =============================================================================
// Mapa del capitulo: el TERRENO, no una rejilla de salas
// =============================================================================
// Desde la zona continua (commit add8a30) un capitulo es UN mapa de hasta 160x88 tiles, no
// un tablero de salas sueltas. El mapa grande seguia pintando el modelo VIEJO —una celda
// por sala, con separacion entre ellas— y el usuario, jugando el APK, lo describio como
// "cuadriculado": estaba describiendo una estructura que ya no existe.
//
// Se pintan FAMILIAS de tile, no tipos. Veinticinco colores a 1,4 unidades por tile son
// ruido; lo que tiene que leerse de un vistazo es por donde se anda, por donde no, y donde
// hay agua. Lo que si lleva color propio es lo que aparece una vez por mapa (cofre, altar,
// relato, arco de salida): no compite con nada porque no se repite.
enum MapFam : int {
    FAM_GROUND = 0, FAM_PATH, FAM_WATER, FAM_DEEP, FAM_SOLID, FAM_PIT,
    FAM_DOOR, FAM_CHEST, FAM_ALTAR_GOD, FAM_ALTAR_BAAL, FAM_EVENT, FAM_EXIT, FAM_COUNT
};

static int tile_family(TileType t) {
    switch (t) {
        case TileType::Path: case TileType::Ford: case TileType::Bridge:   return FAM_PATH;
        case TileType::Water:                                              return FAM_WATER;
        case TileType::River:                                              return FAM_DEEP;
        case TileType::Wall: case TileType::Rock: case TileType::Tree:
        case TileType::Crack: case TileType::Jar:                          return FAM_SOLID;
        case TileType::Pit:                                                return FAM_PIT;
        case TileType::DoorNorth: case TileType::DoorSouth:
        case TileType::DoorEast:  case TileType::DoorWest:
        case TileType::Doorway:                                            return FAM_DOOR;
        case TileType::Chest:                                              return FAM_CHEST;
        case TileType::AltarGod:                                           return FAM_ALTAR_GOD;
        case TileType::AltarBaal:                                          return FAM_ALTAR_BAAL;
        case TileType::Event:                                              return FAM_EVENT;
        case TileType::Exit:                                               return FAM_EXIT;
        default:                                                           return FAM_GROUND;
    }
}

// =============================================================================
// La textura del mapa: terreno PINTADO, no un mosaico de tiles
// =============================================================================
// La version anterior pintaba un rect por tramo de tiles del mismo color: 1,3 unidades por
// tile, trece colores planos y la niebla recortada en rectangulos de celda. Eso es un
// mosaico, y el usuario lo leyo -con razon- como pixel art, en un juego que dejo el pixel
// art atras en 2026-08-25.
//
// Ahora el terreno se pinta en una TEXTURA y el filtrado lineal hace el resto. Las mascaras
// de tile (solido / agua / camino / hueco) se suavizan a resolucion de tile y se muestrean
// bilineal: una mascara binaria suavizada es un campo continuo, y umbralizar ese campo da
// siluetas curvas en vez de escalones. Del gradiente del campo sale la luz (desde arriba a
// la izquierda) y de su filo, el contorno de tinta. Eso es lo que convierte una rejilla en
// un mapa dibujado.
//
// Terreno y NIEBLA van en dos texturas a proposito:
//  - el terreno depende del capitulo y se genera UNA sola vez, en la carga;
//  - la niebla cambia cada vez que se pisa una celda nueva, asi que es una textura de 4 px
//    por celda estirada con filtrado lineal. Repintarla cuesta microsegundos y el degradado
//    sale suave gratis. Con una sola textura habria que repintar medio megapixel al cruzar
//    cada celda, y eso es un tiron en plena exploracion.
static constexpr int MAP_PX = 5;   // pixeles de textura por tile de terreno

static inline f32 map_smoothstep(f32 a, f32 b, f32 v) {
    if (b <= a) return v >= b ? 1.0f : 0.0f;
    f32 t = (v - a) / (b - a);
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return t * t * (3.0f - 2.0f * t);
}

// Ruido de valor (rejilla + interpolacion suave). Es el grano de pergamino: sin el, la
// tierra es un plano de color liso y vuelve a parecer una ficha de color.
static inline f32 map_hash(int x, int y) {
    u32 h = static_cast<u32>(x) * 374761393u + static_cast<u32>(y) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return static_cast<f32>((h ^ (h >> 16)) & 0xFFFFu) / 65535.0f;
}
static f32 map_noise(f32 x, f32 y, f32 cell) {
    const f32 gx = x / cell, gy = y / cell;
    const int x0 = static_cast<int>(std::floor(gx)), y0 = static_cast<int>(std::floor(gy));
    f32 ax = gx - x0, ay = gy - y0;
    ax = ax * ax * (3.0f - 2.0f * ax);
    ay = ay * ay * (3.0f - 2.0f * ay);
    const f32 a = map_hash(x0, y0),     b = map_hash(x0 + 1, y0);
    const f32 c = map_hash(x0, y0 + 1), d = map_hash(x0 + 1, y0 + 1);
    return (a * (1.0f - ax) + b * ax) * (1.0f - ay) + (c * (1.0f - ax) + d * ax) * ay;
}

// El TERRENO. Solo depende del capitulo y de sus dimensiones, asi que se genera una vez y
// se reutiliza: romper una tinaja no repinta el mapa (un tile suelto no se ve a esta
// escala) y a cambio no hay ni un tiron durante la partida.
static u32 zone_terrain_texture(Renderer& renderer, const Room& z, int& out_w, int& out_h) {
    static u64 cached_sig = 0;
    static u32 cached_tex = TEXTURE_NONE;
    static int cached_w = 0, cached_h = 0;

    const int TW = z.tiles_w, TH = z.tiles_h;
    if (TW <= 0 || TH <= 0) return TEXTURE_NONE;
    const u64 sig = (static_cast<u64>(z.chapter) << 40) ^ (static_cast<u64>(TW) << 20) ^ static_cast<u64>(TH);
    TextureEntry* live = renderer.get_texture(cached_tex);
    if (sig == cached_sig && live && live->texture) {
        out_w = cached_w; out_h = cached_h;
        return cached_tex;
    }

    // --- Mascaras a resolucion de tile, suavizadas ------------------------------
    const size_t N = static_cast<size_t>(TW) * TH;
    std::vector<f32> solid(N, 0.0f), water(N, 0.0f), deep(N, 0.0f), path(N, 0.0f), pit(N, 0.0f);
    for (int ty = 0; ty < TH; ty++) {
        for (int tx = 0; tx < TW; tx++) {
            const size_t i = static_cast<size_t>(ty) * TW + tx;
            switch (tile_family(z.tiles[ty][tx])) {
                case FAM_SOLID: solid[i] = 1.0f; break;
                case FAM_WATER: water[i] = 1.0f; break;
                case FAM_DEEP:  deep[i]  = 1.0f; break;
                case FAM_PATH:
                case FAM_DOOR:  path[i]  = 1.0f; break;
                case FAM_PIT:   pit[i]   = 1.0f; break;
                default: break;
            }
        }
    }
    auto blur = [&](std::vector<f32>& f) {
        std::vector<f32> t(f.size(), 0.0f);
        for (int y = 0; y < TH; y++) {
            for (int x = 0; x < TW; x++) {
                f32 sum = 0.0f, wsum = 0.0f;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        const int nx = x + dx, ny = y + dy;
                        if (nx < 0 || ny < 0 || nx >= TW || ny >= TH) continue;
                        const f32 w = (dx == 0 && dy == 0) ? 4.0f : ((dx == 0 || dy == 0) ? 2.0f : 1.0f);
                        sum += f[static_cast<size_t>(ny) * TW + nx] * w;
                        wsum += w;
                    }
                }
                t[static_cast<size_t>(y) * TW + x] = sum / wsum;
            }
        }
        f.swap(t);
    };
    blur(solid); blur(water); blur(deep); blur(path); blur(pit);

    // El gradiente del campo solido, precalculado: es de donde sale el relieve, y sacarlo
    // aqui ahorra cuatro muestreos bilineales POR PIXEL.
    std::vector<f32> gx(N, 0.0f), gy(N, 0.0f);
    for (int y = 0; y < TH; y++) {
        for (int x = 0; x < TW; x++) {
            const int xm = x > 0 ? x - 1 : x, xp = x < TW - 1 ? x + 1 : x;
            const int ym = y > 0 ? y - 1 : y, yp = y < TH - 1 ? y + 1 : y;
            gx[static_cast<size_t>(y) * TW + x] = solid[static_cast<size_t>(y) * TW + xp] - solid[static_cast<size_t>(y) * TW + xm];
            gy[static_cast<size_t>(y) * TW + x] = solid[static_cast<size_t>(yp) * TW + x] - solid[static_cast<size_t>(ym) * TW + x];
        }
    }

    auto samp = [&](const std::vector<f32>& f, f32 fx, f32 fy) -> f32 {
        fx = std::min(std::max(fx - 0.5f, 0.0f), static_cast<f32>(TW) - 1.001f);
        fy = std::min(std::max(fy - 0.5f, 0.0f), static_cast<f32>(TH) - 1.001f);
        const int x0 = static_cast<int>(fx), y0 = static_cast<int>(fy);
        const int x1 = std::min(x0 + 1, TW - 1), y1 = std::min(y0 + 1, TH - 1);
        const f32 ax = fx - x0, ay = fy - y0;
        const f32 a = f[static_cast<size_t>(y0) * TW + x0], b = f[static_cast<size_t>(y0) * TW + x1];
        const f32 c = f[static_cast<size_t>(y1) * TW + x0], d = f[static_cast<size_t>(y1) * TW + x1];
        return (a * (1.0f - ax) + b * ax) * (1.0f - ay) + (c * (1.0f - ax) + d * ax) * ay;
    };

    // --- El dibujo --------------------------------------------------------------
    const int W = TW * MAP_PX, H = TH * MAP_PX;
    std::vector<u8> px(static_cast<size_t>(W) * H * 4, 255);
    for (int y = 0; y < H; y++) {
        const f32 ty = (y + 0.5f) / MAP_PX;
        for (int x = 0; x < W; x++) {
            const f32 tx = (x + 0.5f) / MAP_PX;
            const f32 sv = samp(solid, tx, ty), wv = samp(water, tx, ty), dv = samp(deep, tx, ty);
            const f32 pv = samp(path, tx, ty),  iv = samp(pit, tx, ty);

            // Tierra con grano de pergamino (dos octavas: mancha grande + veta fina)
            const f32 grain = map_noise(static_cast<f32>(x), static_cast<f32>(y), 13.0f) * 0.64f
                            + map_noise(static_cast<f32>(x), static_cast<f32>(y), 3.5f)  * 0.36f;
            const f32 tone = 0.80f + 0.42f * grain;          // mas recorrido: el pardo plano apagaba el mapa
            f32 R = 132.0f * tone, G = 113.0f * tone, B = 82.0f * tone;

            // Camino: una vereda apenas mas clara, con el borde deshilachado por el propio
            // grano. Va FLOJO a proposito. Los caminos unen centros de celda, asi que
            // pintados con fuerza trazan una reticula rectangular sobre el mapa entero, y
            // esa reticula es exactamente lo que el usuario seguia leyendo como "celdas"
            // aunque el terreno ya estuviera pintado. Se ven, pero no mandan.
            const f32 tp = map_smoothstep(0.26f, 0.70f, pv + (grain - 0.5f) * 0.30f);
            R += (152.0f - R) * tp * 0.48f; G += (131.0f - G) * tp * 0.48f; B += (96.0f - B) * tp * 0.48f;

            // Agua, con orilla clara justo en el filo
            const f32 av = wv + dv;
            if (av > 0.16f) {
                const f32 md = dv / std::max(0.001f, av);
                f32 wr = 78.0f + (42.0f - 78.0f) * md;
                f32 wg = 142.0f + (88.0f - 142.0f) * md;
                f32 wb = 176.0f + (128.0f - 176.0f) * md;
                const f32 shore = std::max(0.0f, 1.0f - std::fabs(av - 0.33f) / 0.14f);
                wr += 66.0f * shore; wg += 58.0f * shore; wb += 38.0f * shore;
                const f32 tw = map_smoothstep(0.24f, 0.54f, av);
                R += (wr - R) * tw; G += (wg - G) * tw; B += (wb - B) * tw;
            }

            // Hueco: negro, con el filo oscuro para que se lea como agujero
            const f32 tpit = map_smoothstep(0.24f, 0.54f, iv);
            R += (16.0f - R) * tpit; G += (12.0f - G) * tpit; B += (18.0f - B) * tpit;

            // Solido (roca, arbol, muro) con relieve y contorno de tinta
            if (sv > 0.12f) {
                const f32 nx = samp(gx, tx, ty), ny = samp(gy, tx, ty);
                f32 light = 1.0f - (nx + ny) * 1.35f;                 // luz desde arriba-izquierda
                light = std::min(1.75f, std::max(0.5f, light));
                const f32 ts = map_smoothstep(0.22f, 0.52f, sv);
                R += (70.0f * light - R) * ts; G += (63.0f * light - G) * ts; B += (66.0f * light - B) * ts;
                const f32 ink = std::max(0.0f, 1.0f - std::fabs(sv - 0.29f) / 0.115f);
                const f32 k = 1.0f - 0.52f * ink;
                R *= k; G *= k; B *= k;
            }

            u8* o = &px[(static_cast<size_t>(y) * W + x) * 4];
            o[0] = static_cast<u8>(std::min(255.0f, std::max(0.0f, R)));
            o[1] = static_cast<u8>(std::min(255.0f, std::max(0.0f, G)));
            o[2] = static_cast<u8>(std::min(255.0f, std::max(0.0f, B)));
            o[3] = 255;
        }
    }

    cached_tex = renderer.create_texture_from_rgba("@zonemap", W, H, px.data());
    cached_sig = sig;
    cached_w = W; cached_h = H;
    out_w = W; out_h = H;
    return cached_tex;
}

// La NIEBLA: 8 px por celda, estirados con filtrado lineal sobre el mapa entero. Es lo que
// sustituye a los rectangulos negros de celda, que eran la mitad de lo "cuadriculado".
//
// Y no es solo un velo: es una capa de LUZ Y SOMBRA. Hasta el 2026-09-12 "descubierto"
// significaba nada mas "sin velo", y sobre un terreno pardo apagado una celda pisada se
// seguia viendo oscura; ahora lo pisado lleva un realce calido y lo desconocido el velo de
// siempre. Sigue siendo UNA textura y UN draw.
static u32 zone_fog_texture(Renderer& renderer, const FloorMap& fm, const int* vis, int& out_w, int& out_h) {
    static u64 cached_sig = 0;
    static u32 cached_tex = TEXTURE_NONE;

    const int CW = std::max(1, fm.grid_w), CH = std::max(1, fm.grid_h);
    const int SUB = 8;   // 4 se quedaba corto: dos desenfoques se comian casi una celda entera
    const int W = CW * SUB, H = CH * SUB;
    out_w = W; out_h = H;

    u64 sig = static_cast<u64>(CW) * 31u + static_cast<u64>(CH);
    for (int i = 0; i < fm.room_count && i < Config::ZONE_CELLS_MAX; i++)
        sig = sig * 3u + static_cast<u64>(vis[i] & 3);
    TextureEntry* live = renderer.get_texture(cached_tex);
    if (sig == cached_sig && live && live->texture) return cached_tex;

    // Oscuridad por celda: 0 = pisada (transparente), 1 = nunca vista (opaca)
    std::vector<f32> dark(static_cast<size_t>(CW) * CH, 1.0f);
    for (int i = 0; i < fm.room_count && i < Config::ZONE_CELLS_MAX; i++) {
        const auto& node = fm.rooms[i];
        if (node.interior_of >= 0) continue;                 // los interiores no van en la rejilla
        if (node.grid_x < 0 || node.grid_y < 0 || node.grid_x >= CW || node.grid_y >= CH) continue;
        // 0 = pisada (realce), 0,78 = entrevista, 1 = nunca vista (velo al maximo). El
        // 0,78 esta calibrado sobre el reparto de abajo: deja la entrevista claramente
        // entre las dos, no a medio camino del realce.
        dark[static_cast<size_t>(node.grid_y) * CW + node.grid_x] = (vis[i] >= 2) ? 0.0f : (vis[i] == 1 ? 0.78f : 1.0f);
    }

    std::vector<f32> f(static_cast<size_t>(W) * H, 1.0f);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            f[static_cast<size_t>(y) * W + x] = dark[static_cast<size_t>(y / SUB) * CW + (x / SUB)];
    // Se DILATA lo claro antes de tocar el filo: un minimo sobre vecindad de 2 px mueve la
    // frontera ~2 px FUERA de la celda descubierta. Sin esto, el ruido del filo y los dos
    // desenfoques de abajo mordian HACIA DENTRO (con SUB = 8, casi medio lado de celda) y
    // una celda visitada se veia oscura salvo en el centro: era la mitad del bug de "lo
    // descubierto se ve oscuro". Ahora se comen ese margen y el filo cae en el borde real.
    {
        constexpr int R = 2;
        std::vector<f32> g(f.size(), 1.0f);
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                f32 m = f[static_cast<size_t>(y) * W + x];
                for (int dy = -R; dy <= R; dy++) {
                    for (int dx = -R; dx <= R; dx++) {
                        const int nx = std::min(W - 1, std::max(0, x + dx));
                        const int ny = std::min(H - 1, std::max(0, y + dy));
                        m = std::min(m, f[static_cast<size_t>(ny) * W + nx]);
                    }
                }
                g[static_cast<size_t>(y) * W + x] = m;
            }
        }
        f.swap(g);
    }

    // Filo ORGANICO. La niebla se decide POR CELDA, asi que su frontera nace siendo una
    // recta vertical u horizontal: una rejilla de rectas, que es justo lo que se queria
    // quitar del mapa. Se mueve el filo con ruido, y SOLO el filo -donde la vecindad no es
    // uniforme-, porque perturbar el interior mancharia de claros la zona a oscuras.
    {
        std::vector<f32> g = f;
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                const f32 c = f[static_cast<size_t>(y) * W + x];
                bool edge = false;
                for (int dy = -1; dy <= 1 && !edge; dy++) {
                    for (int dx = -1; dx <= 1 && !edge; dx++) {
                        const int nx = std::min(W - 1, std::max(0, x + dx));
                        const int ny = std::min(H - 1, std::max(0, y + dy));
                        if (std::fabs(f[static_cast<size_t>(ny) * W + nx] - c) > 0.01f) edge = true;
                    }
                }
                if (!edge) continue;
                // Ruido SOLO HACIA FUERA (max(0, n)): simetrico, sobre una celda pisada
                // (c = 0) levantaba hasta 0,42 de oscuridad DENTRO de lo descubierto. El
                // filo sigue siendo organico, pero ya no puede invadir lo pisado.
                const f32 n = map_noise(static_cast<f32>(x), static_cast<f32>(y), 2.6f) - 0.5f;
                g[static_cast<size_t>(y) * W + x] = std::min(1.0f, std::max(0.0f, c + std::max(0.0f, n) * 0.85f));
            }
        }
        f.swap(g);
    }

    for (int pass = 0; pass < 2; pass++) {                    // dos pasadas: el filo deja de ser una recta
        std::vector<f32> t(f.size(), 0.0f);
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                f32 sum = 0.0f, wsum = 0.0f;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        const int nx = std::min(W - 1, std::max(0, x + dx));
                        const int ny = std::min(H - 1, std::max(0, y + dy));
                        const f32 w = (dx == 0 && dy == 0) ? 3.0f : 1.0f;
                        sum += f[static_cast<size_t>(ny) * W + nx] * w;
                        wsum += w;
                    }
                }
                t[static_cast<size_t>(y) * W + x] = sum / wsum;
            }
        }
        f.swap(t);
    }

    // Luz y sombra en la MISMA textura: por debajo de 0,5 un realce calido (lo pisado sale
    // claro, que es lo que se pedia), por encima el velo oscuro de siempre.
    std::vector<u8> px(static_cast<size_t>(W) * H * 4, 0);
    for (size_t i = 0; i < f.size(); i++) {
        const f32 v = std::min(1.0f, std::max(0.0f, f[i]));
        if (v < 0.5f) {
            const f32 k = (0.5f - v) * 2.0f;      // 1 en el centro de lo pisado
            px[i * 4 + 0] = 250; px[i * 4 + 1] = 238; px[i * 4 + 2] = 210;
            px[i * 4 + 3] = static_cast<u8>(k * 46.0f);
        } else {
            const f32 k = (v - 0.5f) * 2.0f;      // 1 en lo nunca visto
            px[i * 4 + 0] = 20; px[i * 4 + 1] = 17; px[i * 4 + 2] = 24;
            px[i * 4 + 3] = static_cast<u8>(k * 252.0f);
        }
    }
    cached_tex = renderer.create_texture_from_rgba("@zonefog", W, H, px.data());
    cached_sig = sig;
    return cached_tex;
}

// Chincheta: disco con borde oscuro. Se lee a cualquier tamano y no compite con el terreno.
static void map_pin(Renderer& renderer, f32 cx, f32 cy, f32 r, Color fill) {
    renderer.draw_panel({cx - r - 0.9f, cy - r - 0.9f, (r + 0.9f) * 2.0f, (r + 0.9f) * 2.0f}, {14, 11, 16, 220}, r + 0.9f);
    renderer.draw_panel({cx - r, cy - r, r * 2.0f, r * 2.0f}, fill, r);
}

// Rellena `vis`: 2 = celda pisada, 1 = entrevista (adyacente, Ojos Abiertos, inicio y, cuando
// ya se sabe donde esta, la del jefe). Lo usan el mapa grande y el minimapa pequeno, que
// pintan lo MISMO. Desde el 2026-09-24 la celda del jefe NO sale de serie: se aprende
// (rooms.h, get_boss_intel).
static void zone_visibility(const HUDContext& ctx, const Room& z, int intel, int* vis, int& cur_out) {
    const FloorMap* fm = ctx.floor_map;
    int cur = fm->current_room;
    if (cur >= 0 && cur < fm->room_count && fm->rooms[cur].interior_of >= 0)
        cur = fm->rooms[cur].interior_of;
    cur_out = cur;
    for (int i = 0; i < z.cell_count && i < fm->room_count; i++) {
        const auto& node = fm->rooms[i];
        if (node.interior_of >= 0) continue;          // los interiores no estan en la rejilla
        bool adjacent = ctx.map_revealed;
        if (cur >= 0 && cur < fm->room_count)
            for (int c = 0; c < 4; c++) if (fm->rooms[cur].connections[c] == i) adjacent = true;
        const bool boss_known = node.type == RoomType::Boss && intel >= 2;
        if (z.cells[i].visited || i == cur)       vis[i] = 2;
        else if (adjacent || boss_known || node.type == RoomType::Start) vis[i] = 1;
    }
}

// =============================================================================
// Puntos de interes (2026-09-24): "en el minimapa deben aparecer los puntos que se deben
// explorar"
// =============================================================================
// Cada celda especial tiene UNA cosa que buscar, y el mapa la marca donde esta de verdad
// (`CellState::poi_tx/ty`, no el centro de cuatro pantallas). Tres estados:
//   - celda sin descubrir: una "?" en el centro de la celda. Dice QUE hay algo, no QUE es:
//     es lo que convierte la niebla en una razon para ir;
//   - celda vista: la chincheta de su color (y en el mapa grande, su icono);
//   - hecho (cofre abierto, relato resuelto, elite despejada...): la chincheta apagada.
// La celda del jefe no lleva "?": su sitio se gana con pistas (get_boss_intel).
enum MapPoiKind : int { POI_CHEST = 0, POI_ALTAR, POI_STORY, POI_ELITE, POI_DOOR, POI_KINDS };
struct MapPoi { int kind; int cell; f32 tx, ty; bool done; };

static const Color POI_COLOR[POI_KINDS] = {
    {255, 210,  60, 255},   // cofre
    {120, 200, 255, 255},   // altar
    {255, 226, 150, 255},   // relato
    {200, 120, 255, 255},   // elite
    {206, 150,  96, 255},   // puerta de un interior
};

static int zone_pois(const HUDContext& ctx, const Room& z, MapPoi* out, int cap) {
    const FloorMap* fm = ctx.floor_map;
    int n = 0;
    auto add = [&](int kind, int cell, int tx, int ty, bool done) {
        if (n < cap) out[n++] = {kind, cell, tx + 0.5f, ty + 0.5f, done};
    };
    for (int i = 0; i < z.cell_count && i < fm->room_count; i++) {
        const auto& node = fm->rooms[i];
        if (node.interior_of >= 0) continue;
        const CellState& cs = z.cells[i];
        switch (node.type) {
            case RoomType::Treasure: if (cs.poi_tx >= 0) add(POI_CHEST, i, cs.poi_tx, cs.poi_ty, cs.chest_opened); break;
            case RoomType::Altar:    if (cs.poi_tx >= 0) add(POI_ALTAR, i, cs.poi_tx, cs.poi_ty, cs.altar_used);   break;
            case RoomType::Event:    if (cs.poi_tx >= 0) add(POI_STORY, i, cs.poi_tx, cs.poi_ty, cs.event_used);   break;
            case RoomType::Elite:    add(POI_ELITE, i, cs.hub_tx, cs.hub_ty, cs.cleared); break;
            default: break;
        }
        // La puerta del interior: "hecha" cuando su premio (altar o cofre) ya se ha usado.
        // Dentro del interior el estado vivo es la sala activa, no la copia de la ranura 1.
        if (node.interior_room >= 0 && z.has_doorway) {
            const int ir = node.interior_room;
            bool done = false;
            const Room* in = (ctx.room && ctx.room->type == RoomType::Interior) ? ctx.room
                           : (ir < FloorMap::MAX_ROOMS && fm->room_generated[ir]) ? &fm->room_cache[1] : nullptr;
            if (in) done = in->cells[0].altar_used || in->cells[0].chest_opened;
            add(POI_DOOR, i, z.door_tx, z.door_ty, done);
        }
    }
    return n;
}

// Los "?" de lo que queda por descubrir: uno por celda NO vista que guarda algo sin hacer.
static void draw_unknown_marks(Renderer& renderer, const Room& z, const FloorMap& fm, const MapPoi* pois, int n,
                               const int* vis, f32 mx, f32 my, f32 s, f32 r, f32 text_scale) {
    bool marked[Config::ZONE_CELLS_MAX] = {};
    for (int k = 0; k < n; k++) {
        const MapPoi& p = pois[k];
        if (p.done || p.cell < 0 || p.cell >= Config::ZONE_CELLS_MAX || vis[p.cell] || marked[p.cell]) continue;
        marked[p.cell] = true;
        const auto& node = fm.rooms[p.cell];
        const f32 cx = mx + (node.grid_x + 0.5f) * z.cell_w * s;
        const f32 cy = my + (node.grid_y + 0.5f) * z.cell_h * s;
        map_pin(renderer, cx, cy, r, {58, 48, 40, 235});
        const f32 tw = renderer.measure_text("?", text_scale);
        renderer.draw_text("?", cx - tw * 0.5f, cy - r * 0.95f, {250, 230, 180, 255}, text_scale);
    }
}

// Las chinchetas de lo ya visto. `icons`: el mapa grande pinta el icono dentro (cofre,
// altar, calavera...); el minimapa, a 1,4 unidades de radio, solo el color.
static void draw_known_pois(Renderer& renderer, const HUDContext& ctx, const MapPoi* pois, int n,
                            const int* vis, f32 mx, f32 my, f32 s, f32 r, bool icons) {
    for (int k = 0; k < n; k++) {
        const MapPoi& p = pois[k];
        if (p.cell < 0 || p.cell >= Config::ZONE_CELLS_MAX || !vis[p.cell]) continue;
        const f32 cx = mx + p.tx * s, cy = my + p.ty * s;
        const Color c = POI_COLOR[p.kind];
        if (p.done) {   // hecho: apagada y mas pequena, para que lo pendiente destaque
            map_pin(renderer, cx, cy, r * 0.62f, {static_cast<u8>(c.r / 2 + 40), static_cast<u8>(c.g / 2 + 40),
                                                  static_cast<u8>(c.b / 2 + 40), 200});
            continue;
        }
        map_pin(renderer, cx, cy, r, c);
        if (!icons || ctx.tileset == TEXTURE_NONE) continue;
        const f32 is = r * 1.5f;
        switch (p.kind) {
            case POI_CHEST: renderer.draw_sprite_rect(ctx.tileset, get_tile_src(TileSprite::Chest), {cx - is * 0.5f, cy - is * 0.5f, is, is}); break;
            case POI_ELITE: renderer.draw_sprite_rect(ctx.tileset, get_tile_src(TileSprite::Skull), {cx - is * 0.5f, cy - is * 0.5f, is, is}); break;
            case POI_ALTAR: {   // sprite de 1x2: solo la mitad de arriba (la llama y el ara)
                Rect src = get_tile_src(TileSprite::AltarGod); src.h *= 0.62f;
                renderer.draw_sprite_rect(ctx.tileset, src, {cx - is * 0.5f, cy - is * 0.55f, is, is * 1.24f});
                break;
            }
            case POI_DOOR: {
                Rect src = get_tile_src(TileSprite::Door); src.y += src.h * 0.25f; src.h *= 0.75f;
                renderer.draw_sprite_rect(ctx.tileset, src, {cx - is * 0.4f, cy - is * 0.6f, is * 0.8f, is * 1.2f});
                break;
            }
            case POI_STORY: {   // "!" de quien tiene algo que contar
                const f32 tw = renderer.measure_text("!", 0.62f);
                renderer.draw_text("!", cx - tw * 0.5f, cy - r * 0.95f, {70, 44, 16, 255}, 0.62f);
                break;
            }
            default: break;
        }
    }
}

// El jefe segun lo que se sabe. Nivel 2: la chincheta roja con su cruz. Nivel 1: el RUMOR,
// un anillo discontinuo rojo con una "?" — dice "por aqui", no "aqui".
static void draw_boss_intel(Renderer& renderer, const HUDContext& ctx, const Room& z, int intel,
                            f32 mx, f32 my, f32 s, f32 pin_r, bool big) {
    if (!z.has_arena || intel <= 0) return;
    if (intel >= 2) {
        int ax, ay;
        z.arena_anchor(ax, ay);
        const f32 bx = mx + (ax + 0.5f) * s, by = my + (ay + 0.5f) * s;
        map_pin(renderer, bx, by, pin_r, {214, 62, 52, 255});
        if (big) {
            renderer.draw_rect({bx - 2.0f, by - 0.5f, 4.0f, 1.0f}, {255, 232, 226, 255});
            renderer.draw_rect({bx - 0.5f, by - 2.0f, 1.0f, 4.0f}, {255, 232, 226, 255});
        }
        return;
    }
    f32 cx = 0.0f, cy = 0.0f, r = 0.0f;
    boss_rumor_circle(z, cx, cy, r);
    const f32 px = mx + cx * s, py = my + cy * s, pr = r * s;
    const f32 mw = z.tiles_w * s, mh = z.tiles_h * s;
    const int dots = big ? 44 : 22;
    const f32 pulse = 0.75f + 0.25f * std::sin(ctx.time * 3.0f);
    const u8 a = static_cast<u8>(230 * pulse);
    for (int i = 0; i < dots; i++) {
        const f32 t = (i + (big ? 0.0f : 0.5f)) / dots * 6.2831853f;
        const f32 dx = px + std::cos(t) * pr, dy = py + std::sin(t) * pr;
        if (dx < mx || dy < my || dx > mx + mw || dy > my + mh) continue;   // fuera del mapa, nada
        const f32 d = big ? 1.6f : 0.9f;
        renderer.draw_rect({dx - d * 0.5f, dy - d * 0.5f, d, d}, {224, 70, 56, a});
    }
    const f32 qx = std::min(std::max(px, mx + pin_r * 2.0f), mx + mw - pin_r * 2.0f);
    const f32 qy = std::min(std::max(py, my + pin_r * 2.0f), my + mh - pin_r * 2.0f);
    map_pin(renderer, qx, qy, pin_r, {150, 40, 34, 235});
    const f32 ts = big ? 0.62f : 0.42f;
    const f32 tw = renderer.measure_text("?", ts);
    renderer.draw_text("?", qx - tw * 0.5f, qy - pin_r * 0.95f, {255, 226, 214, 255}, ts);
}

// El terreno con su marco, dentro del rect dado. Lo comparten el mapa grande y el minimapa:
// tener DOS dibujos distintos del mismo sitio era justo lo que hacia que el pequeno siguiera
// siendo una rejilla de cajas cuando el grande ya no lo era.
static void draw_zone_canvas(Renderer& renderer, const Room& z, const FloorMap& fm,
                             const int* vis, Rect area, f32& out_mx, f32& out_my, f32& out_s) {
    const f32 s = std::min(area.w / std::max(1, z.tiles_w), area.h / std::max(1, z.tiles_h));
    const f32 mw = z.tiles_w * s, mh = z.tiles_h * s;
    const f32 mx = area.x + (area.w - mw) * 0.5f, my = area.y + (area.h - mh) * 0.5f;
    out_mx = mx; out_my = my; out_s = s;

    // Sombra arrojada, marco de pergamino, filete oscuro y el mapa dentro. El marco usa el
    // mismo panel de 9 trozos que los botones: una sola pieza para todo el juego.
    const f32 pad = std::max(1.6f, s * 1.2f);
    renderer.draw_panel({mx - pad, my - pad + 1.5f, mw + pad * 2.0f, mh + pad * 2.0f}, {0, 0, 0, 120}, pad + 2.0f);
    renderer.draw_panel({mx - pad, my - pad, mw + pad * 2.0f, mh + pad * 2.0f}, {96, 78, 56, 255}, pad + 2.0f);
    renderer.draw_panel({mx - 1.2f, my - 1.2f, mw + 2.4f, mh + 2.4f}, {34, 28, 24, 255}, 2.4f);

    int tw = 0, th = 0;
    const u32 terrain = zone_terrain_texture(renderer, z, tw, th);
    if (terrain != TEXTURE_NONE)
        renderer.draw_sprite_rect(terrain, {0, 0, static_cast<f32>(tw), static_cast<f32>(th)}, {mx, my, mw, mh});
    else
        renderer.draw_rect({mx, my, mw, mh}, {112, 96, 72, 255});

    int fw = 0, fh = 0;
    const u32 fog = zone_fog_texture(renderer, fm, vis, fw, fh);
    if (fog != TEXTURE_NONE)
        renderer.draw_sprite_rect(fog, {0, 0, static_cast<f32>(fw), static_cast<f32>(fh)}, {mx, my, mw, mh});
}

// Donde esta el profeta, en unidades de pantalla. Dentro de un interior no hay punto que
// valga: se marca el centro de su celda de FUERA, que es donde esta en la rejilla.
static bool prophet_marker(const HUDContext& ctx, const Room& z, int cur, f32 mx, f32 my, f32 s,
                           f32& px, f32& py) {
    const FloorMap* fm = ctx.floor_map;
    if (ctx.player_in_zone) {
        px = mx + (ctx.player_pos.x / Config::TILE_SIZE) * s;
        py = my + (ctx.player_pos.y / Config::TILE_SIZE) * s;
        return true;
    }
    if (cur >= 0 && cur < fm->room_count) {
        px = mx + (fm->rooms[cur].grid_x + 0.5f) * z.cell_w * s;
        py = my + (fm->rooms[cur].grid_y + 0.5f) * z.cell_h * s;
        return true;
    }
    return false;
}

static void draw_zone_map(Renderer& renderer, const HUDContext& ctx, const Room& z) {
    const FloorMap* fm = ctx.floor_map;
    const f32 GW = static_cast<f32>(Config::GAME_WIDTH), GH = static_cast<f32>(Config::GAME_HEIGHT);
    renderer.draw_rect({0, 0, GW, GH}, {6, 5, 9, 205});
    renderer.draw_text_centered(tr("MAPA DEL CAPÍTULO", "CHAPTER MAP"), 3.0f, {240, 220, 160, 255});

    // Area util: debajo del titulo y el objetivo, encima de las dos lineas de pie. La
    // escala sale de DIVIDIR, no de una constante: los capitulos 5 y 10 tienen una fila
    // de celdas mas (6x5) y con un valor fijo se saldrian por abajo.
    // AREA_Y = 31 y no 24: justo debajo del panel de vitales, que llega a 30,75. Con 24 el
    // mapa le cortaba por la mitad el nombre del nivel de fe y parecia un fallo de dibujo.
    const BossIntel intel = ctx.tutorial ? BossIntel{} : get_boss_intel(z, *fm, ctx.map_revealed);
    int vis[Config::ZONE_CELLS_MAX] = {};
    int cur = -1;
    zone_visibility(ctx, z, intel.level, vis, cur);

    f32 mx = 0.0f, my = 0.0f, s = 0.0f;
    draw_zone_canvas(renderer, z, *fm, vis, {13.0f, 31.0f, 294.0f, 118.0f}, mx, my, s);

    MapPoi pois[Config::ZONE_CELLS_MAX * 2];
    const int npois = zone_pois(ctx, z, pois, Config::ZONE_CELLS_MAX * 2);

    // --- Rotulos: es lo que contesta "¿que hago aqui?" -----------------------------
    // Van ENCIMA de la chincheta de su punto (no en el centro de la celda: con celdas de
    // cuatro pantallas el centro puede caer lejos de la cosa). Solo los de celdas vistas.
    for (int i = 0; i < z.cell_count && i < fm->room_count; i++) {
        if (!vis[i]) continue;
        const auto& node = fm->rooms[i];
        if (node.interior_of >= 0) continue;
        const char* label = nullptr;
        Color plate = {28, 23, 20, 228};
        switch (node.type) {
            case RoomType::Start:    label = tr("INICIO", "START"); break;
            case RoomType::Boss:     if (intel.level >= 2) { label = tr("JEFE", "BOSS"); plate = {122, 36, 30, 238}; } break;
            case RoomType::Treasure: label = tr("TESORO", "TREASURE"); plate = {104, 76, 18, 235}; break;
            case RoomType::Altar:    label = "ALTAR";                  plate = { 26, 60, 96, 235}; break;
            case RoomType::Elite:    label = tr("ELITE", "ELITE");     plate = { 68, 34, 96, 235}; break;
            case RoomType::Event:    label = tr("RELATO", "STORY");    plate = { 86, 70, 26, 235}; break;
            default: break;
        }
        if (!label) continue;
        const CellState& cs = z.cells[i];
        f32 cx = mx + (cs.hub_tx + 0.5f) * s;
        f32 cy = my + (cs.hub_ty + 0.5f) * s - 7.5f;
        if (cs.poi_tx >= 0) { cx = mx + (cs.poi_tx + 0.5f) * s; cy = my + (cs.poi_ty + 0.5f) * s - 7.5f; }
        // El del JEFE cuelga BAJO su chincheta: su celda no tiene el centro donde esta la
        // arena, y encima del pin choca con el borde del mapa (la arena va pegada al borde).
        if (node.type == RoomType::Boss && z.has_arena) {
            int bx, by;
            z.arena_anchor(bx, by);
            cx = mx + (bx + 0.5f) * s;
            cy = my + (by + 0.5f) * s + 8.0f;
            // Con la arena en la fila de abajo (arena South) colgaria fuera del mapa: encima
            if (cy + 4.0f > my + z.tiles_h * s) cy -= 16.0f;
        }
        const f32 lw = renderer.measure_text(label, 0.55f);
        cx = std::min(std::max(cx, mx + lw * 0.5f + 3.0f), mx + z.tiles_w * s - lw * 0.5f - 3.0f);
        cy = std::min(std::max(cy, my + 4.2f), my + z.tiles_h * s - 4.2f);
        renderer.draw_panel({cx - lw * 0.5f - 3.0f, cy - 4.0f, lw + 6.0f, 8.0f}, plate, 3.0f);
        renderer.draw_text(label, cx - lw * 0.5f, cy - 2.8f,
                           node.type == RoomType::Boss ? Color{255, 226, 220, 255}
                                                       : Color{240, 230, 210, 245}, 0.55f);
    }

    // --- Lo que queda por explorar, lo visto, el jefe y la salida ---------------------
    draw_unknown_marks(renderer, z, *fm, pois, npois, vis, mx, my, s, 3.4f, 0.62f);
    draw_known_pois(renderer, ctx, pois, npois, vis, mx, my, s, 3.2f, true);
    draw_boss_intel(renderer, ctx, z, intel.level, mx, my, s, 3.4f, true);
    if (z.exit_open) {
        const f32 ex = mx + (z.exit_tx + 0.5f) * s, ey = my + (z.exit_ty + 0.5f) * s;
        map_pin(renderer, ex, ey, 2.6f + 0.6f * std::sin(ctx.time * 4.0f), {104, 226, 142, 255});
    }

    // --- Donde esta el profeta --------------------------------------------------
    f32 px = 0.0f, py = 0.0f;
    if (prophet_marker(ctx, z, cur, mx, my, s, px, py)) {
        // Halo que late: en un mapa grande, el punto propio es lo primero que se busca.
        const f32 halo = 4.6f + 1.5f * std::sin(ctx.time * 3.4f);
        if (renderer.shadow_tex != TEXTURE_NONE)
            renderer.draw_sprite_rect(renderer.shadow_tex, {0, 0, 64, 32},
                                      {px - halo, py - halo, halo * 2.0f, halo * 2.0f},
                                      {255, 236, 170, 120});
        map_pin(renderer, px, py, 2.4f, {255, 246, 214, 255});
    }

    // --- Cuanto se ha explorado: celdas pisadas, puntos hechos y pistas del jefe -----
    // Arriba a la derecha, fuera del panel de vitales (que acaba en x = 126).
    if (!ctx.tutorial) {
        int cells = 0, seen = 0, done = 0;
        for (int i = 0; i < z.cell_count && i < fm->room_count; i++) {
            if (fm->rooms[i].interior_of >= 0) continue;
            cells++;
            if (z.cells[i].visited) seen++;
        }
        for (int k = 0; k < npois; k++) if (pois[k].done) done++;
        char stats[96];
        snprintf(stats, sizeof(stats), tr("Explorado %d/%d   Hallazgos %d/%d   Pistas %d/%d",
                                          "Explored %d/%d   Finds %d/%d   Clues %d/%d"),
                 seen, cells, done, npois, std::min(intel.clues, BOSS_CLUES_FOR_EXACT), BOSS_CLUES_FOR_EXACT);
        const f32 sw = renderer.measure_text(stats, 0.6f);
        renderer.draw_text(stats, GW - sw - 6.0f, 18.0f, {214, 200, 170, 235}, 0.6f);
    }

    // EL OBJETIVO DEL CAPITULO (`ChapterLore::objective`) va aqui abajo y no bajo el titulo:
    // arriba es donde esta el panel de vitales y un objetivo largo se le montaba encima del
    // porcentaje de fe. Es lo que convierte el mapa en una respuesta a "¿que hago aqui?".
    if (ctx.room) {
        const char* obj = get_chapter_lore(ctx.room->chapter).objective;
        if (obj && obj[0]) renderer.draw_text_centered(obj, 152.0f, {248, 216, 130, 255}, 0.85f);
    }
    // La linea de ayuda sigue a lo que se sabe del jefe: con el jefe oculto, "llega a la
    // chincheta roja" seria pedir algo que el mapa todavia no ensena.
    const char* help =
        intel.level >= 2 ? tr("Llega al JEFE (la chincheta roja) y derrótalo para abrir la salida",
                               "Reach the BOSS (red pin) and beat it to open the exit")
      : intel.level == 1 ? tr("El jefe anda por la zona roja: acércate o busca otra pista",
                              "The boss is somewhere in the red ring: go closer or find another clue")
                         : tr("¿Dónde está el jefe? Relatos, élites y altares dan pistas. ? = por explorar",
                              "Where is the boss? Stories, elites and altars give clues. ? = unexplored");
    renderer.draw_text_centered(help, 162.0f, {200, 200, 200, 255}, 0.7f);
    renderer.draw_text_centered(ctx.input && ctx.input->touch_ui
                                ? tr("Toca el mapa para volver", "Tap the map to go back")
                                : tr("Suelta TAB para volver", "Release TAB to go back"),
                                171.0f, {160, 160, 160, 255}, 0.7f);
}

// El minimapa pequeno: el MISMO terreno que el mapa grande, en pequeno. Antes era una
// rejilla de cajas con borde -una caja por celda- y eso es lo que el usuario seguia leyendo
// como "cuadriculado" aunque el grande ya pintara terreno. Su rect exterior sale de
// `HUD::minimap_rect`, que comparte `Input::touch_minimap`: dibujo y hit-test siguen
// saliendo de la MISMA formula.
static void draw_zone_minimap(Renderer& renderer, const HUDContext& ctx, const Room& z) {
    const FloorMap* fm = ctx.floor_map;
    const Rect frame = HUD::minimap_rect(*fm);
    if (frame.w <= 0.0f) return;

    const BossIntel intel = ctx.tutorial ? BossIntel{} : get_boss_intel(z, *fm, ctx.map_revealed);
    int vis[Config::ZONE_CELLS_MAX] = {};
    int cur = -1;
    zone_visibility(ctx, z, intel.level, vis, cur);

    f32 mx = 0.0f, my = 0.0f, s = 0.0f;
    draw_zone_canvas(renderer, z, *fm, vis,
                     {frame.x + 2.2f, frame.y + 2.2f, frame.w - 4.4f, frame.h - 4.4f}, mx, my, s);

    MapPoi pois[Config::ZONE_CELLS_MAX * 2];
    const int npois = zone_pois(ctx, z, pois, Config::ZONE_CELLS_MAX * 2);
    draw_unknown_marks(renderer, z, *fm, pois, npois, vis, mx, my, s, 1.7f, 0.36f);
    draw_known_pois(renderer, ctx, pois, npois, vis, mx, my, s, 1.25f, false);
    draw_boss_intel(renderer, ctx, z, intel.level, mx, my, s, 1.6f, false);
    if (z.exit_open)
        map_pin(renderer, mx + (z.exit_tx + 0.5f) * s, my + (z.exit_ty + 0.5f) * s, 1.4f, {104, 226, 142, 255});

    f32 px = 0.0f, py = 0.0f;
    if (prophet_marker(ctx, z, cur, mx, my, s, px, py))
        map_pin(renderer, px, py, 1.4f, {255, 246, 214, 255});
}

void HUD::draw_minimap(Renderer& renderer, const HUDContext& ctx, bool big) {
    const FloorMap* fm = ctx.floor_map;
    if (!fm || fm->room_count <= 0) return;

    // Los DOS mapas pintan el terreno de la zona. La rejilla de celdas de abajo se queda
    // solo como respaldo para cuando no hay zona construida (un interior suelto): si hay
    // zona, ni el grande ni el pequeno vuelven a dibujar una caja por sala.
    const Room* zone = ctx.zone;
    if (zone && zone->cell_count > 1) {
        if (big) draw_zone_map(renderer, ctx, *zone);
        else     draw_zone_minimap(renderer, ctx, *zone);
        return;
    }

    f32 cw = big ? 34.0f : 9.0f;
    f32 ch = big ? 18.0f : 6.0f;
    f32 gap = big ? 12.0f : 2.0f;
    f32 total_w = fm->grid_w * (cw + gap) - gap;
    f32 total_h = fm->grid_h * (ch + gap) - gap;
    // El minimapa pequeno pierde la SEPARACION entre celdas: el capitulo es un territorio
    // continuo y los huecos lo contaban como salas sueltas, que es lo que el usuario leyo
    // como "cuadriculado". El rect de fuera no cambia ni un pixel a proposito —lo comparte
    // `Input::touch_minimap` para que tocarlo abra el mapa grande— asi que las celdas se
    // ensanchan hasta llenarlo en vez de moverlo.
    // Mismas medidas que `minimap_rect` (12x7 por celda desde el mapa grande).
    if (!big) { gap = 0.0f; cw = 12.0f; ch = 7.0f; total_w = fm->grid_w * cw; total_h = fm->grid_h * ch; }
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
        // El jefe ya no sale de serie (pistas del jefe, 2026-09-24): sin zona no hay pistas.
        bool known = visited || adjacent || i == cur || node.type == RoomType::Start;
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
        if (node.type == RoomType::Boss && known) {
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
                case RoomType::Boss:     label = known ? tr("JEFE", "BOSS") : nullptr; break;
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
// La linea bajo el minimapa: hacia donde queda el jefe, SEGUN LO QUE SE SABE (rooms.h,
// get_boss_intel). Sin pistas no hay direccion; con el rumor, el rumbo aproximado a la zona
// roja ("~NE"); con el sitio, las celdas que faltan como siempre ("2N 1E").
void HUD::draw_objective(Renderer& renderer, const HUDContext& ctx) {
    const FloorMap* fm = ctx.floor_map;
    if (!fm || fm->room_count <= 0) return;
    const auto& cur = fm->rooms[fm->current_room];
    const auto& boss = fm->rooms[fm->boss_room];
    int dx = boss.grid_x - cur.grid_x;
    int dy = boss.grid_y - cur.grid_y;
    const int intel = (ctx.zone && !ctx.tutorial) ? get_boss_intel(*ctx.zone, *fm, ctx.map_revealed).level : 0;

    char buf[48];
    // En el tutorial el mapa es el del capitulo 1 —el que se va a jugar al salir— y no la
    // sala de tres piezas que se esta pisando: una direccion de jefe aqui seria mentira.
    if (ctx.tutorial) {
        snprintf(buf, sizeof(buf), "%s", tr("Mapa del cap. 1", "Chapter 1 map"));
    } else if (intel <= 0) {
        snprintf(buf, sizeof(buf), "%s", tr("Jefe: ???", "Boss: ???"));
    } else if (intel == 1) {
        f32 cx = 0.0f, cy = 0.0f, r = 0.0f;
        boss_rumor_circle(*ctx.zone, cx, cy, r);
        Vec2 from = ctx.player_in_zone ? ctx.player_pos
                  : Vec2{(cur.grid_x + 0.5f) * Config::ROOM_W_UNITS, (cur.grid_y + 0.5f) * Config::ROOM_H_UNITS};
        const f32 ddx = cx * Config::TILE_SIZE - from.x, ddy = cy * Config::TILE_SIZE - from.y;
        if (ddx * ddx + ddy * ddy < (r * Config::TILE_SIZE) * (r * Config::TILE_SIZE) * 0.25f)
            snprintf(buf, sizeof(buf), "%s", tr("Jefe: cerca", "Boss: near"));
        else
            snprintf(buf, sizeof(buf), tr("Jefe: ~%s", "Boss: ~%s"), compass_name(ddx, ddy, true));
    } else if (dx == 0 && dy == 0) {
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
    const Rect mm = minimap_rect(*fm);
    f32 y = mm.y + mm.h + 3.0f;
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

    // Hasta dos lineas, partidas por '\n'. Lo pide la pista de milagro, que tiene que decir
    // DOS cosas ("toca para lanzarlo" / "manten para ver que hace") y en una linea no cabian
    // sin bajar la escala hasta lo ilegible. Un mensaje de una sola linea sale exactamente
    // igual que antes.
    const char* lines[2] = {ctx.message, nullptr};
    char buf[192];
    const char* br = std::strchr(ctx.message, '\n');
    if (br) {
        size_t n = std::min(sizeof(buf) - 1, static_cast<size_t>(br - ctx.message));
        std::memcpy(buf, ctx.message, n);
        buf[n] = '\0';
        lines[0] = buf;
        lines[1] = br + 1;
    }
    const int count = lines[1] ? 2 : 1;

    f32 scale = 1.0f, tw = 0.0f;
    for (int pass = 0; pass < 2; pass++) {
        tw = 0.0f;
        for (int i = 0; i < count; i++) tw = std::max(tw, renderer.measure_text(lines[i], scale));
        if (tw <= 300.0f) break;
        scale = 0.75f;
    }
    const f32 lh = 11.0f * scale;
    f32 y = Config::GAME_HEIGHT - 44.0f - (count - 1) * lh;
    renderer.draw_rect({(Config::GAME_WIDTH - tw) * 0.5f - 4, y - 2, tw + 8, 12 + (count - 1) * lh},
                       {0, 0, 0, static_cast<u8>(140 * a)});
    for (int i = 0; i < count; i++)
        renderer.draw_text_centered(lines[i], y + i * lh, {255, 240, 200, alpha}, scale);
}
