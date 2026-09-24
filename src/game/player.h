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
    // Elias arranca con 1 (2 desde el cap. 4); Eliseo con 2, y 3 con la mejora meta 6
    MiracleSlot miracle_slots[Config::MAX_MIRACLE_SLOTS];
    int active_slot_count = 1;

    // Inventory
    std::vector<u32> items;           // Item IDs picked up this run (choice screen)
    // GDD 7.3: "Reliquias — muy poderoso, 1 slot". UNA sola a la vez; tomar otra
    // SUSTITUYE a la anterior y sus efectos se van con ella (ver apply_relic_effects).
    u32 relic_slot = UINT32_MAX;
    u32 relic_replaced = UINT32_MAX;  // La que acaba de salir, para poder nombrarla en el aviso
    u32 weapon_slot = UINT32_MAX;     // Secondary weapon
    u32 consumable_slot = UINT32_MAX; // Held consumable (R)
    // "Aceite Inagotable" (2 Reyes 4:6): el consumible que llevas tiene cargas y solo se
    // suelta cuando se vacia la ultima. Sin cargas el milagro "Consumibles x2" del GDD no
    // tenia donde apoyarse. 0 = no llevas nada.
    u8  consumable_charges = 0;

    // Item-driven modifiers
    f32 damage_buff_timer = 0.0f;     // Vino Nuevo: +25% damage while > 0
    f32 miracle_power     = 1.0f;     // Arca del Pacto: miracle damage multiplier
    // El Manto de Elias: multiplicador de la recarga de los milagros (0.75 = 25% mas rapido).
    // Se aplica al fijar el cooldown en MiracleSlot::try_cast, no al descontarlo, para que
    // la barra del HUD y el propio contador cuenten lo mismo.
    f32 miracle_haste     = 1.0f;
    // Cosmetico: id de la skin del cayado (ver get_staff_skin). NO toca ninguna cifra.
    // Lo fija Engine::apply_settings_from_save desde SaveData::skin_equipped.
    u32 staff_skin        = 0;
    f32 secondary_cd      = 0.0f;     // Recarga del ataque secundario del arma equipada

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
    // "Ojos Abiertos" (2 Reyes 6:17): el mapa del capitulo queda abierto para siempre (el
    // Engine lo apaga al cambiar de capitulo) y durante la duracion se marca lo que hay en
    // la sala. Los lee HUD::draw_minimap y el pase de dibujo del Engine.
    bool map_revealed  = false;
    f32  reveal_timer  = 0.0f;

    // VFX continuos: los milagros con duracion se ven mientras duran, no solo al lanzarlos.
    // Son acumuladores, no estado de juego: nada de esto afecta al combate.
    f32  aura_timer    = 0.0f;   // Pulso del aura de Doble Porcion / burbujas de Sanar Aguas
    f32  chariot_timer = 0.0f;   // Estela de fuego del Carro mientras dura la embestida
    // El CARRO que se ve (2026-09-24, "tiene que aparecer un carro de fuego de verdad, como
    // en el ascenso de Elias"): aparece al lanzarlo, lleva al profeta DENTRO mientras dura la
    // embestida (su propio sprite va entre las dos capas del carro) y al soltarlo sigue
    // adelante subiendo al cielo hasta desvanecerse. Solo es dibujo: la embestida, el dano y
    // el cruce de huecos siguen siendo los de siempre. Lo pinta Engine::collect_chariot_draws
    // con assets/sprites/chariot.png (tools/blender/mantle_sprites/chariot.py).
    f32  chariot_fx_t    = -1.0f;      // segundos desde que aparecio; < 0 = no hay carro
    f32  chariot_fx_free = 0.0f;       // segundos desde que solto al profeta (0 = aun lo lleva)
    Vec2 chariot_fx_pos  = {0, 0};     // el ANCLA (suelo de la caja) en el mundo
    Vec2 chariot_fx_dir  = {1, 0};
    f32  chariot_fx_lift = 0.0f;       // cuanto ha subido tras soltarlo (unidades)
    f32  trail_timer   = 0.0f;   // Cadencia de la estela
    bool crossed_gap   = false;  // ya se ha logueado el cruce de hueco de esta embestida
    f32  reveal_pulse  = 0.0f;   // Cadencia de las marcas de Ojos Abiertos
    f32  dash_trail_timer = 0.0f;   // Cadencia de la estela del dash

    // Feedback for the Engine (consumed each frame). El motivo del fallo va separado
    // porque "no puedes" sin decir por que es la queja numero uno del sistema de fe.
    enum class MiracleEvent : u8 { None, Cast, NeedTier, NeedCost, OnCooldown };
    MiracleEvent last_event = MiracleEvent::None;
    MiracleID last_cast = MiracleID::COUNT;   // milagro del ultimo evento (lanzado o fallido)
    f32 shake_request = 0.0f;
    f32 flash_request = 0.0f;   // Destello blanco a pantalla completa (Altar Consumido)
    std::vector<Entity> pending_kills;   // Enemies killed by miracles (Engine awards faith/drops)

    // texture_id: sprite sheet from Engine::character_textures (TEXTURE_NONE = placeholder)
    void init_as_elias(ECSWorld& world, u32 texture_id = TEXTURE_NONE);
    void init_as_eliseo(ECSWorld& world, u32 texture_id = TEXTURE_NONE);
    void update(ECSWorld& world, struct Input& input, struct FaithSystem& faith, f32 dt);

    // Direccion en la que sale el disparo, ya resuelta segun el ajuste de auto-mira
    // (Config::AimMode). Es el UNICO sitio donde se decide: el ataque principal y el
    // secundario la llaman, y antes cada uno llevaba su copia de la misma decision.
    Vec2 resolve_aim(ECSWorld& world, const struct Input& input) const;

    // Efectos de reliquia. Se RECALCULAN desde relic_slot en vez de acumularse al
    // conceder: es la unica forma de que quitar una reliquia deshaga lo suyo. Idempotente
    // a proposito — se vuelve a llamar al reintentar capitulo, al cargar y en la Ascension.
    void apply_relic_effects(struct FaithSystem& faith);
    void cast_miracle(ECSWorld& world, struct FaithSystem& faith, MiracleID id);
    // Ataque secundario del arma equipada (weapon_slot). Honda = piedra que atraviesa,
    // Vara = barrido de largo alcance. Devuelve true si llego a disparar.
    bool fire_secondary(ECSWorld& world, struct FaithSystem& faith, Vec2 aim, f32 damage_mult);
    // Apply an item picked on the choice screen (stats, relic/weapon/consumable slots).
    // Returns the message to show. `record` = false when re-applying inherited items.
    const char* grant_item(ECSWorld& world, struct FaithSystem& faith,
                           const struct ItemDef& def, bool record = true);
    bool use_consumable(ECSWorld& world, struct FaithSystem& faith,
                        const struct ItemRegistry& registry);
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
    // El profeta frena EN Player::update (aceleracion y frenada propias). Dejar
    // aqui la friccion de system_movement hacia que las dos se pelearan y el tope
    // de velocidad real se quedaba un tercio por debajo del de la ficha.
    world.movements[entity].friction = 0.0f;

    // Collider
    world.add(entity, Component::Collider);
    world.colliders[entity].shape = ColliderComponent::Shape::Circle;
    world.colliders[entity].radius = 6.0f;  // Smaller than sprite for forgiving collision
    world.colliders[entity].offset = {0, 6}; // Feet position (sheet feet baseline = frame row 28)

    // Milagros: el primer slot lleva un Tier 1 (5 de fe) para que la Q sirva desde la
    // primera sala. Fuego del Cielo es Tier 3 y exige nivel Siervo Fiel (50 de fe): con
    // la fe inicial justo en 50, un solo golpe recibido lo bloqueaba. Entra con el
    // segundo slot en el cap. 4, cuando la fe ya se sostiene alta.
    miracle_slots[0].equip(MiracleID::CuervosDeQuerit);
    miracle_slots[1].equip(MiracleID::FuegoDelCielo);

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
    // El profeta frena EN Player::update (aceleracion y frenada propias). Dejar
    // aqui la friccion de system_movement hacia que las dos se pelearan y el tope
    // de velocidad real se quedaba un tercio por debajo del de la ficha.
    world.movements[entity].friction = 0.0f;

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
