// =============================================================================
// MANTLE - Peligros de suelo telegrafiados (ver hazards.h)
// =============================================================================

#include "game/hazards.h"
#include "game/combat.h"
#include "game/faith.h"
#include "game/rooms.h"
#include "render/renderer.h"
#include <SDL3/SDL.h>
#include <cmath>
#include <algorithm>
#include <vector>

static constexpr f32 PI          = 3.14159265f;
static constexpr f32 LINGER      = 0.24f;   // cola sin dano: el fogonazo se apaga en vez de cortarse
static constexpr f32 BODY        = 4.0f;    // medio cuerpo del profeta a la altura de los pies
static constexpr f32 POOL_BITE   = 0.9f;    // una charca muerde, como mucho, una vez cada esto
static constexpr f32 POOL_FADE   = 0.6f;    // la charca se apaga en su ultimo tramo

const char* hazard_kind_name(HazardKind k) {
    switch (k) {
        case HazardKind::Circle: return "Circle";
        case HazardKind::Lane:   return "Lane";
        case HazardKind::Cone:   return "Cone";
        case HazardKind::Ring:   return "Ring";
        case HazardKind::Pool:   return "Pool";
        case HazardKind::Push:   return "Push";
        default:                 return "?";
    }
}

// =============================================================================
// Constructores
// =============================================================================
Hazard hz_circle(Entity owner, Vec2 at, f32 radius, f32 warn, f32 damage, Color color, HazardFx fx) {
    Hazard h;
    h.kind = HazardKind::Circle;
    h.owner = owner; h.pos = at; h.radius = radius;
    h.warn = warn; h.active = 0.10f; h.damage = damage; h.color = color; h.fx = fx;
    return h;
}

Hazard hz_lane(Entity owner, Vec2 from, Vec2 dir, f32 length, f32 width, f32 warn, f32 active,
               f32 damage, Color color, HazardFx fx) {
    Hazard h;
    h.kind = HazardKind::Lane;
    h.owner = owner; h.pos = from; h.dir = dir.normalized();
    if (h.dir.length_sq() < 0.5f) h.dir = {0, 1};
    h.length = length; h.width = width;
    h.warn = warn; h.active = active; h.damage = damage; h.color = color; h.fx = fx;
    return h;
}

Hazard hz_cone(Entity owner, Vec2 apex, Vec2 dir, f32 radius, f32 half_angle, f32 warn,
               f32 damage, Color color, HazardFx fx) {
    Hazard h;
    h.kind = HazardKind::Cone;
    h.owner = owner; h.pos = apex; h.dir = dir.normalized();
    if (h.dir.length_sq() < 0.5f) h.dir = {0, 1};
    h.radius = radius; h.angle = half_angle;
    h.warn = warn; h.active = 0.12f; h.damage = damage; h.color = color; h.fx = fx;
    return h;
}

Hazard hz_ring(Entity owner, Vec2 center, f32 r_from, f32 r_to, f32 band, f32 warn, f32 travel,
               f32 damage, Vec2 gap_dir, f32 gap_half, Color color) {
    Hazard h;
    h.kind = HazardKind::Ring;
    h.owner = owner; h.pos = center; h.r_from = r_from; h.radius = r_to; h.width = band;
    h.dir = gap_dir.normalized();
    if (h.dir.length_sq() < 0.5f) h.dir = {0, 1};
    h.angle = gap_half;
    h.warn = warn; h.active = std::max(0.1f, travel); h.damage = damage; h.color = color;
    return h;
}

Hazard hz_pool(Entity owner, Vec2 at, f32 radius, f32 life, f32 damage, Color color) {
    Hazard h;
    h.kind = HazardKind::Pool;
    h.owner = owner; h.pos = at; h.radius = radius;
    h.warn = 0.35f;            // crece: no muerde hasta estar entera
    h.active = life; h.damage = damage; h.color = color;
    return h;
}

Hazard hz_push(Entity owner, Vec2 from, Vec2 dir, f32 length, f32 width, f32 warn, f32 active,
               f32 force, Color color) {
    Hazard h;
    h.kind = HazardKind::Push;
    h.owner = owner; h.pos = from; h.dir = dir.normalized();
    if (h.dir.length_sq() < 0.5f) h.dir = {1, 0};
    h.length = length; h.width = width;
    h.warn = warn; h.active = active; h.force = force; h.color = color;
    h.damage = 0.0f;
    return h;
}

static f32 hazard_linger(const Hazard& h) {
    switch (h.kind) {
        case HazardKind::Pool: return 0.0f;    // se apaga DENTRO de su vida (POOL_FADE)
        case HazardKind::Push: return 0.25f;
        default:               return LINGER;
    }
}

static const char* owner_name(const ECSWorld& w, Entity owner) {
    if (owner == ENTITY_NONE || !w.is_alive(owner) || !w.has(owner, Component::EnemyAI)) return "-";
    return w.enemy_ais[owner].name;
}

