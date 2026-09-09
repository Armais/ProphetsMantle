# Interiores y secretos

Viene del `CLAUDE.md` de la raiz. Pertenece a **`src/game/`** (`rooms.h/cpp` y los milagros
de `player.cpp`): leelo antes de tocar interiores, tinajas, grietas, huecos o
`Room::open_secrets`.

### Interiores con muros de verdad (GDD2.0 fase 3, 2026-08-26)

La otra mitad de la gramatica de *A Link to the Past*. El exterior dejo de tener muros en la
fase 1; aqui el muro vuelve **donde significa algo**: dentro de un edificio. Uno por capitulo,
con su nombre (`get_interior_name`): la cueva del arroyo, el templo de Baal, el palacio de Acab,
el aposento sunamita...

- **Un interior mide UNA pantalla**, y con eso la camara se queda clavada sola —`Camera` recorta
  contra `Room::world_w/h`—, o sea que el "interior sala a sala" del GDD **sale gratis**, igual
  que salio en la arena del jefe. Esa era toda la maquinaria que hacia falta y ya estaba puesta.
- **No se genera por biomas: se monta con una PLANTILLA** (`build_interior`). Es lo contrario del
  exterior a proposito: fuera el generador reparte terreno, dentro hay una habitacion hecha, con
  su anillo de muro, cuatro pilares que cortan la linea de vision, antorchas flanqueando la
  puerta, tinajas contra las paredes y **siempre un premio** (altar en capitulos impares, cofre
  en pares). **Sin enemigos**: el premio de explorar no puede ser otra pelea.
- **`apply_interior_style`**: dentro no hay hierba. Con el suelo del bioma, un interior se lee
  como un PATIO — el mismo arenal con flores y matas, solo que con muros alrededor. Se cambia la
  familia de suelo por la de silleria y los decorados por grietas, cascotes y moho.

**El enlace, que es donde estaba el riesgo del track:**

- El nodo del interior va **DETRAS de la rejilla** en `FloorMap::rooms`, con `grid_x/grid_y = -1`
  (asi `room_at(gx,gy)` no lo devuelve nunca). `RoomNode::interior_room` en la sala de fuera y
  `interior_of` en el interior.
- **El guardado NO cambia de formato**, que era la condicion de abandono: `SaveData::current_room`
  es un INDICE y la rejilla se regenera igual con la misma semilla, asi que guardar dentro de un
  interior funciona tal cual.
- **El minimapa no miente**: `draw_minimap` **salta** los nodos con `interior_of >= 0`, y estando
  dentro marca como actual **la sala de fuera** — dejar el mapa sin ninguna sala marcada es peor
  que aproximar.
- `Engine::use_doorway` guarda la sala en `room_cache` antes de irse, igual que una transicion
  normal: al volver, los enemigos muertos siguen muertos y el cofre abierto sigue abierto.
- `Engine::spawn_tx/spawn_ty` manda sobre `entry_direction`: por una puerta **no se llega por un
  borde**. Y la puerta respeta el **coyote time**, porque al salir se aparece justo debajo de la
  de fuera y sin esa gracia se volveria a entrar en bucle.

**El anfitrion del interior se elige SIN sortear** (`(chapter*7 + k) % room_count`, saltando la
sala de inicio y la del jefe). Con `randi_range` ahi, el sorteo entero se desplazaba y con el
**todos los mapas de la partida**: la sonda de tacto lo caza en el acto — el bot se salia de la
sala a media medida y los leones y los arqueros daban CERO bajas. Regla general que vale para
cualquier cosa que se anada a `FloorMap::generate`: **si no necesita azar, no lo gastes**.

La **puerta** (`TileType::Doorway`, celda `door` en las filas 7-8 del tileset) se coloca en la
sala de fuera **con el mapa ya terminado y las sendas trazadas** — puesta antes, `carve_trail` la
araba como cualquier obstaculo — y se apoya en la senda: una puerta en mitad del descampado no se
encuentra. El vano oscuro es **geometria hundida**, no un plano oscuro: con luz de estudio,
Workbench aclara cualquier plano oscuro hasta gris medio (misma leccion que la boca de la cueva
en portraits.py).

Guion: **`scripts/interiores_check.txt`**, con el comando de autoplay **`door`** — deja al
profeta **al lado** de la puerta, no encima (pisarla la dispara, y asi no se podria fotografiar
antes de cruzarla) y dice por donde entrar. Ojo: el tile de al lado tiene que estar **dentro** de
la sala, porque `is_tile_walkable` deja pasar fuera de la rejilla (lo necesita
`try_room_transition`) y la puerta del muro sur de un interior dejaba al profeta en el vacio.
Gate del log: la secuencia `Loaded room 7 -> 12 -> 7`.

### Secretos y milagros como llaves (GDD2.0 fase 4, 2026-08-26)

La tercera queja del usuario era que *"no se motiva al jugador a investigar"*. La fase 1 dio
sitio que recorrer; esto da **razones para desviarse**, con el mecanismo de *A Link to the Past*:
los milagros son las llaves, y lo que abren se ve mucho antes de tener con que abrirlo.

**El gate de diseño, que manda sobre todo lo demas: un secreto tiene que VERSE antes de poder
abrirse.** Si la grieta no se ve hasta que tienes el milagro, no hay motivo para volver — y
volver es todo el punto. Por eso la tinaja, la grieta y el hueco **estorban el paso**: el jugador
tropieza con ellos en el capitulo 1 y no puede abrirlos hasta el 3 o el 4.

