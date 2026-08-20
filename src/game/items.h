#pragma once

// =============================================================================
// MANTLE - Item System (GDD 7.3)
// =============================================================================

#include "core/types.h"
#include <algorithm>
#include <string>
#include <vector>

// =============================================================================
// Item Categories (from GDD)
// =============================================================================
enum class ItemCategory : u8 {
    Weapon,      // Honda, Vara de Aaron — Ataque secundario
    Passive,     // Pan de Cada Dia (+10 HP) — Permanente en run
    Relic,       // Tablas de la Ley — Muy poderoso, 1 slot
    Consumable,  // Vino Nuevo (+25% dano) — Uso unico
    Heredable    // Antorcha del Altar — Pasan a Eliseo
};

enum class ItemRarity : u8 {
    Common,
    Uncommon,
    Rare,
    Legendary  // Relics are always legendary
};

// Color de rareza: lo usan el HUD (marcos de las cartas y de la fila de objetos) y el
// Codice. Vive aqui para que las dos pantallas no se desincronicen.
inline Color item_rarity_color(ItemRarity r) {
    switch (r) {
        case ItemRarity::Common:    return {200, 195, 185, 255};
        case ItemRarity::Uncommon:  return {120, 210, 120, 255};
        case ItemRarity::Rare:      return {110, 175, 245, 255};
        default:                    return {245, 205, 90, 255};   // Legendary (reliquias)
    }
}

// =============================================================================
// Item Definition
// =============================================================================
struct ItemDef {
    u32 id;
    const char* name;
    const char* description;
    ItemCategory category;
    ItemRarity rarity;
    bool heredable;  // Transfers to Eliseo on ascension

    // Stat modifiers (simple flat bonuses)
    f32 hp_bonus;
    f32 damage_bonus;
    f32 speed_bonus;
    f32 faith_bonus;
    f32 attack_speed_bonus;  // Negative = faster

    // Ultimo a proposito: los inicializadores de arriba lo dejan en false sin tocarlos.
    // true = solo se gana en un encuentro biblico, nunca sale en las cartas de eleccion.
    bool event_only;
};

// =============================================================================
// Item Registry
// =============================================================================
struct ItemRegistry {
    std::vector<ItemDef> items;

    void init() {
        // === WEAPONS ===
        items.push_back({0, "Honda de David",
            "Ataque a distancia secundario", ItemCategory::Weapon,
            ItemRarity::Common, false, 0, 5, 0, 0, 0});

        items.push_back({1, "Vara de Aarón",
            "Ataque cuerpo a cuerpo de largo alcance", ItemCategory::Weapon,
            ItemRarity::Uncommon, false, 0, 8, 0, 0, -0.05f});

        // === PASSIVES ===
        items.push_back({2, "Pan de Cada Día",
            "+10 HP máximo", ItemCategory::Passive,
            ItemRarity::Common, false, 10, 0, 0, 0, 0});

        items.push_back({3, "Aceite de la Unción",
            "+5% daño, +5 Fe", ItemCategory::Passive,
            ItemRarity::Uncommon, false, 0, 2, 0, 5, 0});

        items.push_back({4, "Sandalias del Profeta",
            "+15% velocidad", ItemCategory::Passive,
            ItemRarity::Common, false, 0, 0, 30, 0, 0});

        items.push_back({5, "Maná del Cielo",
            "+20 HP, +3 Fe", ItemCategory::Passive,
            ItemRarity::Rare, false, 20, 0, 0, 3, 0});

        // === RELICS ===
        items.push_back({6, "Tablas de la Ley",
            "Fe no baja de 25", ItemCategory::Relic,
            ItemRarity::Legendary, false, 0, 0, 0, 0, 0});

        items.push_back({7, "Arca del Pacto",
            "+50% daño de milagros", ItemCategory::Relic,
            ItemRarity::Legendary, false, 0, 0, 0, 0, 0});

        // === CONSUMABLES ===
        items.push_back({8, "Vino Nuevo",
            "+25% daño por 30s", ItemCategory::Consumable,
            ItemRarity::Common, false, 0, 0, 0, 0, 0});

        items.push_back({9, "Agua del Jordán",
            "Cura 30 HP", ItemCategory::Consumable,
            ItemRarity::Common, false, 0, 0, 0, 0, 0});

        // === HEREDABLES ===
        items.push_back({10, "Antorcha del Altar",
            "+3 daño (pasa a Eliseo)", ItemCategory::Passive,
            ItemRarity::Uncommon, true, 0, 3, 0, 0, 0});

        items.push_back({11, "Piedra del Memorial",
            "+10 HP (pasa a Eliseo)", ItemCategory::Passive,
            ItemRarity::Uncommon, true, 10, 0, 0, 0, 0});

        items.push_back({12, "Ceniza del Carmelo",
            "+8 Fe inicial (pasa a Eliseo)", ItemCategory::Passive,
            ItemRarity::Rare, true, 0, 0, 0, 8, 0});

        // === SOLO DE ENCUENTRO (game/events.h) — no entran en el sorteo de cartas ===
        items.push_back({13, "Tinaja de Harina",
            "+15 HP máximo (pasa a Eliseo)", ItemCategory::Passive,
            ItemRarity::Rare, true, 15, 0, 0, 0, 0, true});

        items.push_back({14, "Vasija de Aceite",
            "+4 daño (pasa a Eliseo)", ItemCategory::Passive,
            ItemRarity::Rare, true, 0, 4, 0, 0, 0, true});
    }

