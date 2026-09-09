# `tools/blender/` - el pipeline de sprites (Blender 4.1 + blender-mcp)

**Regla que manda sobre todas las demas: cada render se mira imagen a imagen antes de
copiarlo a `assets/`.** La cobertura del log no basta - la tunica rota de la estampa 5 del
prologo llevaba semanas ahi porque solo se miraba ese numero.

```
tools/
  blender/mantle_sprites/ - Python pipeline (Blender 4.1 + blender-mcp) that builds the 10 rigged
                            characters procedurally, animates them per AnimID and renders the sheets
                            (run.py: `main(only=[...])`, default quantize=False = smooth post_soft;
                            quantize=True = legacy pixel-art). FRAME=128, SS=2, feet baseline = row 112.
                            Stylisation in characters.py: HEAD_SCALE 1.45 (parts on the head bone scaled about
                            HEAD_PIVOT), LION_HEAD_SCALE, palette through parts.vivid() (SAT 1.35, skin 1.15).
                            Faces: `characters._face()` (sclera + iris + brow + nose + mouth, EYE_*/BROW_TILT
                            constants, `eyes`/`brow`/`brow_color`/`mouth` keys per SPEC) hangs off the `head`
                            bone, so hair/hood/helmet MUST leave z>=1.60 of the head clear or the rostro
                            disappears again. Smoothing: `parts.box(round=..., taper=...)` bevels every edge
                            (opt-in, so tiles.py stays crisp) and `parts.tube(caps=, rim_round=)` gives open
                            rings (helmet band, hood cowl) and soft cap edges.
                            tiles.py renders the terrain tileset (`tiles.main(SPRITES_DIR, TMP_DIR)`, CELL=64,
                            TILE_ROWS=9; `only=[...]` patches the existing tiles.png instead of blanking it);
                            its LAYOUT must match `TileSprite` in src/game/rooms.h. GROUND = opaque
                            top-down FLAT cells, DETAILS = transparent top-down decals, SMALL/PROPS/PROPS_TALL = 30 deg.
                            cine.py renders the 8 prologue panels AND the 10 chapter-intro
                            panels, PANELS[11..20] = capitulo+10 (`cine.main(SPRITES_DIR, TMP_DIR, only=[n])`,
                            `cine.flatten(n, bg=<cielo de la escena>)` para verlas compuestas). Es el UNICO camino
                            con camara ANCHA y en PERSPECTIVA: las hojas y el tileset usan orto porque un sprite
                            tiene que casar con la rejilla, pero en orto no hay horizonte y los montes lejanos
                            flotan sobre el suelo. Escala: 1 unidad de Blender = 1 METRO (humano 1.75).
                            **`setup_cine` va PRIMERO en cada escena** y el decorado se cuelga de la camara:
                            `frame_at(y, safe)` da la banda `(x0,x1,zb,zt)` visible en ese plano, `near_y(z, bite)`
                            la distancia a la que el borde inferior corta el suelo (lo que se apoye MAS LEJOS se ve
                            entero y flota en mitad del panel en vez de entrar por el borde), `lift(zb,zt,k)` la
                            altura para asomar `k` dentro del cuadro, y `spread(n,x0,x1)` reparte. `SAFE_NEAR` 0.78
                            = el borde de PANTALLA (el motor solo ensena el 80% central del panel) menos el paneo;
                            recorta SOLO en X, porque en vertical el panel se ve entero. Colocar el primer plano a
                            ojo es lo que dejo cinco de las ocho capas NEAR en blanco (2026-08-21).
                            `render_panel` loguea la cobertura de cada capa: <3% = perdida fuera del cuadro,
                            MID >90% = tapa el cielo y la capa lejana entera. Mirar ese numero SIEMPRE.
                            Mas reglas: el suelo (`ground(far=)`) tiene que ACABAR antes de lo que haya en la capa
                            lejana o lo tapa; para que se vea cielo hace falta `elev < atan(18/lens * 0.45)` (con
                            lens 45 son 11,4 grados: por encima, el cuadro entero cae bajo el horizonte y la capa
                            media sale 100% opaca); `flat_layer(FAR)` (luz FLAT, sin cavity ni specular) para
                            cordilleras y humo, o el cavity subraya cada esfera solapada ("collar de perlas") — NO
                            para silleria, ahi el cavity es justo lo que dibuja las juntas; Workbench con luz de
                            estudio ACLARA cualquier plano oscuro hasta gris medio, asi que un vano negro se hace
                            con geometria hundida (jambas + sofito + fondo), no bajando el color; los props de
                            tiles.py van a escala de tile (una antorcha mide 1,85 m: `scale=2.4` la deja en 4,4 m,
                            saliendose por el techo); las poses se aplican en el espacio del armature y X negativo
                            lleva el brazo AL FRENTE (-90 = horizontal, -180 = vertical), pero girar el brazo del
                            cayado (`hand.R`) mas alla de la horizontal VUELCA el baculo y deja la llama a la
                            altura de la cintura, y de frente a camara un brazo alzado pasa por delante de la cara;
                            y `render.post_soft` SOLO vale para marcos cuadrados (`reshape(px, ss, px, ss)`) — los
                            paneles usan `render.post_soft_rect`.
  blender/blend/          - Source .blend per character (not runtime assets)
  blender/mantle_sprites/vfx.py       - hoja de efectos (EEVEE + emision, ver seccion propia)
  blender/mantle_sprites/portraits.py - las 28 celdas (Workbench, encuadre por bounding box;
                                        19 retratos de personaje + 3 viñetas de escena en `SCENES`)
```

