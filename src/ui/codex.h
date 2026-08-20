#pragma once

// =============================================================================
// MANTLE - Códice: lo que el profeta ha visto
// =============================================================================
//
// Cuatro pestañas (Capítulos / Bestiario / Milagros / Objetos) con las entradas
// que el jugador ha encontrado. Cada entrada lleva su pasaje de 1-2 Reyes
// (game/lore.h). El descubrimiento persiste entre partidas en los bitmasks de
// SaveData, y el contador "Descubierto X/Y" cuenta las cuatro pestañas.
//
// Es el "bestiario completo" que el GDD hereda en la Ascensión y las "lore
// entries" que systems/progression.h prometía.
//
// =============================================================================

#include "render/renderer.h"
#include "input/input.h"
#include "game/lore.h"
#include "game/items.h"
#include "systems/save.h"

// La pestaña de milagros se llamaba "Relics" y listaba milagros: el nombre estaba
// sencillamente equivocado. Las reliquias son objetos y viven en Items.
enum class CodexTab : u8 { Chapters, Bestiary, Miracles, Items, COUNT };

struct CodexScreen {
    CodexTab tab = CodexTab::Chapters;
    int selected = 0;
    int scroll   = 0;

    // true = el jugador ha pedido cerrar
    bool update(Input& input, const SaveData& save, const ItemRegistry& registry);
    void render(Renderer& renderer, const SaveData& save, const ItemRegistry& registry, u32 tileset) const;

    int entry_count(const SaveData& save, const ItemRegistry& registry) const;
};

// Marcar descubrimientos (los llama el Engine cuando ocurren)
inline void codex_see_enemy(SaveData& s, EnemyType t)   { s.codex_enemies  |= (1u << static_cast<u32>(t)); }
inline void codex_see_item(SaveData& s, u32 item_id)    { if (item_id < 32) s.codex_items |= (1u << item_id); }
inline void codex_see_miracle(SaveData& s, MiracleID m) { s.codex_miracles |= (1u << static_cast<u32>(m)); }
inline void codex_see_chapter(SaveData& s, int chapter) { if (chapter >= 1 && chapter <= 32) s.codex_chapters |= (1u << (chapter - 1)); }

inline bool codex_knows_enemy(const SaveData& s, EnemyType t)   { return (s.codex_enemies  >> static_cast<u32>(t)) & 1u; }
inline bool codex_knows_item(const SaveData& s, u32 item_id)    { return item_id < 32 && ((s.codex_items >> item_id) & 1u); }
inline bool codex_knows_miracle(const SaveData& s, MiracleID m) { return (s.codex_miracles >> static_cast<u32>(m)) & 1u; }
inline bool codex_knows_chapter(const SaveData& s, int chapter) { return chapter >= 1 && chapter <= 32 && ((s.codex_chapters >> (chapter - 1)) & 1u); }
