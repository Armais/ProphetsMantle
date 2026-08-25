// =============================================================================
// MANTLE - Player Implementation
// =============================================================================

#include "game/player.h"
#include "input/input.h"
#include "game/faith.h"
#include "game/combat.h"
#include "game/enemies.h"   // spawn_disciple: el Ejercito Celestial invoca de verdad
#include "game/items.h"
#include "game/rooms.h"   // TileSprite: celdas del tileset que usan los VFX
#include "core/config.h"
#include "render/sprites.h"
#include <algorithm>
#include <cmath>

// =============================================================================
// Player::resolve_aim - a donde va el disparo (GDD 4.2 / 4.3)
// =============================================================================
// Tres estados, no un interruptor:
//   Off       -> tu direccion y punto. En teclado eso son las 8 direcciones del ultimo
//                movimiento; en tactil, el arrastre.
//   Assisted  -> tu direccion, corregida una FRACCION hacia el objetivo, y solo si cae
//                dentro de un cono estrecho. El disparo sigue siendo tuyo: si no apuntas
//                a nadie, no dispara a nadie.
//   On        -> con las manos quietas fija el objetivo prioritario (pick_target). Es el
//                comportamiento que el GDD pide en movil, donde apuntar con el pulgar no
//                es fiable. Un apuntado manual explicito lo sigue pisando.
Vec2 Player::resolve_aim(ECSWorld& world, const Input& input) const {
    const TransformComponent& transform = world.transforms[entity];

    Vec2 aim = input.get_aim_axis();          // bot / arrastre tactil / stick derecho
    const bool manual = aim.length_sq() >= 0.01f;
    if (!manual) aim = last_move_dir;

    switch (input.aim_mode) {
        case Config::AimMode::Off:
            break;

        case Config::AimMode::On:
            if (!manual) {
                Entity target = pick_target(world, transform.position);
                if (target != ENTITY_NONE)
                    aim = world.transforms[target].position - transform.position;
            }
            break;

        case Config::AimMode::Assisted: {
            Entity target = pick_target(world, transform.position);
            if (target == ENTITY_NONE) break;
            Vec2 to = world.transforms[target].position - transform.position;
            if (to.length_sq() < 0.01f || aim.length_sq() < 0.01f) break;
            Vec2 d = aim.normalized();
            Vec2 t = to.normalized();
            // Fuera del cono no se toca nada: eso es lo que lo separa de "On".
            f32 cos_cone = std::cos(Config::Feel::AIM_ASSIST_CONE_DEG * 3.14159265f / 180.0f);
            if (d.x * t.x + d.y * t.y < cos_cone) break;
            aim = (d * (1.0f - Config::Feel::AIM_ASSIST_PULL) +
                   t * Config::Feel::AIM_ASSIST_PULL);
            break;
        }
    }

    if (aim.length_sq() < 0.01f) aim = last_move_dir;
    return aim.normalized();
}

