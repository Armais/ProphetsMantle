#pragma once

// =============================================================================
// MANTLE - Item System (GDD 7.3)
// =============================================================================

#include "core/i18n.h"
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

    // true = nace BLOQUEADO y no entra en el sorteo hasta que su mejora de El Santuario
    // esta comprada (MetaUpgrade::unlocks_item -> SaveData::items_unlocked). Es lo que el
    // GDD promete en el paso 7 del bucle: las ofrendas desbloquean objetos, no solo cifras.
    bool starts_locked = false;
};

// =============================================================================
// Cosmeticos: skins del cayado (GDD 4.x)
// =============================================================================
// Una skin es un TINTE, no una hoja de sprites nueva. Es una decision de coste: una
// variante dibujada serian 8 PNG de 896x1920 mas por skin, y una regla del proyecto es
// que `assets/` no crece si se puede evitar (mismo argumento que la paleta por capitulo
// de get_enemy_chapter_tint). Lo que se tine es lo que el jugador ve salir del cayado:
// el proyectil y el fogonazo de la boca.
//
// NO dan ventaja: el GDD dice "NO pay-to-win" y aqui no hay ni una cifra que cambie.
struct StaffSkin {
    const char* name;
    const char* name_en;
    Color shot;      // proyectil
    Color muzzle;    // fogonazo
};

constexpr u32 STAFF_SKIN_COUNT = 4;

inline StaffSkin get_staff_skin(u32 id) {
    switch (id) {
        case 1:  return {"Brasa del Carmelo", "Ember of Carmel",
                         {255, 120,  60, 255}, {255, 140,  80, 220}};
        case 2:  return {"Luz del Jordán",    "Light of the Jordan",
                         {170, 230, 255, 255}, {200, 240, 255, 220}};
        case 3:  return {"Ceniza del Horeb",  "Ash of Horeb",
                         {200, 190, 220, 255}, {215, 205, 230, 220}};
        default: return {"Cayado del Profeta", "Prophet's Staff",
                         {255, 255, 255, 255}, {0, 0, 0, 0}};   // 0,0,0,0 = el de siempre
    }
}

// =============================================================================
// Item Registry
// =============================================================================
struct ItemRegistry {
    std::vector<ItemDef> items;

