// =============================================================================
// MANTLE - Jefes: una ESTRATEGIA por capitulo (2026-10-03)
// =============================================================================
//
// Hasta hoy los diez jefes eran el enemigo base a 1,6 de escala con los mismos verbos
// (perseguir + rafaga/anillo + invocar). Ahora cada uno pelea a su manera, con fases, con
// telegrafos en el suelo (game/hazards.h) y con una VENTANA que el jugador aprende a
// castigar. El diseno, las cifras y como se gana cada pelea estan en src/game/CLAUDE.md,
// seccion "Los diez jefes (2026-10-03)". Aqui: el como.
//
// Convenciones de todo el fichero:
//   - `boss_state` / `state_timer` son la maquina de estados de cada patron (sus valores,
//     en el enum de su seccion). `vuln_mult` > 1 = ventana: el aro dorado del suelo lo dice.
//   - `health.invulnerable` se pone CADA frame (system_combat_timers lo resetea antes).
//   - Toda cifra de aviso pasa por `wk()` (mas aviso en movil) y toda velocidad de embestida
//     por `sk()` (Config::MOBILE_ENEMY_SPEED_MULT): la pelea no es mas dura en el telefono.
//   - Las lineas `Boss <nombre> phase N (...)` y `Hazard <Tipo> spawned` son el gate de
//     scripts/jefes_check.txt.
//
// =============================================================================

#include "core/i18n.h"
#include "game/enemies.h"
#include "game/combat.h"
#include "game/hazards.h"
#include "game/rooms.h"
#include "platform/platform.h"
#include "core/config.h"
#include "render/renderer.h"
#include "render/sprites.h"
#include <SDL3/SDL.h>
#include <cmath>
#include <algorithm>

namespace {
constexpr f32 PI = 3.14159265f;

f32 wk() { return is_mobile() ? 1.2f : 1.0f; }                                  // aviso
f32 sk() { return is_mobile() ? Config::MOBILE_ENEMY_SPEED_MULT : 1.0f; }       // embestida

// El golpe de referencia de un peligro: ~10% de la vida de Elias en el cap. 1, ~15% de la
// de Eliseo en el 10. Los peligros lo multiplican; los proyectiles siguen con `dmg`.
f32 hit_unit(int chapter) { return 7.5f + 0.75f * static_cast<f32>(chapter); }

Vec2 from_angle(f32 a) { return {std::cos(a), std::sin(a)}; }
f32  angle_of(Vec2 v)  { return std::atan2(v.y, v.x); }
}  // namespace

// =============================================================================
// Utilidades
// =============================================================================
// Los pies DIBUJADOS (fila 112 de la hoja): de ahi salen los telegrafos de cargas y barridos
static Vec2 ground_of(const ECSWorld& w, Entity e) {
    const auto& t = w.transforms[e];
    return t.position + Vec2{0.0f, 12.0f * t.scale.y * 0.9f};
}
// Los pies del COLISIONADOR: lo que mira la pisabilidad (system_movement)
static Vec2 collider_feet(const ECSWorld& w, Entity e) {
    const Vec2 off = w.has(e, Component::Collider) ? w.colliders[e].offset : Vec2{0, 0};
    return w.transforms[e].position + off;
}
static bool walkable(const ECSWorld& w, Vec2 feet) {
    return !w.active_room || w.active_room->is_walkable_world(feet);
}
// El suelo de la arena (20x11 tiles). Sin arena (nunca en juego normal), una pantalla
// alrededor de donde nacio el jefe.
static Rect arena_of(const ECSWorld& w, Entity e) {
    if (w.active_room && w.active_room->has_arena) return w.active_room->arena_rect_world();
    const Vec2 c = w.enemy_ais[e].ring_center;
    return {c.x - 160.0f, c.y - 88.0f, 320.0f, 176.0f};
}
static Vec2 clamp_in(Rect a, Vec2 p, f32 m) {
    return {std::max(a.x + m, std::min(p.x, a.right() - m)),
            std::max(a.y + m, std::min(p.y, a.bottom() - m))};
}
// El ESCENARIO: la parte de la arena que no tapa el HUD (vida y fe arriba a la izquierda,
// minimapa arriba a la derecha, barra del jefe abajo). Visto en las capturas: un jefe que
// se aparta del profeta acababa bajo el minimapa. Los teletransportes, la plata y el
// "guardar distancia" se quedan aqui dentro.
static Rect stage_of(const ECSWorld& w, Entity e) {
    const Rect a = arena_of(w, e);
    return {a.x + 36.0f, a.y + 44.0f, a.w - 72.0f, a.h - 96.0f};
}
// Distancia libre desde unos pies en un rumbo, a pasos de 4 unidades (para que el carril
// pintado acabe donde de verdad acaba la carga: en el muro o en su largo maximo)
static f32 wall_distance(const ECSWorld& w, Vec2 feet, Vec2 dir, f32 max_len) {
    for (f32 s = 4.0f; s <= max_len; s += 4.0f)
        if (!walkable(w, feet + dir * s)) return s - 4.0f;
    return max_len;
}
static bool alive_entity(const ECSWorld& w, Entity e) {
    return e != ENTITY_NONE && w.is_alive(e) && w.has(e, Component::Health) && w.healths[e].is_alive();
}
static Color base_tint(const ECSWorld& w, Entity e) {
    // Con hoja propia, a todo color; con la del enemigo base, la paleta del capitulo
    const auto& ai = w.enemy_ais[e];
    const bool borrowed = ai.enemy_type < 8 && w.sprites[e].texture_id == w.enemy_textures[ai.enemy_type];
    return borrowed ? get_enemy_chapter_tint(ai.chapter) : Color::white();
}
static void face(ECSWorld& w, Entity e, Vec2 dir) {
    const Direction d = direction_from_vec(dir);
    if (d != Direction::None) w.transforms[e].facing = d;
}
static void play(ECSWorld& w, Entity e, AnimID a) {
    if (w.has(e, Component::Animation)) play_anim(w.animations[e], a, true);
}
static void steer(ECSWorld& w, Entity e, Vec2 dir, f32 speed) {
    w.movements[e].velocity = dir * speed;
}
// Se acerca hasta `far`, se aleja por debajo de `near` y entre medias rodea al profeta
static void keep_range(ECSWorld& w, Entity e, Vec2 to_player, f32 dist, f32 near_d, f32 far_d,
                       f32 strafe) {
    auto& m = w.movements[e];
    if (dist < near_d)      m.velocity = to_player * -m.speed;
    else if (dist > far_d)  m.velocity = to_player * m.speed * 0.8f;
    else {
        const Vec2 perp = {-to_player.y, to_player.x};
        const f32 side = (static_cast<int>(w.time / 2.3f + e) % 2 == 0) ? 1.0f : -1.0f;
        m.velocity = perp * (m.speed * strafe * side);
    }
    // Fuera del escenario (bajo el HUD) vuelve hacia dentro: retirarse no es esconderse
    const Rect st = stage_of(w, e);
    const Vec2 p = w.transforms[e].position;
    Vec2 back = {0, 0};
    if (p.x < st.x) back.x = 1.0f; else if (p.x > st.right()) back.x = -1.0f;
    if (p.y < st.y) back.y = 1.0f; else if (p.y > st.bottom()) back.y = -1.0f;
    if (back.length_sq() > 0.0f) m.velocity = (m.velocity * 0.4f + back.normalized() * m.speed);
}
static void turn_toward(Vec2& face_dir, Vec2 want, f32 max_rad) {
    f32 a = angle_of(face_dir), b = angle_of(want);
    f32 d = b - a;
    while (d > PI) d -= 2.0f * PI;
    while (d < -PI) d += 2.0f * PI;
    d = std::max(-max_rad, std::min(max_rad, d));
    face_dir = from_angle(a + d);
}
static void enter_phase(ECSWorld& w, Entity e, u8 phase, const char* what, Color c, f32 flash) {
    auto& ai = w.enemy_ais[e];
    ai.phase = phase;
    SDL_Log("Boss %s phase %d (%s)", ai.name, static_cast<int>(phase), what);
    const Vec2 p = w.transforms[e].position;
    spawn_particles(w, p, c, 22, 120.0f, 0.6f);
    spawn_fx(w, p, FxID::Ring, c, 90.0f, 0.55f, 11);
    if (flash > 0.0f) w.flash_request = std::max(w.flash_request, flash);
    w.shake_request = std::max(w.shake_request, 3.0f);
}
// Secuaces vivos DE ESTE JEFE: por su celda, no los de toda la zona. Con el recuento
// global, los enemigos de las celdas vecinas (activas pero fuera de la arena) llenaban el
// techo y Ben-adad no traia ni una oleada (visto en jefes_check: 20 vivos en la zona).
static int count_boss_minions(const ECSWorld& w, Entity boss) {
    const int cell = w.enemy_ais[boss].home_cell;
    int n = 0;
    for (u32 i = 0; i < w.entity_count; i++) {
        if (!w.is_alive(i) || !is_hostile(w, i)) continue;
        if (w.has(i, Component::Health) && !w.healths[i].is_alive()) continue;
        const auto& ai = w.enemy_ais[i];
        if (ai.is_boss || ai.boss_group || ai.prop_kind != 0) continue;
        if (cell >= 0 && ai.home_cell != cell) continue;
        n++;
    }
    return n;
}

// Invoca en un sitio concreto (bordes de la arena): los refuerzos ENTRAN, no brotan del jefe
static Entity summon_at(ECSWorld& w, Entity boss, EnemyType type, Vec2 at, int chapter, int max_alive) {
    if (count_boss_minions(w, boss) >= max_alive) return ENTITY_NONE;
    if (!walkable(w, at + Vec2{0, 8})) at = enemy_free_spot(w, at, 18.0f, 1, w.transforms[boss].position);
    Entity s = spawn_enemy(w, type, at, w.enemy_textures[static_cast<u32>(type)], chapter);
    if (s != ENTITY_NONE) {
        w.enemy_ais[s].home_cell = w.enemy_ais[boss].home_cell;   // cuenta en la celda del jefe
        w.enemy_ais[s].aggro_range = 400.0f;                       // la arena entera
        spawn_particles(w, at, {255, 220, 160, 255}, 10, 70.0f, 0.4f);
        spawn_fx(w, at, FxID::Smoke, {220, 210, 190, 200}, 30.0f, 0.5f, 7);
    }
    return s;
}
static int count_props(const ECSWorld& w, Entity owner, BossProp kind) {
    int n = 0;
    for (u32 i = 0; i < w.entity_count; i++) {
        if (!w.is_alive(i) || !w.has(i, Component::EnemyAI)) continue;
        const auto& ai = w.enemy_ais[i];
        if (ai.prop_kind != static_cast<u8>(kind) || ai.owner != owner) continue;
        if (w.has(i, Component::Health) && !w.healths[i].is_alive()) continue;
        n++;
    }
    return n;
}
// Moneda del Acreedor: un orbe dorado que gira (proyectil hostil normal por debajo)
static void shoot_coin(ECSWorld& w, Entity e, Vec2 dir, f32 dmg, f32 speed) {
    Entity p = spawn_projectile(w, w.transforms[e].position + Vec2{0, 2}, dir, dmg, false,
                                ProjectileKind::Fireball, speed, 280.0f);
    if (p == ENTITY_NONE) return;
    if (w.disc_tex != TEXTURE_NONE) {
        // Un disco liso (filtrado suave): la celda del orbe del tileset, a este tamano y con
        // filtrado al vecino, salia como una cruz de pixeles
        w.sprites[p].texture_id = w.disc_tex;
        w.sprites[p].src_rect = {0, 0, 64, 64};
        w.transforms[p].scale = {0.55f, 0.42f};   // de canto: una moneda que gira
    } else if (w.tileset != TEXTURE_NONE) {
        w.sprites[p].src_rect = get_tile_src(TileSprite::Orb);
        w.transforms[p].scale = {0.6f, 0.6f};
    }
    w.sprites[p].tint = {255, 206, 80, 255};
    w.projectiles[p].spin = 420.0f;
}
// Un talento de plata en el suelo (cap. 9). No tiene colisionador: es el cebo de Guiezi,
// no un premio del profeta.
static Entity spawn_silver(ECSWorld& w, Vec2 at) {
    Entity s = w.create();
    if (s == ENTITY_NONE) return s;
    w.add(s, Component::Transform);
    w.transforms[s].position = at;
    w.transforms[s].scale = {0.8f, 0.8f};
    w.add(s, Component::Sprite);
    if (w.tileset != TEXTURE_NONE) {
        w.sprites[s].texture_id = w.tileset;
        w.sprites[s].src_rect = get_tile_src(TileSprite::Orb);
    } else {
        w.sprites[s].src_rect = {0, 0, 7, 7};
    }
    w.sprites[s].tint = {226, 232, 246, 255};
    w.sprites[s].z_order = 4;
    w.add(s, Component::Item);
    w.items[s].type = ItemComponent::Type::Pickup;
    w.items[s].pickup = ItemComponent::Pickup::Silver;
    w.items[s].bob_phase = randf_range(0.0f, 6.28f);
    w.add(s, Component::Lifetime);
    w.lifetimes[s].remaining = 14.0f;
    spawn_particles(w, at, {235, 240, 250, 255}, 6, 50.0f, 0.4f);
    return s;
}
static Entity nearest_silver(const ECSWorld& w, Vec2 from) {
    Entity best = ENTITY_NONE;
    f32 bd = 1e9f;
    for (u32 i = 0; i < w.entity_count; i++) {
        if (!w.is_alive(i) || !w.has(i, Component::Item)) continue;
        if (w.items[i].pickup != ItemComponent::Pickup::Silver) continue;
        f32 d = w.transforms[i].position.distance_to(from);
        if (d < bd) { bd = d; best = static_cast<Entity>(i); }
    }
    return best;
}

const char* boss_invulnerable_hint(BossPattern pattern) {
    switch (pattern) {
        case BossPattern::SacerdoteBaal:    return tr("ÍDOLOS EN PIE — derríbalos", "IDOLS STANDING — break them");
        case BossPattern::ProfetasDeBaal:   return tr("EL CORRO LE PROTEGE — rompe el corro", "THE RING SHIELDS HIM — break the ring");
        case BossPattern::EspirituCorrupto: return tr("BAJO EL AGUA — vigila dónde emerge", "UNDER THE WATER — watch where it rises");
        case BossPattern::LaMuerte:         return tr("INTOCABLE — acércate", "UNTOUCHABLE — get closer");
        default:                            return tr("INTOCABLE", "UNTOUCHABLE");
    }
}