void Player::update(ECSWorld& world, Input& input, FaithSystem& faith, f32 dt) {
    if (entity == ENTITY_NONE || !world.is_alive(entity)) return;

    auto& transform = world.transforms[entity];
    auto& movement  = world.movements[entity];
    auto& combat    = world.combats[entity];

    // ---- Movement ----
    // (no flip_x: sprite sheets author all 4 directions, animation row follows facing)
    // El profeta acelera y frena en vez de arrancar y parar en seco. Ojo: la friccion de
    // system_movement esta a 0 para el jugador (ver Player::init_as_*), porque las dos
    // curvas peleandose recortaban un tercio de la velocidad punta.
    Vec2 move = input.get_move_axis();
    if (move.length_sq() > 0.01f) {
        Vec2 desired = move.normalized() * movement.speed;
        f32  k = std::min(1.0f, Config::Feel::MOVE_ACCEL * dt);
        movement.velocity = movement.velocity + (desired - movement.velocity) * k;
        last_move_dir = move.normalized();   // 8-way; also the shooting direction
        faith.reset_inactivity();
    } else {
        movement.velocity = movement.velocity * (1.0f / (1.0f + Config::Feel::MOVE_BRAKE * dt));
    }

    // ---- Dash ----
    if (input.is_action_just_pressed(Action::Dash) &&
        !movement.is_dashing && movement.dash_cooldown <= 0.0f) {

        Vec2 dash_dir = move.length_sq() > 0.01f ? move.normalized() : Vec2{0, -1};
        movement.is_dashing = true;
        movement.dash_dir = dash_dir;
        movement.dash_timer = Config::DASH_DURATION;
        movement.dash_cooldown = Config::DASH_COOLDOWN;

        // I-frames during dash
        world.healths[entity].iframes_timer = Config::IFRAMES_DURATION;
        dash_trail_timer = 0.0f;
        // (el Engine ve que is_dashing se acaba de encender y pone el SFX, la sacudida
        //  corta y el rumble: aqui no se sabe nada del renderer ni del mando)
    }

    // Estela del dash: sin ella la invulnerabilidad del dash no se lee, porque el
    // parpadeo de i-frames esta apagado justo mientras se esta esquivando.
    if (movement.is_dashing) {
        dash_trail_timer -= dt;
        if (dash_trail_timer <= 0.0f) {
            dash_trail_timer = Config::Feel::DASH_TRAIL_RATE;
            Color trail = (character == CharacterType::Elias) ? Color{255, 190, 110, 170}
                                                             : Color{170, 210, 255, 170};
            spawn_vfx(world, transform.position + Vec2{0, 4}, TileSprite::Star, trail,
                      7.0f, 0.20f, -1.2f, 0.0f, {0, 0}, 6);
        }
    }

    // ---- Attack ----
    if (input.is_action_pressed(Action::Attack) && combat.can_attack()) {
        combat.attack_timer = combat.attack_speed;
        faith.reset_inactivity();

        // Apply faith damage multiplier (+ Doble Porcion, + Vino Nuevo)
        f32 actual_damage = combat.damage * faith.get_damage_multiplier();
        if (double_portion_timer > 0.0f) actual_damage *= 2.0f;
        if (damage_buff_timer > 0.0f)    actual_damage *= 1.25f;

        // A donde va el tiro lo decide resolve_aim, que es quien conoce los tres estados
        // del ajuste de auto-mira. Aqui ya llega normalizado.
        Vec2 aim = resolve_aim(world, input);
        if (input.auto_aim_locks()) input.auto_aim_target = transform.position + aim * 32.0f;

        // Face the aim direction and play the attack animation (restart on every shot)
        Direction aim_dir = direction_from_vec(aim);
        if (aim_dir != Direction::None) transform.facing = aim_dir;
        play_anim(world.animations[entity], facing_anim(AnimID::AttackDown, transform.facing), true);

        ProjectileKind kind = (character == CharacterType::Elias) ? ProjectileKind::Fireball
                                                                  : ProjectileKind::Lightbolt;
        Vec2 muzzle = transform.position + aim * 6.0f;
        Entity shot = spawn_projectile(world, muzzle, aim, actual_damage, true, kind);
        // Skin del cayado: un tinte sobre el proyectil que ya existe. La skin 0 deja el
        // sprite tal cual (tinte blanco), asi que no hay caso especial que mantener.
        StaffSkin skin = get_staff_skin(staff_skin);
        if (shot != ENTITY_NONE && world.has(shot, Component::Sprite))
            world.sprites[shot].tint = skin.shot;

        // Fogonazo en la boca del cayado y retroceso en el propio profeta: el disparo
        // deja de salir de la nada. El retroceso NO pisa un knockback mayor (el de un
        // golpe recibido manda sobre el de tu propio tiro).
        Color muzzle_tint = (character == CharacterType::Elias) ? Color{255, 200, 120, 220}
                                                                : Color{200, 230, 255, 220};
        if (skin.muzzle.a > 0) muzzle_tint = skin.muzzle;
        if (spawn_fx(world, muzzle, FxID::Sparks, muzzle_tint, 16.0f, 0.14f) == ENTITY_NONE)
            spawn_vfx(world, muzzle, TileSprite::Star, muzzle_tint, 9.0f, 0.09f, -2.0f, 0.0f, {0, 0}, 9);
        Vec2 recoil = aim * -Config::Feel::SHOT_RECOIL;
        if (movement.knockback.length_sq() < recoil.length_sq()) movement.knockback = recoil;
        shots_fired++;
    }

    // ---- Ataque secundario del arma (GDD 4.1: LT / clic derecho / L) ----
    if (secondary_cd > 0.0f) secondary_cd -= dt;
    if (weapon_slot != UINT32_MAX && secondary_cd <= 0.0f &&
        input.is_action_just_pressed(Action::AttackSecondary)) {
        Vec2 aim = resolve_aim(world, input);

        f32 mult = faith.get_damage_multiplier();
        if (double_portion_timer > 0.0f) mult *= 2.0f;
        if (damage_buff_timer > 0.0f)    mult *= 1.25f;
        if (fire_secondary(world, faith, aim, mult)) faith.reset_inactivity();
    }

    // ---- Miracles ----
    static const Action MIRACLE_KEYS[Config::MAX_MIRACLE_SLOTS] = {
        Action::Miracle1, Action::Miracle2, Action::Miracle3
    };
    for (int i = 0; i < active_slot_count && i < Config::MAX_MIRACLE_SLOTS; i++) {
        miracle_slots[i].update(dt);

        Action miracle_action = MIRACLE_KEYS[i];
        if (input.is_action_just_pressed(miracle_action)) {
            auto& slot = miracle_slots[i];
            if (slot.is_empty()) continue;
            auto def = get_miracle_def(slot.miracle);
            last_cast = slot.miracle;   // el Engine nombra el milagro tambien al fallar
            if (def.tier > faith.get_max_miracle_tier()) {
                last_event = MiracleEvent::NeedTier;
                continue;
            }
            if (def.faith_cost > faith.current) {
                last_event = MiracleEvent::NeedCost;
                continue;
            }
            if (!slot.is_ready()) {
                last_event = MiracleEvent::OnCooldown;
                continue;
            }
            if (slot.try_cast(faith.current, faith.get_max_miracle_tier(), miracle_haste)) {
                faith.modify(-def.faith_cost, true);   // miracle cost ignores the relic floor
                play_anim(world.animations[entity], AnimID::CastMiracle, true);
                cast_miracle(world, faith, slot.miracle);
                SDL_Log("Cast miracle: %s (cost: %.0f faith)", def.name, def.faith_cost);
            }
        }
    }

    // ---- Ongoing miracle effects ----
    // Los milagros con duracion se VEN mientras duran: sin esto, un buff de 15 segundos
    // era indistinguible de no haber pulsado nada.
    aura_timer -= dt;
    bool pulse = aura_timer <= 0.0f;
    if (pulse) aura_timer = 0.28f;

    if (heal_zone_timer > 0.0f) {
        heal_zone_timer -= dt;
        if (transform.position.distance_to(heal_zone_pos) < 34.0f) {
            world.healths[entity].heal(6.0f * dt);
        }
        // Burbujas subiendo del agua sanada, marcando el borde de la zona
        if (pulse) {
            for (int i = 0; i < 3; i++) {
                f32 a = randf_range(0.0f, 6.2831853f);
                Vec2 p = heal_zone_pos + Vec2{std::cos(a) * 30.0f, std::sin(a) * 19.0f};
                spawn_vfx(world, p, TileSprite::Orb, {130, 215, 245, 210}, 7.0f, 0.7f, 0.25f, 0.0f, {0, -22.0f}, 3);
            }
        }
    }
    if (double_portion_timer > 0.0f) {
        double_portion_timer -= dt;
        // Aura dorada girando alrededor del profeta mientras dura la doble porcion
        if (pulse) {
            for (int i = 0; i < 4; i++) {
                f32 a = double_portion_timer * 3.0f + i * 1.57f;
                spawn_vfx(world, transform.position + Vec2{std::cos(a) * 15.0f, std::sin(a) * 10.0f + 2.0f},
                          TileSprite::Star, {255, 220, 130, 230}, 8.0f, 0.45f, -0.15f, 300.0f, {0, -18.0f}, 11);
            }
        }
    }
    // Estela de fuego del Carro: se deja durante toda la embestida, no solo al lanzarla
    if (chariot_timer > 0.0f) {
        chariot_timer -= dt;
        trail_timer -= dt;
        if (trail_timer <= 0.0f) {
            trail_timer = 0.035f;
            spawn_vfx(world, transform.position + Vec2{0, 4.0f}, TileSprite::Fireball,
                      {255, 165, 70, 235}, 15.0f, 0.4f, -0.7f, 200.0f, {0, -12.0f}, 7);
        }
    }
    if (damage_buff_timer > 0.0f)    damage_buff_timer -= dt;

    // Ojos Abiertos: mientras dura, una marca sobre cada enemigo y cada objeto de la sala.
    // El mapa revelado (map_revealed) dura todo el capitulo; estas marcas, solo la duracion.
    if (reveal_timer > 0.0f) {
        reveal_timer -= dt;
        reveal_pulse -= dt;
        if (reveal_pulse <= 0.0f) {
            reveal_pulse = 0.45f;
            for (u32 e = 0; e < world.entity_count; e++) {
                if (!world.is_alive(e) || !world.has(e, Component::Transform)) continue;
                bool enemy = is_hostile(world, e) &&
                             (!world.has(e, Component::Health) || world.healths[e].is_alive());
                bool drop  = world.has(e, Component::Item) &&
                             world.items[e].type == ItemComponent::Type::Pickup;
                if (!enemy && !drop) continue;
                spawn_vfx(world, world.transforms[e].position - Vec2{0, enemy ? 15.0f : 8.0f},
                          TileSprite::Star,
                          enemy ? Color{255, 245, 200, 235} : Color{170, 240, 255, 235},
                          enemy ? 9.0f : 7.0f, 0.4f, -0.3f, 120.0f, {0, 0}, 12);
            }
        }
    }
    // (the consumable key is handled by the Engine, which owns the ItemRegistry)

    // Position is NOT clamped here — tile collision (walls) blocks non-door edges,
    // and door tiles let the player cross the border to trigger room transitions.
}

