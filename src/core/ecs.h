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
    bool fixed_src   = false;  // celda horneada: system_animation no toca src_rect
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
    f32 burn_timer       = 0.0f;   // Antorcha del Altar: segundos que le quedan ardiendo
    f32 burn_tick        = 0.0f;   // cadencia de la quemadura
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

    // --- Jefes con estrategia propia (2026-10-03, game/bosses.cpp) -------------------
    // Cada patron es una maquina de estados pequena: `boss_state` dice en que paso esta
    // (perseguir, telegrafiar, cargar, aturdido...) y `state_timer` cuanto le queda. Los
    // significados de cada valor viven en bosses.cpp, al lado del patron que los usa.
    u8   boss_state   = 0;
    f32  state_timer  = 0.0f;
    f32  aux_timer    = 0.0f;      // segunda cadencia (anillos, monedas, oleadas...)
    f32  aux2_timer   = 0.0f;      // tercera (curacion de Jezabel, codicia de Guiezi...)
    int  counter      = 0;         // cargas encadenadas, piezas por recoger...
    Vec2 act_dir      = {0, 1};    // rumbo de la carga / del barrido / de la cadena
    Vec2 act_pos      = {0, 0};    // punto objetivo (fin de la carga, emerger, la plata)
    Vec2 face_dir     = {0, 1};    // hacia donde mira el ESCUDO (Capitan de Acab)
    // > -1: los tiros del profeta que entran por el frente (coseno de la semiapertura,
    // contra `face_dir`) rebotan en el escudo. -2 = sin escudo (lo normal).
    f32  shield_cos   = -2.0f;
    f32  vuln_mult    = 1.0f;      // dano recibido x esto: aturdido, de rodillas, el silbo
    f32  fight_time   = 0.0f;      // segundos de pelea, para el log del final
    f32  hp_mark      = 0.0f;      // vida apuntada al empezar a canalizar (Jezabel)
    // PROP del jefe (BossProp en enemies.h): idolos de Baal, senuelos de Guiezi. Son
    // hostiles —se les dispara y cuentan como vivos— pero NO son el jefe: no van en la
    // barra y mueren con el (on_enemy_death_behavior -> on_boss_death).
    u8   prop_kind    = 0;
    Entity owner      = ENTITY_NONE;   // el jefe al que sirve un prop o un miembro del corro
    // Cegado por la hueste celestial (cap. 10, 2 Reyes 6:18): ni ataca ni apunta, vaga.
    f32  blind_timer  = 0.0f;
};

// =============================================================================
// Peligros de suelo TELEGRAFIADOS (2026-10-03, game/hazards.h)
// =============================================================================
// La caja de herramientas de los jefes: lo que va a doler se PINTA en el suelo antes de
// doler. Un peligro no es una entidad (no ocupa ids del ECS, no tiene sprite): es una fila
// de una piscina fija de ECSWorld, y su vida son tres tramos — AVISO (`warn`: el dibujo se
// llena y se enciende), EFECTO (`active`: aqui y solo aqui hace dano) y una COLA corta sin
// dano para el fogonazo. Los respeta todo lo que respeta un proyectil: i-frames del golpe y
// del dash y el modo dios.
enum class HazardKind : u8 {
    Circle,   // aviso y estallido en un circulo
    Lane,     // rectangulo desde un punto en un rumbo: cargas, cadenas, lluvia de flechas
    Cone,     // barrido en abanico (guadana, espada)
    Ring,     // onda que se abre (o se CIERRA) con un hueco por donde pasar o dashear
    Pool,     // charca persistente: dano por contacto mientras dura
    Push,     // viento: empuja al profeta mientras esta dentro
    COUNT
};

// Lo que significa el impacto para quien lo lanzo (lo resuelve update_hazards)
enum class HazardTag : u8 {
    None,
    Chain,     // la cadena del Acreedor: tira del profeta hacia el y le cobra fe
    Debt,      // el anillo de la deuda: cobra fe
    Ripple,    // la sombra del Espiritu sumergido: solo dibujo, sigue al dueno
    Ritual,    // el fuego que pide el corro de Baal
    Shield,    // el arco del escudo (dibujo)
};