// =============================================================================
// Nacimiento
// =============================================================================
Entity spawn_boss(ECSWorld& world, EnemyType type, Vec2 position, u32 texture_id, bool own_sheet,
                  int chapter, const char* boss_name, BossPattern pattern) {
    Entity e = spawn_enemy(world, type, position, texture_id, chapter);
    if (e == ENTITY_NONE) return e;

    auto def = get_enemy_def(type);
    // Vida: la formula de siempre (200 + 50 por capitulo) por un factor por patron, medido
    // con el bot de autoplay (punteria perfecta, ~55 de dano por segundo: un jugador de
    // verdad, esquivando, mete la mitad). Sube donde la estrategia deja ventanas muy
    // generosas (los idolos rotos, la plata, el silbo) y donde la pelea tiene fases que hay
    // que llegar a VER; el corro, el agua y la Muerte ya alargan la pelea por si solos.
    f32 hp = 200.0f + 50.0f * chapter;
    switch (pattern) {
        case BossPattern::CapitanDeAcab:  hp *= 1.40f; break;
        case BossPattern::SacerdoteBaal:  hp *= 1.30f; break;
        case BossPattern::ProfetasDeBaal: hp *= 0.75f; break;   // el corro pone el resto
        case BossPattern::Desesperacion:  hp *= 1.70f; break;   // cuatro fases que se tienen que ver
        case BossPattern::AcabYJezabel:   hp *= 0.90f; break;   // Acab; Jezabel lleva 0,65 mas
        case BossPattern::Acreedor:       hp *= 1.30f; break;
        case BossPattern::Guiezi:         hp *= 1.50f; break;
        default: break;
    }
    world.healths[e].max = hp;
    world.healths[e].current = hp;
    world.combats[e].damage = def.damage * 1.25f;
    world.movements[e].speed = std::max(50.0f, def.speed * 0.72f) * sk();
    // Con hoja propia el arte ya es "de jefe": 1,4 lee como jefe sin tapar la arena
    const f32 s = own_sheet ? 1.4f : 1.6f;
    world.transforms[e].scale = {s, s};
    world.colliders[e].radius = 11.0f * s / 1.6f;
    world.colliders[e].offset = {0, 5.0f * s};
    if (own_sheet) world.sprites[e].tint = Color::white();
    auto& ai = world.enemy_ais[e];
    ai.is_boss = true;
    ai.name = boss_name ? boss_name : def.name;
    ai.aggro_range = 400.0f;
    ai.shoot_timer = 2.0f;
    ai.leap_timer = 2.2f;
    ai.boss_pattern = static_cast<u8>(pattern);
    ai.phase = 0;
    ai.special_timer = 3.0f;
    ai.aux_timer = 4.0f;
    ai.aux2_timer = 6.0f;
    ai.partner = ENTITY_NONE;
    ai.ring_center = position;
    ai.face_dir = {0, 1};
    SDL_Log("Boss %s spawned: hp %.0f, hoja %s, escala %.2f", ai.name, hp,
            own_sheet ? "propia" : "del enemigo base", s);
    return e;
}

// -----------------------------------------------------------------------------
// El corro del Carmelo (1 R 18:26-29)
// -----------------------------------------------------------------------------
// Un sumo sacerdote (la cabeza, `is_boss`) y sus profetas (`boss_group`) "saltando
// alrededor del altar" (18:26). El capitulo termina cuando cae el ultimo: la sala se
// limpia por `count_living_enemies() == 0`, asi que un grupo encaja por construccion.
// "Los 450" se transmiten como REPOSICION — la cabeza rellena el corro desde el pueblo —,
// no como numero en pantalla. Desde el 2026-10-03, ademas, EL CORRO LE PROTEGE: con tres
// o mas de pie la cabeza es intocable (y se ve: una atadura roja de cada uno a el).
static constexpr int BAAL_RING_SLOTS  = 6;
static constexpr f32 BAAL_RING_RADIUS = 44.0f;
static constexpr f32 BAAL_RING_LEASH  = 120.0f;   // mas lejos del corro, lo persiguen
static constexpr int BAAL_SHIELD_MIN  = 3;        // con tantos de pie, la cabeza no se toca

static f32 ring_slot_angle(int slot) { return slot * (6.28318f / BAAL_RING_SLOTS); }

int count_boss_group(const ECSWorld& world) {
    int n = 0;
    for (u32 i = 0; i < world.entity_count; i++) {
        if (!world.is_alive(i) || !world.has(i, Component::EnemyAI)) continue;
        if (!world.enemy_ais[i].boss_group) continue;
        if (world.has(i, Component::Health) && !world.healths[i].is_alive()) continue;
        n++;
    }
    return n;
}

static int free_ring_slot(const ECSWorld& world) {
    for (int s = 0; s < BAAL_RING_SLOTS; s++) {
        const f32 want = ring_slot_angle(s);
        bool taken = false;
        for (u32 i = 0; i < world.entity_count && !taken; i++) {
            if (!world.is_alive(i) || !world.has(i, Component::EnemyAI) || !world.enemy_ais[i].boss_group) continue;
            if (world.has(i, Component::Health) && !world.healths[i].is_alive()) continue;
            taken = std::fabs(world.enemy_ais[i].ring_phase - want) < 0.01f;
        }
        if (!taken) return s;
    }
    return -1;
}

static Entity spawn_baal_prophet(ECSWorld& world, Entity head, Vec2 center, int slot, int chapter) {
    const f32 a = ring_slot_angle(slot);
    Vec2 want = center + Vec2{std::cos(a), std::sin(a)} * BAAL_RING_RADIUS;
    Vec2 at = want;
    if (world.active_room && !world.active_room->is_walkable_world(want + Vec2{0, 8}))
        at = enemy_free_spot(world, want, 18.0f, slot, center);
    Entity m = spawn_enemy(world, EnemyType::ProfetaDeBaal, at,
                           world.enemy_textures[static_cast<u32>(EnemyType::ProfetaDeBaal)], chapter);
    if (m == ENTITY_NONE) return m;
    auto& ai = world.enemy_ais[m];
    ai.boss_group   = true;
    ai.boss_pattern = static_cast<u8>(BossPattern::ProfetasDeBaal);
    ai.name         = tr("Profeta de Baal", "Prophet of Baal");
    ai.ring_center  = center;
    ai.ring_phase   = a;
    ai.phase        = 0;
    ai.owner        = head;
    ai.shoot_timer  = 1.2f + 0.4f * slot;   // escalonados: el corro no dispara a la vez
    world.transforms[m].scale = {1.15f, 1.15f};
    // 1,3 y no 1,6 desde que el corro protege a la cabeza: hay que poder ralearlo
    world.healths[m].max *= 1.3f;
    world.healths[m].current = world.healths[m].max;
    SDL_Log("Baal prophet slot %d at (%.0f,%.0f)%s", slot, at.x, at.y,
            (at.x != want.x || at.y != want.y) ? " (recolocado)" : "");
    return m;
}

// Jezabel: la segunda cabeza del cap. 5. Va en el grupo (barra compartida), no es `is_boss`
static Entity spawn_jezabel(ECSWorld& world, Entity acab, Vec2 at, const BossSheets& sh, int chapter) {
    const u32 tex = (sh.partner != TEXTURE_NONE) ? sh.partner
                  : world.enemy_textures[static_cast<u32>(EnemyType::ProfetaDeBaal)];
    Entity j = spawn_enemy(world, EnemyType::ProfetaDeBaal, at, tex, chapter);
    if (j == ENTITY_NONE) return j;
    const f32 hp = (200.0f + 50.0f * chapter) * 0.65f;
    world.healths[j].max = hp;
    world.healths[j].current = hp;
    world.combats[j].damage = world.combats[acab].damage * 0.8f;
    world.movements[j].speed = 70.0f * sk();
    const f32 s = sh.partner_own ? 1.3f : 1.45f;
    world.transforms[j].scale = {s, s};
    world.colliders[j].radius = 8.0f;
    world.colliders[j].offset = {0, 5.0f * s};
    if (sh.partner_own) world.sprites[j].tint = Color::white();
    auto& ai = world.enemy_ais[j];
    ai.boss_group   = true;
    ai.boss_pattern = static_cast<u8>(BossPattern::AcabYJezabel);
    ai.name         = "Jezabel";
    ai.owner        = acab;
    ai.partner      = acab;
    ai.aggro_range  = 400.0f;
    ai.shoot_timer  = 2.0f;
    ai.special_timer = 3.5f;   // primeras maldiciones
    ai.aux_timer    = 2.0f;    // teletransporte
    ai.aux2_timer   = 6.0f;    // primera curacion
    ai.ring_center  = at;
    SDL_Log("Boss Jezabel spawned: hp %.0f, hoja %s", hp, sh.partner_own ? "propia" : "del enemigo base");
    return j;
}

Entity spawn_boss_group(ECSWorld& world, EnemyType type, Vec2 position, Vec2 ring_center,
                        const BossSheets& sheets, int chapter, const char* boss_name, BossPattern pattern) {
    if (pattern == BossPattern::AcabYJezabel) {
        // El rey y la reina, de pie a los dos lados del ancla: los mismos sitios en que
        // estaban sus figuras en la escena del jefe, que ahora son ELLOS (scenes.h JEFE_5).
        const u32 acab_tex = (sheets.main != TEXTURE_NONE) ? sheets.main
                           : world.enemy_textures[static_cast<u32>(EnemyType::SoldadoDeAcab)];
        Entity acab = spawn_boss(world, EnemyType::SoldadoDeAcab, position + Vec2{-32.0f, 0.0f},
                                 acab_tex, sheets.main_own, chapter, "Acab", pattern);
        if (acab == ENTITY_NONE) return acab;
        world.movements[acab].speed = 72.0f * sk();
        world.enemy_ais[acab].leap_timer = 1.0f;   // abre con una carga: el rey entra embistiendo
        Entity jez = spawn_jezabel(world, acab, position + Vec2{32.0f, 0.0f}, sheets, chapter);
        world.enemy_ais[acab].partner = jez;
        return acab;
    }
    Entity head = spawn_boss(world, type, position, sheets.main, sheets.main_own, chapter, boss_name, pattern);
    if (head == ENTITY_NONE || pattern != BossPattern::ProfetasDeBaal) return head;
    // El nombre del GRUPO ("Los 450 Profetas de Baal") va en la barra; el de la cabeza, aqui
    world.enemy_ais[head].name = tr("Sumo sacerdote de Baal", "High priest of Baal");
    world.enemy_ais[head].ring_center = ring_center;
    world.enemy_ais[head].aux_timer = 9.0f;   // primer ritual
    // Cinco de pie al abrir la pelea; el sexto hueco lo rellena la cabeza
    for (int s = 0; s < BAAL_RING_SLOTS - 1; s++) spawn_baal_prophet(world, head, ring_center, s, chapter);
    return head;
}

// =============================================================================
// Cap. 1 - El Capitan de Acab (1 R 18:10): el cazador
// =============================================================================
// Carga en CARRIL: lo pinta 0,8 s antes y luego lo recorre. Si el carril acaba en el muro
// se estrella y queda ATURDIDO 1,6 s, sin escudo y con dano x1,5. Mientras no lo esta, su
// ESCUDO para los tiros que entran por un frente de 120 grados — que gira despacio, asi
// que rodearle tambien vale. Al 50% pide dos arqueros a los bordes y encadena dos cargas.
// Como se gana: de espaldas a un muro, esperar el carril, apartarse y castigar el golpe.
enum : u8 { CAP_CHASE = 0, CAP_AIM, CAP_CHARGE, CAP_STUN, CAP_SKID };
static constexpr f32 CAP_CHARGE_SPEED = 270.0f;
static constexpr f32 CAP_CHARGE_MAX   = 210.0f;
static constexpr f32 CAP_AIM_TIME     = 0.8f;
static constexpr f32 CAP_STUN_TIME    = 1.6f;
static constexpr f32 CAP_SHIELD_COS   = 0.5f;    // frente de 120 grados
static constexpr f32 CAP_TURN_RATE    = 2.6f;    // rad/s

