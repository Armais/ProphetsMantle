#pragma once

// =============================================================================
// MANTLE - Códice: lo que el profeta ha visto
// =============================================================================
//
// Cinco pestañas (Capítulos / Bestiario / Milagros / Objetos / Logros) con las
// entradas que el jugador ha encontrado. Cada entrada lleva su pasaje de 1-2 Reyes
// (game/lore.h). El descubrimiento persiste entre partidas en los bitmasks de
// SaveData, y el contador "Descubierto X/Y" cuenta las cuatro pestañas MAS los relatos
// (game/events.h): un encuentro es contenido encontrado igual que un objeto o un bicho.
//
// Es el "bestiario completo" que el GDD hereda en la Ascensión y las "lore
// entries" que systems/progression.h prometía.
//
// =============================================================================

#include "render/renderer.h"
#include "render/sprites.h"   // namespace Portrait
#include "input/input.h"
#include "game/lore.h"
#include "game/items.h"
#include "game/events.h"
#include "systems/save.h"
#include "systems/achievements.h"

// La pestaña de milagros se llamaba "Relics" y listaba milagros: el nombre estaba
// sencillamente equivocado. Las reliquias son objetos y viven en Items.
// Los logros son la quinta pestaña y no una pantalla aparte: el Códice ya ES "lo que el
// profeta ha visto", y una pantalla nueva colgando del menú principal sería un sitio más
// donde no mirar. Ojo: NO cuentan para el contador "Descubierto X/Y" — ese mide
// contenido encontrado, y un logro es una proeza, no una entrada de bestiario.
enum class CodexTab : u8 { Chapters, Bestiary, Miracles, Items, Achievements, COUNT };

struct CodexScreen {
    CodexTab tab = CodexTab::Chapters;
    int selected = 0;
    int scroll   = 0;

    // true = el jugador ha pedido cerrar
    bool update(Input& input, const SaveData& save, const ItemRegistry& registry);
    // `portraits` puede ser TEXTURE_NONE: el Codice se dibuja igual, solo que sin retrato.
    void render(Renderer& renderer, const SaveData& save, const ItemRegistry& registry,
                u32 tileset, u32 portraits = TEXTURE_NONE) const;

    static bool knows_achievement(const SaveData& s, AchievementID a) {
        return (s.achievements >> static_cast<u32>(a)) & 1u;
    }

    int entry_count(const SaveData& save, const ItemRegistry& registry) const;
};

// Marcar descubrimientos (los llama el Engine cuando ocurren)
inline void codex_see_enemy(SaveData& s, EnemyType t)   { s.codex_enemies  |= (1u << static_cast<u32>(t)); }
inline void codex_see_item(SaveData& s, u32 item_id)    { if (item_id < 32) s.codex_items |= (1u << item_id); }
inline void codex_see_miracle(SaveData& s, MiracleID m) { s.codex_miracles |= (1u << static_cast<u32>(m)); }
inline void codex_see_chapter(SaveData& s, int chapter) { if (chapter >= 1 && chapter <= 32) s.codex_chapters |= (1u << (chapter - 1)); }
// Relatos: un bit por id de `namespace Ev`. u64 porque el pozo ya son 35.
inline void codex_see_event(SaveData& s, int id)        { if (id >= 0 && id < 64) s.codex_events |= (1ull << id); }

inline bool codex_knows_enemy(const SaveData& s, EnemyType t)   { return (s.codex_enemies  >> static_cast<u32>(t)) & 1u; }
inline bool codex_knows_item(const SaveData& s, u32 item_id)    { return item_id < 32 && ((s.codex_items >> item_id) & 1u); }
inline bool codex_knows_miracle(const SaveData& s, MiracleID m) { return (s.codex_miracles >> static_cast<u32>(m)) & 1u; }
inline bool codex_knows_chapter(const SaveData& s, int chapter) { return chapter >= 1 && chapter <= 32 && ((s.codex_chapters >> (chapter - 1)) & 1u); }
inline bool codex_knows_event(const SaveData& s, int id)        { return id >= 0 && id < 64 && ((s.codex_events >> id) & 1ull); }

// Cuantos relatos de un capitulo se han visto ya. Es lo que hace que perderse un
// encuentro SE NOTE: el mapa coloca 2-3 salas de un pozo de 3-4, asi que en una sola
// partida no se ven todos ni queriendo.
inline int codex_chapter_events(const SaveData& s, int chapter, int& out_total) {
    EventPool p = get_chapter_events(chapter);
    out_total = p.count;
    int seen = 0;
    for (int i = 0; i < p.count; i++) if (codex_knows_event(s, p.first + i)) seen++;
    return seen;
}
