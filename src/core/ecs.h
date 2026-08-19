#pragma once

// =============================================================================
// MANTLE - Entity Component System (Data-Oriented, Simple)
// =============================================================================
//
// Design: Struct-of-Arrays for cache efficiency.
// Each component type is stored in a contiguous array.
// Entities are just IDs (indices).
// No inheritance, no virtuals - just data and functions.
//
// =============================================================================

#include "core/types.h"
#include "core/config.h"
#include <vector>
#include <bitset>
#include <string>

// =============================================================================
// Entity
// =============================================================================
using Entity = u32;
constexpr Entity ENTITY_NONE = UINT32_MAX;
constexpr u32 MAX_ENTITIES = 512;

// =============================================================================
// Component IDs
// =============================================================================
enum class Component : u32 {
    Transform = 0,
    Sprite,
    Animation,
    Health,
    Combat,
    Movement,
    Collider,
    Faith,          // Only for player
    MiracleUser,    // Only for player
    EnemyAI,
    Projectile,
    Item,
    Lifetime,       // Auto-destroy after time (particles, effects)
    COUNT
};

constexpr u32 COMPONENT_COUNT = static_cast<u32>(Component::COUNT);
using ComponentMask = std::bitset<COMPONENT_COUNT>;

// =============================================================================
// Component Data Structures
// =============================================================================

struct TransformComponent {
    Vec2 position = {0, 0};
    Vec2 scale    = {1, 1};
    f32  rotation = 0.0f;
    Direction facing = Direction::Down;
};

struct SpriteComponent {
    u32 texture_id   = TEXTURE_NONE;  // Index into loaded textures (TEXTURE_NONE = placeholder)
    // Source rect in the texture, in TEXELS (Config::ASSET_SCALE per game unit). Drawn size in
    // game units = src_rect.w / ASSET_SCALE * scale. Placeholder (no texture): src_rect.w is
    // used directly as the world size.
    Rect src_rect    = {0, 0, 32.0f * Config::ASSET_SCALE, 32.0f * Config::ASSET_SCALE};
    Color tint       = Color::white();
    i32 z_order      = 0;      // Draw order
    bool flip_x      = false;
    bool visible      = true;
};

struct AnimationComponent {
    u32 current_frame = 0;
    u32 frame_count   = 1;
    f32 frame_time    = 0.1f;   // Seconds per frame
    f32 timer         = 0.0f;
    bool looping      = true;
    bool playing      = true;
    u32 row           = 0;      // Row in spritesheet (each row = one animation)
};

struct HealthComponent {
    f32 current = 100.0f;
    f32 max     = 100.0f;
    bool invulnerable = false;
    f32 iframes_timer = 0.0f;
    f32 hit_flash     = 0.0f;   // Seconds left of the white/red hit flash

    bool is_alive() const { return current > 0.0f; }
    f32 ratio() const { return current / max; }
    void take_damage(f32 amount) {
        if (!invulnerable && iframes_timer <= 0.0f) {
            current = std::max(0.0f, current - amount);
        }
    }
    void heal(f32 amount) {
        current = std::min(max, current + amount);
    }
};

struct CombatComponent {
    f32 damage         = 10.0f;
    f32 attack_speed   = Config::ATTACK_COOLDOWN;
    f32 attack_timer   = 0.0f;   // Cooldown remaining
    f32 range          = 100.0f;
    bool can_attack() const { return attack_timer <= 0.0f; }
};

struct MovementComponent {
    Vec2 velocity      = {0, 0};
    f32  speed         = 200.0f;
    f32  friction      = 10.0f;  // Deceleration factor
    Vec2 knockback     = {0, 0}; // Impulse (px/s) applied on top of velocity, decays fast
    // Dash
    bool is_dashing    = false;
    Vec2 dash_dir      = {0, 0};
    f32  dash_timer    = 0.0f;
    f32  dash_cooldown = 0.0f;
};

struct ColliderComponent {
    enum class Shape : u8 { AABB, Circle };
    Shape shape     = Shape::AABB;
    Vec2 offset     = {0, 0};   // Offset from transform position
    Vec2 size       = {16, 16}; // Width/Height for AABB
    f32  radius     = 8.0f;     // For circle colliders
    bool is_trigger = false;    // Trigger = no physics response, just events
    u32  layer      = 0;        // Collision layer bitmask
};

enum class AIBehavior : u8 {
    MeleeDirect,     // Soldado de Acab: charge at player
    Ranged,          // Arquero Real: keep distance, shoot
    Caster,          // Profeta de Baal: cast spells, heal allies
    FastLeaper,      // Leon del Desierto: quick jumps
    MeleeResistant,  // Soldado Sirio: slow but tanky
    BuffAllies,      // General Sirio: buff nearby allies
    ExplodeOnDeath,  // Leproso Enloquecido: explode when killed
    SplitOnDeath     // Agua Corrupta: split into smaller units
};