### Pulido de las estampas: la tunica sigue a las piernas y nada sale poligonal (2026-09-09)

El usuario ensenó la estampa 5 del prologo — Elias arrodillado con «los pies fuera de la
tunica» — y pidio dos cosas: que eso se detecte ANTES de entregar, y un pulido general de
los 22 paneles («mas detalle, que no se vea tan poligonal»). Lo primero es un metodo, no
codigo: **cada render de `cine.py` se mira panel a panel (`flatten` + recorte al 80%
central) antes de copiarlo a `assets/`**. La tunica llevaba semanas rota en DOCE actores de
once paneles y nadie lo vio porque solo se miraba la cobertura del log.

- **`cine._drape_legs`**: la falda de characters.py es un cono rigido colgado de `hips`, y
  al doblar un muslo 80 grados la pierna sale por la pared del cono. Para cualquier pose con
  un muslo a mas de 45 grados (`POSE_KNEEL`, `POSE_DRINK`, `POSE_ENTHRONED`, `POSE_RECOIL`)
  se cambia por CUATRO piezas, cada una en su hueso: cadera, regazo sobre el muslo mas
  doblado (thigh), rodilla + caida hasta el suelo (shin) y cola sobre el otro muslo. Va ANTES
  de posar y con el armature en el origen (`attach` fija en reposo). El regazo se ESTRECHA
  hacia la rodilla y va plano (y x0.58): de seccion constante era un cojin rectangular
  sobre el que el profeta parecia sentado. Las poses de pie no cambian ni un vertice.
- **`parts.set_quality('cine')`** (bisel 8 tramos, esferas 64x40, tubos 48, `DETAIL`):
  `render_panel` lo pone al montar y lo RESTAURA al acabar, porque una hoja renderizada
  despues en la misma sesion de Blender tiene que salir como siempre. `DETAIL` enciende los
  pliegues de las tunicas (`tube(flutes=9, flute_amp=0.07)`): nueve lobulos que crecen hacia
  el bajo. A 128 px no se ven y no se piden.
- **`parts.rock`** (esfera con cuatro ondas fijas de radio, semilla por nombre): riscos
  (`spire`), penascos (`boulder`), el monton de piedras (`rock_cluster`, que sustituye a
  `tiles.build_rock` en los paneles) y hasta los muros de agua del Jordan (rough 0,05 =
  agua que ondula). La caja biselada con 3 tramos era un cristal de cuarzo y la esfera
  lisa, un globo.
