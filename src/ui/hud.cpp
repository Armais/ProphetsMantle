// =============================================================================
// MANTLE - HUD Implementation
// =============================================================================

#include "ui/hud.h"
#include "core/config.h"
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

static Color rarity_color(ItemRarity r) {
    switch (r) {
        case ItemRarity::Common:    return {200, 195, 185, 255};
        case ItemRarity::Uncommon:  return {120, 210, 120, 255};
        case ItemRarity::Rare:      return {110, 175, 245, 255};
        default:                    return {245, 205, 90, 255};
    }
}

static const char* rarity_name(ItemRarity r) {
    switch (r) {
        case ItemRarity::Common:    return "COMUN";
        case ItemRarity::Uncommon:  return "POCO COMUN";
        case ItemRarity::Rare:      return "RARO";
        default:                    return "LEGENDARIO";
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

void draw_choice_cards(Renderer& renderer, const char* title, const ChoiceCard* cards,
                       int count, int selected, u32 tileset) {
    const f32 W = static_cast<f32>(Config::GAME_WIDTH);
    const f32 H = static_cast<f32>(Config::GAME_HEIGHT);
    renderer.draw_rect({0, 0, W, H}, {10, 8, 16, 175});

    renderer.draw_text_centered(title, 12, {245, 205, 90, 255}, 1.3f);

    constexpr f32 CARD_W = 92.0f, CARD_H = 86.0f, GAP = 8.0f, TOP = 34.0f;
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

        // Name (up to 2 lines)
        char lines[2][40];
        int n = wrap_text(renderer, card.name, CARD_W - 8.0f, 0.8f, lines, 2);
        for (int l = 0; l < n; l++)
            centred(lines[l], cx, y + 30.0f + l * 8.0f, on ? TEXT_LIGHT : Color{215, 208, 195, 255}, 0.8f);

        f32 ty = y + 30.0f + n * 8.0f + 3.0f;
        if (card.tag[0]) { centred(card.tag, cx, ty, rc, 0.55f); ty += 7.0f; }
        if (card.extra[0]) { centred(card.extra, cx, ty, {230, 170, 90, 255}, 0.55f); ty += 7.0f; }

        // Description (up to 3 lines)
        char dl[3][40];
        int dn = wrap_text(renderer, card.desc, CARD_W - 8.0f, 0.62f, dl, 3);
        for (int l = 0; l < dn; l++)
            centred(dl[l], cx, ty + 2.0f + l * 6.5f, {180, 172, 160, 255}, 0.62f);
    }

    renderer.draw_text_centered("[A/D] elegir     [ENTER] tomar", H - 14.0f, {170, 160, 150, 220}, 0.7f);
}

void draw_item_choice(Renderer& renderer, const char* title, const ItemDef* const defs[3],
                      int count, int selected, u32 tileset) {
    ChoiceCard cards[3];
    for (int i = 0; i < count && i < 3; i++) {
        if (!defs[i]) continue;
        cards[i].name   = defs[i]->name;
        cards[i].tag    = rarity_name(defs[i]->rarity);
        cards[i].extra  = defs[i]->heredable ? "HEREDABLE" : "";
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
        snprintf(tags[i], sizeof(tags[i]), "TIER %d  -  %.0f FE", d.tier, d.faith_cost);
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
    draw_miracle_slots(renderer, player, ctx, faith);
    draw_minimap(renderer, ctx, ctx.show_map);
    if (!ctx.show_map) draw_objective(renderer, ctx);
    if (ctx.boss != ENTITY_NONE) draw_boss_bar(renderer, world, ctx.boss);
    draw_message(renderer, ctx);
    draw_banner(renderer, ctx);
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

    // Percentage + level name
    char buf[48];
    snprintf(buf, sizeof(buf), "%.0f%%", faith.current);
    renderer.draw_text(buf, BAR_X + BAR_W + 4, y - 2.0f, TEXT_LIGHT, 0.85f);
    renderer.draw_text(faith.get_level_name(), BAR_X, y + h + 1.0f, faith_color, 0.65f);
}

// Small icon row under the vitals: one cell per item taken this run. The held consumable
// gets a highlighted frame and its key, since it is the only one the player can trigger.
void HUD::draw_inventory(Renderer& renderer, const Player& player, const HUDContext& ctx) {
    if (ctx.tileset == TEXTURE_NONE || !ctx.registry || player.items.empty()) return;
    constexpr f32 S = 9.0f, Y = 33.0f;
    f32 x = 3.0f;
    int shown = 0;
    for (u32 id : player.items) {
        if (shown >= 12) break;
        const ItemDef* def = ctx.registry->get(id);
        if (!def) continue;
        bool held = (id == player.consumable_slot);
        // Frame carries the rarity colour; the icon stays untinted so it reads at 9 units
        renderer.draw_rect({x - 1, Y - 1, S + 2, S + 2}, held ? Color{245, 205, 90, 230} : rarity_color(def->rarity));
        renderer.draw_rect({x, Y, S, S}, {22, 16, 26, 220});
        renderer.draw_sprite_rect(ctx.tileset, get_tile_src(category_icon(def->category)), {x, Y, S, S});
        x += S + 2.0f;
        shown++;
    }
    if (player.consumable_slot != UINT32_MAX)
        renderer.draw_text("R", x + 1.0f, Y + 1.5f, {245, 205, 90, 230}, 0.6f);
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
    static const char* keys[2] = {"Q", "E"};
    const f32 S = 22.0f;   // slot size
    for (int i = 0; i < player.active_slot_count; i++) {
        f32 x = static_cast<f32>(Config::GAME_WIDTH) - 28.0f - i * 27.0f;
        f32 y = static_cast<f32>(Config::GAME_HEIGHT) - 36.0f;

        const auto& slot = player.miracle_slots[i];
        bool locked = false;
        if (!slot.is_empty()) {
            auto def = get_miracle_def(slot.miracle);
            locked = def.tier > faith.get_max_miracle_tier() || def.faith_cost > faith.current;
        }
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

        // Cooldown sweep (bottom-up)
        if (!slot.is_empty() && !slot.is_ready()) {
            auto def = get_miracle_def(slot.miracle);
            f32 cd_ratio = def.cooldown > 0.0f ? slot.cooldown_remaining / def.cooldown : 0.0f;
            renderer.draw_rect({x, y + S * (1 - cd_ratio), S, S * cd_ratio}, {0, 0, 0, 150});
        }
        // Ready glow line
        if (ready) renderer.draw_rect({x, y + S - 1.0f, S, 1.0f}, {255, 220, 120, 200});

        // Key hint badge under the slot
        f32 kx = x + S * 0.5f, ky = y + S + 5.0f;
        if (renderer.shadow_tex != TEXTURE_NONE)
            renderer.draw_sprite_rect(renderer.shadow_tex, {0, 0, 64, 32}, {kx - 6, ky - 5, 12, 10}, {0, 0, 0, 180});
        renderer.draw_text(keys[i], kx - renderer.measure_text(keys[i], 0.7f) * 0.5f, ky - 3.0f, {235, 225, 200, 255}, 0.7f);

        // Cost (faith orb + number) above the slot
        if (!slot.is_empty()) {
            auto def = get_miracle_def(slot.miracle);
            char cost[8];
            snprintf(cost, sizeof(cost), "%.0f", def.faith_cost);
            Color cc = locked ? Color{240, 120, 100, 255} : Color{160, 210, 250, 255};
            f32 tw = renderer.measure_text(cost, 0.65f);
            f32 cx0 = x + S - tw - 1.0f;
            if (ctx.tileset != TEXTURE_NONE)
                renderer.draw_sprite_rect(ctx.tileset, get_tile_src(TileSprite::Orb), {cx0 - 7.0f, y - 8.5f, 7, 7});
            renderer.draw_text(cost, cx0, y - 9.0f, cc, 0.65f);
        }
    }
}

// -----------------------------------------------------------------------------
// Minimap: top-right. Each room a small cell; visited = filled, current = bright,
// boss = red with skull-ish mark, treasure/altar = gold/blue dot when visited or adjacent.
// -----------------------------------------------------------------------------
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
        renderer.draw_text_centered("MAPA DEL CAPITULO", oy - 16.0f, {240, 220, 160, 255});
    } else {
        renderer.draw_rect({ox - 2, oy - 2, total_w + 4, total_h + 4}, {0, 0, 0, 110});
    }

    int cur = fm->current_room;
    bool blink = (static_cast<int>(ctx.time * 3.0f) % 2) == 0;

    for (int i = 0; i < fm->room_count; i++) {
        const auto& node = fm->rooms[i];
        bool visited = fm->room_generated[i] && fm->room_cache[i].visited;
        bool adjacent = false;
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
        if (mark.a) renderer.draw_rect({x + cw * 0.5f - 1.5f, y + ch * 0.5f - 1.5f, 3, 3}, mark);
        if (node.type == RoomType::Boss) {
            // small skull-ish cross
            renderer.draw_rect({x + cw * 0.5f - 0.5f, y + 1, 1, ch - 2}, {255, 240, 240, 255});
            renderer.draw_rect({x + 1, y + ch * 0.5f - 0.5f, cw - 2, 1}, {255, 240, 240, 255});
        }
        if (visited && fm->room_cache[i].cleared && node.type != RoomType::Boss && big) {
            renderer.draw_text("ok", x + 2, y + 4, {40, 30, 20, 255}, 0.75f);
        }
        if (big) {
            const char* label = nullptr;
            switch (node.type) {
                case RoomType::Start:    label = "INICIO"; break;
                case RoomType::Boss:     label = "JEFE"; break;
                case RoomType::Treasure: label = (visited || adjacent) ? "TESORO" : nullptr; break;
                case RoomType::Altar:    label = (visited || adjacent) ? "ALTAR" : nullptr; break;
                case RoomType::Elite:    label = (visited || adjacent) ? "ELITE" : nullptr; break;
                default: break;
            }
            if (label) renderer.draw_text(label, x + 1, y + ch + 1, {220, 210, 190, 255}, 0.6f);
        }
    }

    if (big) {
        renderer.draw_text_centered("Llega al JEFE (rojo) y derrotalo para abrir la salida", oy + total_h + 12.0f, {200, 200, 200, 255}, 0.75f);
        renderer.draw_text_centered("Suelta TAB para volver", oy + total_h + 22.0f, {160, 160, 160, 255}, 0.75f);
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
        bool cleared = ctx.room && ctx.room->cleared;
        snprintf(buf, sizeof(buf), cleared ? "Salida abierta" : "!! JEFE !!");
    } else {
        // e.g. "Jefe: 2N 1E"
        char part1[12] = "", part2[12] = "";
        if (dy != 0) snprintf(part1, sizeof(part1), "%d%c", std::abs(dy), dy < 0 ? 'N' : 'S');
        if (dx != 0) snprintf(part2, sizeof(part2), "%s%d%c", dy != 0 ? " " : "", std::abs(dx), dx < 0 ? 'O' : 'E');
        snprintf(buf, sizeof(buf), "Jefe: %s%s", part1, part2);
    }
    f32 tw = renderer.measure_text(buf, 0.75f);
    f32 y = 4.0f + fm->grid_h * 8.0f + 3.0f;
    renderer.draw_text(buf, Config::GAME_WIDTH - tw - 5.0f, y, {230, 215, 170, 255}, 0.75f);
    renderer.draw_text("TAB: mapa", Config::GAME_WIDTH - renderer.measure_text("TAB: mapa", 0.6f) - 5.0f, y + 8.0f, {150, 150, 150, 255}, 0.6f);
}

void HUD::draw_boss_bar(Renderer& renderer, const ECSWorld& world, Entity boss) {
    if (boss == ENTITY_NONE || !world.is_alive(boss) || !world.has(boss, Component::Health)) return;
    const auto& h = world.healths[boss];
    if (!h.is_alive()) return;
    f32 w = 160.0f, x = (Config::GAME_WIDTH - w) * 0.5f, y = Config::GAME_HEIGHT - 14.0f;
    const char* name = world.has(boss, Component::EnemyAI) ? world.enemy_ais[boss].name : "JEFE";
    // Name plate + framed red bar with a skull badge on the left
    f32 nw = renderer.measure_text(name, 0.8f);
    renderer.draw_rect({(Config::GAME_WIDTH - nw) * 0.5f - 6, y - 11.0f, nw + 12, 9.0f}, {0, 0, 0, 120});
    renderer.draw_text_centered(name, y - 11.0f, {255, 225, 185, 255}, 0.8f);
    draw_bar(renderer, {x, y, w, 6}, h.ratio(), {215, 55, 45, 255}, 1.25f);
    draw_badge(renderer, world.tileset, TileSprite::Skull, x - 6.0f, y + 3.0f, 11.0f);
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