int spawn_hazard(ECSWorld& world, const Hazard& h) {
    for (int i = 0; i < MAX_HAZARDS; i++) {
        if (world.hazards[i].used) continue;
        Hazard& s = world.hazards[i];
        s = h;
        s.used = true;
        s.t = 0.0f;
        s.hit_done = false;
        s.burst_done = false;
        s.tick = 0.0f;
        // La sombra del Espiritu es dibujo: no ensucia el log de cada frame
        if (h.tag != HazardTag::Ripple)
            SDL_Log("Hazard %s spawned (%s) at (%.0f,%.0f) warn %.2f dmg %.1f",
                    hazard_kind_name(h.kind), owner_name(world, h.owner), h.pos.x, h.pos.y,
                    h.warn, h.damage);
        return i;
    }
    SDL_Log("Hazard %s DESCARTADO: piscina llena (%d)", hazard_kind_name(h.kind), MAX_HAZARDS);
    return -1;
}

void clear_hazards(ECSWorld& world) {
    for (Hazard& h : world.hazards) h = Hazard{};
}

void clear_hazards_of(ECSWorld& world, Entity owner) {
    if (owner == ENTITY_NONE) return;
    for (Hazard& h : world.hazards)
        if (h.used && h.owner == owner) h = Hazard{};
}

int count_hazards_of(const ECSWorld& world, Entity owner, HazardKind kind) {
    int n = 0;
    for (const Hazard& h : world.hazards)
        if (h.used && h.owner == owner && h.kind == kind) n++;
    return n;
}

int cleanse_pools(ECSWorld& world, Vec2 at, f32 radius) {
    int n = 0;
    for (Hazard& h : world.hazards) {
        if (!h.used || h.kind != HazardKind::Pool || h.tag == HazardTag::Ripple) continue;
        if (h.pos.distance_to(at) > radius + h.radius) continue;
        // El agua sanada se lleva la charca: burbujas claras en vez de un corte en seco
        spawn_particles(world, h.pos, {150, 230, 250, 255}, 8, 50.0f, 0.5f);
        spawn_fx(world, h.pos, FxID::Heal, {170, 240, 255, 220}, h.radius * 2.4f, 0.5f, 3);
        h = Hazard{};
        n++;
    }
    if (n > 0) SDL_Log("Hazard Pool cleansed x%d (Sanar Aguas)", n);
    return n;
}

// =============================================================================
// Geometria
// =============================================================================
// Radio de la onda en el instante `t` (solo tiene sentido con t >= warn)
static f32 ring_radius(const Hazard& h) {
    f32 s = (h.t - h.warn) / h.active;
    s = std::max(0.0f, std::min(1.0f, s));
    return h.r_from + (h.radius - h.r_from) * s;
}

static bool in_gap(const Hazard& h, Vec2 d) {
    if (h.angle <= 0.0f) return false;
    Vec2 n = d.normalized();
    if (n.length_sq() < 0.5f) return false;
    return n.dot(h.dir) >= std::cos(h.angle);
}

// ¿El punto `p` (pies) esta dentro del efecto, con `body` de holgura?
static bool hazard_hits(const Hazard& h, Vec2 p, f32 body) {
    Vec2 d = p - h.pos;
    switch (h.kind) {
        case HazardKind::Circle:
        case HazardKind::Pool:
            return d.length() <= h.radius + body;
        case HazardKind::Lane:
        case HazardKind::Push: {
            f32 along  = d.dot(h.dir);
            f32 across = std::fabs(d.x * h.dir.y - d.y * h.dir.x);
            return along >= -body && along <= h.length + body && across <= h.width * 0.5f + body;
        }
        case HazardKind::Cone: {
            f32 dist = d.length();
            if (dist > h.radius + body) return false;
            if (dist < 6.0f) return true;   // pegado al apice: dentro, mire a donde mire
            return (d / dist).dot(h.dir) >= std::cos(h.angle);
        }
        case HazardKind::Ring: {
            f32 r = ring_radius(h);
            if (std::fabs(d.length() - r) > h.width * 0.5f + body) return false;
            return !in_gap(h, d);
        }
        default: return false;
    }
}

// =============================================================================
// Simulacion
// =============================================================================
static Vec2 feet_of(const ECSWorld& w, Entity e) {
    // Los pies DIBUJADOS (fila 112 de la hoja = 12 unidades bajo el centro a escala 1): un
    // telegrafo se lee contra donde se ve al profeta pisando, no contra su colisionador.
    return w.transforms[e].position + Vec2{0.0f, 9.0f};
}

static bool hurt_player(ECSWorld& w, FaithSystem& faith, CombatEvents& ev, const Hazard& h, Vec2 from) {
    Entity p = w.player;
    if (p == ENTITY_NONE || !w.has(p, Component::Health)) return false;
    auto& hp = w.healths[p];
    // Exactamente lo que comprueba un proyectil enemigo: i-frames (golpe y dash) mandan
    if (!hp.is_alive() || hp.invulnerable || hp.iframes_timer > 0.0f) return false;
    if (h.damage > 0.0f) apply_damage(w, ENTITY_NONE, p, h.damage);
    else                 hp.iframes_timer = 0.6f;   // sin dano (cadena pura): no repetir cada frame
    if (w.has(p, Component::Movement)) {
        Vec2 away = (w.transforms[p].position - from).normalized();
        if (away.length_sq() < 0.5f) away = {0, 1};
        w.movements[p].knockback = away * 130.0f;
    }
    faith.on_take_damage();
    ev.player_hits++;
    return true;
}

