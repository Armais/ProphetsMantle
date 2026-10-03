#pragma once

// =============================================================================
// MANTLE - Peligros de suelo telegrafiados (la caja de herramientas de los jefes)
// =============================================================================
//
// Lo que va a doler se PINTA en el suelo antes de doler (2026-10-03). Los datos viven en
// `ECSWorld::hazards` (core/ecs.h: `Hazard`, `HazardKind`); aqui estan los constructores,
// la simulacion y el dibujo. Reglas que no se negocian:
//
//   - Un peligro tiene AVISO (se llena y se enciende segun se acaba el tiempo: el jugador
//     lee CUANDO), EFECTO (solo aqui hace dano) y una cola sin dano para el fogonazo.
//   - El dano al profeta respeta lo mismo que un proyectil: i-frames del golpe, i-frames
//     del dash (que es lo que permite DASHEAR a traves de un barrido) y el modo dios.
//   - Muere con su dueno: un jefe abatido no deja su lluvia de fuego cayendo.
//   - Se dibuja en el SUELO: despues del terreno y antes de personajes, sombras y props.
//
// =============================================================================

#include "core/ecs.h"

struct FaithSystem;
struct CombatEvents;
struct Renderer;

// -----------------------------------------------------------------------------
// Constructores: rellenan un Hazard (quien llama puede retocar campos) y spawn_hazard
// lo mete en la piscina.
// -----------------------------------------------------------------------------
Hazard hz_circle(Entity owner, Vec2 at, f32 radius, f32 warn, f32 damage, Color color,
                 HazardFx fx = HazardFx::None);
Hazard hz_lane(Entity owner, Vec2 from, Vec2 dir, f32 length, f32 width, f32 warn, f32 active,
               f32 damage, Color color, HazardFx fx = HazardFx::None);
Hazard hz_cone(Entity owner, Vec2 apex, Vec2 dir, f32 radius, f32 half_angle, f32 warn,
               f32 damage, Color color, HazardFx fx = HazardFx::None);
// Onda: de `r_from` a `r_to` en `travel` segundos tras `warn`. Con r_from > r_to se CIERRA.
// `gap_half` > 0 deja un hueco centrado en `gap_dir` por donde no hace dano.
Hazard hz_ring(Entity owner, Vec2 center, f32 r_from, f32 r_to, f32 band, f32 warn, f32 travel,
               f32 damage, Vec2 gap_dir, f32 gap_half, Color color);
Hazard hz_pool(Entity owner, Vec2 at, f32 radius, f32 life, f32 damage, Color color);
Hazard hz_push(Entity owner, Vec2 from, Vec2 dir, f32 length, f32 width, f32 warn, f32 active,
               f32 force, Color color);

// Devuelve el indice en la piscina, o -1 si esta llena (el jefe pierde ESE ataque, nunca
// el juego). Loguea `Hazard <Tipo> spawned (...)`: es la linea que miran los guiones.
int  spawn_hazard(ECSWorld& world, const Hazard& h);

// Una vez por frame en Playing, despues de system_collision (necesita fe y eventos).
void update_hazards(ECSWorld& world, FaithSystem& faith, CombatEvents& events, f32 dt);

void clear_hazards(ECSWorld& world);                    // todos (cambio de sala, retry...)
void clear_hazards_of(ECSWorld& world, Entity owner);   // los de un dueno que ha caido
int  count_hazards_of(const ECSWorld& world, Entity owner, HazardKind kind);
// Sanar Aguas (2 Reyes 2:21): las charcas que toca el agua sanada se disuelven
int  cleanse_pools(ECSWorld& world, Vec2 at, f32 radius);
const char* hazard_kind_name(HazardKind k);

// -----------------------------------------------------------------------------
// Dibujo
// -----------------------------------------------------------------------------
// Texturas procedurales (se generan en Engine::init, como las del Renderer): nada de PNG
// nuevos en assets/. Todas BLANCAS: el color lo pone el tinte de cada peligro.
struct HazardArt {
    u32 fill   = TEXTURE_NONE;   // disco con el borde mas vivo que el centro (un cuenco de luz)
    u32 rim    = TEXTURE_NONE;   // aro fino con halo (el borde que late)
    u32 band   = TEXTURE_NONE;   // perfil de haz: brillo en el centro, se apaga a los lados
    u32 dot    = TEXTURE_NONE;   // punto de luz: brillante en el centro, se apaga hacia fuera
    u32 seg    = TEXTURE_NONE;   // el mismo haz con los extremos en rampa: trozos que se solapan sin costura
    u32 lane   = TEXTURE_NONE;   // rectangulo con los cantos encendidos
    static constexpr int SECTORS = 3;
    u32 sector[SECTORS] = {TEXTURE_NONE, TEXTURE_NONE, TEXTURE_NONE};   // abanicos con el apice en el centro
    f32 sector_deg[SECTORS] = {40.0f, 55.0f, 70.0f};                     // su semiapertura
    u32 tileset = TEXTURE_NONE;  // flechas de los carriles, estrellas del aturdido
    bool ready = false;
};
void init_hazard_art(Renderer& renderer, HazardArt& art, u32 tileset);

// Pasada de SUELO (dentro de la pasada de mundo, antes de draw_sorted). `view` acota.
void render_hazards(Renderer& renderer, const ECSWorld& world, const HazardArt& art, Rect view);

// Piezas sueltas que tambien usan los jefes para sus marcas (bosses.cpp)
void hz_draw_disc(Renderer& r, const HazardArt& art, Vec2 c, f32 radius, Color col);
void hz_draw_rim(Renderer& r, const HazardArt& art, Vec2 c, f32 radius, Color col);
void hz_draw_beam(Renderer& r, const HazardArt& art, Vec2 a, Vec2 b, f32 thick, Color col);
void hz_draw_sector(Renderer& r, const HazardArt& art, Vec2 apex, Vec2 dir, f32 radius,
                    f32 half_angle, Color col);
void hz_draw_sparkle(Renderer& r, const HazardArt& art, Vec2 c, f32 size, f32 deg, Color col);
