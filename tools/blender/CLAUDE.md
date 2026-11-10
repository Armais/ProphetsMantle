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
                            Stylisation in characters.py (ADULTA desde 2026-11-10, ver la seccion
                            "Personajes maduros"): HEAD_SCALE 1.06 (era 1.45; parts on the head bone scaled
                            about HEAD_PIVOT), LION_HEAD_SCALE 1.12, palette through parts.vivid() (SAT 1.0,
                            skin 0.96, VAL 0.97; era 1.35/1.15).
                            Faces: `characters._face()` (arco superciliar + ojo pequeno con parpado + iris +
                            ceja + pomulos + nariz en cuna + boca; EYE_*/BROW_TILT constants,
                            `eyes`/`brow`/`brow_color`/`mouth` keys per SPEC; `FACE_KEYS` = las piezas que
                            un jefe con otra cara quita) hangs off the `head`
                            bone, so hair/hood/helmet MUST leave z>=1.60 of the head clear or the rostro
                            disappears again. Smoothing: `parts.box(round=..., taper=...)` bevels every edge
                            (opt-in, so tiles.py stays crisp) and `parts.tube(caps=, rim_round=)` gives open
                            rings (helmet band, hood cowl) and soft cap edges.
                            tiles.py renders the terrain tileset (`tiles.main(SPRITES_DIR, TMP_DIR)`, CELL=128 =
                            8 texels/unit, GUTTER=2 px extruded per side so the engine filters it
                            LINEAR without bleeding (pitch 132; `BASE_CELL`=64 keeps the world size of
                            every model), TILE_ROWS=9; `only=[...]` patches the existing tiles.png instead of blanking it);
                            its LAYOUT must match `TileSprite` in src/game/rooms.h. GROUND = opaque
                            top-down FLAT cells, DETAILS = transparent top-down decals, SMALL/PROPS/PROPS_TALL = 30 deg.
                            cine.py renders the 8 prologue panels AND the 10 chapter-intro
                            panels, PANELS[11..20] = capitulo+10, mas PANELS[31..40] = plano B con
                            estampa propia del capitulo idx-30 (solo el 31, `cap01b_escondite`, de
                            momento; `ChapterCine::art_b`) (`cine.main(SPRITES_DIR, TMP_DIR, only=[n])`,
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
- **El signo de las piernas estaba al reves en las cuatro poses de arrodillado/sentado**
  (`POSE_KNEEL`, `POSE_DRINK`, `POSE_ENTHRONED` y `POSE_RECOIL`), y eso es lo que el usuario
  vio como «Elias con el cuerpo al reves» (dos quejas, 2026-09-09). Una rotacion X positiva
  es la MISMA rotacion para todos los huesos: la punta de un hueso que apunta ARRIBA (spine,
  chest, head) va hacia delante y la de uno que apunta ABAJO (muslos, brazos) va hacia
  ATRAS. `POSE_POINT` ya lo decia de los brazos; las piernas se escribieron con el signo de
  la columna y el muslo «alzado» salia horizontal hacia atras a la altura de la cadera. Con
  la falda rigida solo asomaba una pierna suelta; con el drapeado, la rodilla entera detras
  de la espalda — un fallo de pose que el drapeado no creo pero si hizo evidente. Regla:
  muslo adelantado = X NEGATIVO; espinilla que baja desde una rodilla alzada = X positivo
  (deshace el del muslo); espinilla que va por el suelo hacia atras = X positivo grande.
  `_drape_legs` elige el muslo del regazo por el valor ABSOLUTO del giro.
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

## Iconos y Carro de Fuego: `icons.py` y `chariot.py` (2026-09-24)

- **`icons.py`** -> `assets/sprites/icons.png`: 32 iconos (22 objetos por `ItemDef::id` + 10
  milagros en 22 + `MiracleID`), 8x4 celdas de 128 px. Workbench con el estilo de los props
  (cavity, contorno de 3 px: a 9 unidades uno de 2 se pierde) y el ENCUADRE MEDIDO por la caja
  de cada objeto (camara a 35 grados, `MARGIN` 1,12), como portraits.py. La tabla `ICONS` fija
  el orden de las celdas y tiene su espejo en `render/sprites.h` (`namespace Icon`). Lo que
  costo pasadas, mirando la hoja de contacto: la honda con dos tramos por cuerda salia un
  hexagono (una cuerda recta por lado: un triangulo que se lee como honda); las sandalias
  hay que GIRARLAS +50 en X o la suela sale de canto; el manto no se lee ni como capa (una
  pantalla de lampara) ni como pliegues apilados (una tarta): es una tela ondulada
  (`heightfield`) con el orillo de oro; y el Fuego del Cielo es la NUBE con su columna, no un
  cometa (primero salio una oruga de bolas y luego una zanahoria).
- **`chariot.py`** -> `assets/sprites/chariot.png`: el carro y los caballos de fuego de
  `cine.carro_fuego` / `cine.caballo_fuego`, rehechos por piezas para GALOPAR (cuatro
  fotogramas: patas en fase, radios girando, llamas latiendo) y para separarse en DOS CAPAS:
  `back` (todo, detras del profeta) y `front` (las piezas del carro mas cerca de la camara que
  el ancla y por encima del suelo de la caja: la pared cercana, el peto, la rueda de este
  lado). Filas este / sur / norte (el oeste es el espejo), 256 px, a la escala de las hojas;
  el ANCLA (suelo de la caja) cae en `ANCHOR`, copiada en `Engine::collect_chariot_draws`.
  La lanza de la estampa (2,4 m) dejaba el tiro suelto: en el sprite el yugo va a 1,7 m.
  Halo calido en el post (`_glow`, como vfx.py).
- **Blender sin ventana y sin GPU** (lo que se uso en un contenedor Linux): el modulo `bpy`
  de PyPI (4.2 LTS; 4.1 no esta publicado) en un venv con numpy y pillow, y Mesa para EGL
  (`libegl1 libegl-mesa0 libgl1-mesa-dri`) con `EGL_PLATFORM=surfaceless`. Workbench renderiza
  por software (llvmpipe) sin cambiar el aspecto; el aviso `EGL_BAD_MATCH` sale y no importa.
  Runner: `sys.path.insert(0, 'tools/blender')` e `icons.main(out, tmp)` / `chariot.main(out, tmp)`.
- **Iconos con luz de EEVEE (2026-10-03, `icons.EEVEE = True`)**: con Workbench cada icono era
  plastico mate con la luz plana de los props, y un icono se mira de cerca. Ahora `_eevee_look`
  monta tres soles —principal calido arriba a la izquierda, CONTRALUZ frio y fuerte que recorta
  el borde contra el fondo oscuro de la celda, relleno del cielo— y pasa los materiales planos
  de `parts.get_mat` a Principled con laca (coat); los de `METAL_KEYS` (oro, bronce, lanzas)
  van MEDIO metalicos y los de `GLOW_KEYS` (fuego, estrellas, luz) emiten. Modelos, encuadre y
  contorno no cambian; `gain` 1,08 en el post (el 1,7 por defecto compensa a Workbench).
  Dos trampas que costaron pasadas: **la rotacion del contraluz** (la camara mira a +Y; una X
  POSITIVA del sol manda la luz hacia +Y, o sea DE CARA; el contraluz va con X negativa y sin
  girar 180 en Z, que lo devolvia de cara) y **el metal pleno sale oscuro** sin entorno que
  reflejar (el oro del Arca se apagaba): 0,5 conserva el color y gana el brillo. EEVEE corre
  sin GPU en el mismo venv (llvmpipe), ~40 s por icono: las 32 son unos 20 minutos.
- **Los 19 iconos de El Santuario (2026-10-03, `SANCTUARY`, celdas 32..50)**. La hoja pasa a
  8x7 y `main(only=...)` pega la hoja anterior aunque tenga MENOS filas (`_load` compara el
  ancho), asi que ampliarla no obliga a renderizar las 32 de antes. Piezas nuevas que sirven
  para cualquier icono: `_flame` (una esfera estirada en GOTA que se cimbrea: con cajas
  afiladas las llamas eran piramides y la hoguera una corona), `_heart` (esfera deformada
  por la curva del corazon: dos esferas y un cono dejaban un escalon), `_torus`, `_prism` /
  `_flat_poly` (con bisel), `_chain` (tubos con esfera en cada codo), `_sparkle` (destello de
  cuatro puntas DE CARA a la camara, con los ejes de pantalla `_SR`/`_SU`/`_SF`) y
  `_grid_surface`. **`POST_GLOW` / `FRAME_MARGIN`**: resplandor detras del icono en el post
  (alfa desenfocado y tenido, por fuera del contorno) con margen propio para que no se
  corte; un toro translucido de "aura" salia OPACO y gris (EEVEE no le daba alfa a la
  pelicula). Lo que costo pasadas, mirando la hoja de contacto a tamano real y a 40 px (el
  de la lista): la pluma del cuervo suelta era un cuchillo (va el cuervo posado sobre el
  corazon); una caja redondeada no es una sandalia (suela con silueta de pie, `_sole_poly`,
  y a 1,45 de escala o se pierde entre las estelas); un halo con rayos alrededor es un sol;
  tres chorros resbalando por la tinaja eran tentaculos y gotas saltando, burbujas (va
  llena al ras con UN chorro); un altar dorado con varas es el Arca (madera oscura, bronce,
  cuernos grandes y la llama); y el manto: tela plana con destellos = alfombra magica, plana
  con luz = felpudo, capa vista desde arriba = cubo de papel. Lo que se lee como MANTO es la
  silueta de capa DE ESPALDAS (`_cape`: cuello, hombros que caen, costados rectos, bajo
  ondulado, capucha en gota) girada -30 en X para que la espalda mire a la camara.

## Jefes con hoja propia y retratos al doble (2026-10-03)

- **`bosses.py`**: nueve recetas (`SPECS` + una funcion `_extras_*` por jefe) sobre
  `build_humanoid`; `characters.build` las despacha antes de mirar `characters.SPECS`. Lo que
  hace JEFE a un jefe es la silueta: escudo torre, tiara con cuernos, cuernos enormes y dos
  cuchillos, espectro sin piernas (`_float_robe` quita muslos y pies), columna de agua con
  corona de espuma, prestamista ensanchado (`_widen`) con turbante y cadena, la Muerte con
  calavera, alas y guadana, Guiezi con lepra y dos talentos, Ben-adad con tiara asiria. Los
  cuernos que pasaban de 0,35 de largo salian del cuadro de 128 px en el ataque.
- **`portraits.py` a 192 px** (antes 96: el motor los pinta a 32 unidades = 128 px a 720p) y
  con los nueve jefes al final de `LAYOUT` (35..43, `Portrait::BOSS_*`). Los dialogos de jefe
  de `scenes.h` ya hablan con su cara y no con la de un enemigo raso.

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

## Personajes maduros (2026-11-10)

Peticion del usuario: *"posiblemente tengas que mejorar los modelos 3D de los personajes, que
no se vean tan infantiles, sino mas maduros"*. Era la figurita de Clash Royale: cabeza x1,45
(4,5 cabezas de alto), ojos de boton blancos, nariz de bola, manos de bola, cajas con su
"panel" en el pecho y una paleta saturada x1,35. Lo que la hizo adulta, por orden de peso a
128 px (todo en `characters.py`, y por tanto en hojas, jefes, retratos, PNJ, estampas y paneles):

- **La cabeza a escala casi real**: `HEAD_SCALE` 1,06 (~6 cabezas de alto). El craneo es un
  ovalo (`_head` escala 0.90/0.98/1.10) con **mandibula y menton** y **orejas**; ocupa el mismo
  sitio que la esfera de antes, asi que casco, corona, pano y capucha encajan sin tocarlos.
- **La cara se lee por sus sombras**: arco superciliar (`_brow_ridge`) que hunde los ojos,
  ojo pequeno y almendrado (`EYE_R` 0,021, era 0,039) con **parpado** que lo entorna, cejas
  finas, pomulos discretos, nariz en CUNA que nace de la frente. Las mujeres (`slim`) llevan
  arco y nariz mas finos y sin menton marcado. Sin el arco, los ojos flotaban en una bola.
- **Barba unida al pelo** por patillas; Elias con melena y mechones (`brow='wild'` + pelo
  largo: "varon velludo", 2 Reyes 1:8) y raya en medio (`hair_fringe` + `hair_fringe2`);
  Eliseo calvo pero con **corona de pelo** (`fringe`): pelado del todo era un bebe.
- **Tronco en V subdividido** (`_soft`: Subsurf 2 con TODAS las caras suaves — con las planas
  en flat la subdivision salia a rayas), brazo con biceps que se afina, mano cerrada pequena.
- **Tunica aplastada en Y** (`_squash_y` 0,80) con pliegues tambien en las hojas (7 lobulos,
  0,05) y orla oscura en el bajo; el cinturon cenido (el aro redondo era un flotador).
- **Capa de verdad** (`_cloak`): un cono abierto de 230 grados que abraza los hombros y cae
  hasta las corvas con pliegues. La losa de antes era una tabla (o una mochila) y sus
  "hombreras", de rugby.
- **Color de tierra**: `SAT` 1,0, `SKIN_SAT` 0,96, `VAL` 0,97, y Elias/Eliseo un punto mas
  profundos en su SPEC. El naranja contra azul que los distingue se conserva.
- Jefes: `bosses._glow_eyes` y la calavera quitan la cara con `C.FACE_KEYS`, y los que se
  ponen turbante o tiara quitan tambien `hair_fringe2`.
- **Retratos por BUSTO** (`portraits.BUST_Z` 1,52 / `BUST_ORTHO` 0,78) para los humanoides:
  con la cabeza adulta, encuadrar por la caja dejaba la cara en un tercio del cuadro (la
  estiraban el cayado, la corona o el penacho). Leon, agua y vinetas siguen por la caja.
- **Banco de pruebas** que vale para cualquier retoque: render de un busto a 512 px mas cuatro
  frames de la hoja a tamano real, para varios personajes en una imagen. Se mira ANTES de
  lanzar las 21 hojas (2,5 min por tanda de siete en un contenedor de 4 nucleos).

Re-renderizado entero el mismo dia en el contenedor Linux (`bpy` 4.2): 12 hojas + 9 jefes,
`portraits.png`, `npcs.png`, las 46 estampas (`tools/estampas`) y los 23 paneles de `cine.py`.
Los recortes de fotograma que avisa `render_sheet` (muerte del Capitan y del Espiritu, ataque
de la Muerte) ya estaban en las hojas de antes.

**`look.py` y Blender 4.2**: el EEVEE de 4.2 se llama `BLENDER_EEVEE_NEXT` y ya no tiene sombras
suaves, tamano de mapa de sombras ni bloom en la escena. `setup_eevee` prueba el nombre viejo y
cae al nuevo, y `_set` salta lo que no exista; en el Blender 4.1 del escritorio no cambia nada.
Comparado panel a panel: misma luz, misma niebla, sombras proyectadas algo mas suaves. ~3,5 min
por panel por CPU (llvmpipe).