// Lo que significa el impacto para el dueno (cadena, deuda)
static void on_player_hit(ECSWorld& w, FaithSystem& faith, Hazard& h) {
    Entity p = w.player;
    if (p == ENTITY_NONE) return;
    const Vec2 pp = w.transforms[p].position;
    const bool owner_ok = h.owner != ENTITY_NONE && w.is_alive(h.owner) &&
                          w.has(h.owner, Component::Health) && w.healths[h.owner].is_alive();
    switch (h.tag) {
        case HazardTag::Chain: {
            if (owner_ok && w.has(p, Component::Movement)) {
                Vec2 to = (w.transforms[h.owner].position - pp).normalized();
                w.movements[p].knockback = to * 360.0f;   // la cadena tira de ti
            }
            faith.modify(-h.faith_drain);
            if (owner_ok) {
                auto& oh = w.healths[h.owner];
                oh.heal(oh.max * 0.03f);   // cobra lo que se lleva
                spawn_particles(w, w.transforms[h.owner].position, {255, 214, 110, 255}, 10, 60.0f, 0.5f);
            }
            spawn_particles(w, pp, {255, 214, 110, 255}, 10, 70.0f, 0.5f);
            SDL_Log("Hazard Chain hit: el Acreedor cobra %.0f de fe", h.faith_drain);
            break;
        }
        case HazardTag::Debt:
            faith.modify(-h.faith_drain);
            spawn_particles(w, pp, {255, 214, 110, 255}, 12, 80.0f, 0.5f);
            SDL_Log("Hazard Debt hit: la deuda cobra %.0f de fe", h.faith_drain);
            break;
        default:
            break;
    }
}

static void burst_fx(ECSWorld& w, const Hazard& h) {
    Color c = h.color;
    Vec2 at = h.pos;
    f32 size = 24.0f;
    switch (h.kind) {
        case HazardKind::Circle: size = h.radius * 2.6f; break;
        case HazardKind::Lane:   at = h.pos + h.dir * (h.length * 0.5f); size = h.width * 2.0f; break;
        case HazardKind::Cone:   at = h.pos + h.dir * (h.radius * 0.55f); size = h.radius * 1.4f; break;
        default: break;
    }
    switch (h.fx) {
        case HazardFx::Fire:
            spawn_fx(w, at, FxID::Explosion, {255, 214, 170, 240}, size, 0.42f);
            spawn_particles(w, at, {255, 150, 60, 255}, 8, 90.0f, 0.4f);
            break;
        case HazardFx::Dark:
            spawn_fx(w, at, FxID::Smoke, {170, 120, 210, 220}, size * 0.9f, 0.6f, 7);
            spawn_particles(w, at, {190, 110, 240, 255}, 7, 70.0f, 0.4f);
            break;
        case HazardFx::Holy:
            spawn_fx(w, at, FxID::Holy, {255, 240, 200, 240}, size, 0.5f);
            break;
        case HazardFx::Water:
            spawn_fx(w, at, FxID::Ring, {150, 230, 220, 220}, size, 0.45f, 7);
            spawn_particles(w, at, {120, 210, 200, 255}, 8, 70.0f, 0.45f);
            break;
        case HazardFx::Dust:
            spawn_fx(w, at, FxID::Smoke, {205, 185, 150, 220}, size, 0.55f, 7);
            spawn_fx(w, at, FxID::Impact, {255, 225, 190, 230}, size * 0.7f, 0.25f);
            spawn_particles(w, at, {190, 170, 140, 255}, 8, 90.0f, 0.45f);
            break;
        case HazardFx::Coin:
            spawn_fx(w, at, FxID::Sparks, {255, 230, 160, 240}, size * 0.8f, 0.35f);
            spawn_particles(w, at, {235, 235, 245, 255}, 8, 80.0f, 0.4f);
            break;
        default:
            if (h.kind != HazardKind::Ring && h.kind != HazardKind::Push && h.kind != HazardKind::Pool)
                spawn_particles(w, at, {c.r, c.g, c.b, 255}, 8, 80.0f, 0.4f);
            break;
    }
}