// =============================================================================
// Ataque secundario del arma equipada
// =============================================================================
bool Player::fire_secondary(ECSWorld& world, FaithSystem& faith, Vec2 aim, f32 damage_mult) {
    (void)faith;
    if (entity == ENTITY_NONE || !world.is_alive(entity)) return false;
    auto& transform = world.transforms[entity];
    f32 base = world.has(entity, Component::Combat) ? world.combats[entity].damage : 10.0f;

    Direction dir = direction_from_vec(aim);
    if (dir != Direction::None) transform.facing = dir;
    // La Vara se blande con la pose de milagro, no con la de disparo: es el unico gesto
    // distinto que hay en la hoja de sprites y hace que las dos armas no se confundan.
    play_anim(world.animations[entity],
              weapon_slot == 1 ? AnimID::CastMiracle : facing_anim(AnimID::AttackDown, transform.facing),
              true);

    switch (weapon_slot) {
        case 0: {   // Honda de David (1 Samuel 17:40): una piedra rápida que atraviesa
            Entity p = spawn_projectile(world, transform.position + aim * 7.0f, aim,
                                        base * damage_mult * 1.6f, true,
                                        ProjectileKind::Arrow, 320.0f, 240.0f);
            if (p != ENTITY_NONE && world.has(p, Component::Projectile))
                world.projectiles[p].pierce = true;
            // Volteo de la honda: la piedra sale de un giro, no de la nada
            spawn_vfx(world, transform.position + aim * 9.0f, TileSprite::Star, {235, 225, 200, 235},
                      11.0f, 0.22f, -0.5f, 900.0f, aim * 40.0f, 11);
            spawn_particles(world, transform.position + aim * 9.0f, {210, 200, 180, 255}, 4, 55.0f, 0.2f);
            secondary_cd = 0.9f;
            return true;
        }
        case 1: {   // Vara de Aarón (Números 17:8): barrido ancho delante del profeta
            constexpr f32 REACH = 34.0f;
            f32 dmg = base * damage_mult * 1.3f;
            bool any = false;
            for (u32 e = 0; e < world.entity_count; e++) {
                if (!world.is_alive(e) || !is_hostile(world, e)) continue;
                if (!world.has(e, Component::Transform) || !world.has(e, Component::Health)) continue;
                Vec2 to = world.transforms[e].position - transform.position;
                if (to.length_sq() > REACH * REACH) continue;
                // Arco frontal de ~120 grados: barrer no es golpear en todas direcciones
                Vec2 n = to.normalized();
                if (n.x * aim.x + n.y * aim.y < 0.35f) continue;
                if (apply_damage(world, entity, e, dmg)) {
                    if (world.has(e, Component::Movement))
                        world.movements[e].knockback = n * 150.0f;
                    if (!world.healths[e].is_alive()) pending_kills.push_back(e);
                    any = true;
                }
            }
            // Arco visible: cinco brotes verdes barriendo el cono frontal, de un lado a otro
            for (int i = 0; i < 5; i++) {
                f32 t = -0.55f + i * 0.275f;
                Vec2 s = {aim.x * std::cos(t) - aim.y * std::sin(t),
                          aim.x * std::sin(t) + aim.y * std::cos(t)};
                spawn_vfx(world, transform.position + s * (REACH * 0.55f), TileSprite::Star,
                          {150, 240, 130, 250}, 17.0f, 0.38f + i * 0.03f, 0.5f, 240.0f, s * 55.0f, 11);
            }
            spawn_particles(world, transform.position + aim * 16.0f, {245, 225, 170, 255}, 8, 90.0f, 0.28f);
            shake_request = std::max(shake_request, any ? 2.0f : 0.8f);
            secondary_cd = 0.75f;
            return true;
        }
        case 16: {  // Arco de Joas (2 Reyes 13:17): "saeta de salvacion de Jehova"
            // Una RAFAGA, no un tiro mas fuerte: es lo que la separa de la Honda, que
            // manda una sola piedra que atraviesa. Tres saetas abiertas 14 grados: a
            // bocajarro caben las tres en un cuerpo, y de lejos barren un frente.
            constexpr f32 SPREAD = 0.245f;   // ~14 grados
            f32 dmg = base * damage_mult * 0.75f;
            for (int i = -1; i <= 1; i++) {
                f32 a = SPREAD * i;
                Vec2 d = {aim.x * std::cos(a) - aim.y * std::sin(a),
                          aim.x * std::sin(a) + aim.y * std::cos(a)};
                spawn_projectile(world, transform.position + d * 7.0f, d, dmg, true,
                                 ProjectileKind::Arrow, 300.0f, 200.0f);
            }
            spawn_particles(world, transform.position + aim * 9.0f, {230, 210, 160, 255}, 5, 60.0f, 0.2f);
            secondary_cd = 1.1f;
            return true;
        }
        default:
            return false;
    }
}

