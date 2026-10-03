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

    // ---- LA ALFORJA (2026-10-03) ----------------------------------------------------
    // Los consumibles ya no salen en las cartas ni se pisan unos a otros: se ENCUENTRAN en
    // el mundo (secretos, elites, cofres, algun enemigo) y van a la alforja. Cada hueco
    // guarda UN tipo con sus cargas; recoger otro igual suma carga, uno nuevo ocupa un hueco
    // libre, y con la alforja llena se queda en el suelo (nada se pierde en silencio).
    // Teclado: 1-4 usan ese hueco, R el elegido, rueda / [ ] lo cambian. Mando: X usa,
    // cruceta izq/der cambia. Tactil: USAR usa, mantenerlo cambia. "Aceite Inagotable"
    // (2 Reyes 4:6) suma cargas al elegido. `belt_cap` sale de El Santuario (Alforja Ancha).
    static constexpr int BELT_MAX   = 4;   // = SAVE_BELT_SLOTS
    static constexpr int BELT_STACK = 5;   // cargas por hueco al recoger (el milagro llega a 9)
    struct BeltSlot {
        u32 id = UINT32_MAX;
        u8  charges = 0;
        bool empty() const { return id == UINT32_MAX || charges == 0; }
    };
    BeltSlot belt[BELT_MAX];
    int belt_cap = 2;
    int belt_sel = 0;
    bool belt_can_take(u32 id) const;          // ¿cabe una carga mas de este consumible?
    bool belt_add(u32 id, int n = 1, int stack_cap = BELT_STACK);
    int  belt_slot_of(u32 id) const;           // hueco que ya lleva ese tipo, o -1
    int  belt_count() const;                   // huecos ocupados
    void belt_fix_selection();                 // el elegido nunca apunta a un hueco vacio
    void belt_cycle(int dir);                  // siguiente / anterior hueco ocupado
    // Usa una carga del hueco `slot` (-1 = el elegido). Devuelve el id usado (UINT32_MAX
    // si no se gasto nada) y deja en `fx` lo que ha hecho, ya escrito ("+35 de vida"), o
    // por que no se gasto ("Ya tienes la vida llena": el Agua no se tira a lo tonto).
    u32  use_belt(int slot, ECSWorld& world, struct FaithSystem& faith,
                  const struct ItemRegistry& registry, char* fx, int fx_n);

    // Item-driven modifiers
    f32 damage_buff_timer = 0.0f;     // Vino Nuevo: +25% damage while > 0

    // ---- Efectos PROPIOS de los objetos (2026-10-03) -------------------------------
    // "Los objetos no son logicos": casi todos eran un +HP o un +dano que no se veia. Ahora
    // cada pasivo trae una mecanica que se nota jugando. Se RECALCULAN desde `items` en
    // recompute_item_mods (idempotente, como apply_relic_effects): se llama al conceder y
    // al rehacer el equipo. Los que toca el combate se copian a ECSWorld::mods cada frame.
    f32  clear_heal        = 0.0f;   // Pan de Cada Dia: vida al despejar un grupo
    f32  miracle_cost_mult = 1.0f;   // Aceite de la Uncion: milagros mas baratos
    f32  dash_cd_mult      = 1.0f;   // Sandalias del Profeta: dash antes
    f32  heart_drop_mult   = 1.0f;   // Mana del Cielo: mas corazones
    f32  heart_heal_mult   = 1.0f;   // Panes de Cebada: los corazones curan mas
    f32  kill_faith        = 0.0f;   // Ceniza del Carmelo: fe por baja
    bool burning_shots     = false;  // Antorcha del Altar: el disparo prende
    bool has_shield        = false;  // Piedra del Memorial: escudo de un golpe
    bool shield_up         = false;
    f32  shield_timer      = 0.0f;   // recarga del escudo (s)
    static constexpr f32 SHIELD_RECHARGE = 12.0f;
    void recompute_item_mods();
    // Coste en fe de un milagro con los descuentos que lleve el profeta
    f32 miracle_cost(MiracleID id) const;
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
    // El FUEGO DEL CIELO que se ve caer (2026-09-24, "la cinematica deberia ver como cae un
    // fuego del cielo"; 2 Reyes 1:10, "descienda fuego del cielo, y consumate a ti y a tus
    // cincuenta"). Al lanzarlo el cielo se oscurece, una bola grande baja desde fuera de
    // pantalla sobre el sitio donde se invoco y otra mas pequena sobre cada enemigo del
    // radio; el DANO llega cuando tocan suelo (SKY_FIRE_FALL), no antes. Lo pinta
    // Engine::render_sky_fire por ENCIMA del mundo: el fuego viene del cielo, no pasa por
    // detras de los arboles.
    static constexpr int SKY_FIRE_BOLTS = 8;
    struct SkyBolt { Vec2 target; f32 start; f32 size; };
    SkyBolt sky_bolts[SKY_FIRE_BOLTS] = {};
    int  sky_bolt_count = 0;
    f32  sky_fire_t     = -1.0f;   // segundos desde que se invoco; < 0 = no hay fuego en el aire
    Vec2 sky_fire_pos   = {0, 0};  // donde cae la bola grande (y el centro del dano)
    f32  sky_fire_mult  = 1.0f;    // multiplicador de dano apuntado AL LANZARLO
    bool sky_fire_hit   = false;   // ya ha tocado suelo (el dano se aplica una vez)
    bool sky_fire_landed = false;  // aviso de un frame para el Engine: sonido del impacto
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
    // Fuego del Cielo en el aire: avanza la caida y, al tocar suelo, dano + llave del bioma
    void update_sky_fire(ECSWorld& world, f32 dt);
    // Ataque secundario del arma equipada (weapon_slot). Honda = piedra que atraviesa,
    // Vara = barrido de largo alcance. Devuelve true si llego a disparar.
    bool fire_secondary(ECSWorld& world, struct FaithSystem& faith, Vec2 aim, f32 damage_mult);
    // Apply an item picked on the choice screen (stats, relic/weapon/consumable slots).
    // Returns the message to show. `record` = false when re-applying inherited items.
    const char* grant_item(ECSWorld& world, struct FaithSystem& faith,
                           const struct ItemDef& def, bool record = true);
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