static void boss_captain(ECSWorld& w, Entity e, Vec2 to_player, f32 dist, f32 dt) {
    auto& ai = w.enemy_ais[e];
    auto& m = w.movements[e];
    const int chapter = ai.chapter;
    if (w.healths[e].ratio() < 0.5f && ai.phase == 0) {
        enter_phase(w, e, 1, "pide arqueros y encadena cargas", {255, 205, 130, 255}, 0.0f);
        const Rect a = arena_of(w, e);
        summon_at(w, e, EnemyType::ArqueroReal, {a.x + 20.0f, a.y + a.h * 0.5f}, chapter, 6);
        summon_at(w, e, EnemyType::ArqueroReal, {a.right() - 20.0f, a.y + a.h * 0.5f}, chapter, 6);
    }
    w.sprites[e].tint = base_tint(w, e);

    switch (ai.boss_state) {
    case CAP_CHASE: {
        ai.shield_cos = CAP_SHIELD_COS;
        ai.vuln_mult = 1.0f;
        turn_toward(ai.face_dir, to_player, CAP_TURN_RATE * dt);
        steer(w, e, to_player, dist > 20.0f ? m.speed : 0.0f);
        if (ai.leap_timer <= 0.0f && dist < 240.0f) {
            const Vec2 dir = to_player;
            const f32 len = wall_distance(w, collider_feet(w, e), dir, CAP_CHARGE_MAX);
            if (len < 40.0f) { ai.leap_timer = 0.7f; break; }   // pegado al muro: no embiste
            ai.act_dir = dir;
            ai.face_dir = dir;
            ai.act_pos = collider_feet(w, e) + dir * len;
            Hazard h = hz_lane(e, ground_of(w, e), dir, len + 8.0f, 24.0f, CAP_AIM_TIME * wk(),
                               len / (CAP_CHARGE_SPEED * sk()) + 0.1f, 0.0f, {255, 96, 64, 255});
            h.hits_player = false;   // el golpe es su cuerpo (contacto); esto es el aviso
            spawn_hazard(w, h);
            ai.boss_state = CAP_AIM;
            ai.state_timer = CAP_AIM_TIME * wk();
            m.velocity = {0, 0};
            face(w, e, dir);
            play(w, e, AnimID::CastMiracle);
        }
        break;
    }
    case CAP_AIM:
        m.velocity = {0, 0};
        ai.face_dir = ai.act_dir;
        ai.state_timer -= dt;
        if (static_cast<int>(ai.state_timer * 20.0f) % 3 == 0)
            spawn_particles(w, ground_of(w, e), {200, 180, 150, 255}, 1, 30.0f, 0.3f);   // rasca el suelo
        if (ai.state_timer <= 0.0f) {
            ai.boss_state = CAP_CHARGE;
            ai.state_timer = CAP_CHARGE_MAX / (CAP_CHARGE_SPEED * sk()) + 0.35f;
            play(w, e, AnimID::Dash);
        }
        break;
    case CAP_CHARGE: {
        const Vec2 feet = collider_feet(w, e);
        const bool blocked = !walkable(w, feet + ai.act_dir * 9.0f);
        const bool arrived = (ai.act_pos - feet).dot(ai.act_dir) <= 0.0f;
        ai.state_timer -= dt;
        if (blocked || arrived || ai.state_timer <= 0.0f) {
            m.velocity = {0, 0};
            m.knockback = {0, 0};
            if (blocked) {
                ai.boss_state = CAP_STUN;
                ai.state_timer = CAP_STUN_TIME;
                ai.vuln_mult = 1.5f;
                ai.shield_cos = -2.0f;
                w.shake_request = std::max(w.shake_request, 3.5f);
                const Vec2 hit = ground_of(w, e) + ai.act_dir * 10.0f;
                spawn_fx(w, hit, FxID::Impact, {255, 235, 190, 240}, 34.0f, 0.3f);
                spawn_fx(w, hit, FxID::Smoke, {200, 185, 160, 210}, 40.0f, 0.6f, 7);
                spawn_particles(w, hit, {210, 190, 150, 255}, 14, 100.0f, 0.5f);
                SDL_Log("Boss %s stunned (se estrella contra el muro)", ai.name);
            } else {
                ai.boss_state = CAP_SKID;
                ai.state_timer = 0.45f;
            }
        } else {
            m.velocity = ai.act_dir * CAP_CHARGE_SPEED * sk();
            m.knockback = {0, 0};
            spawn_particles(w, ground_of(w, e), {200, 180, 150, 255}, 1, 25.0f, 0.3f);
        }
        break;
    }
    case CAP_STUN:
        m.velocity = {0, 0};
        ai.shield_cos = -2.0f;
        ai.vuln_mult = 1.5f;
        w.sprites[e].tint = {255, 236, 180, 255};
        ai.state_timer -= dt;
        if (ai.state_timer <= 0.0f) {
            ai.boss_state = CAP_CHASE;
            ai.vuln_mult = 1.0f;
            ai.counter = 0;
            ai.leap_timer = (ai.phase > 0) ? 2.4f : 3.0f;
        }
        break;
    case CAP_SKID:
        m.velocity = m.velocity * 0.85f;
        ai.state_timer -= dt;
        if (ai.state_timer <= 0.0f) {
            ai.boss_state = CAP_CHASE;
            // Al 50%: la segunda carga sale encadenada a la primera si esta no acabo en el muro
            if (ai.phase > 0 && ai.counter == 0) { ai.counter = 1; ai.leap_timer = 0.15f; }
            else                                 { ai.counter = 0; ai.leap_timer = (ai.phase > 0) ? 2.4f : 3.0f; }
        }
        break;
    default: ai.boss_state = CAP_CHASE; break;
    }
}

// =============================================================================
// Cap. 2 - El Sacerdote de Baal: el oficiante
// =============================================================================
// Levanta TRES idolos (props con vida; la celda del altar de Baal del tileset). Mientras
// quede uno en pie es INTOCABLE y lo dice una atadura morada de cada idolo a el. Rotos los
// tres, 10 s de ventana; 1,4 s antes de que vuelvan a brotar, su sitio se pinta en el suelo
// (y quema a quien este encima). Ataca con LLUVIA DE FUEGO (3 circulos, 5 desde el 50%,
// uno siempre sobre el profeta) y abanicos de rayos oscuros.
// Como se gana: idolos primero (cada uno da fe, como un profeta), y en la ventana de 7 s, a el.
static constexpr f32 PRIEST_WINDOW = 7.0f;
static constexpr f32 PRIEST_REGROW_WARN = 1.4f;

static void idol_spots(const ECSWorld& w, Entity boss, Vec2 out[3]) {
    const Rect a = arena_of(w, boss);
    out[0] = {a.x + 46.0f, a.y + 46.0f};
    out[1] = {a.right() - 46.0f, a.y + 46.0f};
    out[2] = {a.x + a.w * 0.5f, a.y + a.h * 0.60f};   // mas abajo lo tapa la barra del jefe
}

static Entity spawn_idol(ECSWorld& w, Entity boss, Vec2 at, int chapter) {
    Entity i = w.create();
    if (i == ENTITY_NONE) return i;
    w.add(i, Component::Transform);
    w.transforms[i].position = at;
    w.transforms[i].scale = {1.15f, 1.15f};
    w.add(i, Component::Sprite);
    if (w.tileset != TEXTURE_NONE) {
        w.sprites[i].texture_id = w.tileset;
        w.sprites[i].src_rect = get_tile_src(TileSprite::AltarBaal);
    } else {
        w.sprites[i].src_rect = {0, 0, 12, 20};
        w.sprites[i].tint = {142, 68, 173, 255};
    }
    w.sprites[i].fixed_src = true;   // una celda del tileset, no una hoja: nadie la anima
    w.sprites[i].z_order = 5;
    // Animation solo para que, al romperse, sea un CADAVER que se apaga (apply_damage)
    w.add(i, Component::Animation);
    play_anim(w.animations[i], AnimID::IdleDown, true);
    w.add(i, Component::Health);
    w.healths[i].max = 40.0f + 8.0f * chapter;
    w.healths[i].current = w.healths[i].max;
    w.add(i, Component::Collider);
    w.colliders[i].shape = ColliderComponent::Shape::Circle;
    w.colliders[i].radius = 8.0f;
    w.colliders[i].offset = {0, 6};
    // Sin Movement ni Combat: no se mueve, no golpea, no se le empuja (combat.cpp push)
    w.add(i, Component::EnemyAI);
    auto& ai = w.enemy_ais[i];
    ai.behavior   = AIBehavior::Caster;
    ai.name       = tr("Ídolo de Baal", "Idol of Baal");
    ai.enemy_type = static_cast<u32>(EnemyType::ProfetaDeBaal);   // al caer da la fe de un profeta
    ai.chapter    = static_cast<u8>(chapter);
    ai.prop_kind  = static_cast<u8>(BossProp::Idol);
    ai.owner      = boss;
    ai.home_cell  = w.enemy_ais[boss].home_cell;
    spawn_fx(w, at, FxID::Smoke, {170, 110, 210, 220}, 44.0f, 0.7f, 7);
    spawn_particles(w, at, {190, 110, 240, 255}, 12, 80.0f, 0.5f);
    return i;
}

static void raise_idols(ECSWorld& w, Entity e) {
    Vec2 spots[3];
    idol_spots(w, e, spots);
    int n = 0;
    for (const Vec2& s : spots) {
        Vec2 at = walkable(w, s + Vec2{0, 6}) ? s : enemy_free_spot(w, s, 16.0f, n, s);
        if (spawn_idol(w, e, at, w.enemy_ais[e].chapter) != ENTITY_NONE) n++;
    }
    SDL_Log("Boss %s: idolos en pie (%d) -> intocable", w.enemy_ais[e].name, n);
}

static void boss_priest(ECSWorld& w, Entity e, Vec2 player_pos, Vec2 to_player, f32 dist, f32 dmg, f32 dt) {
    auto& ai = w.enemy_ais[e];
    auto& h = w.healths[e];
    const f32 hit = hit_unit(ai.chapter);
    if (ai.counter == 0) { raise_idols(w, e); ai.counter = 1; }
    const int idols = count_props(w, e, BossProp::Idol);
    h.invulnerable = idols > 0;
    w.sprites[e].tint = base_tint(w, e);
    if (ai.counter == 1 && idols == 0) {
        ai.counter = 2;
        ai.aux_timer = PRIEST_WINDOW;
        ai.aux2_timer = 1.0f;   // > 0: el aviso del rebrote todavia no ha salido
        SDL_Log("Boss %s: idolos derribados -> vulnerable %.0f s", ai.name, PRIEST_WINDOW);
        spawn_particles(w, w.transforms[e].position, {255, 230, 160, 255}, 18, 90.0f, 0.5f);
        w.shake_request = std::max(w.shake_request, 2.5f);
    }
    if (ai.counter == 2) {
        ai.aux_timer -= dt;
        ai.vuln_mult = 1.15f;   // sin sus idolos, desamparado: el aro dorado lo dice
        if (ai.aux_timer <= PRIEST_REGROW_WARN && ai.aux2_timer > 0.0f) {
            ai.aux2_timer = -1.0f;
            Vec2 spots[3];
            idol_spots(w, e, spots);
            for (const Vec2& s : spots)
                spawn_hazard(w, hz_circle(e, s + Vec2{0, 8}, 20.0f, PRIEST_REGROW_WARN, hit * 0.8f,
                                          {170, 80, 220, 255}, HazardFx::Dark));
        }
        if (ai.aux_timer <= 0.0f) {
            ai.counter = 1;
            ai.vuln_mult = 1.0f;
            raise_idols(w, e);
        }
    }
    if (h.ratio() < 0.5f && ai.phase == 0)
        enter_phase(w, e, 1, "lluvia de fuego doble", {255, 140, 80, 255}, 0.0f);

    keep_range(w, e, to_player, dist, 70.0f, 140.0f, 0.45f);

    // LLUVIA DE FUEGO: uno siempre donde esta el profeta, los demas alrededor
    if (ai.special_timer <= 0.0f) {
        ai.special_timer = ai.phase ? 3.2f : 4.2f;
        const int n = ai.phase ? 5 : 3;
        const Rect a = arena_of(w, e);
        const Vec2 pg = player_pos + Vec2{0.0f, 9.0f};
        for (int i = 0; i < n; i++) {
            Vec2 at = pg;
            if (i > 0) at = pg + from_angle(randf_range(0.0f, 2.0f * PI)) * randf_range(26.0f, 54.0f);
            at = clamp_in(a, at, 10.0f);
            spawn_hazard(w, hz_circle(e, at, 18.0f, (1.0f + 0.12f * i) * wk(), hit * 0.9f,
                                      {255, 118, 48, 255}, HazardFx::Fire));
        }
        play(w, e, AnimID::CastMiracle);
    }
    if (ai.shoot_timer <= 0.0f && enemy_has_los(w, w.transforms[e].position, player_pos)) {
        ai.shoot_timer = ai.phase ? 2.1f : 2.7f;
        enemy_volley(w, e, to_player, ai.phase ? 7 : 5, 0.2f, ProjectileKind::Darkbolt, dmg * 0.5f, 140.0f, 280.0f);
    }
}

// =============================================================================
// Cap. 3 - Los 450 profetas de Baal (1 R 18:26-29)
// =============================================================================
// La cabeza (sumo sacerdote) es INTOCABLE mientras el corro tenga 3 o mas de pie: hay que
// ralearlo antes de que la cabeza lo rellene (uno cada 6 s; 4 s en el frenesi). Cada 11 s
// (8 en el frenesi) el corro se para y CLAMA: un gran circulo de fuego en el centro del
// altar y, al estallar, una onda con un hueco. Durante el ritual el corro esta quieto y no
// dispara: es el mejor momento para ralearlo. Al 50% el frenesi: "se sajaban con cuchillos"
// (18:28) — mas rapidos, y se hieren solos.
enum : u8 { BAAL_DANCE = 0, BAAL_RITUAL };

static void update_baal_prophet(ECSWorld& world, Entity e, Vec2 player_pos, Vec2 to_player,
                                f32 dist, f32 dmg, f32 dt) {
    auto& ai = world.enemy_ais[e];
    auto& movement = world.movements[e];
    auto& health = world.healths[e];
    const bool frenzy = health.ratio() < 0.5f;
    if (frenzy && ai.phase == 0) {
        ai.phase = 1;
        movement.speed *= 1.35f;
        spawn_particles(world, world.transforms[e].position, {200, 40, 40, 255}, 12, 80.0f, 0.5f);
    }
    if (frenzy) health.current = std::max(1.0f, health.current - health.max * 0.012f * dt);

    // El ritual: el corro se para, mira al altar y clama. No dispara.
    const bool ritual = alive_entity(world, ai.owner) && world.enemy_ais[ai.owner].boss_state == BAAL_RITUAL;
    if (ritual) {
        movement.velocity = {0, 0};
        face(world, e, ai.ring_center - world.transforms[e].position);
        if (randf() < 3.0f * dt)
            spawn_particles(world, world.transforms[e].position - Vec2{0, 10}, {230, 80, 60, 255}, 1, 20.0f, 0.6f);
        return;
    }

    if (player_pos.distance_to(ai.ring_center) > BAAL_RING_LEASH) {
        movement.velocity = (dist > 40.0f) ? to_player * movement.speed : Vec2{0, 0};
    } else {
        const f32 omega = frenzy ? 1.4f : 0.9f;
        const f32 a = ai.ring_phase + world.time * omega;
        const f32 r = BAAL_RING_RADIUS + 8.0f * std::sin(world.time * 3.0f + ai.ring_phase);
        Vec2 target = ai.ring_center + Vec2{std::cos(a), std::sin(a)} * r;
        Vec2 d = target - world.transforms[e].position;
        f32 l = d.length();
        movement.velocity = (l > 1.0f) ? d * (std::min(movement.speed, l * 4.0f) / l) : Vec2{0, 0};
    }
    if (dist < ai.attack_range * 1.5f && ai.shoot_timer <= 0.0f &&
        enemy_has_los(world, world.transforms[e].position, player_pos)) {
        ai.shoot_timer = frenzy ? 1.8f : 2.6f;
        enemy_shoot(world, e, to_player, ProjectileKind::Darkbolt, dmg, 120.0f, 260.0f);
    }
}