void update_hazards(ECSWorld& world, FaithSystem& faith, CombatEvents& events, f32 dt) {
    const Entity p = world.player;
    const bool has_player = p != ENTITY_NONE && world.has(p, Component::Transform) &&
                            world.has(p, Component::Health);
    const Vec2 pfeet = has_player ? feet_of(world, p) : Vec2{-9999, -9999};

    for (int i = 0; i < MAX_HAZARDS; i++) {
        Hazard& h = world.hazards[i];
        if (!h.used) continue;

        // Muere con su dueno (las charcas tambien: el agua corrupta se va con su espiritu)
        if (h.owner != ENTITY_NONE) {
            const bool alive = world.is_alive(h.owner) && (!world.has(h.owner, Component::Health) ||
                                                           world.healths[h.owner].is_alive());
            if (!alive) { h = Hazard{}; continue; }
            if (h.follow_owner && world.has(h.owner, Component::Transform)) {
                const auto& ot = world.transforms[h.owner];
                h.pos = ot.position + Vec2{0.0f, 12.0f * ot.scale.y * 0.9f};
            }
        }

        const f32 before = h.t;
        h.t += dt;
        const f32 end = h.warn + h.active + hazard_linger(h);
        if (h.t >= end) { h = Hazard{}; continue; }

        const bool active = h.t >= h.warn && h.t < h.warn + h.active;
        if (h.t >= h.warn && before < h.warn && !h.burst_done) {
            h.burst_done = true;
            if (h.tag != HazardTag::Ripple) {
                burst_fx(world, h);
                if (h.kind == HazardKind::Circle || h.kind == HazardKind::Lane ||
                    h.kind == HazardKind::Cone || h.kind == HazardKind::Ring)
                    world.hazard_bursts++;
            }
        }
        if (!active || h.tag == HazardTag::Ripple) continue;

        // --- Viento: empuja mientras dura (el dash lo ignora: system_movement no aplica
        //     el knockback durante el dash) ---
        if (h.kind == HazardKind::Push) {
            if (has_player && hazard_hits(h, pfeet, 0.0f) && world.has(p, Component::Movement)) {
                auto& m = world.movements[p];
                const f32 along = m.knockback.dot(h.dir);
                if (along < h.force) m.knockback += h.dir * (h.force - along);
            }
            continue;
        }

        // --- Charca: muerde por contacto, con su propia cadencia ---
        if (h.kind == HazardKind::Pool) {
            h.tick -= dt;
            if (h.tick > 0.0f || h.damage <= 0.0f) continue;
            if (has_player && h.hits_player && hazard_hits(h, pfeet, 0.0f)) {
                if (hurt_player(world, faith, events, h, h.pos)) {
                    h.tick = POOL_BITE;
                    spawn_particles(world, pfeet, {h.color.r, h.color.g, h.color.b, 255}, 5, 40.0f, 0.4f);
                }
            }
            continue;
        }

        // --- Golpe de un solo uso: Circle / Lane / Cone / Ring ---
        if (h.hits_player && !h.hit_done && has_player && hazard_hits(h, pfeet, BODY)) {
            if (hurt_player(world, faith, events, h, h.pos)) {
                h.hit_done = true;
                on_player_hit(world, faith, h);
            } else if (world.healths[p].iframes_timer > 0.0f) {
                // Esquivado con i-frames (el dash a traves del barrido): ya no vuelve a
                // morder en este peligro, aunque los i-frames se acaben a mitad.
                h.hit_done = true;
            }
        }
        if (h.hits_enemies && h.damage > 0.0f && h.t - dt < h.warn + 0.02f) {
            for (u32 e = 0; e < world.entity_count; e++) {
                if (!world.is_alive(e) || !is_hostile(world, e) || !world.has(e, Component::Health)) continue;
                if (static_cast<Entity>(e) == h.owner || !world.healths[e].is_alive()) continue;
                if (world.enemy_ais[e].is_boss || world.enemy_ais[e].boss_group) continue;
                if (!hazard_hits(h, world.transforms[e].position + Vec2{0.0f, 9.0f}, 3.0f)) continue;
                if (apply_damage(world, ENTITY_NONE, e, h.damage))
                    on_enemy_killed(world, faith, e, events);
            }
        }
    }
}

// =============================================================================
// Arte procedural
// =============================================================================
static f32 smooth(f32 a, f32 b, f32 x) {
    f32 t = std::max(0.0f, std::min(1.0f, (x - a) / (b - a)));
    return t * t * (3.0f - 2.0f * t);
}

// Rellena una textura blanca cuyo alfa sale de `alpha(u, v)`, con u y v en [-1, 1]
template <typename F>
static u32 make_tex(Renderer& r, const char* name, int w, int h, F alpha) {
    std::vector<u8> rgba(static_cast<size_t>(w) * h * 4, 0);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            const f32 u = (x + 0.5f) / w * 2.0f - 1.0f;
            const f32 v = (y + 0.5f) / h * 2.0f - 1.0f;
            f32 a = std::max(0.0f, std::min(1.0f, alpha(u, v)));
            u8* px = &rgba[(static_cast<size_t>(y) * w + x) * 4];
            px[0] = px[1] = px[2] = 255;
            px[3] = static_cast<u8>(std::round(a * 255.0f));
        }
    }
    return r.create_texture_from_rgba(name, w, h, rgba.data(), true);
}