- **`cine.mound_field`**: una cordillera de MID/NEAR es UNA superficie (mapa de alturas =
  maximo de las cupulas, desenfocado) y no esferas solapadas, porque el cavity subrayaba
  cada solape (collar de perlas). Tres trampas que costaron una pasada cada una: (1) fuera
  de su elipse la cupula tiene que SEGUIR bajando (falda) — cortada a ras dejaba un escalon
  en dientes de sierra y aplanada a cero un delantal; (2) el recorte al plano `floor` se
  hace SUBIENDO los vertices al plano, no tirando celdas, o el borde sale en sierra;
  (3) **en FAR siguen siendo esferas** salvo `skirt=`: en varias estampas la camara mira tan
  abajo que el horizonte queda fuera del cuadro (05: elev 12 con lente 55) y lo que se ve
  de la sierra lejana es la MITAD INFERIOR de las esferas — un mapa de alturas ahi
  desaparece entero (L0 al 0%, y el log lo canta). Con la camara horizontal pasa lo
  contrario (08: lentejas flotando) y ahi `skirt=0.02` apoya la sierra en el horizonte.
  `stagger=` baja la separacion de las tres filas para un filo de cumbre de cerca
  (con 1,5 m quedaban VENTANAS de cielo entre filas).
- **`road_strip` es UNA malla** (`parts.ribbon`, perpendicular media en cada punto) y el
  trazado se remuestrea con Catmull-Rom: un cuadrilatero suelto por tramo dejaba una junta
  visible en cada codo, y con el rio suavizado eran veinte. La hierba (`tuft`) se arquea
  (`curl`): una cuna recta afilada es un diente.
- De regalo: `principe_samaria` llevaba un digito devanagari en un color hex (`'#233a६6'`)
  que `int()` tragaba por casualidad.

Gates: cero `PERDIDA` en el log de `cine.main`, y los tres guiones del motor
(`intro_check`, `intros_check`, `asccine_check`) con cero `falta la capa` y cero
`TEXT TRUNCATED`. Los PNG pesan lo mismo (33-34 MB los 66).

### Los paneles se iluminan con EEVEE: `look.py` (2026-09-09, segunda pasada)

El pulido de arriba no bastaba: el usuario pidio «meterle cana a los assets» y que las
estampas «se vean profesionales». Lo que las hacia maqueta no era la malla sino la LUZ:
Workbench de estudio (plana, sin sombras que se proyecten), un color liso por material,
cero distancia y suelos vacios. `look.apply(idx, ...)` se ejecuta en `render_panel` DESPUES
de montar la escena y no toca ningun constructor — cine.py sigue pidiendo colores lisos con
`M()` y look.py los convierte:

- **Luz**: sol calido con sombras suaves (cascadas limitadas a 70 m: con `clip_end` 20000
  el mapa de sombras se repartia por kilometros y las de cerca salian a manchas), un
  contraluz frio, ambiente del color del cielo, GTAO. Ocho rigs en `KINDS` (day, dusk,
  sunset, overcast, night, fire, interior, interior_night) y la tabla `LOOK` dice cual
  lleva cada panel. **La noche tiene luna** (sol 1.9, ambiente 0.9): a 1.0/0.6 la estampa 14
  era una mancha azul marino con una bolita naranja.
- **Las llamas emiten Y alumbran**: `torch_lights` pone una luz puntual por objeto de
  material emisivo (`EMISSIVE` por subcadena del nombre: fire, light, coals, fmane...).
  **Hay que llamar a `view_layer.update()` antes de leer `matrix_world`**: sin eso las
  piezas colgadas de un hueso devuelven la matriz de reposo o la identidad, y la llama del
  cayado alumbraba el ORIGEN de la escena — un charco naranja al pie de la camara en cinco
  estampas. La emision va a 1.8 (2.6 el nucleo): a 5.0 la llama era un disco amarillo
  recortado. El bloom de EEVEE si sirve aqui, al reves que en vfx.py: las antorchas van
  delante de algo opaco.