// =============================================================================
// Items (choice screen / inheritance)
// Recalcula los efectos de la reliquia que se lleva AHORA. Los dos efectos de reliquia
// (`FaithSystem::floor_value` de las Tablas y `Player::miracle_power` del Arca) no los
// escribe nadie mas, asi que basta con volver a su base y aplicar el slot: por eso puede
// llamarse mil veces sin acumular. Ninguna de las dos reliquias lleva bonus planos, y si
// alguna los llevara habria que devolverlos aqui tambien.
void Player::apply_relic_effects(FaithSystem& faith) {
    faith.floor_value = 0.0f;
    miracle_power     = 1.0f;

    if (relic_slot == 6) faith.floor_value = 25.0f;   // Tablas de la Ley
    if (relic_slot == 7) miracle_power     = 1.5f;    // Arca del Pacto
}

// =============================================================================
const char* Player::grant_item(ECSWorld& world, FaithSystem& faith, const ItemDef& def, bool record) {
    if (entity == ENTITY_NONE || !world.is_alive(entity)) return "";
    // Una reliquia ocupa UN slot (GDD 7.3), y eso vale tambien para la LISTA: concederla
    // dos veces no la duplica en la fila de objetos ni en el guardado. En juego normal no
    // puede pasar (`roll_choices` no ofrece lo que ya llevas), pero el comando `give` de
    // los guiones si, y ahi es donde se vio.
    const bool already_owned = std::find(items.begin(), items.end(), def.id) != items.end();
    if (record && !(def.category == ItemCategory::Relic && already_owned))
        items.push_back(def.id);

    // Flat stat bonuses (every category can carry them)
    if (def.hp_bonus != 0.0f && world.has(entity, Component::Health)) {
        world.healths[entity].max += def.hp_bonus;
        world.healths[entity].heal(def.hp_bonus);
    }
    if (world.has(entity, Component::Combat)) {
        world.combats[entity].damage += def.damage_bonus;
        world.combats[entity].attack_speed =
            std::max(0.08f, world.combats[entity].attack_speed + def.attack_speed_bonus);
    }
    if (def.speed_bonus != 0.0f && world.has(entity, Component::Movement))
        world.movements[entity].speed += def.speed_bonus;
    if (def.faith_bonus != 0.0f) faith.modify(def.faith_bonus);

    switch (def.category) {
        case ItemCategory::Relic:
            // Un solo slot (GDD 7.3): la que llevabas SALE, y sale de verdad — deja de
            // contar en `items` (fila del HUD, guardado, herencia) y sus efectos se
            // deshacen, porque apply_relic_effects los recalcula desde cero.
            relic_replaced = relic_slot;
            if (relic_replaced != UINT32_MAX && relic_replaced != def.id) {
                auto it = std::find(items.begin(), items.end(), relic_replaced);
                if (it != items.end()) items.erase(it);
            }
            relic_slot = def.id;
            apply_relic_effects(faith);
            break;
        case ItemCategory::Weapon:
            // Ataque secundario en Action::AttackSecondary (ver fire_secondary): la Honda
            // dispara una piedra que atraviesa, la Vara barre en arco. El bonus plano de
            // daño sigue aplicándose arriba como en cualquier otro objeto.
            weapon_slot = def.id;
            secondary_cd = 0.0f;
            break;
        case ItemCategory::Consumable:
            // Otra vasija del mismo aceite suma carga; una distinta reemplaza lo que llevabas
            if (consumable_slot == def.id) consumable_charges = static_cast<u8>(std::min(9, consumable_charges + 1));
            else { consumable_slot = def.id; consumable_charges = 1; }
            break;
        default:
            break;
    }

    // El Manto de Elias (item 15): no es reliquia — no ocupa relic_slot ni compite con las
    // Tablas o el Arca. Abre el tercer slot y acorta la recarga. Se vuelve a otorgar al
    // reintentar el capitulo y al cargar la partida, asi que ambos efectos son idempotentes.
    if (def.id == 15) {
        active_slot_count = std::max(active_slot_count, Config::MAX_MIRACLE_SLOTS);
        miracle_haste = 0.75f;
    }
    return def.description;
}