void init_hazard_art(Renderer& r, HazardArt& art, u32 tileset) {
    art.tileset = tileset;
    // El cuenco: borde mas vivo que el centro, que es lo que se lee como "zona" y no como mancha
    art.fill = make_tex(r, "@hz_fill", 96, 96, [](f32 u, f32 v) {
        f32 d = std::sqrt(u * u + v * v);
        f32 edge = std::max(0.0f, std::min(1.0f, (1.0f - d) * 40.0f));
        return (0.42f + 0.58f * smooth(0.45f, 1.0f, d)) * edge;
    });
    // El aro: una linea fina a 0,94 del radio con su halo
    art.rim = make_tex(r, "@hz_rim", 128, 128, [](f32 u, f32 v) {
        f32 d = std::sqrt(u * u + v * v);
        f32 core = std::exp(-((d - 0.94f) / 0.022f) * ((d - 0.94f) / 0.022f));
        f32 halo = std::exp(-((d - 0.94f) / 0.07f) * ((d - 0.94f) / 0.07f)) * 0.45f;
        return std::max(core, halo);
    });
    // El haz: perfil gaussiano a lo ancho, uniforme a lo largo (los trozos encadenan)
    art.band = make_tex(r, "@hz_band", 16, 32, [](f32, f32 v) {
        f32 core = std::exp(-(v / 0.22f) * (v / 0.22f));
        f32 halo = std::exp(-(v / 0.6f) * (v / 0.6f)) * 0.55f;
        return std::max(core, halo);
    });
    // El punto de luz (nucleo de los destellos)
    art.dot = make_tex(r, "@hz_dot", 32, 32, [](f32 u, f32 v) {
        f32 d2 = u * u + v * v;
        return std::exp(-d2 / 0.12f) + 0.35f * std::exp(-d2 / 0.45f) * (d2 < 1.0f ? 1.0f : 0.0f);
    });
    // El trozo de circunferencia: el mismo perfil, con los extremos en rampa (tienda). Los
    // trozos se solapan a medio largo y las rampas se suman: sin las costuras en "|||" que
    // salian con trozos de canto duro (visto en la burbuja del Sacerdote).
    art.seg = make_tex(r, "@hz_seg", 32, 32, [](f32 u, f32 v) {
        f32 core = std::exp(-(v / 0.22f) * (v / 0.22f));
        f32 halo = std::exp(-(v / 0.6f) * (v / 0.6f)) * 0.55f;
        return std::max(core, halo) * (1.0f - std::fabs(u));
    });
    // El carril: cantos largos encendidos, interior suave, y los extremos redondeados
    art.lane = make_tex(r, "@hz_lane", 64, 32, [](f32 u, f32 v) {
        f32 av = std::fabs(v), au = std::fabs(u);
        f32 edge = std::max(0.0f, std::min(1.0f, (1.0f - av) * 16.0f));
        f32 ends = std::max(0.0f, std::min(1.0f, (1.0f - au) * 24.0f));
        return (0.42f + 0.58f * smooth(0.62f, 0.92f, av)) * edge * ends;
    });
    for (int i = 0; i < HazardArt::SECTORS; i++) {
        const f32 half = art.sector_deg[i] * PI / 180.0f;
        char name[24];
        SDL_snprintf(name, sizeof(name), "@hz_sector%d", static_cast<int>(art.sector_deg[i]));
        art.sector[i] = make_tex(r, name, 128, 128, [half](f32 u, f32 v) {
            f32 d = std::sqrt(u * u + v * v);
            if (d > 1.0f) return 0.0f;
            f32 th = std::fabs(std::atan2(v, u));
            if (th > half) return 0.0f;
            f32 aa_r = std::max(0.0f, std::min(1.0f, (1.0f - d) * 48.0f));
            f32 aa_t = std::max(0.0f, std::min(1.0f, (half - th) * d * 48.0f + 0.15f));
            f32 rim = std::max(smooth(0.82f, 0.98f, d), smooth(half - 0.14f, half - 0.02f, th) * 0.8f);
            return (0.40f + 0.60f * rim) * aa_r * aa_t;
        });
    }
    art.ready = art.fill != TEXTURE_NONE && art.rim != TEXTURE_NONE && art.band != TEXTURE_NONE;
    SDL_Log("Hazard art: %s", art.ready ? "ok" : "SIN TEXTURAS (los telegrafos iran en rectangulos)");
}

// =============================================================================
// Dibujo
// =============================================================================
static Color with_alpha(Color c, f32 a) {
    return {c.r, c.g, c.b, static_cast<u8>(std::max(0.0f, std::min(255.0f, a)))};
}
static Color mix_white(Color c, f32 k, f32 a) {
    k = std::max(0.0f, std::min(1.0f, k));
    return {static_cast<u8>(c.r + (255 - c.r) * k), static_cast<u8>(c.g + (255 - c.g) * k),
            static_cast<u8>(c.b + (255 - c.b) * k),
            static_cast<u8>(std::max(0.0f, std::min(255.0f, a)))};
}

// La capa OSCURA de debajo: sobre la arena clara del desierto un rojo translucido no se
// leia (visto en la primera captura del Capitan). Un fondo oscuro del mismo tono y el
// relleno vivo encima dan contraste en cualquier suelo.
static Color shade(Color c, f32 a) {
    return {static_cast<u8>(c.r * 0.28f), static_cast<u8>(c.g * 0.22f), static_cast<u8>(c.b * 0.26f),
            static_cast<u8>(std::max(0.0f, std::min(255.0f, a)))};
}

static void quad(Renderer& r, u32 tex, Vec2 center, f32 w, f32 h, f32 deg, Color c) {
    const TextureEntry* t = r.get_texture(tex);
    if (!t || !t->texture) {
        r.draw_rect({center.x - w * 0.5f, center.y - h * 0.5f, w, h}, with_alpha(c, c.a * 0.5f));
        return;
    }
    r.draw_sprite_rect(tex, {0.0f, 0.0f, static_cast<f32>(t->width), static_cast<f32>(t->height)},
                       {center.x - w * 0.5f, center.y - h * 0.5f, w, h}, c, false, deg);
}