- **Materia**: `materialize` pasa cada material a nodos (Principled + HSV cuyo Value lo
  mueve un ruido a dos escalas, grano + manchas). La familia sale del NOMBRE del material
  (`_kind_of`: terrain, cloth, skin, hair, metal, water, emissive, prop) y `SURFACE` da
  amplitud, rugosidad, metalico y specular por familia.
- **Aire**: niebla exponencial `1 - exp(-z/D)` mezclada en el propio shader (Camera Data ->
  Mix Shader con una Emission del color del cielo BAJO del motor, tabla `SKY` copiada de
  ui/cinematic.cpp). Las capas planas (siluetas lejanas) van sin luz (Emission 1.35, que
  compensa el gain 1.7 de Workbench para el que se pintaron) y con un 30% menos de niebla.
  Cuando el cielo del motor es de un color raro, la niebla lo tine todo: 16 (verde) y 8
  (naranja) llevan `fog=` propio en `LOOK`, y 13 tambien para que el mar siga siendo mar.
- **Suelo**: `dress` reparte guijarros, penascos y matas por hash sobre el suelo que
  cine.py registra en `_REG` (ground, actores, props y cauces), esquivandolos. `dress=None`
  en interiores y ciudad: un guijarro en el enlosado es basura.
- **Post**: `post_soft_rect(gain=1.0)` (el 1.7 era para Workbench) + `look.grade` (curva S
  1.10, saturacion 1.10). El contorno oscuro se queda: es el estilo del juego.

**Render headless, no MCP**: 20 s por panel (antes 7) y el cliente MCP aborta a los 120 s
sin salida. `blender -b --factory-startup --python <runner> -- <out> [ids]` con un runner
que importe `mantle_sprites.run` y llame a `cine.main`; el rig se construye sin ventana.
Componer con PIL sobre el degradado de `look.SKY` (el cielo lo pinta el motor, no Blender)
y mirar los 22 en dos hojas de contacto ANTES de copiar nada.

## Efectos: `vfx.py` (el unico que NO usa el pipeline de personajes)

El arte lo genera `tools/blender/mantle_sprites/vfx.py`, que **no usa nada del pipeline
de personajes** y por buenas razones:

- **EEVEE, no Workbench.** Workbench es un sombreador de sólidos: no tiene emisión, y un
  fogonazo sin emisión es una bola de plástico gris.
- **Sin contorno oscuro.** Los 2 px de `post_soft` hacen que un personaje se lea como
  figurita; en una chispa solo la convierten en pegatina.
- **El resplandor se hace en el post, no con el bloom de EEVEE.** Comprobado: con
  `film_transparent` el bloom pinta color fuera de la geometría pero **no escribe alfa**,
  así que el halo se recorta y desaparece. `_glow()` desenfoca brillo **y** alfa.
- **Un toro sale de canto.** `primitive_torus_add` lo deja en el plano XY, o sea de perfil
  para esta cámara: sin girarlo 90° en X sale una raya horizontal, no un aro. Y como la
  escala de Blender es local y se aplica antes de girar, el aplastado en pantalla se pide
  en el eje local **Y**.
- Un haz de luz es un **cono**, no una esfera con la Z multiplicada por diez: eso es una
  cápsula de farmacia.

**Retratos** (`assets/sprites/portraits.png`, 6x2, celda de 96 px = 24 unidades). Los doce
personajes en primer plano, y **sí** con el pipeline de personajes tal cual (Workbench,
cavity, contorno): un retrato tiene que parecer el mismo muñeco que se ve en la sala.
`tools/blender/mantle_sprites/portraits.py` **encuadra midiendo la caja** del personaje ya
construido, no con alturas fijas: el león es un cuadrúpedo y el Agua Corrupta una masa más
ancha que alta, y con medidas de humano salían diminutos o cortados.

## Retratos y vinetas: `portraits.py` + las recetas de `characters.py`

El uso que el juego hace de estas celdas esta en `docs/encuentros-y-lore.md`.

