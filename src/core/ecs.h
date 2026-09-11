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
    ExplodeOnDeath,  // Idolatra de Rimon: su brasero prende al caer
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
    Vec2 ring_center    = {0, 0};  // Corro de Baal: centro de la orbita (boss_group)
    const char* name    = "???";   // Display name (from EnemyDef)
    bool is_boss        = false;
    // Miembro del GRUPO del jefe (el corro de profetas del Carmelo, game/enemies.cpp
    // spawn_boss_group): cuenta en la barra compartida y su baja se loguea, pero NO es
    // `is_boss` — las recompensas, el logro, la sacudida y el +90 de la auto-mira siguen
    // siendo de UNA entidad, la cabeza. Si cuenta contra el techo de invocaciones.
    bool boss_group     = false;
    f32  ring_phase     = 0.0f;    // Angulo propio en el corro (boss_group)
    // Celda de la zona en que nacio (zona continua, 2026-09-10). Limpieza de celda y
    // despawn van SIEMPRE por celda de origen, nunca por posicion: los enemigos persiguen
    // a traves de las costuras. -1 = fuera de toda celda (cuenta en todas, no se despawnea).
    int  home_cell      = -1;
    u32  enemy_type     = 0;       // EnemyType index (for death effects / faith rewards)

    // Jefes: patron propio por capitulo (game/enemies.h BossPattern)
    u8  boss_pattern    = 0;
    u8  phase           = 0;       // Fase alcanzada (los umbrales solo se cruzan una vez)
    f32 special_timer   = 0.0f;    // Invocaciones / teleports / curaciones
    Entity partner      = ENTITY_NONE;   // Acab y Jezabel

    // Conductas del Acto II (game/enemies.h: on_enemy_death_behavior / update_death_fuses)
    u8  chapter          = 1;      // Capitulo en que nacio (escala a los hijos que se dividen)
    u8  split_generation = 0;      // Agua Corrupta: 0 = original; los hijos no vuelven a dividirse
    f32 death_fuse       = 0.0f;   // Idolatra: brasas prendiendo sobre el cadaver (telegrafia)
    f32 buff_timer       = 0.0f;   // General Sirio: segundos que le queda el aura recibida
    f32 base_damage      = 0.0f;   // Valores previos al aura (0 = nunca buffeado)
    f32 base_speed       = 0.0f;

    // Faccion. Un DISCIPULO (Ejercito Celestial, 2 Reyes 6:17) lleva este mismo
    // componente —es un combatiente con IA como cualquier otro— pero con `ally` puesto.
    // Todo lo que preguntaba "tiene EnemyAI?" para decir "es un enemigo" tiene que
    // preguntar `is_hostile()`: contar la sala, el auto-apuntado, los milagros de area,
    // el dano por contacto y las recompensas al morir.
    bool ally            = false;
    f32  life_left       = 0.0f;   // > 0: se desvanece al agotarse (invocacion temporal)
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
        // Lista llena = proyectil agotado: sin este corte, con mark_hit callado el 9o
        // objetivo recibia el dano CADA frame mientras la piedra lo cruzaba.
        if (hit_count >= 8) return true;
        for (u8 i = 0; i < hit_count; i++) if (hit_list[i] == e) return true;
        return false;
    }
    void mark_hit(Entity e) {
        if (hit_count < 8) hit_list[hit_count++] = e;
    }
};

struct ItemComponent {
    enum class Type : u8 { Weapon, Passive, Relic, Consumable, Heredable, Pickup };
    // `Offering` es la unica recogida que SOBREVIVE A LA PARTIDA: va derecha a la
    // meta-progresion. Es lo que hace que explorar de mas pague aunque mueras.
    enum class Pickup : u8 { None, Heart, FaithOrb, Offering };
    Type   type         = Type::Passive;
    Pickup pickup       = Pickup::None;
    u32    item_id      = 0;
    f32    value        = 0.0f;   // Heal amount / faith amount for pickups
    f32    bob_phase    = 0.0f;
    bool   is_heredable = false;  // Transfers to Eliseo on ascension
};

