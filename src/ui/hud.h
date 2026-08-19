#pragma once

// =============================================================================
// MANTLE - HUD (In-Game Overlay) (GDD 4.2)
// =============================================================================
//
// Layout:
//   Top-left:     HP / FE bars + faith level name
//   Top-right:    Minimap (rooms visited, current, boss marked) + objective hint
//   Bottom-right: Miracle slots
//   Bottom-center: Boss health bar (boss rooms) / transient messages
//   Center:       Chapter banner
//
// =============================================================================

#include "render/renderer.h"
#include "game/faith.h"
#include "game/player.h"
#include "game/rooms.h"
#include "game/items.h"
#include "core/ecs.h"

struct HUDContext {
    const FloorMap* floor_map = nullptr;
    const Room*     room      = nullptr;
    Entity          boss      = ENTITY_NONE;
    int             chapter   = 1;
    const char*     banner    = nullptr;   // Big centred title (chapter start)
    const char*     subbanner = nullptr;   // Smaller line under the banner
    f32             banner_t  = 0.0f;      // Seconds remaining
    const char*     message   = nullptr;   // Small transient message
    f32             message_t = 0.0f;
    f32             time      = 0.0f;
    bool            show_map  = false;     // Tab held: big map
    u32             tileset   = TEXTURE_NONE;
    const ItemRegistry* registry = nullptr;   // Needed to draw the inventory icons
};

// Choice overlay (GameScene::Choice): up to 3 cards drawn over the frozen world.
// Items and miracles both feed the same card renderer.
struct ChoiceCard {
    const char* name  = "";
    const char* tag   = "";      // rarity / tier line, in `accent`
    const char* extra = "";      // "HEREDABLE", "Sustituye a X"... (may be empty)
    const char* desc  = "";
    Color accent      = {200, 195, 185, 255};
    TileSprite icon   = TileSprite::Star;
};

void draw_choice_cards(Renderer& renderer, const char* title, const ChoiceCard* cards,
                       int count, int selected, u32 tileset);
// `defs[i]` may be null if an offer could not be rolled.
void draw_item_choice(Renderer& renderer, const char* title, const ItemDef* const defs[3],
                      int count, int selected, u32 tileset);
// `extras[i]` is an optional line per card ("Sustituye a Cuervos de Querit").
void draw_miracle_choice(Renderer& renderer, const char* title, const MiracleID* ids, int count,
                         int selected, u32 tileset, const char* const* extras);

struct HUD {
    bool visible = true;

    void render(Renderer& renderer, const Player& player,
                const ECSWorld& world, const FaithSystem& faith, const HUDContext& ctx);

private:
    void draw_health_bar(Renderer& renderer, f32 current, f32 max, const HUDContext& ctx);
    void draw_faith_bar(Renderer& renderer, const FaithSystem& faith, const HUDContext& ctx);
    void draw_miracle_slots(Renderer& renderer, const Player& player, const HUDContext& ctx,
                            const FaithSystem& faith);
    void draw_minimap(Renderer& renderer, const HUDContext& ctx, bool big);
    void draw_boss_bar(Renderer& renderer, const ECSWorld& world, Entity boss);
    void draw_banner(Renderer& renderer, const HUDContext& ctx);
    void draw_message(Renderer& renderer, const HUDContext& ctx);
    void draw_objective(Renderer& renderer, const HUDContext& ctx);
    void draw_inventory(Renderer& renderer, const Player& player, const HUDContext& ctx);
};