bool Player::use_consumable(ECSWorld& world, FaithSystem& faith, const ItemRegistry& registry) {
    if (consumable_slot == UINT32_MAX) return false;
    const ItemDef* def = registry.get(consumable_slot);
    if (!def) { consumable_slot = UINT32_MAX; return false; }

    switch (def->id) {
        case 8:  damage_buff_timer = 30.0f; break;                                  // Vino Nuevo
        case 9:  if (world.has(entity, Component::Health)) world.healths[entity].heal(30.0f); break;
        // Higos de Ezequias (2 Reyes 20:7). El pasaje no acaba en que la llaga sane: acaba
        // en "anadire a tus dias quince anos" (20:6). Por eso sube el MAXIMO y no cura y ya.
        case 17: if (world.has(entity, Component::Health)) {
                     world.healths[entity].max += 15.0f;
                     world.healths[entity].heal(15.0f);
                 }
                 break;
        // Sal de Eliseo (2 Reyes 2:21): "Yo sane estas aguas". El unico consumible que
        // devuelve FE, que es el recurso del juego y hasta ahora solo se ganaba jugando.
        case 18: faith.modify(20.0f); break;
        default: break;
    }
    // "El aceite se detuvo cuando no quedo vasija vacia" (2 Reyes 4:6): mientras haya
    // cargas la vasija sigue en la mano y no se toca el inventario.
    if (consumable_charges > 1) { consumable_charges--; return true; }

    // Consumables are removed from the inventory list too, so re-rolls can offer them again
    for (size_t i = 0; i < items.size(); i++) {
        if (items[i] == consumable_slot) { items.erase(items.begin() + i); break; }
    }
    consumable_slot = UINT32_MAX;
    consumable_charges = 0;
    return true;
}