// Vida limitada + animacion de "efecto": la usan particulas, proyectiles y los VFX de
// milagro (spawn_vfx). Con `total` > 0 el sprite se puede desvanecer, crecer y girar
// sin necesidad de un componente aparte.
struct LifetimeComponent {
    f32 remaining = 1.0f;
    f32 total     = 0.0f;   // Duracion inicial (0 = no hay fundido: `fade` se ignora)
    f32 grow      = 0.0f;   // Escala ganada por segundo (anillos que se abren)
    f32 spin      = 0.0f;   // Grados por segundo
    bool fade     = false;  // Alpha del tinte = remaining / total
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
    Entity gap_crosser = ENTITY_NONE;

    // Active room for tile collision
    Room* active_room = nullptr;

    // Shared tileset texture (pickups / projectiles use its small sprites)
    u32 tileset = TEXTURE_NONE;
    // Hoja de efectos propia (assets/sprites/vfx.png). TEXTURE_NONE = spawn_fx no dibuja.
    u32 vfx_sheet = TEXTURE_NONE;
    // Hojas por EnemyType, para que los jefes puedan invocar secuaces con su sprite
    // correcto sin conocer al Engine (lo rellena Engine::load_sprites).
    u32 enemy_textures[8] = {TEXTURE_NONE, TEXTURE_NONE, TEXTURE_NONE, TEXTURE_NONE,
                             TEXTURE_NONE, TEXTURE_NONE, TEXTURE_NONE, TEXTURE_NONE};
    f32 time    = 0.0f;   // Accumulated world time (bobbing, water animation)

    // Event counters consumed by the Engine each frame
    u32 altars_smashed = 0;   // Baal altars destroyed by player projectiles
    // Hallazgos pendientes de convertirse en recogida (GDD2.0 fase 4). Van por aqui y no
    // soltando la pieza en el sitio para que haya UN SOLO lugar que decida que paga un
    // secreto: el Engine. Lo llenan el tiro que rompe una tinaja y los milagros-llave.
    std::vector<Vec2> loot_drops;
    u32 offerings_found = 0;  // ofrendas recogidas este frame (van a la meta-progresion)
    // Quien puede cruzar huecos ahora mismo. El Carro de Fuego (2 Reyes 2:11) es la llave de
    // los precipicios, y es lo unico que convierte un hueco en una puerta cerrada en vez de
    // en un muro. Es UNA entidad y no un componente porque dura dos segundos.
    f32 shake_request  = 0.0f;   // Sacudida pedida desde el mundo (estallido del brasero)

    // Floating damage numbers (filled by apply_damage, drawn and aged by the Engine)
    struct DamagePopup { Vec2 pos; f32 amount; f32 life; bool boss; };
    std::vector<DamagePopup> popups;
    f32 hitstop = 0.0f;       // > 0: freeze the simulation for a couple of frames (impacto)

    // Contadores de la sonda de tacto (comando `feel` del autoplay). Solo miden: nada de
    // lo que hay aqui se lee desde el juego, y por eso no van al guardado.
    f32 dmg_dealt = 0.0f;     // Dano que el jugador (o los suyos) han metido
    f32 dmg_taken = 0.0f;     // Dano que ha comido el jugador

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
    int count_living_enemies(int cell = -1) const;   // solo hostiles; cell >= 0 = solo esa celda de la zona
};

// =============================================================================
// Faccion (ver EnemyAIComponent::ally)
// =============================================================================
inline bool is_hostile(const ECSWorld& w, Entity e) {
    return w.has(e, Component::EnemyAI) && !w.enemy_ais[e].ally;
}
inline bool is_ally(const ECSWorld& w, Entity e) {
    return w.has(e, Component::EnemyAI) && w.enemy_ais[e].ally;
}