void hz_draw_disc(Renderer& r, const HazardArt& art, Vec2 c, f32 radius, Color col) {
    if (radius <= 0.2f) return;
    quad(r, art.fill, c, radius * 2.0f, radius * 2.0f, 0.0f, col);
}
void hz_draw_beam(Renderer& r, const HazardArt& art, Vec2 a, Vec2 b, f32 thick, Color col) {
    Vec2 d = b - a;
    f32 len = d.length();
    if (len < 0.5f) return;
    const f32 deg = std::atan2(d.y, d.x) * 57.2957795f;
    quad(r, art.band, (a + b) * 0.5f, len, thick, deg, col);
}
void hz_draw_sparkle(Renderer& r, const HazardArt& art, Vec2 c, f32 size, f32 deg, Color col) {
    // Estrella de cuatro puntas: dos haces cruzados y un nucleo. Suave a cualquier tamano
    // (las celdas del tileset van con filtrado al vecino y a 8 unidades se escalonan).
    const u32 core = (art.dot != TEXTURE_NONE) ? art.dot : art.fill;
    quad(r, art.band, c, size * 2.2f, size * 0.38f, deg, col);
    quad(r, art.band, c, size * 2.2f, size * 0.38f, deg + 90.0f, col);
    quad(r, core, c, size * 1.1f, size * 1.1f, 0.0f, mix_white(col, 0.7f, col.a));
}

void hz_draw_sector(Renderer& r, const HazardArt& art, Vec2 apex, Vec2 dir, f32 radius,
                    f32 half_angle, Color col) {
    // La textura mas cercana en semiapertura: la diferencia de unos grados no se ve, la
    // de un abanico mal medido si (el golpe saldria donde no se pinto).
    int best = 0;
    f32 want = half_angle * 180.0f / PI, err = 1e9f;
    for (int i = 0; i < HazardArt::SECTORS; i++) {
        f32 e = std::fabs(art.sector_deg[i] - want);
        if (e < err) { err = e; best = i; }
    }
    const f32 deg = std::atan2(dir.y, dir.x) * 57.2957795f;
    quad(r, art.sector[best], apex, radius * 2.0f, radius * 2.0f, deg, col);
}

static void draw_lane_rect(Renderer& r, const HazardArt& art, Vec2 from, Vec2 dir, f32 len, f32 w,
                           Color col) {
    if (len <= 0.5f) return;
    const f32 deg = std::atan2(dir.y, dir.x) * 57.2957795f;
    quad(r, art.lane, from + dir * (len * 0.5f), len, w, deg, col);
}

// Circunferencia de GROSOR FIJO en unidades: trozos de haz tangentes, saltando el hueco
// (`gap_half` > 0, centrado en `gap_dir`). Es el borde de todo lo redondo: el aro de la
// textura escala con el radio y en un circulo de 13 unidades quedaba de medio pixel.
static void draw_circle_band(Renderer& r, const HazardArt& art, Vec2 center, f32 radius, f32 thick,
                             Vec2 gap_dir, f32 gap_half, Color col, bool mark_gap = true) {
    if (radius < 1.0f) return;
    const f32 circ = 2.0f * PI * radius;
    // Paso de ~4 unidades y trozos del DOBLE de largo: cada punto lo cubren dos rampas
    const int n = std::max(18, std::min(240, static_cast<int>(circ / 4.0f)));
    const f32 step = circ / n;
    const f32 cos_gap = std::cos(gap_half);
    const u32 tex = (art.seg != TEXTURE_NONE) ? art.seg : art.band;
    for (int i = 0; i < n; i++) {
        const f32 a = (i + 0.5f) * 2.0f * PI / n;
        const Vec2 rad = {std::cos(a), std::sin(a)};
        if (gap_half > 0.0f && rad.dot(gap_dir) >= cos_gap) continue;
        quad(r, tex, center + rad * radius, step * 2.0f, thick, a * 57.2957795f + 90.0f, col);
    }
    // Los dos cantos del hueco, marcados: es por donde se pasa
    if (gap_half > 0.0f && mark_gap) {
        const f32 base = std::atan2(gap_dir.y, gap_dir.x);
        for (int s = -1; s <= 1; s += 2) {
            const f32 a = base + s * gap_half;
            const Vec2 c = center + Vec2{std::cos(a), std::sin(a)} * radius;
            quad(r, art.fill, c, thick * 0.8f, thick * 0.8f, 0.0f, mix_white(col, 0.6f, col.a));
        }
    }
}

static void draw_ring_band(Renderer& r, const HazardArt& art, const Hazard& h, f32 radius,
                           f32 thick, Color col, bool mark_gap = true) {
    draw_circle_band(r, art, h.pos, radius, thick, h.dir, h.angle, col, mark_gap);
}

// El borde vivo de un circulo: nucleo fino + halo, a grosor fijo
static void draw_edge(Renderer& r, const HazardArt& art, Vec2 c, f32 radius, Color col, f32 a) {
    draw_circle_band(r, art, c, radius, 6.0f, {1, 0}, 0.0f, Color{col.r, col.g, col.b, static_cast<u8>(std::min(255.0f, a * 0.35f))});
    draw_circle_band(r, art, c, radius, 2.6f, {1, 0}, 0.0f, Color{col.r, col.g, col.b, static_cast<u8>(std::min(255.0f, a))});
}

void hz_draw_rim(Renderer& r, const HazardArt& art, Vec2 c, f32 radius, Color col) {
    if (radius <= 0.2f) return;
    draw_edge(r, art, c, radius, col, col.a);
}

