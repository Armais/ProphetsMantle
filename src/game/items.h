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
            tr("Arma: lanza una piedra que atraviesa a todos. +5 de daño.", "Weapon: hurls a stone that pierces everyone. +5 damage."), ItemCategory::Weapon,
            ItemRarity::Common, false, 0, 5, 0, 0, 0});

        items.push_back({1, tr("Vara de Aarón", "Aaron's Rod"),
            tr("Arma: barrido en arco que empuja. +8 de daño y atacas más rápido.", "Weapon: a pushing arc sweep. +8 damage and faster attacks."), ItemCategory::Weapon,
            ItemRarity::Uncommon, false, 0, 8, 0, 0, -0.05f});

        // === PASSIVES ===
        items.push_back({2, tr("Pan de Cada Día", "Daily Bread"),
            tr("+10 de vida máxima. Al vencer a cada grupo recuperas 8 de vida.", "+10 max health. Beat a group of enemies to recover 8 health."), ItemCategory::Passive,
            ItemRarity::Common, false, 10, 0, 0, 0, 0});

        items.push_back({3, tr("Aceite de la Unción", "Anointing Oil"),
            tr("Tus milagros cuestan un 20% menos de fe. +2 de daño, +5 de fe.", "Your miracles cost 20% less faith. +2 damage, +5 faith."), ItemCategory::Passive,
            ItemRarity::Uncommon, false, 0, 2, 0, 5, 0});

        items.push_back({4, tr("Sandalias del Profeta", "Prophet's Sandals"),
            tr("+20% de velocidad y el dash se recarga un 35% antes.", "+20% speed and your dash recharges 35% sooner."), ItemCategory::Passive,
            ItemRarity::Common, false, 0, 0, 20, 0, 0});

        items.push_back({5, tr("Maná del Cielo", "Manna from Heaven"),
            tr("+20 de vida máxima, +3 de fe. Los enemigos sueltan el doble de corazones.", "+20 max health, +3 faith. Enemies drop twice as many hearts."), ItemCategory::Passive,
            ItemRarity::Rare, false, 20, 0, 0, 3, 0});

        // === RELICS ===
        items.push_back({6, tr("Tablas de la Ley", "Tablets of the Law"),
            tr("Reliquia: tu fe nunca baja de 25.", "Relic: your faith never drops below 25."), ItemCategory::Relic,
            ItemRarity::Legendary, false, 0, 0, 0, 0, 0});

        items.push_back({7, tr("Arca del Pacto", "Ark of the Covenant"),
            tr("Reliquia: tus milagros hacen un 50% más de daño.", "Relic: your miracles deal 50% more damage."), ItemCategory::Relic,
            ItemRarity::Legendary, false, 0, 0, 0, 0, 0});

        // === CONSUMABLES ===
        items.push_back({8, tr("Vino Nuevo", "New Wine"),
            tr("Consumible: +25% de daño durante 30 s.", "Consumable: +25% damage for 30 s."), ItemCategory::Consumable,
            ItemRarity::Common, false, 0, 0, 0, 0, 0});

        items.push_back({9, tr("Agua del Jordán", "Water of the Jordan"),
            tr("Consumible: cura 35 de vida.", "Consumable: heals 35 health."), ItemCategory::Consumable,
            ItemRarity::Common, false, 0, 0, 0, 0, 0});

        // === HEREDABLES ===
        items.push_back({10, tr("Antorcha del Altar", "Altar Torch"),
            tr("+3 de daño. Tus disparos prenden: el enemigo arde 2,5 s.", "+3 damage. Your shots ignite: the enemy burns for 2.5 s."), ItemCategory::Passive,
            ItemRarity::Uncommon, true, 0, 3, 0, 0, 0});

        items.push_back({11, tr("Piedra del Memorial", "Memorial Stone"),
            tr("+10 de vida máxima. Un escudo de piedra para un golpe; vuelve a los 12 s.", "+10 max health. A stone shield blocks one hit; it returns after 12 s."), ItemCategory::Passive,
            ItemRarity::Uncommon, true, 10, 0, 0, 0, 0});

        items.push_back({12, tr("Ceniza del Carmelo", "Ash of Carmel"),
            tr("+8 de fe. Cada enemigo abatido te da 1 de fe más.", "+8 faith. Every enemy you strike down gives 1 more faith."), ItemCategory::Passive,
            ItemRarity::Rare, true, 0, 0, 0, 8, 0});

        // === SOLO DE ENCUENTRO (game/events.h) — no entran en el sorteo de cartas ===
        items.push_back({13, tr("Tinaja de Harina", "Jar of Meal"),
            tr("+15 de vida máxima.", "+15 max health."), ItemCategory::Passive,
            ItemRarity::Rare, true, 15, 0, 0, 0, 0, true});

        items.push_back({14, tr("Vasija de Aceite", "Cruse of Oil"),
            tr("+4 de daño.", "+4 damage."), ItemCategory::Passive,
            ItemRarity::Rare, true, 0, 4, 0, 0, 0, true});

        // === AMPLIACION DEL CATALOGO (16 -> 22 objetos) ===
        // El roadmap pedia "20+" en Beta. Se repartieron donde el pool estaba flojo: las
        // ARMAS eran dos y los CONSUMIBLES dos. La tercera RELIQUIA entra a proposito:
        // con solo dos, el slot unico del GDD 7.3 casi nunca obligaba a elegir.
        // Todos salen de 1-2 Reyes salvo donde se dice; ninguno contradice el texto.

        items.push_back({16, tr("Arco de Joás", "Joash's Bow"),
            tr("Arma: ráfaga de 3 saetas en abanico. +4 de daño.", "Weapon: a fanned volley of 3 arrows. +4 damage."), ItemCategory::Weapon,
            ItemRarity::Uncommon, false, 0, 4, 0, 0, 0, false, true});

        items.push_back({17, tr("Higos de Ezequías", "Hezekiah's Figs"),
            tr("Consumible: cura toda la vida y +10 de vida máxima.", "Consumable: heals you fully and +10 max health."), ItemCategory::Consumable,
            ItemRarity::Uncommon, false, 0, 0, 0, 0, 0, false, true});

        items.push_back({18, tr("Sal de Eliseo", "Elisha's Salt"),
            tr("Consumible: +25 de fe al instante.", "Consumable: +25 faith at once."), ItemCategory::Consumable,
            ItemRarity::Common, false, 0, 0, 0, 0, 0});

        items.push_back({19, tr("Cinto de Cuero", "Leather Belt"),
            tr("Atacas un 12% más rápido.", "You attack 12% faster."), ItemCategory::Passive,
            ItemRarity::Uncommon, true, 0, 0, 0, 0, -0.03f});

        items.push_back({20, tr("El Reloj de Acaz", "The Dial of Ahaz"),
            tr("Reliquia: un golpe mortal por capítulo te deja con media vida.", "Relic: one lethal blow per chapter leaves you at half health."), ItemCategory::Relic,
            ItemRarity::Legendary, false, 0, 0, 0, 0, 0, false, true});

        items.push_back({21, tr("Panes de Cebada", "Loaves of Barley"),
            tr("+18 de vida máxima, +4 de fe. Los corazones te curan un 50% más.", "+18 max health, +4 faith. Hearts heal you 50% more."), ItemCategory::Passive,
            ItemRarity::Rare, true, 18, 0, 0, 4, 0, false, true});

        // === EL MANTO DE ELIAS — el objeto que da titulo al juego ===
        // No se elige ni se encuentra: cae sobre Eliseo en la Ascension
        // (Engine::finish_ascension), asi que va marcado event_only. Su efecto no puede
        // pisar al Arca (miracle_power) ni a las Tablas (floor_value): abre el tercer slot
        // y acorta la recarga, que es lo que nadie mas toca. Ver Player::grant_item.
        items.push_back({15, tr("El Manto de Elías", "Elijah's Mantle"),
            tr("+1 hueco de milagro y los milagros se recargan un 25% antes.", "+1 miracle slot and miracles recharge 25% sooner."), ItemCategory::Passive,
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
            // Los consumibles ya no se ELIGEN (2026-10-03): una carta es una decision de
            // partida entera y un consumible casi siempre perdia contra un pasivo. Se
            // encuentran en el mundo y van a la alforja (roll_consumable).
            if (it.category == ItemCategory::Consumable) continue;
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

    // QUE consumible cae en el mundo. `r` en [0,1): lo pasa quien suelta (el Engine lo
    // saca de la POSICION para los secretos, que asi pagan siempre lo mismo). Agua del
    // Jordan es lo mas comun: curar es lo que mas se necesita. Los Higos solo con su
    // mejora de El Santuario comprada (`unlocked_mask`, bits de SaveData::items_unlocked).
    static u32 roll_consumable(f32 r, u32 unlocked_mask) {
        const bool figs = (unlocked_mask >> 17) & 1u;
        // Agua 40 · Vino 25 · Sal 20 · Higos 15 (sin Higos se reparte entre los otros)
        const f32 w_agua = 40.0f, w_vino = 25.0f, w_sal = 20.0f, w_higo = figs ? 15.0f : 0.0f;
        f32 x = r * (w_agua + w_vino + w_sal + w_higo);
        if ((x -= w_agua) < 0.0f) return 9;
        if ((x -= w_vino) < 0.0f) return 8;
        if ((x -= w_sal)  < 0.0f) return 18;
        return figs ? 17u : 9u;
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