// Como se ve el estallido (spawn_fx + particulas del color del peligro)
enum class HazardFx : u8 { None, Fire, Dark, Holy, Water, Dust, Coin };

struct Hazard {
    bool       used   = false;
    HazardKind kind   = HazardKind::Circle;
    HazardTag  tag    = HazardTag::None;
    HazardFx   fx     = HazardFx::None;
    Vec2 pos    = {0, 0};     // centro (Circle/Ring/Pool) u origen (Lane/Cone/Push)
    Vec2 dir    = {1, 0};     // rumbo (Lane/Cone/Push); en Ring, el centro del HUECO
    f32  radius = 16.0f;      // Circle/Pool: radio; Cone: alcance; Ring: radio FINAL
    f32  r_from = 0.0f;       // Ring: radio al empezar (mayor que `radius` = anillo que se cierra)
    f32  length = 0.0f;       // Lane/Push
    f32  width  = 0.0f;       // Lane/Push: ancho; Ring: grosor de la banda
    f32  angle  = 0.0f;       // Cone: semiapertura (rad); Ring: semiapertura del hueco (0 = sin hueco)
    f32  warn   = 0.8f;       // segundos de aviso
    f32  active = 0.12f;      // segundos de efecto
    f32  t      = 0.0f;       // reloj desde que nacio
    f32  damage = 0.0f;
    f32  force  = 0.0f;       // Push: unidades/s de empuje
    f32  faith_drain = 0.0f;  // Chain / Debt
    f32  tick   = 0.0f;       // Pool: espera hasta el siguiente mordisco
    bool hits_player  = true;
    bool hits_enemies = false;
    bool hit_done     = false;   // el golpe de un solo uso ya conecto
    bool burst_done   = false;   // el fogonazo del inicio del efecto ya salio
    bool follow_owner = false;   // la posicion se pega a los pies del dueno cada frame
    Color color  = {255, 120, 60, 255};
    Entity owner = ENTITY_NONE;
};
constexpr int MAX_HAZARDS = 64;

struct ProjectileComponent {
    Vec2 direction   = {0, 0};
    Vec2 origin      = {0, 0};       // Where it was spawned
    f32  speed       = Config::PROJECTILE_SPEED;
    f32  damage      = 10.0f;
    f32  max_range   = Config::PROJECTILE_RANGE;
    Entity owner     = ENTITY_NONE;  // Who fired this
    bool friendly    = true;          // Player projectile?
    bool pierce      = false;         // Goes through enemies (miracles)
    bool staff       = false;         // tiro del cayado (lo que prende la Antorcha del Altar)
    f32  spin        = 0.0f;          // Grados/s. 0 = el sprite apunta hacia donde vuela;
                                      // la piedra de la Honda gira sobre si misma
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
    // `Silver` son los talentos de plata de Guiezi (cap. 9): estan en el suelo pero NO
    // tienen colisionador — no son para el profeta, son el cebo por el que Guiezi se
    // arrodilla (game/bosses.cpp).
    // `Consumable` (2026-10-03): un consumible tirado en el mundo; `item_id` dice cual. Va a
    // la alforja, y si no cabe se queda en el suelo (no caduca como un corazon).
    enum class Pickup : u8 { None, Heart, FaithOrb, Offering, Silver, Consumable };
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
    // Disco blanco de borde suave (Renderer::disc_tex): las monedas del Acreedor. El tileset
    // va con filtrado al vecino y a tamano de proyectil se escalona.
    u32 disc_tex = TEXTURE_NONE;
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
    // La alforja (2026-10-03). Los consumibles se encuentran en el mundo: quien suelta uno
    // apunta la POSICION aqui (el Engine decide cual, segun lo desbloqueado) y la recogida
    // apunta el id en `consumables_found` para que el Engine lo meta en la alforja.
    std::vector<Vec2> consumable_drops;
    std::vector<u32>  consumables_found;
    // Hoja de iconos de objeto (assets/sprites/icons.png): la recogida de un consumible se
    // dibuja con SU icono, no con una celda generica del tileset.
    u32 icons_sheet = TEXTURE_NONE;

