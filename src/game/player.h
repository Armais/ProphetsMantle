#pragma once

// =============================================================================
// MANTLE - Player System (Elias & Eliseo) (GDD 2.3)
// =============================================================================

#include "core/types.h"
#include "core/config.h"
#include "core/ecs.h"
#include "render/sprites.h"
#include "game/miracles.h"
#include <vector>

// =============================================================================
// Player Character Type
// =============================================================================
enum class CharacterType : u8 {
    Elias,   // Act I - Ofensivo, explosivo
    Eliseo   // Act II - Versatil, sanador
};

// =============================================================================
// Player Data (wraps ECS entity with player-specific logic)
// =============================================================================
struct Player {
    Entity entity = ENTITY_NONE;
    CharacterType character = CharacterType::Elias;

    // Miracle slots
    MiracleSlot miracle_slots[2];  // Eliseo gets 2 from start, Elias gets 1 (2 from ch4+)
    int active_slot_count = 1;

    // Inventory
    std::vector<u32> items;           // Item IDs picked up this run (choice screen)
    u32 relic_slot = UINT32_MAX;      // Single relic slot
    u32 weapon_slot = UINT32_MAX;     // Secondary weapon
    u32 consumable_slot = UINT32_MAX; // Held consumable (R)

    // Item-driven modifiers
    f32 damage_buff_timer = 0.0f;     // Vino Nuevo: +25% damage while > 0
    f32 miracle_power     = 1.0f;     // Arca del Pacto: miracle damage multiplier

    // Meta
    bool took_damage_this_room = false;
    int rooms_cleared = 0;
    int shots_fired = 0;
    int kills = 0;
    Vec2 last_move_dir = {0, 1};  // Last non-zero movement direction (8-way) = shooting direction

    // Ongoing miracle effects
    Vec2 heal_zone_pos = {0, 0};
    f32  heal_zone_timer = 0.0f;       // Sanar Aguas
    f32  double_portion_timer = 0.0f;  // Doble Porcion (damage x2)

    // Feedback for the Engine (consumed each frame)
    enum class MiracleEvent : u8 { None, Cast, NeedFaith };
    MiracleEvent last_event = MiracleEvent::None;
    MiracleID last_cast = MiracleID::COUNT;
    f32 shake_request = 0.0f;
    std::vector<Entity> pending_kills;   // Enemies killed by miracles (Engine awards faith/drops)

    // texture_id: sprite sheet from Engine::character_textures (TEXTURE_NONE = placeholder)
    void init_as_elias(ECSWorld& world, u32 texture_id = TEXTURE_NONE);
    void init_as_eliseo(ECSWorld& world, u32 texture_id = TEXTURE_NONE);
    void update(ECSWorld& world, struct Input& input, struct FaithSystem& faith, f32 dt);
    void cast_miracle(ECSWorld& world, struct FaithSystem& faith, MiracleID id);
    // Apply an item picked on the choice screen (stats, relic/weapon/consumable slots).
    // Returns the message to show. `record` = false when re-applying inherited items.
    const char* grant_item(ECSWorld& world, struct FaithSystem& faith,
                           const struct ItemDef& def, bool record = true);
    bool use_consumable(ECSWorld& world, const struct ItemRegistry& registry);
};

// =============================================================================
// Player Factory Functions
// =============================================================================

inline void Player::init_as_elias(ECSWorld& world, u32 texture_id) {
    character = CharacterType::Elias;
    active_slot_count = Config::ELIAS_MIRACLE_SLOTS;

    entity = world.create();
    if (entity == ENTITY_NONE) return;

    world.player = entity;

    // Transform
    world.add(entity, Component::Transform);
    world.transforms[entity].position = {
        Config::GAME_WIDTH * 0.5f,
        Config::GAME_HEIGHT * 0.5f
    };

    // Sprite
    world.add(entity, Component::Sprite);
    world.sprites[entity].texture_id = texture_id;
    world.sprites[entity].src_rect = {0, 0, static_cast<f32>(SHEET_FRAME_SIZE), static_cast<f32>(SHEET_FRAME_SIZE)};
    world.sprites[entity].z_order = 10;

    // Animation
    world.add(entity, Component::Animation);
    world.animations[entity].frame_count = 4;
    world.animations[entity].frame_time = 0.15f;

    // Health
    world.add(entity, Component::Health);
    world.healths[entity].max = Config::ELIAS_HP;
    world.healths[entity].current = Config::ELIAS_HP;

    // Combat
    world.add(entity, Component::Combat);
    world.combats[entity].damage = Config::ELIAS_DAMAGE;

    // Movement
    world.add(entity, Component::Movement);
    world.movements[entity].speed = Config::ELIAS_SPEED;

    // Collider
    world.add(entity, Component::Collider);
    world.colliders[entity].shape = ColliderComponent::Shape::Circle;
    world.colliders[entity].radius = 6.0f;  // Smaller than sprite for forgiving collision
    world.colliders[entity].offset = {0, 6}; // Feet position (sheet feet baseline = frame row 28)

    // Miracles: Fuego del Cielo (needs Siervo Fiel faith), Cuervos unlocked with slot 2 (ch4+)
    miracle_slots[0].equip(MiracleID::FuegoDelCielo);
    miracle_slots[1].equip(MiracleID::CuervosDeQuerit);

    SDL_Log("Player created: Elias (HP:%.0f SPD:%.0f DMG:%.0f)",
            Config::ELIAS_HP, Config::ELIAS_SPEED, Config::ELIAS_DAMAGE);
}

inline void Player::init_as_eliseo(ECSWorld& world, u32 texture_id) {
    character = CharacterType::Eliseo;
    active_slot_count = Config::ELISEO_MIRACLE_SLOTS;

    entity = world.create();
    if (entity == ENTITY_NONE) return;

    world.player = entity;

    world.add(entity, Component::Transform);
    world.transforms[entity].position = {
        Config::GAME_WIDTH * 0.5f,
        Config::GAME_HEIGHT * 0.5f
    };

    world.add(entity, Component::Sprite);
    world.sprites[entity].texture_id = texture_id;
    world.sprites[entity].src_rect = {0, 0, static_cast<f32>(SHEET_FRAME_SIZE), static_cast<f32>(SHEET_FRAME_SIZE)};
    world.sprites[entity].z_order = 10;

    world.add(entity, Component::Animation);
    world.animations[entity].frame_count = 4;
    world.animations[entity].frame_time = 0.15f;

    world.add(entity, Component::Health);
    world.healths[entity].max = Config::ELISEO_HP;
    world.healths[entity].current = Config::ELISEO_HP;

    world.add(entity, Component::Combat);
    world.combats[entity].damage = Config::ELISEO_DAMAGE;

    world.add(entity, Component::Movement);
    world.movements[entity].speed = Config::ELISEO_SPEED;

    world.add(entity, Component::Collider);
    world.colliders[entity].shape = ColliderComponent::Shape::Circle;
    world.colliders[entity].radius = 6.0f;
    world.colliders[entity].offset = {0, 6};

    // "Doble porcion": two slots from the start
    miracle_slots[0].equip(MiracleID::SanarAguas);
    miracle_slots[1].equip(MiracleID::EjercitoCelestial);

    SDL_Log("Player created: Eliseo (HP:%.0f SPD:%.0f DMG:%.0f)",
            Config::ELISEO_HP, Config::ELISEO_SPEED, Config::ELISEO_DAMAGE);
}