// =============================================================================
// Miracle effects
// =============================================================================
static void ring_of_bolts(ECSWorld& world, Vec2 center, int count, ProjectileKind kind,
                          f32 damage, f32 speed, f32 range, bool pierce) {
    for (int i = 0; i < count; i++) {
        f32 a = (6.2831853f * i) / count;
        Entity p = spawn_projectile(world, center, {std::cos(a), std::sin(a)}, damage, true, kind, speed, range);
        if (p != ENTITY_NONE) world.projectiles[p].pierce = pierce;
    }
}

// -----------------------------------------------------------------------------
// Piezas de VFX. Ningun milagro comparte firma visual con otro: la hoja de sprites
// solo tiene una fila CastMiracle, asi que lo que distingue un poder de otro es lo
// que pasa en pantalla, no la pose del profeta.
// -----------------------------------------------------------------------------

// Corona que se abre: `count` sprites lanzados en circulo, sin dano ni colision
static void vfx_ring(ECSWorld& w, Vec2 c, TileSprite s, Color col, int count,
                     f32 size, f32 speed, f32 life, f32 spin = 0.0f) {
    for (int i = 0; i < count; i++) {
        f32 a = (6.2831853f * i) / count;
        spawn_vfx(w, c, s, col, size, life, 0.0f, spin, {std::cos(a) * speed, std::sin(a) * speed});
    }
}

// Columna que cae del cielo sobre `c`: segmentos apilados que bajan y se apagan
static void vfx_column(ECSWorld& w, Vec2 c, TileSprite s, Color col, int segments,
                       f32 size, f32 life, f32 fall_speed) {
    for (int i = 0; i < segments; i++)
        spawn_vfx(w, c - Vec2{0, size * 0.8f * i}, s, col, size,
                  std::max(0.12f, life - i * 0.02f), 0.0f, 0.0f, {0, fall_speed}, 11);
}

// Haz que sube desde el suelo (ejercito celestial, aceite que brota)
static void vfx_pillar(ECSWorld& w, Vec2 c, TileSprite s, Color col, int segments,
                       f32 size, f32 life) {
    for (int i = 0; i < segments; i++)
        spawn_vfx(w, c, s, col, size, life + i * 0.05f, 0.6f, -90.0f, {0, -55.0f - i * 22.0f}, 11);
}

