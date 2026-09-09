# GDD v1.0: las tablas que el codigo aun refleja

Viene del `CLAUDE.md` de la raiz. **No es un plan**: es referencia de lo implementado. La
fuente de verdad de diseno es `GDD2.0.md`. Los controles se movieron a
`src/input/CLAUDE.md`.

**OBSOLETO: `../IdeaJuegoEliasEliseo.pdf` (GDD v1.0, Febrero 2026).** Está implementado de cabo
a rabo y ya no dirige nada; se conserva como documento histórico. Lo que sigue en esta sección
son las tablas del v1.0 que el código **aún refleja** (cifras de personaje, fe, milagros,
enemigos, controles): valen como referencia de lo implementado, no como plan. Si el v1.0 y
`GDD2.0.md` se contradicen, manda `GDD2.0.md`.

### Elevator Pitch

"Vive la era mas oscura de Israel como los profetas Elias y Eliseo. Desafia reyes, destruye altares de Baal, y cuando Elias ascienda al cielo, recoge su manto y continua su legado con el doble de su poder."

### Design Pillars

- **Fe como Recurso**: Faith is mana, shield, and divine connection
- **Milagros como Mecanica**: Abilities based on real biblical miracles
- **Legado Heredado**: Elias->Eliseo transition transfers power and progress
- **Accesible pero Profundo**: Easy to learn, hard to master

### Core Loop (20-35 min run)

1. EXPLORAR habitacion/zona
2. COMBATIR enemigos con Cayado + Milagros
3. RECOLECTAR Fe, Items, Reliquias
4. ELEGIR mejora entre 3 opciones
5. AVANZAR a siguiente habitacion
6. BOSS al final del capitulo
7. META-PROGRESO: Desbloquear items/personajes

### Two-Act Structure & Chapters

**ACTO I: ELIAS — El Profeta de Fuego**

| Cap | Nombre | Bioma | Boss |
|-----|--------|-------|------|
| 1 | El Arroyo Querit | Canon/Arroyo | Capitan de Acab |
| 2 | La Viuda de Sarepta | Aldea fenicia | Sacerdote Baal + La Muerte |
| 3 | El Monte Carmelo | Montana | 450 Profetas de Baal |
| 4 | La Cueva de Horeb | Desierto/Cueva | Tu Propia Desesperacion |
| 5 | El Llamado | Campos | Acab y Jezabel |

**TRANSICION: EL ASCENSO** — Elias sube al cielo, el manto cae a Eliseo, "Doble porcion"

**ACTO II: ELISEO — El Profeta de la Gracia**

| Cap | Nombre | Bioma | Boss |
|-----|--------|-------|------|
| 6 | Las Aguas de Jerico | Ciudad corrupta | Espiritu de Corrupcion |
| 7 | El Aceite de la Viuda | Aldea pobre | El Acreedor |
| 8 | El Hijo Sunamita | Casa noble | La Muerte (Mayor) |
| 9 | Naaman el Sirio | Campamento/Jordan | Lepra + Guiezi |
| 10 | El Ejercito Celestial | Ciudad sitiada | Ben-Adad |

**EPILOGO: El Legado** (Desbloqueble)

### The Ascension (Central Mechanic)

When Act I ends, Eliseo INHERITS from Elias:
- 50% of accumulated Faith
- Items marked as "Heredable"
- Elias miracles (unlocked but weaker)
- El Manto de Elias (unique core item)
- Complete bestiary (enemy knowledge)

Eliseo GAINS new:
- His own unique miracles
- "Doble porcion": 2 miracle slots from start
- Superior healing abilities
- Expanded Disciples system

### Characters

| Aspecto | Elias | Eliseo |
|---------|-------|--------|
| Arquetipo | Ofensivo, explosivo | Versatil, sanador |
| Arma base | Cayado de Fuego | Cayado de Luz |
| Fortaleza | Dano masivo | Curacion, buffs |
| Debilidad | Poca curacion | Menos dano directo |
| HP Base | 80 | 100 |
| Velocidad | 210 | 190 |
| Dano base | 12 | 10 |
| Slots milagro | 1 (2 en cap 4+) | 2 desde inicio |