static void boss_baal_head(ECSWorld& w, Entity e, Vec2 to_player, f32 dist, f32 dmg, f32 dt) {
    auto& ai = w.enemy_ais[e];
    auto& m = w.movements[e];
    auto& h = w.healths[e];
    const f32 hit = hit_unit(ai.chapter);
    const bool frenzy = h.ratio() < 0.5f;
    if (frenzy && ai.phase == 0) {
        enter_phase(w, e, 1, "frenesi: se sajaban con cuchillos", {200, 40, 40, 255}, 0.0f);
        m.speed *= 1.35f;
    }
    if (frenzy) h.current = std::max(1.0f, h.current - h.max * 0.012f * dt);   // se hieren solos

    const int ring_alive = count_boss_group(w);
    h.invulnerable = ring_alive >= BAAL_SHIELD_MIN;

    // Repone el corro desde el pueblo (los 450): UNO cada 6 s, al hueco del caido
    if (ai.special_timer <= 0.0f) {
        ai.special_timer = frenzy ? 4.0f : 6.0f;
        const int slot = free_ring_slot(w);
        if (slot >= 0 && ring_alive < BAAL_RING_SLOTS) {
            Entity mm = spawn_baal_prophet(w, e, ai.ring_center, slot, ai.chapter);
            if (mm != ENTITY_NONE) {
                w.enemy_ais[mm].home_cell = ai.home_cell;   // cuenta en la celda del jefe
                spawn_particles(w, w.transforms[mm].position, {200, 120, 255, 255}, 8, 70.0f, 0.3f);
            }
        }
    }

    if (ai.boss_state == BAAL_RITUAL) {
        m.velocity = {0, 0};
        ai.state_timer -= dt;
        if (ai.state_timer <= 0.0f) {
            // El fuego no baja... y la onda sale del altar con un hueco
            const f32 gap = randf_range(0.0f, 2.0f * PI);
            spawn_hazard(w, hz_ring(e, ai.ring_center, 30.0f, 175.0f, 10.0f, 0.2f, 1.5f, hit * 0.9f,
                                    from_angle(gap), 0.5f, {225, 80, 200, 255}));
            ai.boss_state = BAAL_DANCE;
            ai.aux_timer = frenzy ? 8.0f : 11.0f;
        }
        return;
    }

    ai.aux_timer -= dt;
    if (ai.aux_timer <= 0.0f && ring_alive >= 2) {
        ai.boss_state = BAAL_RITUAL;
        ai.state_timer = 2.3f * wk();
        Hazard c = hz_circle(e, ai.ring_center + Vec2{0, 6}, 54.0f, 2.2f * wk(), hit * 1.4f,
                             {235, 70, 55, 255}, HazardFx::Fire);
        c.tag = HazardTag::Ritual;
        spawn_hazard(w, c);
        SDL_Log("Boss %s: ritual del corro (%d clamando)", ai.name, ring_alive);
        play(w, e, AnimID::CastMiracle);
        return;
    }
    if (ai.aux_timer <= 0.0f) ai.aux_timer = 2.0f;   // sin corro no hay ritual: lo reintenta

    m.velocity = (dist > 60.0f) ? to_player * m.speed : Vec2{0, 0};
    if (ai.shoot_timer <= 0.0f) {
        ai.shoot_timer = frenzy ? 1.6f : 3.0f;
        enemy_ring(w, e, to_player, frenzy ? 10 : 6, ProjectileKind::Darkbolt, dmg * 0.45f, 120.0f, 260.0f);
    }
}

// =============================================================================
// Cap. 4 - Tu Propia Desesperacion (1 R 19:11-12)
// =============================================================================
// Cuatro fases por vida, cada una anunciada con un fogonazo, una sacudida y una corona de
// particulas de su color (sin banner): VIENTO (rafagas que cruzan la arena entera + ondas
// con hueco), TERREMOTO (ondas de suelo con hueco + grietas que salen de el), FUEGO (lineas
// de circulos que MARCHAN hacia el profeta) y el SILBO APACIBLE: deja de atacar y recibe
// dano doble. "Jehova no estaba en el viento... ni en el terremoto... ni en el fuego."
// Como se gana: buscar el hueco de cada onda (o dashearla), no pelear contra el viento, y
// en el silencio, no dejar de orar... es decir, de disparar.
enum : u8 { DES_FLOAT = 0, DES_SHIFT };

static void boss_despair(ECSWorld& w, Entity e, Vec2 player_pos, Vec2 to_player, f32 dist, f32 dt) {
    auto& ai = w.enemy_ais[e];
    auto& m = w.movements[e];
    auto& h = w.healths[e];
    const f32 hit = hit_unit(ai.chapter);
    const f32 ratio = h.ratio();
    const u8 want = (ratio > 0.75f) ? 0 : (ratio > 0.50f) ? 1 : (ratio > 0.25f) ? 2 : 3;
    w.sprites[e].tint = base_tint(w, e);
    if (want > ai.phase) {
        static const char* names[4] = {"viento", "terremoto", "fuego", "el silbo apacible"};
        static const Color cols[4] = {{200, 222, 255, 255}, {214, 176, 120, 255},
                                      {255, 140, 60, 255}, {255, 255, 255, 255}};
        enter_phase(w, e, want, names[want], cols[want], want == 3 ? 0.55f : 0.3f);
        clear_hazards_of(w, e);   // cada fase limpia la anterior: se lee el cambio
        ai.boss_state = DES_SHIFT;
        ai.state_timer = 1.2f;
        ai.aux_timer = 1.6f;
        ai.special_timer = 2.5f;
    }
    if (ai.boss_state == DES_SHIFT) {
        // El cambio de fase es un respiro: quieto, intocable, temblando
        h.invulnerable = true;
        m.velocity = {0, 0};
        ai.state_timer -= dt;
        if (ai.state_timer <= 0.0f) ai.boss_state = DES_FLOAT;
        return;
    }

    if (ai.phase == 3) {
        // El silbo: ya no es un enemigo, es un silencio que hay que cruzar
        m.velocity = m.velocity * 0.9f;
        ai.vuln_mult = 2.0f;
        w.sprites[e].tint = {235, 240, 255, 200};
        if (randf() < 4.0f * dt)
            spawn_particles(w, w.transforms[e].position + Vec2{randf_range(-10.0f, 10.0f), -8.0f},
                            {255, 255, 255, 255}, 1, 15.0f, 0.9f);
        return;
    }

    keep_range(w, e, to_player, dist, 60.0f, 110.0f, 0.35f);
    m.velocity = m.velocity * (ai.phase == 1 ? 0.6f : 1.0f);
    const Rect a = arena_of(w, e);
    const Vec2 g = ground_of(w, e);
    ai.aux_timer -= dt;

    if (ai.phase == 0) {
        // VIENTO: una rafaga cruza la arena entera hacia un lado (nunca dos veces el mismo)
        if (ai.special_timer <= 0.0f) {
            ai.special_timer = 6.5f;
            static const Vec2 dirs[4] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
            ai.counter = (ai.counter + 1 + randi_range(0, 2)) % 4;
            const Vec2 d = dirs[ai.counter];
            const bool horiz = d.y == 0.0f;
            const f32 len = (horiz ? a.w : a.h) + 24.0f, wid = (horiz ? a.h : a.w) + 24.0f;
            const Vec2 from = a.center() - d * (len * 0.5f);
            spawn_hazard(w, hz_push(e, from, d, len, wid, 1.0f * wk(), 2.8f, 62.0f * sk(), {200, 222, 255, 255}));
        }
        if (ai.aux_timer <= 0.0f) {
            ai.aux_timer = 3.2f;
            spawn_hazard(w, hz_ring(e, g, 10.0f, 185.0f, 9.0f, 0.6f * wk(), 1.9f / sk(), hit * 0.8f,
                                    from_angle(randf_range(0.0f, 2.0f * PI)), 0.45f, {196, 220, 255, 255}));
        }
    } else if (ai.phase == 1) {
        // TERREMOTO: la tierra se abre en ondas con hueco y en grietas desde sus pies
        if (ai.aux_timer <= 0.0f) {
            ai.aux_timer = 2.8f;
            spawn_hazard(w, hz_ring(e, g, 12.0f, 190.0f, 12.0f, 0.7f * wk(), 1.7f / sk(), hit * 0.9f,
                                    from_angle(randf_range(0.0f, 2.0f * PI)), 0.48f, {214, 172, 112, 255}));
            w.shake_request = std::max(w.shake_request, 1.5f);
        }
        if (ai.special_timer <= 0.0f) {
            ai.special_timer = 5.5f;
            const f32 base = angle_of(to_player);
            for (int i = -2; i <= 2; i++)
                spawn_hazard(w, hz_lane(e, g, from_angle(base + i * 0.5f), 160.0f, 12.0f, 1.0f * wk(), 0.18f,
                                        hit, {205, 150, 90, 255}, HazardFx::Dust));
        }
    } else {
        // FUEGO: tres lineas de circulos que avanzan hacia el profeta, uno tras otro
        if (ai.aux_timer <= 0.0f) {
            ai.aux_timer = 3.0f;
            const f32 base = angle_of(to_player);
            for (int l = -1; l <= 1; l++) {
                const Vec2 d = from_angle(base + l * 0.38f);
                for (int i = 0; i < 7; i++) {
                    const Vec2 at = g + d * (26.0f + 22.0f * i);
                    if (!a.contains(at)) break;
                    spawn_hazard(w, hz_circle(e, at, 13.0f, (0.55f + 0.13f * i) * wk(), hit * 0.9f,
                                              {255, 120, 40, 255}, (i % 2 == 0) ? HazardFx::Fire : HazardFx::None));
                }
            }
            play(w, e, AnimID::CastMiracle);
        }
    }
}

// =============================================================================
// Cap. 5 - Acab y Jezabel (1 R 21:20-23): el duo
// =============================================================================
// DOS entidades con barra compartida. ACAB es el bruto: barridos de espada en abanico
// (0,6 s de aviso) cuando el profeta esta cerca —tras cada uno queda abierto 0,7 s— y una
// carga corta. JEZABEL es la que manda: guarda distancia, se teletransporta lejos si el
// profeta se le acerca (deja una maldicion donde estaba), siembra circulos malditos, tira
// rayos y cada ~8 s CURA a Acab con una atadura que se CORTA si ella recibe dano. Cuando
// cae uno, el otro se enfurece (mas rapido, mas seguido).
// Como se gana: cortar cada curacion (es ella quien se queda quieta canalizando), o
// tumbarla a ella primero y luego aguantar a un Acab enfurecido.
enum : u8 { ACAB_CHASE = 0, ACAB_SWEEP, ACAB_OPEN, ACAB_AIM, ACAB_CHARGE };
enum : u8 { JEZ_MOVE = 0, JEZ_HEAL };

static void boss_acab(ECSWorld& w, Entity e, Vec2 to_player, f32 dist, f32 dt) {
    auto& ai = w.enemy_ais[e];
    auto& m = w.movements[e];
    const f32 hit = hit_unit(ai.chapter);
    if (!alive_entity(w, ai.partner) && ai.phase == 0) {
        enter_phase(w, e, 1, "enfurecido: ha caido Jezabel", {255, 70, 50, 255}, 0.0f);
        m.speed *= 1.3f;
    }
    const f32 cad = ai.phase ? 0.65f : 1.0f;
    w.sprites[e].tint = base_tint(w, e);

    switch (ai.boss_state) {
    case ACAB_CHASE:
        ai.vuln_mult = 1.0f;
        steer(w, e, to_player, dist > 18.0f ? m.speed : 0.0f);
        if (dist < 52.0f && ai.special_timer <= 0.0f) {
            ai.act_dir = to_player;
            spawn_hazard(w, hz_cone(e, ground_of(w, e), to_player, 58.0f, 0.96f, 0.6f * wk(), hit * 1.3f,
                                    {255, 92, 70, 255}));
            ai.boss_state = ACAB_SWEEP;
            ai.state_timer = 0.6f * wk() + 0.12f;
            m.velocity = {0, 0};
            face(w, e, to_player);
        } else if (ai.leap_timer <= 0.0f && dist > 60.0f && dist < 170.0f) {
            const f32 len = wall_distance(w, collider_feet(w, e), to_player, 150.0f);
            if (len > 40.0f) {
                ai.act_dir = to_player;
                ai.act_pos = collider_feet(w, e) + to_player * len;
                Hazard hz = hz_lane(e, ground_of(w, e), to_player, len + 6.0f, 22.0f, 0.7f * wk(),
                                    len / (240.0f * sk()) + 0.1f, 0.0f, {255, 110, 80, 255});
                hz.hits_player = false;
                spawn_hazard(w, hz);
                ai.boss_state = ACAB_AIM;
                ai.state_timer = 0.7f * wk();
                m.velocity = {0, 0};
                face(w, e, to_player);
            } else {
                ai.leap_timer = 1.0f;
            }
        }
        break;
    case ACAB_SWEEP:
        m.velocity = {0, 0};
        ai.state_timer -= dt;
        if (ai.state_timer <= 0.12f && ai.counter == 0) {
            ai.counter = 1;
            play(w, e, facing_anim(AnimID::AttackDown, w.transforms[e].facing));
            spawn_fx(w, ground_of(w, e) + ai.act_dir * 26.0f, FxID::Sparks, {255, 220, 190, 230}, 40.0f, 0.25f);
        }
        if (ai.state_timer <= 0.0f) {
            ai.counter = 0;
            ai.boss_state = ACAB_OPEN;
            ai.state_timer = 0.7f;
            ai.special_timer = 2.2f * cad;
        }
        break;
    case ACAB_OPEN:
        // Tras el tajo queda abierto (el aro dorado del suelo lo dice)
        m.velocity = {0, 0};
        ai.vuln_mult = 1.25f;
        ai.state_timer -= dt;
        if (ai.state_timer <= 0.0f) { ai.boss_state = ACAB_CHASE; ai.vuln_mult = 1.0f; }
        break;
    case ACAB_AIM:
        m.velocity = {0, 0};
        ai.state_timer -= dt;
        if (ai.state_timer <= 0.0f) {
            ai.boss_state = ACAB_CHARGE;
            ai.state_timer = 150.0f / (240.0f * sk()) + 0.3f;
            play(w, e, AnimID::Dash);
        }
        break;
    case ACAB_CHARGE: {
        const Vec2 feet = collider_feet(w, e);
        const bool blocked = !walkable(w, feet + ai.act_dir * 9.0f);
        const bool arrived = (ai.act_pos - feet).dot(ai.act_dir) <= 0.0f;
        ai.state_timer -= dt;
        if (blocked || arrived || ai.state_timer <= 0.0f) {
            m.velocity = {0, 0};
            ai.boss_state = ACAB_OPEN;
            ai.state_timer = 0.6f;
            ai.leap_timer = 6.0f * cad;
        } else {
            m.velocity = ai.act_dir * 240.0f * sk();
            m.knockback = {0, 0};
        }
        break;
    }
    default: ai.boss_state = ACAB_CHASE; break;
    }
}