void Player::cast_miracle(ECSWorld& world, FaithSystem& faith, MiracleID id) {
    if (entity == ENTITY_NONE) return;
    Vec2 pos = world.transforms[entity].position;
    // Eliseo inherits Elias' miracles but casts them weaker (GDD: "unlocked but weaker")
    f32 inherit = (character == CharacterType::Eliseo && get_miracle_def(id).owner == Prophet::Elias)
                  ? 0.7f : 1.0f;
    f32 mult = faith.get_damage_multiplier() * miracle_power * inherit;   // Arca del Pacto: x1.5
    last_event = MiracleEvent::Cast;
    last_cast = id;

    switch (id) {
        case MiracleID::CuervosDeQuerit:
            world.healths[entity].heal(15.0f * inherit);
            // Tres cuervos bajan de fuera de pantalla y se cierran sobre el profeta
            for (int i = 0; i < 3; i++) {
                f32 a = 1.1f + i * 2.09f;
                Vec2 off = {std::cos(a) * 58.0f, std::sin(a) * 34.0f - 18.0f};
                spawn_vfx(world, pos + off, TileSprite::Star, {28, 24, 34, 255},
                          16.0f, 0.75f, -0.35f, 520.0f, off * -1.4f, 11);
            }
            spawn_particles(world, pos, {130, 220, 150, 255}, 12, 50.0f, 0.6f);
            break;

        case MiracleID::PalabraDeSequia:
            // Drought: every enemy moves at half speed while the timer runs (see update_enemy_ai)
            world.enemy_slow_timer = get_miracle_def(id).duration;
            // Onda de tierra seca, y polvo levantandose bajo cada enemigo alcanzado
            vfx_ring(world, pos, TileSprite::Star, {182, 140, 74, 250}, 14, 16.0f, 165.0f, 0.75f, 140.0f);
            spawn_fx(world, pos, FxID::Heal, {235, 225, 190, 235}, 54.0f, 0.65f, 11);
            for (u32 e = 0; e < world.entity_count; e++) {
                if (!world.is_alive(e) || !is_hostile(world, e)) continue;
                if (world.has(e, Component::Health) && !world.healths[e].is_alive()) continue;
                spawn_particles(world, world.transforms[e].position, {196, 168, 118, 240}, 5, 34.0f, 0.5f);
            }
            break;

        case MiracleID::FuegoDelCielo: {
            // Divine fire: heavy damage to everything near the prophet + visual ring
            for (u32 e = 0; e < world.entity_count; e++) {
                if (!world.is_alive(e) || !is_hostile(world, e) || !world.has(e, Component::Health)) continue;
                if (!world.healths[e].is_alive()) continue;
                Vec2 ep = world.transforms[e].position;
                if (ep.distance_to(pos) < 80.0f) {
                    if (apply_damage(world, entity, e, 80.0f * mult)) pending_kills.push_back(e);
                    if (world.has(e, Component::Movement))
                        world.movements[e].knockback = (ep - pos).normalized() * 200.0f;
                }
            }
            // El fuego CAE: una columna sobre el profeta antes de que se abra el circulo
            spawn_fx(world, pos, FxID::Holy, {255, 196, 110, 245}, 78.0f, 0.55f, 11);
            spawn_fx(world, pos, FxID::Explosion, {255, 170, 80, 240}, 62.0f, 0.45f);
            vfx_ring(world, pos, TileSprite::Fireball, {255, 150, 60, 245}, 16, 13.0f, 210.0f, 0.5f, 300.0f);
            ring_of_bolts(world, pos, 14, ProjectileKind::Fireball, 0.0f, 240.0f, 80.0f, true);
            shake_request = 5.0f;
            break;
        }

        case MiracleID::CarroDeFuego: {
            // Devastating dash: long i-frame dash that leaves fire behind
            auto& m = world.movements[entity];
            Vec2 dir = m.velocity.length_sq() > 1.0f ? m.velocity.normalized() : Vec2{0, -1};
            m.is_dashing = true;
            m.dash_dir = dir;
            m.dash_timer = Config::DASH_DURATION * 2.5f;
            m.dash_cooldown = 0.0f;
            world.healths[entity].iframes_timer = 1.0f;
            for (int i = 0; i < 6; i++) {
                Entity p = spawn_projectile(world, pos, dir, 30.0f * mult, true, ProjectileKind::Fireball, 300.0f + i * 40.0f, 140.0f);
                if (p != ENTITY_NONE) world.projectiles[p].pierce = true;
            }
            // Ruedas de fuego a los lados y estela que se va dejando durante la embestida
            for (int s = -1; s <= 1; s += 2) {
                Vec2 side = {-dir.y * 9.0f * s, dir.x * 9.0f * s};
                spawn_vfx(world, pos + side, TileSprite::Fireball, {255, 205, 110, 250},
                          17.0f, 0.4f, 1.2f, 720.0f * s, dir * 60.0f, 11);
            }
            chariot_timer = Config::DASH_DURATION * 2.5f;
            trail_timer = 0.0f;
            shake_request = 3.0f;
            break;
        }

        case MiracleID::AltarConsumido:
            for (u32 e = 0; e < world.entity_count; e++) {
                if (!world.is_alive(e) || !is_hostile(world, e) || !world.has(e, Component::Health)) continue;
                if (!world.healths[e].is_alive()) continue;
                f32 dmg = world.enemy_ais[e].is_boss ? 120.0f * mult : 1000.0f;
                if (apply_damage(world, entity, e, dmg)) pending_kills.push_back(e);
            }
            // El altar consumido lo lame todo: fogonazo a pantalla completa y dos coronas
            flash_request = 0.38f;
            spawn_fx(world, pos, FxID::Ring, {255, 244, 210, 245}, 190.0f, 0.55f, 12);
            spawn_fx(world, pos, FxID::Explosion, {255, 220, 160, 245}, 96.0f, 0.5f, 11);
            vfx_ring(world, pos, TileSprite::Fireball, {255, 235, 190, 255}, 20, 18.0f, 300.0f, 0.6f, 260.0f);
            vfx_ring(world, pos, TileSprite::Star, {255, 170, 70, 240}, 14, 12.0f, 150.0f, 0.85f, -420.0f);
            ring_of_bolts(world, pos, 24, ProjectileKind::Fireball, 0.0f, 300.0f, 200.0f, true);
            shake_request = 8.0f;
            break;

        case MiracleID::SanarAguas:
            heal_zone_pos = pos;
            heal_zone_timer = get_miracle_def(id).duration;
            aura_timer = 0.0f;
            // El agua sanada se abre en el suelo: corona azul lenta y ancha. Con celdas de
            // terreno (WaterA) salian cuadrados opacos: los VFX solo usan iconos sueltos.
            vfx_ring(world, pos, TileSprite::Orb, {110, 205, 245, 245}, 14, 13.0f, 105.0f, 0.85f);
            spawn_fx(world, pos, FxID::Heal, {150, 235, 255, 235}, 60.0f, 0.75f, 11);
            spawn_particles(world, pos, {150, 230, 250, 255}, 10, 44.0f, 0.6f);
            break;

        case MiracleID::AceiteInagotable:
            // "Cuando las vasijas fueron llenas, el aceite se detuvo" (2 Reyes 4:6):
            // +2 cargas al consumible que llevas. Con las manos vacias, el aceite te da
            // una vasija con 2 (los ids 8 y 9 son los dos consumibles de game/items.h).
            if (consumable_slot == UINT32_MAX) {
                consumable_slot = (randf() < 0.5f) ? 8u : 9u;
                consumable_charges = 2;
                if (std::find(items.begin(), items.end(), consumable_slot) == items.end())
                    items.push_back(consumable_slot);   // para que salga en la fila del HUD
            } else {
                consumable_charges = static_cast<u8>(std::min(9, consumable_charges + 2));
            }
            // El aceite no se detiene: gotas doradas brotando de los pies hacia arriba
            for (int i = 0; i < 9; i++) {
                Vec2 o = {randf_range(-11.0f, 11.0f), randf_range(-2.0f, 5.0f)};
                spawn_vfx(world, pos + o, TileSprite::Orb, {250, 205, 105, 250}, 8.0f,
                          0.6f + i * 0.03f, 0.5f, 0.0f, {o.x * 1.4f, -70.0f - i * 9.0f}, 11);
            }
            break;

        case MiracleID::OjosAbiertos:
            // "Jehova abrio los ojos del criado, y vio el monte lleno de carros de fuego"
            // (2 Reyes 6:17): el mapa entero del capitulo queda a la vista para lo que
            // queda de capitulo, y durante la duracion se marca lo que hay en la sala.
            map_revealed = true;
            reveal_timer = get_miracle_def(id).duration;
            world.healths[entity].iframes_timer = 3.0f;
            // Los ojos se abren: onda blanca, y un destello sobre cada enemigo revelado
            vfx_ring(world, pos, TileSprite::Star, {245, 245, 255, 235}, 18, 10.0f, 230.0f, 0.55f, 180.0f);
            for (u32 e = 0; e < world.entity_count; e++) {
                if (!world.is_alive(e) || !is_hostile(world, e)) continue;
                if (world.has(e, Component::Health) && !world.healths[e].is_alive()) continue;
                spawn_vfx(world, world.transforms[e].position - Vec2{0, 14.0f}, TileSprite::Star,
                          {255, 255, 235, 245}, 12.0f, 1.4f, -0.25f, 150.0f, {0, 0}, 12);
            }
            break;

        case MiracleID::EjercitoCelestial: {
            // "El monte estaba lleno de gente de a caballo y de carros de fuego"
            // (2 Reyes 6:17). El GDD lo pide literal: TRES guerreros que pelean 10 s.
            // Antes era una descarga de rayos, que es lo mismo que hacen otros milagros.
            f32 life = get_miracle_def(id).duration > 0.0f ? get_miracle_def(id).duration : 10.0f;
            u32 tex = world.enemy_textures[static_cast<u32>(EnemyType::SoldadoDeAcab)];
            int summoned = 0;
            for (int i = 0; i < 3; i++) {
                f32 a = 1.57f + i * 2.09f;
                Vec2 at = pos + Vec2{std::cos(a) * 26.0f, std::sin(a) * 16.0f};
                if (spawn_disciple(world, at, tex, life, 30.0f, 10.0f * mult) != ENTITY_NONE) summoned++;
                // Un haz de luz plantado donde aparece cada uno
                vfx_pillar(world, at, TileSprite::Lightbolt, {200, 230, 255, 245}, 4, 13.0f, 0.42f);
                spawn_fx(world, at, FxID::Holy, {205, 232, 255, 240}, 56.0f, 0.5f, 11);
            }
            // Sin entidades libres el milagro no puede quedarse en nada: vuelve la descarga
            if (summoned == 0)
                ring_of_bolts(world, pos, 8, ProjectileKind::Lightbolt, 25.0f * mult, 220.0f, 200.0f, true);
            break;
        }

        case MiracleID::DoblePorcion:
            double_portion_timer = get_miracle_def(id).duration;
            world.healths[entity].heal(20.0f);
            aura_timer = 0.0f;
            // La doble porcion se lleva encima: dos coronas doradas girando al reves
            vfx_ring(world, pos, TileSprite::Star, {255, 225, 140, 250}, 10, 13.0f, 120.0f, 0.7f, 260.0f);
            vfx_ring(world, pos, TileSprite::Orb, {255, 200, 90, 235}, 10, 10.0f, 62.0f, 0.9f, -260.0f);
            break;

        default:
            break;
    }
}
