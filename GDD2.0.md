# MANTLE — Game Design Document 2.0

> **Estado: vigente.** Sustituye al GDD v1.0 (`../IdeaJuegoEliasEliseo.pdf`, Febrero 2026), que
> queda como documento **histórico**. El v1.0 está implementado de cabo a rabo y por eso dejó de
> dirigir nada: describía un juego que ya existe, no el que falta por hacer.
>
> Este documento vive **dentro del repo** y se versiona con el código a propósito. Un GDD en PDF
> fuera del árbol se desincroniza el primer día.
>
> Fecha de apertura: 2026-08-25.

---

## 0. Por qué existe este documento

MANTLE cumple el v1.0 entero: 10 capítulos jugables, 10 milagros, 22 objetos, 18 logros, dos
idiomas, audio sintetizado propio, controles táctiles y meta-progresión. Y aun así el juego
tiene tres defectos que el v1.0 nunca planteó, porque no eran su tema:

1. **El mundo es diminuto.** Cada sala mide exactamente una pantalla y la cámara está inmóvil.
   Un capítulo son nueve pantallas fijas. No hay nada que *recorrer*.
2. **Hay muros donde no tocan.** Todas las salas van rodeadas de un borde sólido, también en el
   arroyo, en los campos y a la orilla del Jordán. Un acantilado alrededor de un descampado.
3. **El relato se queda corto y no invita a investigar.** Un encuentro por capítulo, y falta más
   de la mitad de lo que 1-2 Reyes cuenta de Elías y Eliseo. El PNJ del encuentro se dibujaba
   como una antorcha.

El GDD 2.0 es el plan para arreglar esos tres, y la referencia contra la que se decide.

---

## 1. Referencia de diseño: *The Legend of Zelda: A Link to the Past*

**ALTTP es la referencia del juego** (decisión del usuario, 2026-08-25). No como estética —el
look de figurita 3D no se toca— sino como **gramática de mundo**: qué es una zona, dónde hay
muros, por qué el jugador se desvía del camino.

| Lo que hace ALTTP | Lo que significa en MANTLE |
|---|---|
| El exterior es continuo y **scrollea**. Sus límites son acantilados, agua, matorral y vallas: **nunca sillería** | Zonas exteriores grandes con la cámara siguiendo al profeta. El borde lo pone el **bioma**, no el generador |
| Los muros están **dentro**: palacios, cuevas, casas. Ahí la cámara va **sala a sala** | Palacio de Acab, templo de Baal, la cueva de Horeb, la casa de la Sunamita. Interiores con muros de verdad y cámara clavada |
| Investigas porque hay **secretos por todas partes**: matorral que se corta, piedras que se levantan, grietas, cuevas ocultas | Matorral, tinajas, piedras, grietas y cachés con ofrendas, fe y objetos |
| Los objetos son **llaves de exploración**: el guante abre zonas que ya pisaste | Los **milagros** son las llaves. Fuego del Cielo abre grietas y quema matorral; Sanar Aguas seca charcas; Ojos Abiertos revela lo invisible; Carro de Fuego cruza huecos |
| Los PNJ **están de pie en el mundo** y se les habla | El encuentro deja de ser un icono del tileset: es un personaje con su sprite y su indicador |
| Un objetivo claro, y un mapa que lo señala | `ChapterLore::objective` ya existe; pasa al mapa grande y al HUD |

**Lo que NO se toma de ALTTP:** no hay llaves ni cerrojos coleccionables por mazmorra, no hay
inventario de objetos activos (el slot activo son los milagros) y no hay mundo oscuro. MANTLE
sigue siendo un roguelike por *runs*, no una aventura con guardado libre.

### Pilares (los cuatro del v1.0 siguen en pie)

- **Fe como recurso**: es maná, escudo y conexión divina a la vez.
- **Milagros como mecánica**: poderes que son episodios reales del texto — y, desde ahora,
  también **herramientas de exploración**.
- **Legado heredado**: Elías → Eliseo, con la Ascensión en medio.
- **Accesible pero profundo**.

Y uno nuevo, que es la lección de ALTTP:

- **El mundo se recorre, no se atraviesa.** Si una sala solo sirve para llegar a la siguiente,
  sobra.

---

## 2. Auditoría bíblica: la historia de Elías y Eliseo

**Regla vigente (mandato del usuario, 2026-08-30): TODOS los relatos del juego van anclados a
Elías o a Eliseo.** El juego cuenta 1 Reyes 17 - 2 Reyes 13 — la historia de los dos profetas —
y nada más. Hubo un intento intermedio de cubrir el libro ENTERO de Reyes (la cadena "El Reino
Partido" con David y Salomón antes del capítulo 1, y un epílogo de 9 escenas hasta la caída de
Jerusalén): el usuario lo rechazó el mismo día (*"este juego va sobre la historia de Elías y
Eliseo, no de David ni de Salomón"*) y se REVIRTIÓ entero. Un relato entra si Elías o Eliseo
están en la escena, o si la escena cumple una palabra suya (el criterio del epílogo).

| Bloque del libro | Vehículo |
|---|---|
| 1 R 17 - 2 R 9 (Elías y Eliseo) | los **pozos de encuentros** de los 10 capítulos (39 relatos, en orden del libro) + prólogo cinemático + el Ascenso + las intros |
| 2 R 9; 13:14-21 (los arcos se cierran) | el epílogo **"El Legado"**: Jehú/Jezabel (la palabra de Elías), el arco de Joás y los huesos de Eliseo |
| Piezas sueltas (2 R 13:17; 20:7; 20:11; 4:43) | ya eran **objetos**: Arco de Joás, Higos de Ezequías, Reloj de Acaz, Panes de Cebada |

El generador coloca TODAS las salas `Event` del pozo (3-5 por capítulo, ver §3.4): la historia
es configuración, no sorteo. El contexto del reino partido (Salomón, los becerros) cabe en UNA
línea del prólogo cinemático — contexto, no protagonismo.

**Regla que no se rompe:** nada contradice 1-2 Reyes. Precedente cerrado: el "Leproso
Enloquecido" del v1.0 se eliminó porque en 2 Reyes 5 Eliseo *sana* al leproso y en 7:3-10 los
cuatro leprosos son los héroes. Y `Scripture::ref` sigue **sin pintarse** en pantalla: el texto
se lee como narración, no como cita.

### Estado por capítulo

Leyenda: ✅ hecho · 🆕 falta por escribir.

#### Prólogo — El cielo cerrado (ya jugable, 8 estampas)
✅ El reino partido · Acab · Jezabel y Baal · la persecución · Dios habla a Elías · «ni rocío ni
lluvia» · los cuervos · título.

#### "El Reino Partido" — ELIMINADA (2026-08-30)
Existió una cadena de 7 paradas (1 R 1-16: David, Salomón, Siquem, la casa de Omri) antes del
primer paso del capítulo 1. **El usuario la rechazó el mismo día en que se estrenó** — el juego
va de Elías y Eliseo, no de David ni de Salomón — y se quitó entera: ids `PRE_*`, maquinaria
`prechain_*` del Engine, comando `reino` y guion `reyes_check.txt`. Si algún día vuelve la
tentación de contar la pre-historia, el sitio es el prólogo cinemático (que ya dice «después de
Salomón, el reino se partió en dos» en una línea), no una cadena de paradas con opciones.

#### Cap. 1 — El Arroyo Querit (1 Reyes 17:1-7)
- ✅ **La audiencia con Acab** (17:1-4, apertura guionizada, 2026-09-09): antes de la intro del
  río, con el motor del gameplay — palacio (interior de sillería, cámara clavada), Acab y
  Jezabel de pie al fondo, Elías delante — y siete cajas SIN decisión: Elías reprende, Acab lo
  desprecia, Jezabel amenaza, la sentencia «ni rocío ni lluvia» (17:1), Acab se burla, Jezabel
  promete la cacería y «La palabra del Señor» que lo manda a Querit (17:2-4). Va ANTES del
  arroyo, en el orden del libro, así que la regla del 2026-08-31 se mantiene: el prólogo la
  sigue narrando y la intro del río ya no repite 17:1-3. Solo en Nueva partida (ni Continuar
  ni reintento), ESC la salta entera; cero arte nuevo (`acab.png`, `jezabel.png`, retratos).
  Es la escena 0 de `game/scenes.h` (ver «Escenas guionizadas» más abajo).
- ✅ **Diálogo con el Capitán de Acab** antes de la pelea (18:10; 2 R 1:9), escena de jefe.
- ✅ Los cuervos de Querit (17:6)
- ✅ **El rastreador de Acab** (18:10, pasada de coherencia 2026-08-31): un explorador del rey
  baja al cañón siguiendo el agua — la sequía que juraste es lo que lo trae a tu arroyo.
  Sustituye a la audiencia «ni rocío ni lluvia» (17:1), que ya es la estampa 6 del prólogo:
  Querit viene DESPUÉS de esa conversación en palacio, y rejugarla aquí rompía el orden del
  libro. El anuncio sigue contado donde toca: en el prólogo.
- ✅ **El arroyo que se seca** (17:7). Decisión: quedarse donde Dios te mandó, o moverte.

#### Cap. 2 — La Viuda de Sarepta (1 Reyes 17:8-24)
- ✅ La viuda y su último puñado de harina (17:12)
- ✅ **La tinaja que no se acaba** (17:14-16)
- ✅ **El hijo muerto y devuelto a la vida** (17:17-24). Es el primer muerto resucitado de la
  Escritura y ahora mismo solo se menciona en el texto de salida del capítulo.

#### Cap. 3 — El Monte Carmelo (1 Reyes 18)
- ✅ El altar del Carmelo y los doce cántaros (18:33)
- ✅ **Abdías y los cien profetas escondidos en cuevas** (18:3-16). Un hombre del palacio de Acab
  jugándose el cuello: el contrapeso perfecto al «me he quedado solo» del capítulo 4.
- ✅ **«¿Hasta cuándo claudicaréis entre dos pensamientos?»** (18:21). El pueblo no responde
  palabra.
- ✅ **La nubecilla como la palma de la mano** y la carrera de Elías delante del carro hasta
  Jezreel (18:41-46).

#### Cap. 4 — La Cueva de Horeb (1 Reyes 19:1-18)
- ✅ La boca de la cueva: viento, terremoto, fuego y el silbo apacible (19:11-12)
- ✅ **El enebro y el ángel con la torta cocida** (19:4-8). Elías pide morirse y lo que recibe es
  comida y sueño. Encaja como sala de descanso del capítulo.
- ✅ **Las tres unciones: Hazael, Jehú y Eliseo** (19:15-16). Es la agenda de todo lo que viene
  después; sin ella, Hazael y Jehú aparecen de la nada en el Acto II.
- ✅ **Los siete mil que no doblaron la rodilla** (19:18).

#### Cap. 5 — El Llamado (1 Reyes 19:19-21, 20, 21, 22; 2 Reyes 1)
- ✅ Eliseo hijo de Safat, arando con doce yuntas (19:20)
- ✅ **La viña de Nabot** (21) como encuentro, no solo como jefe.
- ✅ **Ocozías y el fuego sobre los capitanes** (2 Reyes 1). Dos cincuentenas consumidas y un
  tercer capitán que se arrodilla y suplica: **la decisión del jugador es si perdona al tercero**.
  Episodio icónico de Elías que hoy no está en ninguna parte.
- ❌ **Eliminados (2026-08-30)**: "El profeta vendado" (1 R 20:35-43) y "Micaías" (1 R 22).
  Elías no está en ninguna de las dos escenas — el juego ponía al JUGADOR en el papel de otros
  profetas — y por el mandato de que todo relato vaya anclado a Elías o Eliseo, fuera los dos.

#### Transición — El Ascenso (2 Reyes 2:1-14)
- ✅ Existe como pantalla de herencia.
- ✅ **El viaje Gilgal → Betel → Jericó → Jordán**, con los hijos de los profetas repitiendo en
  cada parada «¿sabes que hoy te quitarán a tu señor?», el manto que golpea el agua y la parte,
  la petición de la doble porción y el carro de fuego. Hoy son cinco líneas de texto; es el
  momento más importante del juego.

#### Cap. 6 — Las Aguas de Jericó (1 Reyes 16:34; 2 Reyes 2:13-25)
- ✅ Los hombres de Jericó y la sal (2:19-22)
- ✅ **Los cincuenta que quieren buscar a Elías** (2:15-18, pasada de coherencia 2026-08-31):
  «quizá lo ha echado el Espíritu en algún monte». Sustituye al encuentro del cruce del Jordán
  (2:13-14), que ya lo cuenta la cinemática del Ascenso — rejugarlo en Jericó era contar dos
  veces la misma escena.
- ✅ **Los muchachos de Betel** (2:23-25). Se trata como **encuentro de juicio, sin combate**:
  la burla al profeta y lo que se sigue de ella. No hay enemigos infantiles.
- ✅ **Hiel de Jericó** (1 R 16:34, 2026-08-30): la ciudad reedificada al precio de dos hijos,
  sobre el juramento de Josué. Es el POR QUÉ de las aguas malas que la sal va a sanar.

#### Cap. 7 — El Aceite de la Viuda (2 Reyes 3; 4:1-7, 38-44)
- ✅ La mujer del profeta muerto y el acreedor (4:1)
- ✅ **Las zanjas de Moab**: cavar en el valle sin viento ni lluvia, y el agua que al alba parece
  sangre a los moabitas (2 Reyes 3:16-23).
- ✅ **«Muerte hay en la olla»** y el puñado de harina (4:38-41).
- ✅ **Los veinte panes de cebada para cien hombres** (4:42-44). Existe como objeto; falta como
  episodio.
- ✅ **El rey de Moab sobre el muro** (2 R 3:26-27, 2026-08-30): el sacrificio del primogénito y
  el sitio que se levanta. Contado con sobriedad («lo que hace allí no debe hacerse»); la
  elección piadosa cobra en el ARMA, como los sirios de Dotán — levantas un sitio ganado.

#### Cap. 8 — El Hijo Sunamita (2 Reyes 4:8-37; 6:1-7; 8:1-6)
- ✅ La sunamita y el aposento de paredes (4:9-10)
- ✅ **El niño muerto**: el báculo de Guiezi que no sirve, y Eliseo tendido sobre el niño hasta
  que la carne entra en calor (4:29-35).
- ✅ **El hacha que flota** (6:1-7). Un hacha prestada, un palo echado al agua. Milagro pequeño y
  doméstico: justo el contrapunto que le falta al Acto II.
- ✅ **La sunamita recupera sus tierras** (8:1-6), siete años después y por la palabra de Guiezi.

#### Cap. 9 — Naamán el Sirio (2 Reyes 5)
- ✅ Naamán a la puerta, ofendido porque no salen a recibirle (5:11)
- ✅ **La muchacha israelita cautiva** que dice a su señora dónde está el profeta (5:2-3). Toda la
  historia arranca en una niña esclava.
- ✅ **Naamán, limpio** (5:15-19, pasada de coherencia 2026-08-31): la carga de tierra de dos
  mulas y el perdón de Rimón — y el presente que Eliseo rechaza, que es EXACTAMENTE la
  tentación que pierde a Guiezi, el jefe del capítulo.
- ✅ Guiezi y la lepra (5:20-27) — existe como jefe del capítulo.
- Hazael y la redoma de Jehú vivían aquí y **se mudaron al cap. 10** (2026-08-31): en el libro
  van DESPUÉS del sitio de Samaria, y ver a Ben-adad en su lecho de muerte un capítulo antes de
  pelear contra él mentía sobre el orden.

#### Cap. 10 — El Ejército Celestial (2 Reyes 6:8 - 9:13)
- ✅ El criado de Eliseo y los carros de fuego (6:15-17)
- ✅ **Los sirios ciegos en Dotán**: los lleva a Samaria, y en vez de matarlos les pone mesa y los
  despide (6:18-23). El acto más contraintuitivo del profeta.
- ✅ **El sitio de Samaria y los cuatro leprosos a la puerta** (6:24 - 7:20). El hambre, la
  profecía de la harina barata mañana a esta hora, y los cuatro que deciden entrar al campamento.
- ✅ **El príncipe que no comió de la harina** (7:2, 7:17): «lo verás con tus ojos, mas no comerás
  de ello».
- ✅ **Hazael y Ben-adad** (8:7-15, aquí desde 2026-08-31): roto el cerco, «¿por qué llora mi
  señor?». Eliseo llora por lo que ese hombre va a hacer. Como «Siete años después» en el
  cap. 8, el relato mira más allá del jefe del capítulo.
- ✅ **La redoma de aceite** (9:1-13, aquí desde 2026-08-31): Eliseo envía al hijo de los
  profetas a ungir a Jehú — «unge, abre la puerta y huye». La última de las tres unciones de
  Horeb, en marcha: es el último relato del juego y el epílogo lo cobra en su primera escena.

#### Epílogo — El Legado (2 Reyes 9; 13:14-21), TRES escenas
Sigue siendo desbloqueable (fe en Profeta Ungido al ganar el cap. 10) y sigue sin mover vida ni
fe. Cierra los dos arcos de los profetas y NADA más (hubo una ampliación a 9 escenas hasta la
caída de Jerusalén; revertida el 2026-08-30 por el mandato de anclar todo a Elías/Eliseo):
- ✅ **Jehú y la muerte de Jezabel** (9): cumple la palabra de Elías sobre la viña de Nabot.
- ✅ **El arco de Joás y las flechas de la victoria** (13:14-19): el lecho de muerte de Eliseo;
  la decisión sigue cambiando las últimas palabras del juego (`joash_full`).
- ✅ **Los huesos de Eliseo** (13:20-21): el último milagro del profeta, ya muerto.

**Recuento (2026-08-31):** **46 piezas jugables de relato** (`Ev::COUNT`): 39 encuentros de
capítulo (pozos 3/3/4/4/3/4/5/4/3/6) + 4 paradas del Ascenso + 3 escenas del epílogo — más el
prólogo cinemático, las 10 intros, la cinemática del Ascenso y los 4 objetos que son pasaje
(Arco de Joás, Higos, Reloj de Acaz, Panes de Cebada). Con eso, TODO episodio de Elías y Eliseo
en 1-2 Reyes tiene su pieza jugable o su objeto — más las 20 escenas guionizadas de
`scenes.h` (fuera del Códice). Techo: 46 de los 64 bits del u64 `codex_events` — crecer
más allá de 64 pide una segunda máscara (guardado v10).

**Escenas guionizadas (2026-09-10, `game/scenes.h`):** lo que la audiencia estrenó en el
cap. 1 lo tiene ahora todo el juego, por mandato del usuario («lo mismo [...] al comienzo de
cada capítulo y a lo largo de todo el juego»; «antes de la pelea con cada jefe debe haber un
diálogo»). **20 escenas**: 10 aperturas (la audiencia + una por capítulo 2-10, ANTES de la
intro cinemática: Sarepta 17:8-11, Elías ante Acab 18:17-19, el mensajero de Jezabel 19:1-3,
camino de Jezreel 19:21/21:1-16, la orilla del Jordán 2 R 2:15, los tres reyes en Edom 3:9-15,
el aposento de Sunem 4:12-16, la carta de Ben-adad 5:5-8, el rey avisado 6:9-13) y 10 diálogos
de jefe (al entrar en la arena, con el jefe de pie: el jefe habla, el profeta responde, la
última caja abre la pelea; una vez por partida y capítulo). Ninguna repite lo que cuenta la
intro ni lo que juega el pozo; los reyes sin retrato (Joram, Josafat) van sin cara. Las ~90
cajas viven fuera de `Ev` (no son relatos que se descubren) y no cuentan en el Códice:
`Ev::COUNT` vuelve a 46 de 64. Guion: `scripts/escenas_check.txt`.

**Pasada de coherencia (2026-08-31):** la historia sigue el ORDEN de 1-2 Reyes de la intro en
adelante: nada que el prólogo o un capítulo anterior ya contó se vuelve a jugar como presente
FUERA DE ORDEN (fuera la audiencia con Acab del POZO del cap. 1 — desde el 2026-09-09 vuelve
como apertura guionizada, antes de Querit — y el cruce del Jordán del cap. 6), los relatos de
2 R 8-9 viven tras el sitio de Samaria (cap. 10), y las intros de los caps. 3, 5, 6 y 10
abren con la bisagra que las une al capítulo anterior («Ve, muéstrate a Acab» 18:1, el encargo
de Horeb, el manto cruzando el Jordán, la guerra siria tras Naamán).

**Lo que queda de arte**: los encuentros sin cara propia llevan `Portrait::NONE` (estado
soportado: la caja se dibuja igual). El muro de Moab pide viñeta; las paradas de las cadenas
van sobre negro y no la necesitan.

### Cómo se sirve todo eso

- ✅ `EventDef` pasa de uno por capítulo a un **pozo por capítulo** (`namespace Ev` + 
  `get_chapter_events`). Desde el mandato de mundo fijo (2026-08-27, ver §3.4), el generador
  coloca **TODOS los relatos del pozo** (3-5 salas `Event` por capítulo, en salas fijadas por
  configuración): la historia no se sortea — en cada partida se puede ver el pozo entero.
- ✅ Cada encuentro visto queda en el Códice (`SaveData::codex_events`, guardado v8). La pestaña
  de Capítulos muestra **«Relatos N/M»**, que es el contador que hace visible lo que te estás
  perdiendo.
- Los encuentros que son **milagro** (la tinaja, el niño resucitado, el hacha que flota) dan un
  efecto de partida; los que son **decisión moral** (el tercer capitán de Ocozías, los sirios de
  Dotán) mueven la fe. Ninguno es solo texto.

---

## 3. El mundo: exterior e interior

### 3.1 Dos clases de espacio

| | **Exterior** | **Interior** |
|---|---|---|
| Tamaño | 2x2 pantallas (40x22 tiles, 640x352 unidades) | 1 pantalla (20x11), como hasta ahora |
| Cámara | Sigue al profeta, recortada contra la zona | Clavada |
| Borde | Del bioma: acantilado, matorral y roca, o ribera | Muro de verdad |
| Ejemplos | Querit, los campos de Jezreel, la ribera del Jordán | Palacio de Acab, templo de Baal, cueva de Horeb, casa de la Sunamita |

**El mismo código sirve para las dos.** `Room` lleva su tamaño (`w`, `h`) y `Camera` ya sabe
recortar contra los límites de la sala: con una sala de una pantalla, el recorte deja la cámara
inmóvil y sale gratis el comportamiento de siempre.

**Hecho el 2026-08-26.** Un interior por capítulo, con plantilla propia (`build_interior`) en vez
de generación por biomas, colgado de una sala exterior por una puerta. Su nodo va detrás de la
rejilla, así que el minimapa lo salta y el guardado —que guarda un índice— no cambió de formato.

### 3.2 Bordes por bioma

`BiomeStyle::border` con tres valores:

- **`Cliff`** — cañón, montaña, cueva. Lo que hay hoy: pared de roca. Correcto en el Querit
  (es un cauce encajonado), en el Carmelo y en Horeb.
- **`Natural`** — desierto, campos, ribera. Franja de dos tiles de roca suelta, matorral,
  palmeras secas y hierba alta. Bloquea, pero **se lee como que el terreno sigue**. Y el lado
  conectado se abre **casi entero**, no por un vano de seis tiles: un campo abierto no es un
  corral con puerta.
- **`Masonry`** — ciudad, casa noble, campamento sitiado. Sillería de verdad, vano estrecho y
  antorchas. Aquí el muro **sí** significa algo.

Reparto: 1 Cliff · 2 Natural · 3 Cliff · 4 Cliff · 5 Natural · 6 Masonry · 7 Natural ·
8 Masonry · 9 Natural · 10 Masonry.

Los "edificios" del interior de la sala (bloques de piedra) **no se restringen**: al mirarlo,
los únicos capítulos que los tienen ya son aldeas, ciudades y cuevas — en los campos abiertos
el contador ya estaba a cero. Lo que sí cambia es la cantidad: escalan con el área, porque dos
bloques sueltos en una sala de 2x2 no se leen como una aldea sino como dos trozos de muro
huérfanos.

### 3.3 Tamaño del capítulo

- Rejilla de **4x3** salas en la mayoría de capítulos, **4x4** en el 5 y el 10.
- 12 salas de 2x2 pantallas = ~48 pantallas por capítulo, contra las 9 de hoy.
- **No todas están en el camino al jefe.** Eso es lo que convierte explorar en una decisión.
- Reparto de tipos por mapa (FIJO por configuración desde 2026-08-27, ver §3.4): en 4x3,
  TODOS los relatos del pozo (3-4) · 1 Tesoro · 1 Altar · 2 Elite · resto normales; en 4x4,
  relatos completos · 2 Tesoro · 2 Altar · 2 Elite. Las 4x3 pierden un Tesoro y un Altar
  respecto al plan original porque los relatos completos no caben de otra forma — y el relato
  pesa más. Hasta 18 enemigos por sala (contra 12): doce enemigos en cuatro pantallas es un
  descampado.

### 3.4 Mundo FIJO (mandato del usuario, 2026-08-27)

> "Los mapas, la disposición de los enemigos, son fijas. No cada vez que juegues cambie.
> Las salas no se pueden generar aleatoriamente en runtime."

- **El terreno y la colocación de enemigos de cada sala son SIEMPRE los mismos**: entre
  lanzamientos, al Continuar y entre partidas nuevas. Como en *A Link to the Past*: Hyrule
  no se re-sortea, se aprende.
- La generación procedural sigue corriendo en runtime, pero como **función pura de
  (capítulo, sala)**: la semilla de cada capítulo es una constante (`Config::CHAPTER_SEEDS`)
  y cada sala re-siembra el generador antes de generarse. Cambiar una semilla es una decisión
  de diseño que redibuja ese capítulo para todo el mundo.
- **La historia y el reparto de salas son configuración, no azar** (decisión del usuario al
  ejecutar el plan): la tabla `CHAPTER_LAYOUT` (rooms.cpp) fija qué celda de la rejilla es
  inicio, jefe, tesoro, altar, elite y qué relato cuenta cada sala Event. Todos los relatos
  del pozo aparecen en todas las partidas.
- **Al Continuar, lo que hiciste queda hecho** (guardado v9): las salas despejadas siguen
  despejadas, el cofre abierto sigue abierto y el secreto abierto sigue abierto. La derrota
  sí rebobina el capítulo entero, enemigos incluidos.
- **Dónde queda la variedad de rejugada**: en las cartas de elección (objetos y milagros),
  los drops, las decisiones de los encuentros y el orden en que explores — no en el mapa.

---

## 4. Exploración: por qué desviarse del camino

**Hecho el 2026-08-26.** La regla que manda sobre todo lo demás: **un secreto tiene que verse
antes de poder abrirse**. Si la grieta no se ve hasta que tienes el milagro, no hay motivo para
volver — y volver es todo el punto. Por eso la tinaja, la grieta y el hueco estorban el paso.

1. ✅ **Secretos de bioma.** Matorral que se corta, tinajas que se rompen, grietas en la roca y
   huecos que cruzar. Detrás: ofrendas, fe o corazones.
2. ✅ **Los milagros como llaves.** Fuego del Cielo quema matorral y abre grietas. Sanar Aguas
   seca una charca. Carro de Fuego cruza un hueco. Ojos Abiertos revela lo oculto.
3. ✅ **Salas opcionales de verdad**: la rejilla 4x3/4x4 de la fase 1 ya deja varias fuera de la
   ruta al jefe, y ahora tienen algo dentro.
4. ✅ **El Códice como colección visible**: «Relatos N/M» por capítulo (fase 2).
5. ✅ **Cachés de ofrendas** que sobreviven a la run: la ofrenda es el pago más probable de un
   secreto (60 %) precisamente porque vida y fe ya las suelta cualquier enemigo.
6. ✅ **El objetivo del capítulo en el mapa grande** (`ChapterLore::objective`).

Nada de esto tocó el formato del guardado: la celda **es** su propio estado y la sala entera se
cachea, así que abrir un secreto persiste durante la partida sin un campo nuevo.

---

## 5. Personajes no jugables

El PNJ del encuentro se dibujaba con la celda de **antorcha** del tileset. Eso se acabó.

- Hoja propia `assets/sprites/npcs.png`: una fila de celdas de 128 px, solo `Idle`. Se generan
  con el mismo pipeline de Blender que los personajes, reusando las recetas que ya existen para
  los retratos (la viuda de Sarepta, Eliseo labrador, el hombre de Jericó, la mujer del profeta,
  la sunamita, Naamán, el criado).
- **No llevan hoja de 15 filas**: no pelean, no andan. Una fila basta y `assets/` no crece.
- El PNJ se dibuja como **prop ordenado por Y**, no como entidad del ECS: en pantalla es lo
  mismo —sombra blanda, orden por profundidad, 32 unidades— y no gasta ni un componente ni una
  de las 512 entidades para alguien que no pelea, no anda y no muere. Lleva un indicador
  flotante sobre la cabeza mientras el encuentro está sin resolver.
- Los que ya tienen retrato lo conservan en la caja de diálogo. Los encuentros que no son una
  persona sino **una escena** (los cuervos, el altar del Carmelo, la boca de la cueva) siguen con
  su viñeta y **sin** PNJ en el suelo: ponerle una cara a una escena sería mentir sobre lo que el
  jugador mira.

---

## 6. Hoja de ruta

| Fase | Qué | Estado |
|---|---|---|
| **1** | Mundo exterior: salas de 2x2 pantallas, cámara viva, bordes por bioma, rejillas mayores, PNJ de pie en la sala | **hecho** (2026-08-26) |
| **2** | Los ~30 encuentros que faltan, con sus retratos y sus efectos | **hecho** (2026-08-26); faltan 9 viñetas de escena |
| **3** | Interiores con muros de verdad: palacio de Acab, templo de Baal, cueva de Horeb, casa de la Sunamita | **hecho** (2026-08-26) |
| **4** | Secretos y milagros como llaves de exploración | **hecho** (2026-08-26) |
| **5** | El Ascenso como secuencia jugable de cuatro etapas (Gilgal, Betel, Jericó, Jordán) | **hecho** (2026-08-26) |
| **6** | El epílogo ampliado: Jehú y Jezabel, el arco de Joás, los huesos de Eliseo | **hecho** (2026-08-26) |

---

## 7. Qué cambia respecto al v1.0

| Tema | v1.0 | 2.0 |
|---|---|---|
| Referencia | ninguna declarada | *A Link to the Past* |
| Sala | 1 pantalla, cámara fija | exterior 2x2 pantallas con cámara; interior 1 pantalla |
| Bordes | muro sólido en todas | según bioma; sillería solo en construido |
| Capítulo | 3x3 salas | 4x3 / 4x4 |
| Encuentros | 1 por capítulo | 3-4 por capítulo, sorteados de un pozo |
| Cobertura bíblica | los episodios de titular | **todo** Elías y Eliseo en 1-2 Reyes |
| PNJ | icono del tileset | entidad con sprite propio |
| Duración de run | 20-35 min | 45-60 min (el autoguardado por sala ya sostiene la sesión móvil de 5-15) |
| Estética | "Pixel Art 16-bit" | figurita 3D suave (decidido 2026-08-19) |
| Personajes desbloqueables | tercer profeta jugable | **cosméticos** (decidido 2026-08-25) |
| Steam | integración Steamworks | logros propios, multiplataforma; Steam en el backlog |

Lo que **no** cambia: las cifras de fe, milagros, enemigos y objetos del v1.0 siguen siendo las
implementadas y valen como referencia. Los pilares siguen en pie. Y la regla de que ningún
contenido contradiga 1-2 Reyes es más fuerte que nunca, porque ahora entra mucho más texto.