    // Lo que el equipo del PROFETA cambia en el combate (Player::recompute_item_mods). El
    // Engine lo copia aqui cada frame antes de mover el mundo, porque combat.cpp no conoce
    // al Player; y lee de vuelta lo que el mundo gasto (el escudo).
    struct PlayerMods {
        f32  heart_drop_mult = 1.0f;   // Mana del Cielo
        f32  heart_heal_mult = 1.0f;   // Panes de Cebada
        f32  kill_faith      = 0.0f;   // Ceniza del Carmelo
        bool burning_shots   = false;  // Antorcha del Altar
        bool shield_up       = false;  // Piedra del Memorial: el proximo golpe no entra
        bool shield_broke    = false;  // se rompio este frame (sonido / aviso en el Engine)
        bool shield_ate_hit  = false;  // el golpe en curso lo comio el escudo: sin fe ni marcador
        // La alforja vista desde el suelo: `belt_stack` = bits de ItemDef::id de los tipos
        // que ya lleva Y admiten otra carga; `belt_free` = huecos vacios. Un tipo nuevo gasta
        // un hueco (se descuenta EN EL ACTO: tres consumibles distintos tocados en el mismo
        // frame con dos huecos libres no pueden entrar los tres). Lo que no cabe se queda en
        // el suelo y `belt_full_touch` avisa.
        u32  belt_stack      = 0;
        int  belt_free       = 0;
        bool belt_full_touch = false;
    };
    PlayerMods mods;
    // El golpe que acaba de entrar lo absorbio el escudo: los que llaman a apply_damage
    // contra el profeta no cobran la fe ni cuentan el golpe. Se consume al leerlo.
    bool take_shield_hit() { bool b = mods.shield_ate_hit; mods.shield_ate_hit = false; return b; }
    // Quien puede cruzar huecos ahora mismo. El Carro de Fuego (2 Reyes 2:11) es la llave de
    // los precipicios, y es lo unico que convierte un hueco en una puerta cerrada en vez de
    // en un muro. Es UNA entidad y no un componente porque dura dos segundos.
    f32 shake_request  = 0.0f;   // Sacudida pedida desde el mundo (estallido del brasero)

    // Floating damage numbers (filled by apply_damage, drawn and aged by the Engine)
    // `kind`: Damage (blanco / dorado de jefe), Heal (verde "+N"), Faith (oro "+N fe") y
    // Burn (naranja, la quemadura de la Antorcha). Va el ultimo con valor por defecto: los
    // push_back de siempre ({pos, dano, 0, jefe}) siguen siendo dano.
    struct DamagePopup {
        enum Kind : u8 { Damage, Heal, Faith, Burn };
        Vec2 pos; f32 amount; f32 life; bool boss; Kind kind = Damage;
    };
    std::vector<DamagePopup> popups;
    f32 hitstop = 0.0f;       // > 0: freeze the simulation for a couple of frames (impacto)

    // Contadores de la sonda de tacto (comando `feel` del autoplay). Solo miden: nada de
    // lo que hay aqui se lee desde el juego, y por eso no van al guardado.
    f32 dmg_dealt = 0.0f;     // Dano que el jugador (o los suyos) han metido
    f32 dmg_taken = 0.0f;     // Dano que ha comido el jugador

    // Global status effects
    f32 enemy_slow_timer = 0.0f;   // "Palabra de Sequia": enemies at half speed while > 0

    // Peligros telegrafiados de los jefes (game/hazards.h). Se vacian con el mundo
    // (clear), al cargar sala, al recolocar al profeta y al morir su dueno.
    Hazard hazards[MAX_HAZARDS];
    // Peticiones de los jefes al Engine, que es quien tiene pantalla y altavoz:
    f32 flash_request  = 0.0f;   // fogonazo blanco (cambio de fase, la hueste celestial)
    f32 darkness_req   = 0.0f;   // La Muerte apaga la luz: radio de la luz alrededor del profeta (0 = no)
    u32 hazard_bursts  = 0;      // peligros que estallaron este frame (un golpe sordo)
    u32 shield_blocks  = 0;      // tiros que rebotaron en un escudo (un "clinc")

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