void render_hazards(Renderer& r, const ECSWorld& world, const HazardArt& art, Rect view) {
    const f32 time = world.time;
    const Rect cull = {view.x - 220.0f, view.y - 220.0f, view.w + 440.0f, view.h + 440.0f};
    for (int i = 0; i < MAX_HAZARDS; i++) {
        const Hazard& h = world.hazards[i];
        if (!h.used || !cull.contains(h.pos)) continue;
        const Color c = h.color;
        // k: cuanto aviso se ha consumido (0 al nacer, 1 al estallar). Se acelera el latido
        // del borde segun se acerca el golpe: es lo que dice CUANDO sin leer ningun numero.
        const f32 k = (h.warn > 0.0f) ? std::min(1.0f, h.t / h.warn) : 1.0f;
        const bool warning = h.t < h.warn;
        const f32 pulse = 0.5f + 0.5f * std::sin(time * (9.0f + 22.0f * k * k) + i);
        const f32 hot = warning ? smooth(0.72f, 1.0f, k) : 1.0f;   // el ultimo tramo, blanco
        // f: tramo de efecto + cola (0 al estallar, 1 al desaparecer)
        const f32 tail = h.active + hazard_linger(h);
        const f32 f = warning ? 0.0f : std::min(1.0f, (h.t - h.warn) / std::max(0.01f, tail));

        switch (h.kind) {
        case HazardKind::Circle: {
            if (warning) {
                hz_draw_disc(r, art, h.pos, h.radius, shade(c, 80.0f + 40.0f * k));
                hz_draw_disc(r, art, h.pos, h.radius, with_alpha(c, 55.0f + 60.0f * k));
                const f32 kk = k * k * (3.0f - 2.0f * k);
                hz_draw_disc(r, art, h.pos, h.radius * kk, mix_white(c, 0.15f + 0.4f * hot, 95.0f + 90.0f * k));
                draw_edge(r, art, h.pos, h.radius, mix_white(c, 0.3f + 0.65f * hot, 255), 150.0f + 100.0f * pulse);
            } else {
                hz_draw_disc(r, art, h.pos, h.radius * (1.0f + 0.12f * f), mix_white(c, 0.75f * (1.0f - f), 235.0f * (1.0f - f)));
                draw_edge(r, art, h.pos, h.radius * (1.0f + 0.4f * f), mix_white(c, 0.8f, 255), 230.0f * (1.0f - f));
            }
            break;
        }
        case HazardKind::Lane: {
            if (warning) {
                draw_lane_rect(r, art, h.pos, h.dir, h.length, h.width, shade(c, 70.0f + 40.0f * k));
                draw_lane_rect(r, art, h.pos, h.dir, h.length, h.width, with_alpha(c, 50.0f + 60.0f * k));
                draw_lane_rect(r, art, h.pos, h.dir, h.length * k, h.width,
                               mix_white(c, 0.1f + 0.4f * hot, 90.0f + 100.0f * k));
                draw_lane_rect(r, art, h.pos, h.dir, h.length, h.width,
                               mix_white(c, 0.35f + 0.6f * hot, 40.0f + 70.0f * pulse));
                // Flechas que corren por el carril: dicen hacia DONDE va el golpe
                if (art.tileset != TEXTURE_NONE && h.length > 30.0f) {
                    const f32 step = 26.0f;
                    const int n = static_cast<int>(h.length / step);
                    const f32 deg = std::atan2(h.dir.y, h.dir.x) * 57.2957795f;
                    for (int a = 0; a < n; a++) {
                        f32 along = std::fmod(time * 70.0f + a * step, h.length - 8.0f) + 4.0f;
                        f32 sz = std::min(12.0f, h.width * 0.8f);
                        r.draw_sprite_rect(art.tileset, get_tile_src(TileSprite::Arrow),
                                           {h.pos.x + h.dir.x * along - sz * 0.5f, h.pos.y + h.dir.y * along - sz * 0.5f, sz, sz},
                                           mix_white(c, 0.5f, 90.0f + 90.0f * pulse), false, deg);
                    }
                }
            } else {
                const f32 a = (h.tag == HazardTag::Chain) ? 160.0f : 235.0f;
                draw_lane_rect(r, art, h.pos, h.dir, h.length, h.width * (1.0f + 0.15f * f),
                               mix_white(c, 0.7f * (1.0f - f), a * (1.0f - f)));
            }
            if (h.tag == HazardTag::Chain && !warning) {
                // La cadena misma: eslabones a lo largo del carril mientras restalla
                const f32 deg = std::atan2(h.dir.y, h.dir.x) * 57.2957795f;
                const int links = static_cast<int>(h.length / 7.0f);
                for (int l = 0; l < links; l++) {
                    Vec2 at = h.pos + h.dir * (l * 7.0f + 3.5f);
                    quad(r, art.rim, at, 7.0f, 4.5f, deg + ((l % 2) ? 0.0f : 90.0f),
                         Color{230, 205, 140, static_cast<u8>(240.0f * (1.0f - f))});
                }
            }
            break;
        }
        case HazardKind::Cone: {
            if (warning) {
                hz_draw_sector(r, art, h.pos, h.dir, h.radius, h.angle, shade(c, 70.0f + 40.0f * k));
                hz_draw_sector(r, art, h.pos, h.dir, h.radius, h.angle, with_alpha(c, 40.0f + 55.0f * k));
                hz_draw_sector(r, art, h.pos, h.dir, h.radius * k, h.angle,
                               mix_white(c, 0.12f + 0.45f * hot, 80.0f + 90.0f * k));
                hz_draw_sector(r, art, h.pos, h.dir, h.radius, h.angle,
                               mix_white(c, 0.3f + 0.6f * hot, 30.0f + 50.0f * pulse));
            } else {
                hz_draw_sector(r, art, h.pos, h.dir, h.radius * (1.0f + 0.08f * f), h.angle,
                               mix_white(c, 0.75f * (1.0f - f), 240.0f * (1.0f - f)));
            }
            break;
        }
        case HazardKind::Ring: {
            const bool closing = h.r_from > h.radius;
            if (warning) {
                if (closing) {
                    // La deuda: el aro entero ya esta donde va a empezar a cerrarse
                    draw_ring_band(r, art, h, h.r_from, h.width * 1.4f,
                                   mix_white(c, 0.2f + 0.6f * hot, 70.0f + 120.0f * k * pulse + 40.0f));
                } else {
                    // El epicentro se carga y la onda se ADELANTA en tenue: se ve el hueco antes
                    hz_draw_disc(r, art, h.pos, h.r_from + 10.0f * k, with_alpha(c, 60.0f + 80.0f * k));
                    draw_edge(r, art, h.pos, h.r_from + 10.0f * k, mix_white(c, 0.4f + 0.5f * hot, 255), 150.0f + 100.0f * pulse);
                    const f32 preview = h.r_from + (h.radius - h.r_from) * (0.22f + 0.1f * k);
                    draw_ring_band(r, art, h, preview, h.width * 1.2f, with_alpha(c, 45.0f + 75.0f * k));
                }
            } else {
                const f32 rr = ring_radius(h);
                const f32 fade = (h.t > h.warn + h.active) ? 1.0f - (h.t - h.warn - h.active) / hazard_linger(h) : 1.0f;
                draw_ring_band(r, art, h, rr, h.width * 2.6f, shade(c, 110.0f * fade), false);
                draw_ring_band(r, art, h, rr, h.width * 2.2f, mix_white(c, 0.15f, 120.0f * fade), false);
                draw_ring_band(r, art, h, rr, h.width * 1.1f, mix_white(c, 0.55f, 245.0f * fade));
            }
            break;
        }
        case HazardKind::Pool: {
            // Crece en el aviso y se apaga en su ultimo tramo
            const f32 grow = warning ? (0.35f + 0.65f * k) : 1.0f;
            const f32 left = h.warn + h.active - h.t;
            const f32 fade = std::max(0.0f, std::min(1.0f, left / POOL_FADE));
            const f32 rad = h.radius * grow;
            if (h.tag == HazardTag::Ripple) {
                // El Espiritu bajo el agua: sombra oscura y ondas que se abren desde el
                hz_draw_disc(r, art, h.pos, rad * 0.8f, Color{8, 30, 34, static_cast<u8>(150 * fade)});
                for (int w = 0; w < 3; w++) {
                    f32 ph = std::fmod(time * 1.4f + w / 3.0f, 1.0f);
                    hz_draw_rim(r, art, h.pos, rad * (0.4f + 0.9f * ph),
                                with_alpha(c, 200.0f * (1.0f - ph) * fade));
                }
                break;
            }
            hz_draw_disc(r, art, h.pos, rad, shade(c, 80.0f * fade));
            hz_draw_disc(r, art, h.pos, rad, with_alpha(c, 105.0f * fade));
            const f32 wob = 1.0f + 0.04f * std::sin(time * 3.0f + i);
            draw_edge(r, art, h.pos, rad * wob, mix_white(c, 0.25f, 255), (120.0f + 60.0f * pulse) * fade);
            // Burbujas: sitio por indice, fase por tiempo
            for (int b = 0; b < 3; b++) {
                f32 ph = std::fmod(time * 0.9f + b * 0.37f + i * 0.13f, 1.0f);
                f32 ang = (i * 2.4f + b * 2.1f);
                Vec2 at = h.pos + Vec2{std::cos(ang), std::sin(ang) * 0.6f} * (rad * 0.55f);
                hz_draw_disc(r, art, at - Vec2{0.0f, ph * 3.0f}, 1.2f + 2.2f * ph,
                             mix_white(c, 0.5f, 200.0f * (1.0f - ph) * fade));
            }
            break;
        }
        case HazardKind::Push: {
            const f32 on = warning ? k : (1.0f - f * f);
            draw_lane_rect(r, art, h.pos, h.dir, h.length, h.width, with_alpha(c, 22.0f + 22.0f * on));
            // Rafagas: trazos que corren en el sentido del viento
            const int n = std::max(10, static_cast<int>(h.length * h.width / 900.0f));
            const f32 deg = std::atan2(h.dir.y, h.dir.x) * 57.2957795f;
            const Vec2 side = {-h.dir.y, h.dir.x};
            const f32 speed = warning ? 60.0f : 260.0f;
            for (int s = 0; s < n; s++) {
                f32 across = (std::fmod(s * 0.618f + i * 0.31f, 1.0f) - 0.5f) * h.width * 0.92f;
                f32 along = std::fmod(time * speed + s * 47.0f, h.length);
                f32 len = warning ? 10.0f : 22.0f;
                Vec2 at = h.pos + h.dir * along + side * across;
                quad(r, art.band, at, len, 3.0f, deg, mix_white(c, 0.6f, (warning ? 90.0f * k : 190.0f) * on));
            }
            break;
        }
        default: break;
        }
    }
}