| Llave | Abre | Pasaje |
|---|---|---|
| Un tiro cualquiera | tinaja (`TileType::Jar`) y matorral | — |
| **Fuego del Cielo** | matorral y **grietas** (`TileType::Crack`), radio 80 | 1 Reyes 18:38 |
| **Sanar Aguas** | **seca la charca**, radio 56 | 2 Reyes 2:21 |
| **Carro de Fuego** | **cruza el hueco** (`TileType::Pit`) mientras dura | 2 Reyes 2:11 |
| **Ojos Abiertos** | (ya existia) revela el mapa y marca enemigos y objetos | 2 Reyes 6:17 |

- **Todo pasa por `Room::open_secrets(centro, radio, mascara, out_loot)`**, un solo sitio. La
  mascara `SecretKind` es lo que decide que abre cada llave. Devuelve DONDE ha aparecido un
  hallazgo y no suelta la pieza: eso lo hace el Engine, para que haya **un solo lugar que decida
  que paga un secreto**.
- **La celda ES el estado.** La sala entera se cachea en `FloorMap::room_cache`, asi que abrir un
  secreto persiste durante la partida **sin tocar el formato del guardado** — que era el criterio
  de abandono del track. Y `Room::tile_hides_loot` va por `tile_hash`: la misma semilla esconde
  lo mismo en los mismos sitios, asi que tampoco hay nada que guardar.
- **La OFRENDA es lo mas probable (60%)**, y a proposito: vida y fe las suelta cualquier enemigo,
  asi que un secreto que pagara sobre todo en eso no daria ningun motivo para desviarse. Lo que
  hace que explorar de mas signifique algo es que pague **entre partidas**
  (`ItemComponent::Pickup::Offering` -> `progression.offerings`, guardado en el acto con
  `save(false)` para no re-sellar la fecha de caducidad). Y **que paga cada hallazgo sale de SU
  POSICION**, no de `randi_range`: sortearlo movería el generador de mapas (es el mismo `rand()`)
  y ademas un guion de regresion no podria exigir una ofrenda concreta.
- **`Entity ECSWorld::gap_crosser`**: quien puede cruzar huecos AHORA. Es una entidad y no un
  componente porque dura dos segundos, y `walkable_at` la mira antes de rendirse. Se apaga sola
  cuando acaba la embestida — si se quedara puesta, el profeta andaria sobre los precipicios el
  resto del capitulo. **Y la red `unstick` tiene que mirar por el MISMO test** (`walkable_at`,
  no `is_walkable_world` a pelo): cuando no lo hacia, sacaba al profeta del hueco en el primer
  frame de la embestida y el Carro nunca cruzaba de verdad — el gate `Chariot: crossing a pit`
  se imprimia igual, porque salta al PISAR el hueco, no al llegar al otro lado (2026-08-27).
- **El Carro de Fuego parado embiste hacia DONDE MIRAS** (`last_move_dir`), no hacia arriba. Con
  el `{0,-1}` que tenia, lanzarlo sin moverse salia siempre al norte — y siendo la unica llave
  que cruza un hueco, el hueco que tenias delante se quedaba sin cruzar.

**Densidad: contada POR SALA, no por area** — es la excepcion a la regla de la fase 1, y se
cerro midiendo. Escalando por area salian 11 grietas, 16 tinajas y **24 tiles de precipicio** en
una sola sala: la sonda de tacto se disparo a 12,6 s de TTK con el bot clavado detras de un
hueco. Valores buenos: 4-7 tinajas en poblado (1-3 fuera), 2-4 grietas en roca (0-2 fuera) y
**1-2 huecos, nunca en la sala de INICIO ni en la arena del jefe** (en la de inicio es donde se
aprende a andar y donde corre la sonda; en la arena solo seria una trampa en una pelea que ya va
justa de sitio). Y **la grieta NO para los tiros** aunque no se pise: son dos masas bajas con una
hendidura y el disparo pasa por encima — un secreto tiene que ser un hallazgo, no cobertura para
el enemigo.

**Dos celdas nuevas del tileset** (fila 7): `jar` y `crack`. La tinaja lleva perfil de PANZA (dos
troncos de cono opuestos) y no un cilindro — un cilindro con asas es un cubo de fregar —, y lo
que la lee como vasija a 16 unidades es el cuello estrecho y el borde volteado, antes que las
asas. La grieta se monta sobre la silueta del canto rodado PARTIDA EN DOS: con cajas salieron
dos maletas grises, y la hendidura tiene que ir **delante y llegar al suelo** (puesta detras la
tapaban las dos mitades y quedaba en una muesca en la coronilla).

**El hueco se pinta como UNA mancha**, no con la celda del tileset: `TileSprite::Pit` dibuja un
ovalo por casilla, y en una banda de dos de ancho **no hay ni un tile interior**, asi que salian
seis ovalos en rejilla. Se pinta un relleno oscuro plano y un labio de tierra SOLO en los lados
que dan a suelo firme, que es lo que lo lee como hundido.

**El objetivo del capitulo sale ya en el mapa grande** (`ChapterLore::objective`, que existia
desde el primer commit y no pintaba nadie: el mapa solo sabia decir "ve al rojo").

Guion: **`scripts/secretos_check.txt`**, con dos comandos de autoplay nuevos que existen porque
sin ellos la comprobacion depende de la suerte del mapa: **`pit`** (coloca al profeta al borde de
un hueco y MIRANDO hacia el) y **`collect`** (atrae todas las recogidas de la sala — un hallazgo
cae donde estaba el secreto y un guion no sabe donde fue). Gates del log: `Secret loot: N
hallazgo(s) ... (N ofrenda, N fe, N vida)`, `Secret offerings: +N (total M)` y `Chariot: crossing
a pit`.