    // IDEMPOTENTE a proposito: `tr()` devuelve un puntero al literal del idioma que este
    // activo EN ESE MOMENTO, asi que al cambiar de idioma hay que volver a construir el
    // registro entero (Engine::relocalize). Sin el clear, la segunda llamada duplicaba los
    // 22 objetos. Nadie guarda un `const ItemDef*` de larga vida, asi que reconstruir el
    // vector es seguro.
    void init() {
        items.clear();

        // === WEAPONS ===
        items.push_back({0, tr("Honda de David", "David's Sling"),
            tr("Ataque a distancia secundario", "Secondary ranged attack"), ItemCategory::Weapon,
            ItemRarity::Common, false, 0, 5, 0, 0, 0});

        items.push_back({1, tr("Vara de Aarón", "Aaron's Rod"),
            tr("Ataque cuerpo a cuerpo de largo alcance", "Long-reach melee sweep"), ItemCategory::Weapon,
            ItemRarity::Uncommon, false, 0, 8, 0, 0, -0.05f});

        // === PASSIVES ===
        items.push_back({2, tr("Pan de Cada Día", "Daily Bread"),
            tr("+10 HP máximo", "+10 max HP"), ItemCategory::Passive,
            ItemRarity::Common, false, 10, 0, 0, 0, 0});

        items.push_back({3, tr("Aceite de la Unción", "Anointing Oil"),
            tr("+5% daño, +5 Fe", "+5% damage, +5 Faith"), ItemCategory::Passive,
            ItemRarity::Uncommon, false, 0, 2, 0, 5, 0});

        items.push_back({4, tr("Sandalias del Profeta", "Prophet's Sandals"),
            tr("+15% velocidad", "+15% speed"), ItemCategory::Passive,
            ItemRarity::Common, false, 0, 0, 30, 0, 0});

        items.push_back({5, tr("Maná del Cielo", "Manna from Heaven"),
            tr("+20 HP, +3 Fe", "+20 HP, +3 Faith"), ItemCategory::Passive,
            ItemRarity::Rare, false, 20, 0, 0, 3, 0});

        // === RELICS ===
        items.push_back({6, tr("Tablas de la Ley", "Tablets of the Law"),
            tr("Fe no baja de 25", "Faith never drops below 25"), ItemCategory::Relic,
            ItemRarity::Legendary, false, 0, 0, 0, 0, 0});

        items.push_back({7, tr("Arca del Pacto", "Ark of the Covenant"),
            tr("+50% daño de milagros", "+50% miracle damage"), ItemCategory::Relic,
            ItemRarity::Legendary, false, 0, 0, 0, 0, 0});

        // === CONSUMABLES ===
        items.push_back({8, tr("Vino Nuevo", "New Wine"),
            tr("+25% daño por 30s", "+25% damage for 30s"), ItemCategory::Consumable,
            ItemRarity::Common, false, 0, 0, 0, 0, 0});

        items.push_back({9, tr("Agua del Jordán", "Water of the Jordan"),
            tr("Cura 30 HP", "Heals 30 HP"), ItemCategory::Consumable,
            ItemRarity::Common, false, 0, 0, 0, 0, 0});

        // === HEREDABLES ===
        items.push_back({10, tr("Antorcha del Altar", "Altar Torch"),
            tr("+3 daño", "+3 damage"), ItemCategory::Passive,
            ItemRarity::Uncommon, true, 0, 3, 0, 0, 0});

        items.push_back({11, tr("Piedra del Memorial", "Memorial Stone"),
            tr("+10 HP", "+10 HP"), ItemCategory::Passive,
            ItemRarity::Uncommon, true, 10, 0, 0, 0, 0});

        items.push_back({12, tr("Ceniza del Carmelo", "Ash of Carmel"),
            tr("+8 Fe inicial", "+8 starting Faith"), ItemCategory::Passive,
            ItemRarity::Rare, true, 0, 0, 0, 8, 0});

        // === SOLO DE ENCUENTRO (game/events.h) — no entran en el sorteo de cartas ===
        items.push_back({13, tr("Tinaja de Harina", "Jar of Meal"),
            tr("+15 HP máximo", "+15 max HP"), ItemCategory::Passive,
            ItemRarity::Rare, true, 15, 0, 0, 0, 0, true});

        items.push_back({14, tr("Vasija de Aceite", "Cruse of Oil"),
            tr("+4 daño", "+4 damage"), ItemCategory::Passive,
            ItemRarity::Rare, true, 0, 4, 0, 0, 0, true});

        // === AMPLIACION DEL CATALOGO (16 -> 22 objetos) ===
        // El roadmap pedia "20+" en Beta. Se repartieron donde el pool estaba flojo: las
        // ARMAS eran dos y los CONSUMIBLES dos. La tercera RELIQUIA entra a proposito:
        // con solo dos, el slot unico del GDD 7.3 casi nunca obligaba a elegir.
        // Todos salen de 1-2 Reyes salvo donde se dice; ninguno contradice el texto.

        items.push_back({16, tr("Arco de Joás", "Joash's Bow"),
            tr("Ráfaga de 3 saetas", "Volley of 3 arrows"), ItemCategory::Weapon,
            ItemRarity::Uncommon, false, 0, 4, 0, 0, 0, false, true});

        items.push_back({17, tr("Higos de Ezequías", "Hezekiah's Figs"),
            tr("+15 HP máximo", "+15 max HP"), ItemCategory::Consumable,
            ItemRarity::Uncommon, false, 0, 0, 0, 0, 0, false, true});

        items.push_back({18, tr("Sal de Eliseo", "Elisha's Salt"),
            tr("+20 Fe", "+20 Faith"), ItemCategory::Consumable,
            ItemRarity::Common, false, 0, 0, 0, 0, 0});

        items.push_back({19, tr("Cinto de Cuero", "Leather Belt"),
            tr("Atacas un 12% más rápido", "You attack 12% faster"), ItemCategory::Passive,
            ItemRarity::Uncommon, true, 0, 0, 0, 0, -0.03f});

        items.push_back({20, tr("El Reloj de Acaz", "The Dial of Ahaz"),
            tr("Un golpe mortal por capítulo te deja vivo", "One lethal blow per chapter leaves you alive"), ItemCategory::Relic,
            ItemRarity::Legendary, false, 0, 0, 0, 0, 0, false, true});

        items.push_back({21, tr("Panes de Cebada", "Loaves of Barley"),
            tr("+18 HP máximo, +4 Fe", "+18 max HP, +4 Faith"), ItemCategory::Passive,
            ItemRarity::Rare, true, 18, 0, 0, 4, 0, false, true});

        // === EL MANTO DE ELIAS — el objeto que da titulo al juego ===
        // No se elige ni se encuentra: cae sobre Eliseo en la Ascension
        // (Engine::finish_ascension), asi que va marcado event_only. Su efecto no puede
        // pisar al Arca (miracle_power) ni a las Tablas (floor_value): abre el tercer slot
        // y acorta la recarga, que es lo que nadie mas toca. Ver Player::grant_item.
        items.push_back({15, tr("El Manto de Elías", "Elijah's Mantle"),
            tr("+1 slot de milagro, -25% de recarga", "+1 miracle slot, -25% cooldown"), ItemCategory::Passive,
            ItemRarity::Legendary, true, 0, 0, 0, 0, 0, true});
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
    // `unlocked_mask`: bits de SaveData::items_unlocked. Un objeto con starts_locked que
    // no tenga su bit puesto NO se ofrece. Se pasa por parametro y no se lee del guardado
    // porque items.h no conoce ni debe conocer el fichero de guardado.
    std::vector<u32> roll_choices(const std::vector<u32>& owned, int count = 3,
                                  u32 unlocked_mask = 0xFFFFFFFFu) const {
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
        // no candidates at all -> no offer. El techo de una run son los SORTEABLES: 19 de
        // los 22 del catalogo (13, 14 y 15 son event_only y se ganan hablando).
        std::vector<u32> picked;
        std::vector<u32> pool;
        std::vector<int> weights;
        int total = 0;
        for (auto& it : items) {
            if (it.event_only) continue;   // se ganan hablando, no eligiendo
            if (it.starts_locked && !(unlocked_mask & (1u << it.id))) continue;   // aun no comprado
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