### Faith System — "El Celo del Senor" (0-100)

**Thresholds & Effects:**

| Nivel | Fe | Nombre | Efectos |
|-------|-----|--------|---------|
| 5 | 90-100 | Fuego del Cielo | +40% dano, milagros potenciados |
| 4 | 75-89 | Profeta Ungido | +25% dano, Tier 1-4 disponible |
| 3 | 50-74 | Siervo Fiel | +10% dano, Tier 1-3 |
| 2 | 25-49 | Fe Vacilante | Normal, solo Tier 1-2 |
| 1 | 10-24 | En el Desierto | -15% dano, solo Tier 1 |
| 0 | 0-9 | Bajo la Retama | -30% dano, sin milagros |

**Gain Faith:** Destruir altar Baal (+8), matar profeta falso (+3), habitacion sin dano (+2), orar en altar (+10), ayudar viuda (+5), derrotar jefe (+15)

**Lose Faith:** Recibir dano (-1/hit), huir de confrontacion (-5), usar milagro (variable), inactividad (-0.5/10s)

### Miracles System

**Elias (5 miracles):**

| Milagro | Coste | Efecto | Tier |
|---------|-------|--------|------|
| Cuervos de Querit | 5 | Cura 10 HP | 1 |
| Palabra de Sequia | 8 | Ralentiza 50% | 1 |
| Fuego del Cielo | 20 | AOE 80 dano | 3 |
| Carro de Fuego | 25 | Dash devastador | 3 |
| Altar Consumido | 30 | Destruye pantalla | 4 |

**Eliseo (5 miracles):**

| Milagro | Coste | Efecto | Tier |
|---------|-------|--------|------|
| Sanar Aguas | 5 | Zona cura 5 HP/s | 1 |
| Aceite Inagotable | 12 | Consumibles x2 | 2 |
| Ojos Abiertos | 15 | Revela ocultos | 2 |
| Ejercito Celestial | 22 | 3 guerreros 10s | 3 |
| Doble Porcion | 30 | Stats x2 por 15s | 4 |

### Enemies

**Acto I:**

| Enemigo | HP | Dano | Comportamiento |
|---------|----|------|----------------|
| Soldado de Acab | 20 | 8 | Melee directo |
| Arquero Real | 15 | 10 | Distancia |
| Profeta de Baal | 35 | 12 | Caster, cura |
| Leon del Desierto | 40 | 18 | Rapido, salta |

**Acto II:**

| Enemigo | HP | Dano | Comportamiento |
|---------|----|------|----------------|
| Soldado Sirio | 25 | 10 | Melee resistente |
| General Sirio | 60 | 18 | Buff aliados |
| Idólatra de Rimón | 20 | 8 | Explota al morir (su brasero) |
| Agua Corrupta | 35 | 10 | Se divide |

### Items System (5 categories)

| Categoria | Ejemplos | Efecto |
|-----------|----------|--------|
| Armas | Honda, Vara de Aaron | Ataque secundario |
| Pasivos | Pan de Cada Dia (+10 HP) | Permanente en run |
| Reliquias | Tablas de la Ley | Muy poderoso, 1 slot |
| Consumibles | Vino Nuevo (+25% dano) | Uso unico |
| Heredables | Antorcha del Altar | Pasan a Eliseo |

### Mobile Adaptations

- Twin-stick preciso → Auto-aim + override manual
- Enemigos ligeramente mas lentos (-10%)
- Auto-save al completar cada habitacion (persiste 48h, luego "abandonada" = 50% ofrendas)
- Sesiones 5-15 min optimo (vs 20-35 min PC)
- UI minimalista, info bajo demanda