static void boss_jezabel(ECSWorld& w, Entity e, Vec2 player_pos, Vec2 to_player, f32 dist, f32 dmg, f32 dt) {
    auto& ai = w.enemy_ais[e];
    auto& m = w.movements[e];
    auto& h = w.healths[e];
    const f32 hit = hit_unit(ai.chapter);
    const bool acab_ok = alive_entity(w, ai.partner);
    if (!acab_ok && ai.phase == 0) {
        enter_phase(w, e, 1, "enfurecida: ha caido Acab", {200, 80, 255, 255}, 0.0f);
        m.speed *= 1.3f;
    }
    const f32 cad = ai.phase ? 0.65f : 1.0f;
    ai.special_timer -= dt;
    ai.aux_timer -= dt;
    ai.aux2_timer -= dt;
    w.sprites[e].tint = base_tint(w, e);

    if (ai.boss_state == JEZ_HEAL) {
        m.velocity = {0, 0};
        ai.vuln_mult = 1.25f;
        if (!acab_ok) { ai.boss_state = JEZ_MOVE; ai.vuln_mult = 1.0f; return; }
        if (h.current < ai.hp_mark - 0.5f) {
            // Cortada: un golpe a la reina rompe la atadura
            ai.boss_state = JEZ_MOVE;
            ai.vuln_mult = 1.0f;
            ai.aux2_timer = 8.0f * cad;
            spawn_particles(w, w.transforms[e].position, {200, 255, 170, 255}, 14, 90.0f, 0.4f);
            spawn_fx(w, w.transforms[e].position, FxID::Sparks, {210, 255, 190, 230}, 30.0f, 0.3f);
            SDL_Log("Boss Jezabel: curacion cortada");
            return;
        }
        ai.state_timer -= dt;
        if (ai.state_timer <= 0.0f) {
            auto& ah = w.healths[ai.partner];
            const f32 amount = ah.max * 0.10f;
            ah.heal(amount);
            spawn_fx(w, w.transforms[ai.partner].position, FxID::Heal, {190, 255, 170, 235}, 44.0f, 0.6f, 11);
            SDL_Log("Boss Jezabel cura a Acab +%.0f (%.0f/%.0f)", amount, ah.current, ah.max);
            ai.boss_state = JEZ_MOVE;
            ai.vuln_mult = 1.0f;
            ai.aux2_timer = 8.0f * cad;
        }
        return;
    }

    // Se teletransporta lejos si el profeta se le echa encima (y deja una maldicion)
    if (dist < 46.0f && ai.aux_timer <= 0.0f) {
        ai.aux_timer = 3.5f * cad;
        const Vec2 from = w.transforms[e].position;
        spawn_hazard(w, hz_circle(e, ground_of(w, e), 18.0f, 0.6f * wk(), hit, {170, 70, 220, 255}, HazardFx::Dark));
        const Rect a = stage_of(w, e);
        Vec2 best = from;
        f32 bd = -1.0f;
        for (int i = 0; i < 8; i++) {
            const Vec2 c = clamp_in(a, a.center() + Vec2{std::cos(i * 0.785f) * a.w * 0.5f,
                                                         std::sin(i * 0.785f) * a.h * 0.5f}, 0.0f);
            if (!walkable(w, c + Vec2{0, 8})) continue;
            const f32 d = c.distance_to(player_pos);
            if (d > bd) { bd = d; best = c; }
        }
        spawn_particles(w, from, {190, 90, 240, 255}, 14, 90.0f, 0.4f);
        w.transforms[e].position = best;
        spawn_particles(w, best, {190, 90, 240, 255}, 14, 90.0f, 0.4f);
        spawn_fx(w, best, FxID::Smoke, {170, 110, 210, 200}, 34.0f, 0.5f, 7);
        SDL_Log("Boss Jezabel se teletransporta a (%.0f,%.0f)", best.x, best.y);
        return;
    }
    keep_range(w, e, to_player, dist, 95.0f, 145.0f, 0.5f);

    if (ai.aux2_timer <= 0.0f && acab_ok && w.healths[ai.partner].current < w.healths[ai.partner].max * 0.98f) {
        ai.boss_state = JEZ_HEAL;
        ai.state_timer = 2.0f;
        ai.hp_mark = h.current;
        play(w, e, AnimID::CastMiracle);
        SDL_Log("Boss Jezabel canaliza la curacion de Acab (cortala golpeandola)");
        return;
    }
    if (ai.aux2_timer <= 0.0f) ai.aux2_timer = 1.5f;

    if (ai.special_timer <= 0.0f) {
        ai.special_timer = 5.0f * cad;
        const int n = ai.phase ? 4 : 2;
        const Rect a = arena_of(w, e);
        const Vec2 pg = player_pos + Vec2{0, 9};
        for (int i = 0; i < n; i++) {
            Vec2 at = (i == 0) ? pg : pg + from_angle(randf_range(0.0f, 2.0f * PI)) * randf_range(28.0f, 60.0f);
            spawn_hazard(w, hz_circle(e, clamp_in(a, at, 10.0f), 17.0f, 1.1f * wk(), hit,
                                      {170, 70, 220, 255}, HazardFx::Dark));
        }
        play(w, e, AnimID::CastMiracle);
    }
    if (ai.shoot_timer <= 0.0f && enemy_has_los(w, w.transforms[e].position, player_pos)) {
        ai.shoot_timer = 2.4f * cad;
        enemy_volley(w, e, to_player, 3, 0.22f, ProjectileKind::Darkbolt, dmg * 0.55f, 130.0f, 280.0f);
    }
}

// =============================================================================
// Cap. 6 - El Espiritu de Corrupcion (2 R 2:19-22)
// =============================================================================
// Deja CHARCAS envenenadas por donde pasa (8 s de vida). Cada ~9 s se SUMERGE: intocable,
// se vuelve una sombra con ondas que avanza hacia el profeta, y EMERGE donde estaba el
// profeta (circulo de aviso de 0,8 s) con una onda con hueco. Al salir queda 1,1 s
// expuesto. Se parte al 66% y al 33% (dos Aguas Corruptas cada vez). Sanar Aguas
// (2 R 2:21) disuelve las charcas que toca.
// Como se gana: no quedarse quieto cuando se sumerge, castigar la salida, y limpiar el
// suelo con Sanar Aguas en vez de pelear sobre veneno.
enum : u8 { SPI_MOVE = 0, SPI_SINK, SPI_UNDER, SPI_RISE, SPI_OPEN };

static void boss_spirit(ECSWorld& w, Entity e, Vec2 player_pos, Vec2 to_player, f32 dist, f32 dmg, f32 dt) {
    auto& ai = w.enemy_ais[e];
    auto& m = w.movements[e];
    auto& h = w.healths[e];
    const f32 hit = hit_unit(ai.chapter);
    const u8 want = (h.ratio() > 0.66f) ? 0 : (h.ratio() > 0.33f) ? 1 : 2;
    if (want > ai.phase) {
        enter_phase(w, e, want, want == 1 ? "se parte (66%)" : "se parte (33%)", {90, 200, 190, 255}, 0.0f);
        Entity s1 = summon_at(w, e, EnemyType::AguaCorrupta, w.transforms[e].position + Vec2{-22, 6}, ai.chapter + 1, 10);
        Entity s2 = summon_at(w, e, EnemyType::AguaCorrupta, w.transforms[e].position + Vec2{22, 6}, ai.chapter + 1, 10);
        (void)s1; (void)s2;
        w.transforms[e].scale = w.transforms[e].scale * 0.92f;
    }
    ai.aux_timer -= dt;
    w.sprites[e].tint = base_tint(w, e);
    if (ai.counter == 0 && ai.boss_state == SPI_MOVE && ai.fight_time < 0.1f) {
        // Al abrir la pelea: charcas primero, la primera inmersion a los 6 s
        ai.aux_timer = 0.6f;
        ai.special_timer = 6.0f;
    }

    switch (ai.boss_state) {
    case SPI_MOVE:
        w.sprites[e].visible = true;
        ai.vuln_mult = 1.0f;
        steer(w, e, to_player, dist > 30.0f ? m.speed * 0.85f : 0.0f);
        if (ai.aux_timer <= 0.0f) {
            ai.aux_timer = (ai.phase >= 1) ? 1.0f : 1.3f;
            if (count_hazards_of(w, e, HazardKind::Pool) < 12)
                spawn_hazard(w, hz_pool(e, ground_of(w, e), 15.0f, 8.0f, hit * 0.55f, {60, 200, 165, 255}));
        }
        if (ai.shoot_timer <= 0.0f) {
            ai.shoot_timer = 3.4f;
            enemy_ring(w, e, to_player, 7, ProjectileKind::Darkbolt, dmg * 0.45f, 115.0f, 250.0f);
        }
        if (ai.special_timer <= 0.0f) {
            ai.boss_state = SPI_SINK;
            ai.state_timer = 0.45f;
            m.velocity = {0, 0};
            spawn_fx(w, ground_of(w, e), FxID::Ring, {120, 220, 210, 220}, 46.0f, 0.45f, 7);
            spawn_particles(w, ground_of(w, e), {120, 220, 210, 255}, 14, 80.0f, 0.45f);
        }
        break;
    case SPI_SINK:
        m.velocity = {0, 0};
        ai.state_timer -= dt;
        w.sprites[e].tint.a = static_cast<u8>(255.0f * std::max(0.0f, ai.state_timer / 0.45f));
        if (ai.state_timer <= 0.0f) {
            ai.boss_state = SPI_UNDER;
            ai.state_timer = 2.0f;
            w.sprites[e].visible = false;
            w.colliders[e].is_trigger = true;   // bajo el agua no empuja a nadie
            Hazard r = hz_pool(e, ground_of(w, e), 18.0f, 3.6f, 0.0f, {70, 170, 160, 255});
            r.tag = HazardTag::Ripple;
            r.follow_owner = true;
            r.hits_player = false;
            r.warn = 0.2f;
            ai.counter = spawn_hazard(w, r);
            SDL_Log("Boss %s se sumerge", ai.name);
        }
        break;
    case SPI_UNDER:
        h.invulnerable = true;
        w.sprites[e].visible = false;
        if (w.has(e, Component::Combat)) w.combats[e].attack_timer = 0.3f;   // bajo el agua no muerde
        steer(w, e, to_player, dist > 6.0f ? 100.0f * sk() : 0.0f);
        ai.state_timer -= dt;
        if (ai.state_timer <= 0.0f) {
            ai.boss_state = SPI_RISE;
            ai.state_timer = 0.8f * wk();
            ai.act_pos = clamp_in(arena_of(w, e), player_pos + Vec2{0, 9}, 14.0f);
            spawn_hazard(w, hz_circle(e, ai.act_pos, 24.0f, 0.8f * wk(), hit * 1.1f, {80, 210, 190, 255}, HazardFx::Water));
        }
        break;
    case SPI_RISE: {
        h.invulnerable = true;
        w.sprites[e].visible = false;
        if (w.has(e, Component::Combat)) w.combats[e].attack_timer = 0.3f;
        // Sube justo donde se pinto: la sombra se desliza hasta el circulo
        const Vec2 to = ai.act_pos - ground_of(w, e);
        steer(w, e, to.normalized(), std::min(to.length() * 6.0f, 220.0f));
        ai.state_timer -= dt;
        if (ai.state_timer <= 0.0f) {
            w.transforms[e].position = ai.act_pos - (ground_of(w, e) - w.transforms[e].position);
            w.sprites[e].visible = true;
            w.colliders[e].is_trigger = false;
            m.velocity = {0, 0};
            if (ai.counter >= 0 && ai.counter < MAX_HAZARDS && w.hazards[ai.counter].used &&
                w.hazards[ai.counter].tag == HazardTag::Ripple && w.hazards[ai.counter].owner == e)
                w.hazards[ai.counter] = Hazard{};
            spawn_hazard(w, hz_ring(e, ai.act_pos, 14.0f, 120.0f, 10.0f, 0.1f, 1.0f / sk(), hit * 0.8f,
                                    from_angle(randf_range(0.0f, 2.0f * PI)), 0.5f, {90, 215, 195, 255}));
            spawn_particles(w, ai.act_pos, {150, 235, 220, 255}, 18, 110.0f, 0.5f);
            ai.boss_state = SPI_OPEN;
            ai.state_timer = 1.1f;
            SDL_Log("Boss %s emerge en (%.0f,%.0f): expuesto", ai.name, ai.act_pos.x, ai.act_pos.y);
        }
        break;
    }
    case SPI_OPEN:
        m.velocity = {0, 0};
        ai.vuln_mult = 1.35f;
        ai.state_timer -= dt;
        if (ai.state_timer <= 0.0f) {
            ai.boss_state = SPI_MOVE;
            ai.vuln_mult = 1.0f;
            ai.special_timer = 9.0f;
        }
        break;
    default: ai.boss_state = SPI_MOVE; break;
    }
}