- **Siete caras nuevas en `characters.py`**: `viuda_sarepta`, `eliseo_labrador`,
  `hombre_jerico`, `mujer_profeta`, `sunamita`, `naaman`, `criado_eliseo`. Existen **solo
  para su retrato**: no andan por el mundo, así que **no** tienen hoja de 896x1920. Eso lo
  garantiza `characters.SHEET_NAMES` — `NAMES` ya no es `list(SPECS.keys())`, porque si lo
  fuera un `run.main()` sin argumentos les fabricaría siete hojas de 15 filas que nadie
  carga.
- **Dos piezas nuevas del builder**, y las dos por una razón concreta:
  - **`headcloth`** (+ `headcloth_len='long'` para velo, + `headcloth_cord`). No vale
    reusar `hood`: la capucha es un cuello **redondo y cerrado** y el paño **cae a los
    lados de la cara**. Cinco de los siete llevan algo en la cabeza, y con una sola
    silueta se parecerían todos en un retrato de 32 unidades.
  - **`band`**: aro liso, sin puntas. `crown` y `diadem` llevan puntas, y unas puntas
    sobre la mujer de Sunem la convertirían en reina — y 2 Reyes 4:8 dice "mujer
    importante", no reina. La lleva ella (oro) y Naamán (oro).
  - Arrancan en z 1.700 como el casquete de pelo, así que **la regla de dejar libre
    z>=1.60 por delante sigue en pie**: la frente y las cejas se ven.
- **Lo que NO hizo falta**, contra lo que se sospechaba al planificarlo: props de mano
  (vasija de aceite, aguijada de bueyes). El encuadre del retrato corta por la cintura y
  todo lo que cuelga de la mano (z ~0.6-0.9) **queda fuera del cuadro**. Lo que distingue
  a un locutor de otro está de los hombros para arriba: paño, pelo, barba y color de túnica.
- **Las dos variantes tenían que distinguirse de su original de un vistazo**, y ese fue el
  gate: `eliseo_labrador` es la misma cara que `eliseo` pero sin el manto blanco y sin el
  cayado de luz (las dos cosas que aún no le han dado), y `naaman` es un noble sirio **sin
  casco ni penacho** — que es justo lo que lleva el General Sirio —, con pelo y barba
  negros a la vista, cinta de oro, teal en vez de rojo y túnica larga.
- Ojo con el color: la viuda de Sarepta y el criado salían los dos **beige** y no había
  forma de decir cuál era cuál. Se separan por **temperatura**: ella cálida (terracota
  gastada), él frío (paño pardo sobre túnica cruda).

**Las tres viñetas** (`SCENES` en `portraits.py`, no en `characters.py`: no son personajes
y solo existen como celda de retrato). Se encuadran distinto — una persona se encuadra por
la cara (`HEAD_FRAC`) y una escena tiene que entrar **entera**— y se miran con **13 grados
de picado** (`SCENE_ELEV`, `setup_scene_cam`): de frente puro la zanja del altar cae justo
detrás del altar y no se ve. Lo que costó tres pasadas y conviene no volver a intentar:

- **El cuervo va DE PERFIL**, mirando a `-X`. Es el único modelo del proyecto que no mira a
  `-Y`, y por una razón: de frente, un pájaro simétrico con algo en el pico sale con la
  comida cruzada sobre la cara y se lee como un babero o como un pico amarillo de tucán.
  De perfil el pico es una cuña que sobresale y el pan cuelga de su punta. Ojo también con
  el giro del pico: la caja ya está construida a lo largo de X, así que girarla 78 grados
  la pone **de pie** y el cuervo mira al cielo.
- **La zanja del altar va DELANTE**, no debajo. Puesta bajo las piedras —o detrás de un
  labio de tierra— el agua no se ve en absoluto, y el agua es justo lo que hace el pasaje.
- **El vano de la cueva es un fondo PLANO y hundido**, no una esfera oscura: una esfera se
  lee como una bola metida en el hueco, no como un hueco. Y la pared son **tres masas
  grandes**, no una corona de esferas iguales (el cavity subraya cada una y sale el "collar
  de perlas" que ya avisa la sección del tileset); lo que estrecha la boca va **pegado a
  las masas y de su mismo color**, o asoma dentro del vano como dos bultos flotando.