struct EnemyAIComponent {
    AIBehavior behavior = AIBehavior::MeleeDirect;
    f32 aggro_range     = 120.0f;
    f32 attack_range    = 20.0f;
    f32 think_timer     = 0.0f;
    f32 think_interval  = 0.3f;   // AI update frequency
    f32 shoot_timer     = 1.0f;   // Ranged/caster/boss volley cooldown
    f32 leap_timer      = 0.0f;   // FastLeaper: time until next leap
    Entity target       = ENTITY_NONE;
    Vec2 wander_target  = {0, 0};
    const char* name    = "???";   // Display name (from EnemyDef)
    bool is_boss        = false;
    u32  enemy_type     = 0;       // EnemyType index (for death effects / faith rewards)
};

struct ProjectileComponent {
    Vec2 direction   = {0, 0};
    Vec2 origin      = {0, 0};       // Where it was spawned
    f32  speed       = Config::PROJECTILE_SPEED;
    f32  damage      = 10.0f;
    f32  max_range   = Config::PROJECTILE_RANGE;
    Entity owner     = ENTITY_NONE;  // Who fired this
    bool friendly    = true;          // Player projectile?
    bool pierce      = false;         // Goes through enemies (miracles)
    Entity hit_list[8] = {ENTITY_NONE, ENTITY_NONE, ENTITY_NONE, ENTITY_NONE,
                          ENTITY_NONE, ENTITY_NONE, ENTITY_NONE, ENTITY_NONE};  // Piercing: already hit
    u8 hit_count = 0;

    bool already_hit(Entity e) const {
        for (u8 i = 0; i < hit_count; i++) if (hit_list[i] == e) return true;
        return false;
    }
    void mark_hit(Entity e) {
        if (hit_count < 8) hit_list[hit_count++] = e;
    }
};

struct ItemComponent {
    enum class Type : u8 { Weapon, Passive, Relic, Consumable, Heredable, Pickup };
    enum class Pickup : u8 { None, Heart, FaithOrb };
    Type   type         = Type::Passive;
    Pickup pickup       = Pickup::None;
    u32    item_id      = 0;
    f32    value        = 0.0f;   // Heal amount / faith amount for pickups
    f32    bob_phase    = 0.0f;
    bool   is_heredable = false;  // Transfers to Eliseo on ascension
};

struct LifetimeComponent {
    f32 remaining = 1.0f;
};

// Forward declarations (tile collision, deferred rendering)
struct Room;
struct Renderer;
struct DrawCmd;

// =============================================================================
// ECS World
// =============================================================================
struct ECSWorld {
    // Entity management
    ComponentMask masks[MAX_ENTITIES];
    u32 entity_count = 0;
    std::vector<Entity> free_list;

    // Component arrays (Struct of Arrays)
    TransformComponent  transforms[MAX_ENTITIES];
    SpriteComponent     sprites[MAX_ENTITIES];
    AnimationComponent  animations[MAX_ENTITIES];
    HealthComponent     healths[MAX_ENTITIES];
    CombatComponent     combats[MAX_ENTITIES];
    MovementComponent   movements[MAX_ENTITIES];
    ColliderComponent   colliders[MAX_ENTITIES];
    EnemyAIComponent    enemy_ais[MAX_ENTITIES];
    ProjectileComponent projectiles[MAX_ENTITIES];
    ItemComponent       items[MAX_ENTITIES];
    LifetimeComponent   lifetimes[MAX_ENTITIES];

    // Special entities (quick access)
    Entity player = ENTITY_NONE;

    // Active room for tile collision
    Room* active_room = nullptr;

    // Shared tileset texture (pickups / projectiles use its small sprites)
    u32 tileset = TEXTURE_NONE;
    f32 time    = 0.0f;   // Accumulated world time (bobbing, water animation)

    // Event counters consumed by the Engine each frame
    u32 altars_smashed = 0;   // Baal altars destroyed by player projectiles

    // Floating damage numbers (filled by apply_damage, drawn and aged by the Engine)
    struct DamagePopup { Vec2 pos; f32 amount; f32 life; bool boss; };
    std::vector<DamagePopup> popups;
    f32 hitstop = 0.0f;       // > 0: freeze the simulation for a couple of frames (boss hits)

    // Global status effects
    f32 enemy_slow_timer = 0.0f;   // "Palabra de Sequia": enemies at half speed while > 0

    void init();
    void clear();

    // Entity lifecycle
    Entity create();
    void destroy(Entity e);
    bool is_alive(Entity e) const;
    void reset_components(Entity e);   // Fresh defaults for a recycled slot (see ecs.cpp)

    // Component access
    bool has(Entity e, Component c) const;
    void add(Entity e, Component c);
    void remove(Entity e, Component c);

    // Update all systems
    void update(f32 dt);

    // Queue all visible entities into the y-sorted draw list
    void collect_draws(std::vector<DrawCmd>& out, Renderer& renderer);

    // Helpers
    int count_living_enemies() const;
};