// =============================================================================
// Cap. 7 - El Acreedor (2 R 4:1-7)
// =============================================================================
// Lanza una CADENA en carril (0,7 s de aviso): si te alcanza, TIRA de ti hacia el y te
// cobra 6 de fe (y el se cura con lo cobrado). Tras cada lanzamiento se queda 1,2 s
// recogiendola: abierto, dano x1,5. Abanicos de MONEDAS. Al 50%, LA DEUDA: un anillo de
// monedas se cierra sobre el profeta; hay que salir antes de que se cierre. De vez en
// cuando llama a dos cobradores (soldados sirios).
// Como se gana: apartarse del carril (o dashearlo) y castigar mientras recoge la cadena.
enum : u8 { CRE_MOVE = 0, CRE_THROW, CRE_REEL };

static void boss_creditor(ECSWorld& w, Entity e, Vec2 player_pos, Vec2 to_player, f32 dist, f32 dmg, f32 dt) {
    auto& ai = w.enemy_ais[e];
    auto& m = w.movements[e];
    const f32 hit = hit_unit(ai.chapter);
    if (w.healths[e].ratio() < 0.5f && ai.phase == 0) {
        enter_phase(w, e, 1, "la deuda", {255, 210, 100, 255}, 0.0f);
        ai.aux_timer = 1.5f;
    }
    ai.aux_timer -= dt;
    ai.aux2_timer -= dt;
    w.sprites[e].tint = base_tint(w, e);

    // Cobradores (aunque este recogiendo la cadena)
    if (ai.aux2_timer <= 0.0f) {
        ai.aux2_timer = 14.0f;
        const Rect a = arena_of(w, e);
        summon_at(w, e, EnemyType::SoldadoSirio, {a.x + 22.0f, a.bottom() - 30.0f}, ai.chapter, 3);
        summon_at(w, e, EnemyType::SoldadoSirio, {a.right() - 22.0f, a.bottom() - 30.0f}, ai.chapter, 3);
    }
    // La deuda: se cierra sobre donde estas
    if (ai.phase > 0 && ai.aux_timer <= 0.0f) {
        ai.aux_timer = 10.0f;
        Hazard d = hz_ring(e, player_pos + Vec2{0, 9}, 66.0f, 0.0f, 9.0f, 0.7f * wk(), 2.6f / sk(), hit,
                           {0, 1}, 0.0f, {255, 205, 90, 255});
        d.tag = HazardTag::Debt;
        d.faith_drain = 5.0f;
        spawn_hazard(w, d);
        SDL_Log("Boss %s: la deuda se cierra", ai.name);
    }

    switch (ai.boss_state) {
    case CRE_MOVE:
        ai.vuln_mult = 1.0f;
        keep_range(w, e, to_player, dist, 70.0f, 120.0f, 0.4f);
        if (ai.special_timer <= 0.0f && dist < 200.0f) {
            const f32 len = std::max(60.0f, wall_distance(w, ground_of(w, e), to_player, 190.0f));
            ai.act_dir = to_player;
            ai.act_pos = ground_of(w, e) + to_player * len;
            Hazard c = hz_lane(e, ground_of(w, e), to_player, len, 14.0f, 0.7f * wk(), 0.14f, hit * 0.6f,
                               {240, 205, 110, 255}, HazardFx::Coin);
            c.tag = HazardTag::Chain;
            c.faith_drain = 6.0f;
            spawn_hazard(w, c);
            ai.boss_state = CRE_THROW;
            ai.state_timer = 0.7f * wk() + 0.14f;
            m.velocity = {0, 0};
            face(w, e, to_player);
            play(w, e, facing_anim(AnimID::AttackDown, w.transforms[e].facing));
            break;
        }
        if (ai.shoot_timer <= 0.0f && enemy_has_los(w, w.transforms[e].position, player_pos)) {
            ai.shoot_timer = 2.8f;
            const int n = ai.phase ? 7 : 5;
            const f32 base = angle_of(to_player);
            for (int i = 0; i < n; i++)
                shoot_coin(w, e, from_angle(base + (i - (n - 1) * 0.5f) * 0.22f), dmg * 0.45f, 150.0f);
        }
        break;
    case CRE_THROW:
        m.velocity = {0, 0};
        ai.state_timer -= dt;
        if (ai.state_timer <= 0.0f) {
            ai.boss_state = CRE_REEL;
            ai.state_timer = 1.2f;
        }
        break;
    case CRE_REEL:
        // Recoge la cadena: abierto
        m.velocity = {0, 0};
        ai.vuln_mult = 1.5f;
        ai.state_timer -= dt;
        if (ai.state_timer <= 0.0f) {
            ai.boss_state = CRE_MOVE;
            ai.vuln_mult = 1.0f;
            ai.special_timer = 4.2f;
        }
        break;
    default: ai.boss_state = CRE_MOVE; break;
    }
}

// =============================================================================
// Cap. 8 - La Muerte (2 R 4:32-35)
// =============================================================================
// Apaga la luz: la arena queda a oscuras salvo un circulo alrededor del profeta (el
// Engine la pinta con `ECSWorld::darkness_req`). Intocable de lejos: hay que tenderse
// sobre el nino (4:34) — solo cede a menos de 40 unidades. Si el profeta se acerca, BARRE
// con la guadana (abanico de 140 grados, 0,65 s de aviso): dashear a traves o retirarse.
// Tras el barrido queda SIN RESUELLO 1,6 s: tocable desde cualquier distancia. Invoca
// idolatras (las sombras con brasero) y, si el profeta huye lejos, lanza anillos lentos.
// Al 50% la luz se estrecha y la guadana vuelve sobre si misma (doble barrido).
enum : u8 { DEA_DRIFT = 0, DEA_SWEEP, DEA_SPENT };
static constexpr f32 DEATH_CLOSE = 40.0f;

static void boss_death(ECSWorld& w, Entity e, Vec2 to_player, f32 dist, f32 dmg, f32 dt) {
    auto& ai = w.enemy_ais[e];
    auto& m = w.movements[e];
    auto& h = w.healths[e];
    const f32 hit = hit_unit(ai.chapter);
    if (h.ratio() < 0.5f && ai.phase == 0)
        enter_phase(w, e, 1, "la luz se estrecha", {170, 170, 220, 255}, 0.0f);
    w.darkness_req = ai.phase ? 60.0f : 76.0f;
    ai.aux_timer -= dt;
    w.sprites[e].tint = base_tint(w, e);

    switch (ai.boss_state) {
    case DEA_DRIFT:
        ai.vuln_mult = 1.0f;
        h.invulnerable = dist >= DEATH_CLOSE;
        steer(w, e, to_player, dist > 22.0f ? m.speed * 0.75f : 0.0f);
        if (dist < 60.0f && ai.special_timer <= 0.0f) {
            ai.act_dir = to_player;
            spawn_hazard(w, hz_cone(e, ground_of(w, e), to_player, 66.0f, 1.22f, 0.65f * wk(), hit * 1.4f,
                                    {205, 215, 255, 255}));
            ai.boss_state = DEA_SWEEP;
            ai.state_timer = 0.65f * wk() + 0.12f;
            ai.counter = (ai.phase > 0) ? 1 : 0;   // al 50%: vuelve con un segundo tajo
            m.velocity = {0, 0};
            face(w, e, to_player);
            break;
        }
        if (dist > 95.0f && ai.shoot_timer <= 0.0f) {
            ai.shoot_timer = 3.0f;
            enemy_ring(w, e, to_player, 8, ProjectileKind::Darkbolt, dmg * 0.5f, 90.0f, 300.0f);
        }
        if (ai.aux_timer <= 0.0f) {
            ai.aux_timer = 9.0f;
            const Rect a = arena_of(w, e);
            summon_at(w, e, EnemyType::IdolatraDeRimon, {a.x + 24.0f + randf_range(0.0f, a.w - 48.0f), a.y + 20.0f},
                      ai.chapter, 3);
        }
        break;
    case DEA_SWEEP:
        m.velocity = {0, 0};
        h.invulnerable = dist >= DEATH_CLOSE;
        ai.state_timer -= dt;
        if (ai.state_timer <= 0.0f) {
            play(w, e, facing_anim(AnimID::AttackDown, w.transforms[e].facing));
            spawn_fx(w, ground_of(w, e) + ai.act_dir * 30.0f, FxID::Sparks, {210, 220, 255, 230}, 50.0f, 0.3f);
            if (ai.counter > 0) {
                // La guadana vuelve: segundo abanico hacia donde esta ahora el profeta
                ai.counter = 0;
                ai.act_dir = to_player;
                spawn_hazard(w, hz_cone(e, ground_of(w, e), to_player, 66.0f, 1.22f, 0.45f * wk(), hit * 1.4f,
                                        {205, 215, 255, 255}));
                ai.state_timer = 0.45f * wk() + 0.12f;
                face(w, e, to_player);
            } else {
                ai.boss_state = DEA_SPENT;
                ai.state_timer = 1.6f;
                SDL_Log("Boss %s sin resuello tras la guadana (ventana)", ai.name);
            }
        }
        break;
    case DEA_SPENT:
        m.velocity = {0, 0};
        h.invulnerable = false;
        ai.vuln_mult = 1.25f;
        if (randf() < 6.0f * dt)
            spawn_particles(w, w.transforms[e].position - Vec2{0, 6}, {180, 180, 210, 255}, 1, 20.0f, 0.6f);
        ai.state_timer -= dt;
        if (ai.state_timer <= 0.0f) {
            ai.boss_state = DEA_DRIFT;
            ai.vuln_mult = 1.0f;
            ai.special_timer = 2.4f;
        }
        break;
    default: ai.boss_state = DEA_DRIFT; break;
    }
}

// =============================================================================
// Cap. 9 - Guiezi el Codicioso (2 R 5:20-27)
// =============================================================================
// Se TELETRANSPORTA cada 5,5 s y deja hasta dos SENUELOS (su misma hoja, translucidos, 1
// de vida, disparan tambien, revientan en plata). El de verdad lleva la marca: chispas
// blancas de lepra sobre la cabeza. CODICIA: cada 12 s lanza tres TALENTOS DE PLATA
// (circulos plateados donde caen) y corre a recogerlos; mientras esta de rodillas
// recogiendo (1,2 s por talento), recibe dano x1,75. La plata de un senuelo reventado tambien le tienta. Al
// 50% deja nubes de LEPRA (charcas palidas) donde se teletransporta.
// Como se gana: aprender la marca, no gastar tiros en senuelos salvo para sembrar plata,
// y castigarle de rodillas.
enum : u8 { GZ_ROAM = 0, GZ_THROW, GZ_RUN, GZ_KNEEL };

static void spawn_decoy(ECSWorld& w, Entity boss, Vec2 at) {
    const auto& bai = w.enemy_ais[boss];
    Entity d = spawn_enemy(w, EnemyType::GeneralSirio, at, w.sprites[boss].texture_id, bai.chapter);
    if (d == ENTITY_NONE) return;
    w.transforms[d].scale = w.transforms[boss].scale;
    w.colliders[d].radius = w.colliders[boss].radius;
    w.colliders[d].offset = w.colliders[boss].offset;
    Color t = base_tint(w, boss);
    t.a = 175;   // un poco translucido: la primera pista
    w.sprites[d].tint = t;
    w.healths[d].max = 1.0f;
    w.healths[d].current = 1.0f;
    w.combats[d].damage = w.combats[boss].damage * 0.4f;
    w.movements[d].speed = w.movements[boss].speed;
    auto& ai = w.enemy_ais[d];
    ai.name = bai.name;
    ai.prop_kind = static_cast<u8>(BossProp::Decoy);
    ai.owner = boss;
    ai.home_cell = bai.home_cell;
    ai.aggro_range = 400.0f;
    ai.state_timer = 10.0f;    // se deshace solo si nadie lo revienta
    ai.shoot_timer = randf_range(0.8f, 1.8f);
    spawn_particles(w, at, {150, 220, 150, 255}, 10, 80.0f, 0.35f);
}

static void update_decoy(ECSWorld& w, Entity e, Vec2 player_pos, Vec2 to_player, f32 dist, f32 dmg, f32 dt) {
    auto& ai = w.enemy_ais[e];
    if (!alive_entity(w, ai.owner)) { w.destroy(e); return; }
    ai.state_timer -= dt;
    if (ai.state_timer <= 0.0f) {
        spawn_particles(w, w.transforms[e].position, {220, 230, 245, 255}, 10, 60.0f, 0.4f);
        w.destroy(e);
        return;
    }
    keep_range(w, e, to_player, dist, 70.0f, 120.0f, 0.5f);
    if (ai.shoot_timer <= 0.0f && enemy_has_los(w, w.transforms[e].position, player_pos)) {
        ai.shoot_timer = 2.6f;
        enemy_volley(w, e, to_player, 2, 0.3f, ProjectileKind::Arrow, dmg, 190.0f, 260.0f);
    }
}