    const ItemDef* get(u32 id) const {
        for (auto& item : items) {
            if (item.id == id) return &item;
        }
        return nullptr;
    }

    // Get random item by category
    const ItemDef* get_random(ItemCategory category) const {
        std::vector<const ItemDef*> pool;
        for (auto& item : items) {
            if (item.category == category) pool.push_back(&item);
        }
        if (pool.empty()) return nullptr;
        return pool[randi_range(0, static_cast<i32>(pool.size()) - 1)];
    }

    // Three distinct offers for the "choose 1 of 3" screen. `owned` items are skipped (a
    // second copy of a relic/weapon does nothing) unless that would leave fewer than 3
    // candidates. Rarity biases the roll: common items show up ~4x as often as legendaries.
    std::vector<u32> roll_choices(const std::vector<u32>& owned, int count = 3) const {
        auto weight = [](ItemRarity r) -> int {
            switch (r) {
                case ItemRarity::Common:    return 40;
                case ItemRarity::Uncommon:  return 26;
                case ItemRarity::Rare:      return 14;
                default:                    return 8;   // Legendary (relics)
            }
        };
        // Never offer something already owned: duplicates used to stack forever and a bot run
        // reached chapter 5 with 21 items. Fewer than `count` candidates -> fewer cards, and
        // no candidates at all -> no offer (13 items is the run ceiling).
        std::vector<u32> picked;
        std::vector<u32> pool;
        std::vector<int> weights;
        int total = 0;
        for (auto& it : items) {
            if (it.event_only) continue;   // se ganan hablando, no eligiendo
            if (std::find(owned.begin(), owned.end(), it.id) != owned.end()) continue;
            pool.push_back(it.id);
            int w = weight(it.rarity);
            weights.push_back(w);
            total += w;
        }
        while (static_cast<int>(picked.size()) < count && !pool.empty()) {
            int r = randi_range(0, total - 1);
            size_t k = 0;
            for (; k + 1 < pool.size(); k++) {
                if (r < weights[k]) break;
                r -= weights[k];
            }
            picked.push_back(pool[k]);
            total -= weights[k];
            pool.erase(pool.begin() + k);
            weights.erase(weights.begin() + k);
        }
        return picked;
    }

    // Get all heredable items (for Elias -> Eliseo transfer)
    std::vector<const ItemDef*> get_heredables() const {
        std::vector<const ItemDef*> result;
        for (auto& item : items) {
            if (item.heredable) result.push_back(&item);
        }
        return result;
    }
};
