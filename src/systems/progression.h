#pragma once

// =============================================================================
// MANTLE - Meta-Progression System (GDD 3.1 step 7)
// =============================================================================
//
// Persists across runs. Players spend "offerings" in El Santuario to unlock:
// - Ranks of permanent stats (vida, dano, velocidad, fe inicial)
// - DONES que cambian la partida (levantarse una vez, objeto inicial, re-sortear cartas,
//   mas ofrendas, mas huecos en la alforja, agua al empezar cada capitulo...)
// - Lo de Eliseo (herencia, tercer slot, discipulo)
// - Objetos que entran en el sorteo, y cosmeticos
//
// Rediseno 2026-10-03 (el usuario: "lo que compras ahi no afecta lo suficiente al
// gameplay"). Las mejoras pasan a tener RANGOS, cada rango dice lo que da AHORA y lo que
// dara el SIGUIENTE, y los dones nuevos se notan desde el primer minuto de partida. Los
// valores de cada rango viven aqui (`meta_*`), y los lee el Engine al crear al profeta:
// una sola tabla para lo que se pinta y lo que se aplica.
//
// =============================================================================

#include "core/types.h"
#include <vector>

// Ids ESTABLES: son bits de SaveData::upgrades_bought e indices de SaveData::upgrade_ranks.
// Del 0 al 14 son los de siempre (una partida v11 con la mejora comprada llega con rango 1).
namespace Meta {
    constexpr u32 VIDA        = 0;   // Provision del Cuervo: +vida maxima
    constexpr u32 DANO        = 1;   // Fuego Interior: +dano base
    constexpr u32 VELOCIDAD   = 2;   // Sandalias Rapidas
    constexpr u32 FE          = 3;   // Fe Renovada: fe inicial
    constexpr u32 HERENCIA    = 4;   // Herencia Ampliada (Eliseo)
    constexpr u32 ALTAR       = 5;   // Altar Portatil: orar sin altar
    constexpr u32 DOBLE       = 6;   // Doble Porcion Mejorada (Eliseo, 3 slots)
    constexpr u32 ESCUELA     = 7;   // La Escuela de Profetas (Eliseo, discipulo)
    constexpr u32 ARCO        = 8;   // desbloquea el Arco de Joas
    constexpr u32 HIGOS       = 9;   // desbloquea los Higos de Ezequias
    constexpr u32 PANES       = 10;  // desbloquea los Panes de Cebada
    constexpr u32 RELOJ       = 11;  // desbloquea El Reloj de Acaz
    constexpr u32 SKIN_BRASA  = 12;
    constexpr u32 SKIN_JORDAN = 13;
    constexpr u32 SKIN_HOREB  = 14;
    constexpr u32 ALIENTO     = 15;  // Aliento de Vida: te levantas al caer
    constexpr u32 SUERTES     = 16;  // Echar Suertes: re-sortear las cartas
    constexpr u32 PRIMICIAS   = 17;  // Primicias: objeto a elegir al empezar
    constexpr u32 DIEZMO      = 18;  // Diezmo: +% de ofrendas
    constexpr u32 ALFORJA     = 19;  // La Alforja del Peregrino: +1 hueco de consumible
    constexpr u32 TINAJA      = 20;  // La Tinaja de Sarepta: agua al empezar cada capitulo
    constexpr u32 COUNT       = 21;
}

// Pestanas de El Santuario. El orden es el de las pestanas en pantalla.
enum class MetaCategory : u8 { Prophet, Gifts, Elisha, Treasure, Cosmetic, COUNT };

constexpr int META_MAX_RANK = 5;

struct MetaUpgrade {
    u32 id;
    const char* name;
    const char* description;   // que hace, en una frase (sin cifras de rango)
    int cost;                  // coste del SIGUIENTE rango (lo mantiene `refresh_cost`)
    bool unlocked;             // rank > 0
    // Objeto que esta mejora mete en el sorteo (ItemDef::id), o NO_ITEM.
    static constexpr u32 NO_ITEM = 0xFFFFFFFFu;
    u32 unlocks_item = NO_ITEM;
    // Skin del cayado que abre esta mejora (ver get_staff_skin), o NO_ITEM.
    u32 unlocks_skin = NO_ITEM;

    MetaCategory category = MetaCategory::Gifts;
    int  max_rank = 1;
    int  costs[META_MAX_RANK] = {};   // coste de cada rango
    int  rank = 0;                    // rangos comprados
    int  icon = -1;                   // celda de icons.png (render/sprites.h, Icon)
    const char* quote = "";           // la linea de sabor del panel de detalle

    bool maxed() const { return rank >= max_rank; }
    int  next_cost() const { return maxed() ? 0 : costs[rank]; }
};

struct ProgressionSystem {
    int offerings = 0;
    std::vector<MetaUpgrade> upgrades;

    void init();
    // Compra el SIGUIENTE rango. false si ya esta al maximo o no llegan las ofrendas.
    bool purchase(u32 upgrade_id);
    bool is_unlocked(u32 upgrade_id) const;   // rango >= 1
    int  rank_of(u32 upgrade_id) const;
    const MetaUpgrade* get(u32 upgrade_id) const;
    void add_offerings(int amount);

    // Rangos comprados / rangos que existen, para el "N/M" de El Santuario.
    int ranks_bought() const;
    int ranks_total() const;
    // La mejora mas barata que se puede comprar AHORA (nullptr si ninguna): la usa la
    // pantalla de fin de partida para decir "ya puedes ofrecer X". Si no llega para
    // ninguna, `cheapest_missing` dice cual queda mas cerca y cuanto falta.
    const MetaUpgrade* cheapest_affordable() const;
    const MetaUpgrade* cheapest_pending() const;
};

// =============================================================================
// Lo que da cada rango. Lo aplica el Engine y lo PINTA El Santuario: una sola tabla.
// =============================================================================
inline f32 meta_hp(int r)            { return 10.0f * r; }            // +10 vida por rango
inline f32 meta_damage(int r)        { return 2.0f * r; }             // +2 dano por rango
inline f32 meta_speed_mult(int r)    { return 1.0f + 0.07f * r; }     // +7% por rango
inline f32 meta_start_faith(int r)   { return 50.0f + 10.0f * r; }    // 50 / 60 / 70
inline f32 meta_inherit_ratio(int r) { return 0.50f + 0.10f * r; }    // 50% / 60% / 70%
inline int meta_altar_prayers(int r) { return r; }                    // oraciones por capitulo
inline int meta_revives(int r)       { return r; }                    // levantarse por intento
inline int meta_rerolls(int r)       { return r; }                    // re-sorteos por capitulo
inline f32 meta_offering_mult(int r) { return 1.0f + 0.15f * r; }     // +15% por rango
inline int meta_belt_slots(int r)    { return 2 + r; }                // 2 / 3 / 4 huecos
inline int meta_jar_water(int r)     { return r; }                    // Aguas del Jordan por capitulo

// Texto de lo que da un rango concreto ("+20 de vida maxima"). Rango 0 = "Nada aun".
// `buf` sale siempre terminado. Lo usa el panel de detalle de El Santuario para el
// "Ahora / Siguiente", y el resumen de bendiciones al empezar partida.
void meta_rank_text(const MetaUpgrade& u, int rank, char* buf, int n);