static void boss_gehazi(ECSWorld& w, Entity e, Vec2 player_pos, Vec2 to_player, f32 dist, f32 dmg, f32 dt) {
    auto& ai = w.enemy_ais[e];
    auto& m = w.movements[e];
    const f32 hit = hit_unit(ai.chapter);
    if (ai.fight_time < 0.05f) ai.aux_timer = 8.0f;   // la primera codicia, tras ver los senuelos
    if (w.healths[e].ratio() < 0.5f && ai.phase == 0)
        enter_phase(w, e, 1, "la lepra se le pega", {225, 235, 215, 255}, 0.0f);
    ai.aux_timer -= dt;
    w.sprites[e].tint = base_tint(w, e);
    // La marca del de verdad: chispas blancas de lepra sobre la cabeza
    if (randf() < 5.0f * dt)
        spawn_particles(w, w.transforms[e].position + Vec2{randf_range(-5.0f, 5.0f), -14.0f * w.transforms[e].scale.y},
                        {250, 252, 255, 255}, 1, 12.0f, 0.7f);

    switch (ai.boss_state) {
    case GZ_ROAM: {
        ai.vuln_mult = 1.0f;
        keep_range(w, e, to_player, dist, 80.0f, 125.0f, 0.5f);
        if (ai.shoot_timer <= 0.0f && enemy_has_los(w, w.transforms[e].position, player_pos)) {
            ai.shoot_timer = 2.2f;
            enemy_volley(w, e, to_player, 2, 0.3f, ProjectileKind::Arrow, dmg * 0.55f, 195.0f, 260.0f);
        }
        if (ai.special_timer <= 0.0f) {
            // Teletransporte + senuelos
            ai.special_timer = ai.phase ? 4.5f : 5.5f;
            const Rect a = stage_of(w, e);
            const Vec2 from = w.transforms[e].position;
            if (ai.phase > 0)
                spawn_hazard(w, hz_pool(e, ground_of(w, e), 22.0f, 7.0f, hit * 0.5f, {226, 236, 214, 255}));
            const f32 base = randf_range(0.0f, 2.0f * PI);
            Vec2 to = from;
            for (int k = 0; k < 6; k++) {
                const Vec2 c = clamp_in(a, player_pos + from_angle(base + k * 1.05f) * 88.0f, 0.0f);
                if (walkable(w, c + Vec2{0, 8}) && c.distance_to(player_pos) > 50.0f) { to = c; break; }
            }
            spawn_particles(w, from, {150, 220, 150, 255}, 12, 90.0f, 0.35f);
            w.transforms[e].position = to;
            spawn_particles(w, to, {150, 220, 150, 255}, 12, 90.0f, 0.35f);
            const int have = count_props(w, e, BossProp::Decoy);
            for (int k = 0; k < 2 - have; k++) {
                const Vec2 c = clamp_in(a, player_pos + from_angle(base + (k + 1) * 2.09f) * 88.0f, 0.0f);
                if (walkable(w, c + Vec2{0, 8})) spawn_decoy(w, e, c);
            }
            SDL_Log("Boss %s se teletransporta (%d senuelos)", ai.name, count_props(w, e, BossProp::Decoy));
            break;
        }
        if (ai.aux_timer <= 0.0f) {
            // CODICIA: tres talentos de plata, donde caen se pinta un circulo plateado
            ai.aux_timer = 12.0f;
            const Rect a = stage_of(w, e);
            for (int k = 0; k < 3; k++) {
                const Vec2 at = clamp_in(a, ground_of(w, e) + from_angle(randf_range(0.0f, 2.0f * PI)) * randf_range(50.0f, 110.0f), 0.0f);
                spawn_hazard(w, hz_circle(e, at, 12.0f, 0.75f * wk(), hit * 0.6f, {226, 232, 246, 255}, HazardFx::Coin));
            }
            ai.boss_state = GZ_THROW;
            ai.state_timer = 0.75f * wk();
            m.velocity = {0, 0};
            play(w, e, AnimID::CastMiracle);
            SDL_Log("Boss %s lanza la plata (codicia)", ai.name);
        }
        break;
    }
    case GZ_THROW:
        m.velocity = {0, 0};
        ai.state_timer -= dt;
        if (ai.state_timer <= 0.0f) {
            // Donde estallaron los circulos plateados, queda la plata
            for (Hazard& hz : w.hazards) {
                if (!hz.used || hz.owner != e || hz.kind != HazardKind::Circle || hz.fx != HazardFx::Coin) continue;
                spawn_silver(w, hz.pos - Vec2{0, 4});
                hz.fx = HazardFx::None;   // una plata por circulo
            }
            ai.boss_state = GZ_RUN;
            ai.state_timer = 4.0f;
        }
        break;
    case GZ_RUN: {
        const Entity s = nearest_silver(w, w.transforms[e].position);
        ai.state_timer -= dt;
        if (s == ENTITY_NONE || ai.state_timer <= 0.0f) { ai.boss_state = GZ_ROAM; break; }
        const Vec2 d = w.transforms[s].position - ground_of(w, e) + Vec2{0, 4};
        if (d.length() < 7.0f) {
            ai.boss_state = GZ_KNEEL;
            ai.state_timer = 1.2f;
            ai.act_pos = w.transforms[s].position;
            m.velocity = {0, 0};
            play(w, e, AnimID::CastMiracle);
            SDL_Log("Boss %s se arrodilla a recoger la plata (dano x1,75)", ai.name);
        } else {
            steer(w, e, d.normalized(), m.speed * 1.45f);
        }
        break;
    }
    case GZ_KNEEL: {
        m.velocity = {0, 0};
        ai.vuln_mult = 1.75f;
        w.sprites[e].tint = {255, 232, 160, 255};
        if (randf() < 8.0f * dt)
            spawn_particles(w, w.transforms[e].position + Vec2{0, 6}, {255, 236, 170, 255}, 1, 30.0f, 0.5f);
        ai.state_timer -= dt;
        if (ai.state_timer <= 0.0f) {
            const Entity s = nearest_silver(w, w.transforms[e].position);
            if (s != ENTITY_NONE && w.transforms[s].position.distance_to(ai.act_pos) < 2.0f) {
                spawn_particles(w, w.transforms[s].position, {235, 240, 250, 255}, 8, 50.0f, 0.4f);
                w.destroy(s);   // se la guarda
            }
            ai.vuln_mult = 1.0f;
            ai.boss_state = (nearest_silver(w, w.transforms[e].position) != ENTITY_NONE) ? GZ_RUN : GZ_ROAM;
            ai.state_timer = 4.0f;
            ai.special_timer = std::max(ai.special_timer, 1.2f);
        }
        break;
    }
    default: ai.boss_state = GZ_ROAM; break;
    }
}

// =============================================================================
// Cap. 10 - Ben-adad de Siria (2 R 6:8-7:7): el asedio
// =============================================================================
// ARTILLERIA de asedio: piedras que caen sobre la arena en patron (rejilla con una columna
// libre, damero, dos filas) — siempre queda un camino. LLUVIA DE FLECHAS en tres carriles
// paralelos que cruzan la arena. Carga de CARRO (rapida, en carril). Oleadas de soldados
// sirios por los bordes. Al 30%, LA HUESTE CELESTIAL (6:17-18): un fogonazo santo ciega a
// sus soldados 6 s (vagan palidos sin atacar) y Ben-adad queda lento y expuesto.
// Como se gana: leer los huecos de la artilleria, no pelearse con la oleada y, cuando el
// cielo se llena de fuego, ir a por el rey.
enum : u8 { BEN_MOVE = 0, BEN_AIM, BEN_CHARGE, BEN_RECOVER };

static void boss_benhadad(ECSWorld& w, Entity e, Vec2 to_player, f32 dist, f32 dmg, f32 dt) {
    auto& ai = w.enemy_ais[e];
    auto& m = w.movements[e];
    const f32 hit = hit_unit(ai.chapter);
    const Rect a = arena_of(w, e);
    const f32 ratio = w.healths[e].ratio();
    if (ratio < 0.6f && ai.phase == 0) {
        enter_phase(w, e, 1, "llegan los generales", {230, 120, 90, 255}, 0.0f);
        summon_at(w, e, EnemyType::GeneralSirio, {a.x + a.w * 0.5f, a.bottom() - 24.0f}, ai.chapter, 6);
    }
    if (ratio < 0.3f && ai.phase < 2) {
        enter_phase(w, e, 2, "la hueste celestial: los sirios quedan ciegos", {255, 236, 170, 255}, 0.65f);
        int blinded = 0;
        for (u32 i = 0; i < w.entity_count; i++) {
            if (!w.is_alive(i) || !is_hostile(w, i) || !w.has(i, Component::Health) || !w.healths[i].is_alive()) continue;
            if (w.enemy_ais[i].is_boss) continue;
            w.enemy_ais[i].blind_timer = 6.0f;
            blinded++;
        }
        // Carros y caballos de fuego alrededor (2 R 6:17)
        for (int k = 0; k < 10; k++) {
            const Vec2 p = a.center() + Vec2{std::cos(k * 0.628f) * a.w * 0.46f, std::sin(k * 0.628f) * a.h * 0.44f};
            spawn_fx(w, p, FxID::Holy, {255, 225, 150, 240}, 46.0f, 0.9f, 11);
            spawn_vfx(w, p, TileSprite::Fireball, {255, 220, 150, 235}, 14.0f, 0.9f, 0.6f, 260.0f);
        }
        SDL_Log("Boss %s: %d soldados cegados 6 s", ai.name, blinded);
        m.speed *= 0.6f;
        ai.aux2_timer = 8.0f;   // tregua de oleadas
        ai.vuln_mult = 1.5f;
        ai.state_timer = 0.0f;
        ai.boss_state = BEN_MOVE;
        clear_hazards_of(w, e);
        ai.hp_mark = 8.0f;      // la ventana de la hueste: 8 s a x1,5, luego x1,2
    }
    w.sprites[e].tint = base_tint(w, e);
    if (ai.phase >= 2) {
        ai.hp_mark -= dt;
        ai.vuln_mult = (ai.hp_mark > 0.0f) ? 1.5f : 1.2f;
    }
    ai.aux_timer -= dt;
    ai.aux2_timer -= dt;

    // Oleadas por los bordes
    if (ai.aux2_timer <= 0.0f) {
        ai.aux2_timer = 9.0f;
        summon_at(w, e, EnemyType::SoldadoSirio, {a.x + 20.0f, a.y + a.h * (0.3f + 0.4f * randf())}, ai.chapter, 5);
        summon_at(w, e, EnemyType::SoldadoSirio, {a.right() - 20.0f, a.y + a.h * (0.3f + 0.4f * randf())}, ai.chapter, 5);
    }

    switch (ai.boss_state) {
    case BEN_MOVE: {
        keep_range(w, e, to_player, dist, 90.0f, 150.0f, 0.3f);
        // ARTILLERIA: rejilla 4x3 con un patron que siempre deja camino
        if (ai.special_timer <= 0.0f) {
            ai.special_timer = (ai.phase >= 2) ? 8.5f : 7.0f;
            const int pattern = ai.counter % 3;
            ai.counter++;
            const int safe_col = randi_range(0, 3), safe_row = randi_range(0, 2);
            int n = 0;
            for (int r = 0; r < 3; r++) {
                for (int c = 0; c < 4; c++) {
                    bool fire = true;
                    if (pattern == 0) fire = (c != safe_col);
                    if (pattern == 1) fire = ((r + c) % 2 == 0);
                    if (pattern == 2) fire = (r != safe_row);
                    if (!fire) continue;
                    const Vec2 at = {a.x + (c + 0.5f) * a.w / 4.0f + randf_range(-6.0f, 6.0f),
                                     a.y + (r + 0.5f) * a.h / 3.0f + randf_range(-5.0f, 5.0f)};
                    spawn_hazard(w, hz_circle(e, at, 22.0f, (1.15f + 0.12f * r) * wk(), hit * 0.9f,
                                              {232, 172, 92, 255}, HazardFx::Dust));
                    n++;
                }
            }
            SDL_Log("Boss %s: artilleria (patron %d, %d piedras)", ai.name, pattern, n);
            break;
        }
        // LLUVIA DE FLECHAS: tres carriles paralelos, horizontales o verticales
        if (ai.aux_timer <= 0.0f) {
            ai.aux_timer = 9.0f;
            const bool horiz = randf() < 0.5f;
            for (int k = 0; k < 3; k++) {
                const f32 t = (k + 0.5f) / 3.0f + randf_range(-0.05f, 0.05f);
                const Vec2 from = horiz ? Vec2{a.x, a.y + a.h * t} : Vec2{a.x + a.w * t, a.y};
                const Vec2 dir  = horiz ? Vec2{1, 0} : Vec2{0, 1};
                spawn_hazard(w, hz_lane(e, from, dir, horiz ? a.w : a.h, 18.0f, 0.95f * wk(), 0.3f, hit * 0.8f,
                                        {240, 222, 170, 255}, HazardFx::Dust));
            }
            SDL_Log("Boss %s: lluvia de flechas", ai.name);
            break;
        }
        // CARRO: embestida rapida en carril (no despues de la hueste: ya no tiene caballos)
        if (ai.phase < 2 && ai.leap_timer <= 0.0f && dist < 260.0f) {
            const f32 len = wall_distance(w, collider_feet(w, e), to_player, 300.0f);
            if (len < 60.0f) { ai.leap_timer = 1.0f; break; }
            ai.act_dir = to_player;
            ai.act_pos = collider_feet(w, e) + to_player * len;
            Hazard hz = hz_lane(e, ground_of(w, e), to_player, len + 8.0f, 28.0f, 0.85f * wk(),
                                len / (330.0f * sk()) + 0.1f, 0.0f, {255, 150, 80, 255});
            hz.hits_player = false;
            spawn_hazard(w, hz);
            ai.boss_state = BEN_AIM;
            ai.state_timer = 0.85f * wk();
            m.velocity = {0, 0};
            face(w, e, to_player);
        }
        break;
    }
    case BEN_AIM:
        m.velocity = {0, 0};
        ai.state_timer -= dt;
        if (ai.state_timer <= 0.0f) {
            ai.boss_state = BEN_CHARGE;
            ai.state_timer = 300.0f / (330.0f * sk()) + 0.3f;
            play(w, e, AnimID::Dash);
        }
        break;
    case BEN_CHARGE: {
        const Vec2 feet = collider_feet(w, e);
        const bool blocked = !walkable(w, feet + ai.act_dir * 9.0f);
        const bool arrived = (ai.act_pos - feet).dot(ai.act_dir) <= 0.0f;
        ai.state_timer -= dt;
        if (blocked || arrived || ai.state_timer <= 0.0f) {
            m.velocity = {0, 0};
            ai.boss_state = BEN_RECOVER;
            ai.state_timer = 0.9f;
            ai.leap_timer = 8.0f;
            if (blocked) w.shake_request = std::max(w.shake_request, 2.5f);
        } else {
            m.velocity = ai.act_dir * 330.0f * sk();
            m.knockback = {0, 0};
            if (randf() < 0.5f) spawn_particles(w, ground_of(w, e), {210, 190, 150, 255}, 1, 30.0f, 0.35f);
        }
        break;
    }
    case BEN_RECOVER:
        m.velocity = {0, 0};
        ai.state_timer -= dt;
        if (ai.state_timer <= 0.0f) ai.boss_state = BEN_MOVE;
        break;
    default: ai.boss_state = BEN_MOVE; break;
    }
}

// =============================================================================
// Despacho
// =============================================================================
void update_boss_entity(ECSWorld& w, Entity e, Vec2 player_pos, Vec2 to_player, f32 dist, f32 dmg, f32 dt) {
    auto& ai = w.enemy_ais[e];
    if (ai.prop_kind == static_cast<u8>(BossProp::Decoy)) { update_decoy(w, e, player_pos, to_player, dist, dmg, dt); return; }
    if (ai.prop_kind != 0) return;   // los idolos no se mueven (ni tienen Movement)
    if (ai.boss_group) {
        ai.fight_time += dt;
        if (ai.boss_pattern == static_cast<u8>(BossPattern::AcabYJezabel))
            boss_jezabel(w, e, player_pos, to_player, dist, dmg, dt);
        else
            update_baal_prophet(w, e, player_pos, to_player, dist, dmg, dt);
        return;
    }

    ai.fight_time += dt;
    ai.special_timer -= dt;
    switch (static_cast<BossPattern>(ai.boss_pattern)) {
        case BossPattern::CapitanDeAcab:    boss_captain(w, e, to_player, dist, dt); break;
        case BossPattern::SacerdoteBaal:    boss_priest(w, e, player_pos, to_player, dist, dmg, dt); break;
        case BossPattern::ProfetasDeBaal:   boss_baal_head(w, e, to_player, dist, dmg, dt); break;
        case BossPattern::Desesperacion:    boss_despair(w, e, player_pos, to_player, dist, dt); break;
        case BossPattern::AcabYJezabel:     boss_acab(w, e, to_player, dist, dt); break;
        case BossPattern::EspirituCorrupto: boss_spirit(w, e, player_pos, to_player, dist, dmg, dt); break;
        case BossPattern::Acreedor:         boss_creditor(w, e, player_pos, to_player, dist, dmg, dt); break;
        case BossPattern::LaMuerte:         boss_death(w, e, to_player, dist, dmg, dt); break;
        case BossPattern::Guiezi:           boss_gehazi(w, e, player_pos, to_player, dist, dmg, dt); break;
        case BossPattern::BenAdad:          boss_benhadad(w, e, to_player, dist, dmg, dt); break;
    }
}

void on_boss_death(ECSWorld& w, Entity e, Vec2 pos) {
    auto& ai = w.enemy_ais[e];
    clear_hazards_of(w, e);
    if (ai.prop_kind == static_cast<u8>(BossProp::Idol)) {
        spawn_fx(w, pos, FxID::Smoke, {170, 110, 210, 220}, 44.0f, 0.7f, 7);
        spawn_particles(w, pos, {200, 120, 250, 255}, 16, 100.0f, 0.5f);
        SDL_Log("Idolo de Baal derribado (%d en pie)", count_props(w, ai.owner, BossProp::Idol));
        return;
    }
    if (ai.prop_kind == static_cast<u8>(BossProp::Decoy)) {
        // Revienta en plata... y la plata le tienta (Guiezi irá a por ella)
        spawn_particles(w, pos, {235, 240, 250, 255}, 16, 100.0f, 0.5f);
        spawn_fx(w, pos, FxID::Sparks, {235, 240, 255, 230}, 34.0f, 0.35f);
        if (alive_entity(w, ai.owner)) spawn_silver(w, pos + Vec2{0, 10});
        return;
    }
    if (ai.is_boss) {
        SDL_Log("Boss %s defeated in %.1f s", ai.name, ai.fight_time);
        // Sus props mueren con el (no bloquean el cierre de la arena) y su plata se va
        for (u32 i = 0; i < w.entity_count; i++) {
            if (!w.is_alive(i)) continue;
            const bool prop = w.has(i, Component::EnemyAI) && w.enemy_ais[i].prop_kind != 0 &&
                              w.enemy_ais[i].owner == e;
            const bool silver = w.has(i, Component::Item) && w.items[i].pickup == ItemComponent::Pickup::Silver;
            if (!prop && !silver) continue;
            spawn_particles(w, w.transforms[i].position, {220, 220, 235, 255}, 8, 60.0f, 0.4f);
            w.destroy(static_cast<Entity>(i));
        }
        w.sprites[e].visible = true;
    }
    // Sin jefe ni sequito vivos, la arena queda limpia de peligros
    bool any = false;
    for (u32 i = 0; i < w.entity_count && !any; i++) {
        if (static_cast<Entity>(i) == e || !w.is_alive(i) || !w.has(i, Component::EnemyAI)) continue;
        const auto& o = w.enemy_ais[i];
        if ((o.is_boss || o.boss_group) && w.has(i, Component::Health) && w.healths[i].is_alive()) any = true;
    }
    if (!any) {
        clear_hazards(w);
        if (ai.boss_group && ai.boss_pattern == static_cast<u8>(BossPattern::AcabYJezabel))
            SDL_Log("Boss %s defeated in %.1f s (cae la ultima del duo)", ai.name, ai.fight_time);
    }
}

// =============================================================================
// Marcas de los jefes
// =============================================================================
static Color pattern_color(BossPattern p) {
    switch (p) {
        case BossPattern::CapitanDeAcab:    return {255, 120, 80, 255};
        case BossPattern::SacerdoteBaal:    return {180, 90, 230, 255};
        case BossPattern::ProfetasDeBaal:   return {230, 80, 70, 255};
        case BossPattern::Desesperacion:    return {170, 190, 240, 255};
        case BossPattern::AcabYJezabel:     return {230, 110, 90, 255};
        case BossPattern::EspirituCorrupto: return {70, 210, 180, 255};
        case BossPattern::Acreedor:         return {240, 200, 100, 255};
        case BossPattern::LaMuerte:         return {150, 150, 210, 255};
        case BossPattern::Guiezi:           return {225, 235, 215, 255};
        case BossPattern::BenAdad:          return {230, 160, 90, 255};
        default:                            return {255, 255, 255, 255};
    }
}

static bool is_boss_like(const ECSWorld& w, u32 i) {
    if (!w.is_alive(i) || !w.has(i, Component::EnemyAI) || !w.has(i, Component::Transform)) return false;
    const auto& ai = w.enemy_ais[i];
    if (!ai.is_boss && !(ai.boss_group && ai.boss_pattern == static_cast<u8>(BossPattern::AcabYJezabel))) return false;
    return w.has(i, Component::Health) && w.healths[i].is_alive() && w.sprites[i].visible;
}

void render_boss_ground(Renderer& r, const ECSWorld& w, const HazardArt& art) {
    const f32 t = w.time;
    for (u32 i = 0; i < w.entity_count; i++) {
        if (!is_boss_like(w, i)) continue;
        const auto& ai = w.enemy_ais[i];
        const auto& tr_ = w.transforms[i];
        const Vec2 g = ground_of(w, i);
        const f32 s = tr_.scale.x;
        const Color c = pattern_color(static_cast<BossPattern>(ai.boss_pattern));
        const f32 pulse = 0.5f + 0.5f * std::sin(t * 3.0f + i);
        // Aura de jefe: lo separa de un enemigo comun aunque compartan hoja
        hz_draw_disc(r, art, g, 13.0f * s, {c.r, c.g, c.b, static_cast<u8>(46 + 26 * pulse)});
        // El ESCUDO del Capitan: el frente que para los tiros, pintado en el suelo
        if (ai.shield_cos > -1.5f) {
            const f32 half = std::acos(std::max(-1.0f, std::min(1.0f, ai.shield_cos)));
            hz_draw_sector(r, art, g, ai.face_dir, 22.0f * s, half, {150, 195, 240, static_cast<u8>(70 + 30 * pulse)});
        }
        // VENTANA: el aro dorado dice "ahora si"
        if (ai.vuln_mult > 1.05f) {
            const f32 fast = 0.5f + 0.5f * std::sin(t * 12.0f);
            hz_draw_rim(r, art, g, 15.0f * s + 2.0f * fast, {255, 220, 110, static_cast<u8>(170 + 80 * fast)});
            hz_draw_disc(r, art, g, 14.0f * s, {255, 220, 120, static_cast<u8>(40 + 40 * fast)});
        }
    }
}

void render_boss_overlays(Renderer& r, const ECSWorld& w, const HazardArt& art) {
    const f32 t = w.time;
    for (u32 i = 0; i < w.entity_count; i++) {
        if (!w.is_alive(i) || !w.has(i, Component::EnemyAI) || !w.has(i, Component::Transform)) continue;
        const auto& ai = w.enemy_ais[i];
        const Vec2 p = w.transforms[i].position;
        const f32 s = w.transforms[i].scale.x;
        const bool alive = w.has(i, Component::Health) && w.healths[i].is_alive();
        if (!alive) continue;

        // Cegado por la hueste celestial (cap. 10): destellos blancos dando vueltas sobre la
        // cabeza. El tinte frio solo no basta: sobre el rojo de un General apenas se notaba.
        if (ai.blind_timer > 0.0f) {
            for (int k = 0; k < 2; k++) {
                const f32 a = t * 4.0f + k * PI + i;
                const Vec2 sp = p + Vec2{std::cos(a) * 6.0f * s, -15.0f * s + std::sin(a) * 2.0f};
                hz_draw_sparkle(r, art, sp, 5.0f, a * 57.3f, {255, 255, 240, 245});
            }
        }

        // Atadura idolo -> sacerdote (morada) y miembro del corro -> sumo sacerdote (roja)
        if (ai.prop_kind == static_cast<u8>(BossProp::Idol) && is_boss_like(w, ai.owner)) {
            const Vec2 a = p - Vec2{0, 10}, b = w.transforms[ai.owner].position - Vec2{0, 4};
            const f32 pulse = 0.5f + 0.5f * std::sin(t * 6.0f + i);
            hz_draw_beam(r, art, a, b, 10.0f, {40, 10, 70, static_cast<u8>(90 + 30 * pulse)});    // sombra: se lee en la arena clara
            hz_draw_beam(r, art, a, b, 7.0f, {170, 80, 245, static_cast<u8>(120 + 60 * pulse)});
            hz_draw_beam(r, art, a, b, 2.6f, {235, 190, 255, static_cast<u8>(210 + 45 * pulse)});
            const f32 k = std::fmod(t * 0.9f + i * 0.21f, 1.0f);
            hz_draw_disc(r, art, Vec2::lerp(a, b, k), 2.4f, {245, 215, 255, 230});
        }
        if (ai.boss_group && ai.boss_pattern == static_cast<u8>(BossPattern::ProfetasDeBaal) &&
            alive_entity(w, ai.owner) && w.healths[ai.owner].invulnerable) {
            const Vec2 a = p - Vec2{0, 6}, b = w.transforms[ai.owner].position - Vec2{0, 4};
            const f32 pulse = 0.5f + 0.5f * std::sin(t * 7.0f + i);
            hz_draw_beam(r, art, a, b, 6.0f, {60, 8, 8, static_cast<u8>(80 + 30 * pulse)});
            hz_draw_beam(r, art, a, b, 3.2f, {245, 90, 75, static_cast<u8>(150 + 80 * pulse)});
        }
        // Jezabel cura a Acab: atadura verde que se corta si la golpean
        if (ai.boss_group && ai.boss_pattern == static_cast<u8>(BossPattern::AcabYJezabel) &&
            ai.boss_state == JEZ_HEAL && alive_entity(w, ai.partner)) {
            const Vec2 a = p - Vec2{0, 6}, b = w.transforms[ai.partner].position - Vec2{0, 6};
            const f32 pulse = 0.5f + 0.5f * std::sin(t * 10.0f);
            hz_draw_beam(r, art, a, b, 9.0f, {120, 230, 120, static_cast<u8>(80 + 50 * pulse)});
            hz_draw_beam(r, art, a, b, 3.0f, {220, 255, 200, 235});
            for (int k = 0; k < 3; k++) {
                const f32 f = std::fmod(t * 1.3f + k / 3.0f, 1.0f);
                hz_draw_disc(r, art, Vec2::lerp(a, b, f), 2.6f, {230, 255, 210, 235});
            }
        }
        if (!ai.is_boss) continue;
        const BossPattern pat = static_cast<BossPattern>(ai.boss_pattern);

        // Burbuja de intocable: aro violeta que tiembla alrededor del cuerpo
        if (w.healths[i].invulnerable && w.sprites[i].visible) {
            const f32 pulse = 0.5f + 0.5f * std::sin(t * 5.0f);
            hz_draw_disc(r, art, p, 14.0f * s, {170, 140, 255, static_cast<u8>(30 + 20 * pulse)});
            hz_draw_rim(r, art, p, 14.0f * s + 1.5f * pulse, {215, 190, 255, static_cast<u8>(160 + 80 * pulse)});
        }
        // Estrellas del aturdido (el Capitan contra el muro)
        if (pat == BossPattern::CapitanDeAcab && ai.boss_state == CAP_STUN) {
            for (int k = 0; k < 3; k++) {
                const f32 a = t * 5.0f + k * 2.094f;
                const Vec2 sp = p + Vec2{std::cos(a) * 9.0f * s, -16.0f * s + std::sin(a) * 3.0f};
                hz_draw_sparkle(r, art, sp, 4.0f, a * 57.3f, {255, 236, 140, 245});
            }
        }
        // La cadena del Acreedor volviendo a su mano
        if (pat == BossPattern::Acreedor && ai.boss_state == CRE_REEL) {
            const f32 k = std::max(0.0f, std::min(1.0f, ai.state_timer / 1.2f));
            const Vec2 hand = ground_of(w, i) - Vec2{0, 8};
            const Vec2 end = Vec2::lerp(hand, ai.act_pos, k);
            const Vec2 d = end - hand;
            const f32 len = d.length();
            if (len > 4.0f) {
                const Vec2 n = d / len;
                const f32 deg = std::atan2(n.y, n.x) * 57.2957795f;
                for (f32 s2 = 3.0f; s2 < len; s2 += 7.0f) {
                    const Vec2 at = hand + n * s2;
                    const int idx = static_cast<int>(s2 / 7.0f);
                    r.draw_sprite_rect(art.rim, {0, 0, 128, 128}, {at.x - 3.5f, at.y - 2.25f, 7.0f, 4.5f},
                                       {230, 205, 140, 235}, false, deg + ((idx % 2) ? 0.0f : 90.0f));
                }
            }
        }
    }
}
