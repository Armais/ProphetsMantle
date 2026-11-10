# `tools/branding/` - icono y logo

### El icono DINAMICO (2026-11-10) — el vigente

Peticion del usuario: *"otro icono para el apk y el exe que sea mas dinamico, mas visual, usa
las mejores practicas en el diseno de iconos"*. El busto a dos (mas abajo) se leia a 32 px pero
era una foto de carnet: dos munecos de frente, quietos, sobre transparente. El nuevo aplica lo
que piden las guias (Material, Apple HIG, las tiendas) y hacen los iconos de juego que
funcionan:

- **Un solo foco**: Elias en busto, tres cuartos, **contrapicado** (camara baja: el heroe
  crece). Dos cabezas a 32 px compiten; una cara con gesto manda.
- **Diagonal**: la llama arriba a la derecha -> la cara -> el manto barriendo abajo a la
  izquierda. Lo simetrico se lee como retrato; la diagonal, como movimiento.
- **Fondo a sangre** con el fuego del cielo (1 R 18:38): degradado radial desde la llama
  (blanco-oro -> naranja -> carmesi -> casi negro), 14 rayos y ascuas. Nada de transparente.
- **Contraste figura/fondo**: la luz se queda CERCA de la llama y el resto cae pronto a
  carmesi oscuro (la tunica de Elias es naranja: con fondo naranja se fundia a 32 px), y un
  filo de luz **calido hacia la llama y frio en el lado contrario** (`_light_wrap`).
- **Encuadre por tamano** (`compose.CROPS` / `crop_for`): de 64 px arriba el busto con el
  manto; 40-48 cara y llama; 32 y menos, mas cerrado. **Contorno redibujado en pixeles de
  destino** (`Art._at`, 1 px hasta 64, 2 hasta 128). **Destello de fuego en los ojos** con
  radio minimo de medio pixel: es lo que hace que la cara tenga mirada a 32 px.
- Sin texto, sin marco de escena: losa redondeada (radio 20,5 %) con filo oscuro y un brillo
  fino arriba para Windows y la ventana; a sangre para Android (el launcher pone su mascara).

**El pipeline**, en dos pasos:

```bash
EGL_PLATFORM=surfaceless <venv-bpy>/bin/python -c "import sys; sys.path.insert(0,'tools/blender'); \
    from mantle_sprites import run, logo; logo.main_heroe(run.ROOT, '/tmp/mantle_hero')"
python tools/branding/make_icon.py preview           # -> assets/branding/{mantle.ico, mark_256.png}
python tools/branding/make_android_icons.py preview  # -> android/app/src/main/res/mipmap-*
```

1. `logo.main_heroe` (Blender, EEVEE, `parts.set_quality('cine')`): SOLO la figura sobre
   transparente -> `src/heroe_1024.png`, y `src/heroe.json` con donde caen la punta del cayado,
   la cara y los iris (0..1 del cuadro). La pose (`HERO_POSE`), el giro y la camara son
   constantes del modulo. Se renderiza mirando a la izquierda y `compose` la VOLTEA.
2. `compose.py` (numpy + PIL, sin Blender): fondo, llama, resplandor, ascuas, light wrap,
   losa y capas del adaptativo. `make_icon.py` y `make_android_icons.py` solo empaquetan.

Lo que costo pasadas, mirando cada prueba (no repetir):

- **La llama en 3D sale un huevo amarillo**: la emision satura y no hay degradado. Se pinta en
  2D (`_flame`: cinco gotas anidadas de rojo a blanco, cimbreadas por el viento, dos lenguas
  laterales, fundidas con un desenfoque — sin el, sale a bandas como un mapa de cotas — y un
  resplandor aditivo). La gota del sprite se borra: asomaba como una canica bajo la pintada.
- **El brazo del cayado se ABRE, no se adelanta**: con X negativo la vara cruza el pecho; y el
  brazo libre ABAJO, que adelantado ponia en primer plano un antebrazo y una mano enormes.
- En la capa delantera del adaptativo la llama perdia el nucleo blanco (alli no hay fondo al
  que sumar el resplandor): se suma tambien dentro de su silueta.
- PIL no desenfoca imagenes en coma flotante (`mode='F'`): `compose._blur` son tres pasadas de
  caja por eje en numpy.

**Gates**: la tira de `make_icon.py preview` (cada tamano a tamano real y x4, claro y oscuro,
32 px marcado) y la de `make_android_icons.py preview` (circulo, redondeado y squircle a 48 y
96 dp + el heredado). Verificado el 2026-11-10: a 32 px se reconoce el profeta barbudo con la
llama; a 16, la cara con el destello y la llama; en las tres mascaras caben cara y llama.
El fondo del adaptativo es procedural (`Art._bg_window`) y se evalua en los 108 dp enteros: el
render de la figura no llega a los bordes del lienzo. `ADAPT` coloca cara y llama en el circulo
seguro de 66 dp.

Las fuentes del icono anterior (`src/logo.png`, `src/mark_512.png`) se retiraron: nada las
consumia. `logo.main()` las sigue sabiendo generar (ya con los munecos adultos).

---

## Historico: el busto a dos (2026-08-26 .. 2026-11-10)

### Icono y logo (2026-08-26, rehecho el mismo dia)

**El primer icono lo rechazo el usuario** ("no se ven a Elias ni Eliseo ni ningun manto") y la
leccion vale mas que el arte: **un icono se juzga a 32 px, no en el render a 1024**. Aquel
tenia DOS motivos distintos —los profetas de cuerpo entero de 64 px para arriba, un pano con
llama de 48 para abajo— y a 32 px no se reconocia ninguno de los dos: una figura de pie mide
cuatro pixeles de ancho, y una tela colgada en el aire con fuego encima es un brasero.

Ahora es **UNA composicion y dos encuadres de ella**: Elias y Eliseo hombro con hombro, de los
hombros para arriba, con el manto cruzando en diagonal entre los dos (orillo de oro en el
bajo). `logo` = busto ancho (256/128/64); `mark` = el mismo busto recortado a las dos cabezas
(48/32/24/16). Lo que sobrevive al reducir es una CABEZA, y lo que distingue a los dos es
pelo y barba oscuros contra calva y naranja contra azul — no el detalle de la cara.

- **El gate es `python tools/branding/contact.py <png>...`**: una tira con el icono a
  16/24/32/48/64/128/256 px sobre fondo claro **y** oscuro, con la columna de 32 marcada. Se
  mira ANTES de dar nada por bueno, y por eso vive aparte de `make_icon.py` (que empaqueta lo
  ya decidido). Fue lo que descarto los otros tres bocetos, y **conviene no repetirlos**: el
  traspaso (Elias echando el manto sobre Eliseo) deja el pano colgando por delante y se lee
  como un escudo; el manto solo, con capucha y llama, sale campana o brasero; y una M
  tipografica aguanta 16 px pero renuncia a los dos profetas.
- Arte: `tools/blender/mantle_sprites/logo.py` (mismo pipeline que los personajes: Workbench,
  cavity, `post_soft`; el icono tiene que parecer el mismo muneco que se juega). El encuadre
  sale **cuadrado por el lado mayor**, asi que estrechar la caja del `mark` es justo lo que
  agranda las cabezas. Ojo con el cayado: son CUATRO objetos (`_staff` y, colgando de el,
  `_tip`/`_tip_core`/`_tip_halo`), y borrar solo el padre deja la punta encendida flotando
  sobre la cabeza del profeta.
- **Las fuentes NO van en `assets/`**: `logo.py` escribe `logo.png` y `mark_512.png` en
  `tools/branding/src/` porque no las carga el juego. En `assets/branding/` queda solo lo que
  se distribuye: `mantle.ico` (lo empotra `mantle.rc`) y `mark_256.png` (`SDL_SetWindowIcon`).
- Empaquetado: `python tools/branding/make_icon.py [preview]` escribe `mantle.ico` **a mano**
  (cabecera + una entrada de 16 bytes por tamano + el PNG de cada uno). `Image.save(sizes=)`
  no vale: mete la MISMA imagen reescalada en todos los tamanos y aqui hace falta un encuadre
  distinto por tamano. Ademas **redibuja el contorno en cada tamano**: el de Blender esta en
  pixeles del render (5 px en 512) y al bajar a 16 se evapora.
- `mantle.rc` en la raiz, anadido a `MANTLE_SOURCES` **solo bajo `if(WIN32)`** — eso da el
  icono del FICHERO. `SDL_SetWindowIcon` en `Engine::init` da el de la ventana y la barra de
  tareas, y cubre Linux y Mac. Para comprobar lo que vera el explorador, extraerlo del `.exe`
  ya compilado: `[System.Drawing.Icon]::ExtractAssociatedIcon($exe)` devuelve justo los 32 px.
- **El manto que cruza es una malla continua** (`logo._sheet`, bmesh), no piezas de `parts`:
  con una caja vertical salio una puerta de armario y con trece cajas en fila un xilofono —
  cada caja trae su bisel y su cavity y se leen como objetos sueltos. Lo que la hace parecer
  tela son los pliegues: ondular la malla en PROFUNDIDAD y dejar que el sombreado dibuje las
  luces. Sus parametros `tilt` (cruza en diagonal) y `flare` (estrecho arriba, ancho abajo)
  son lo que separa una prenda de un trapo tendido. Y **los dos extremos tienen que caber en
  el cuadro**: un pano que se sale por los dos lados pierde el largo y se lee como un tablon.
- Sin llama, a proposito: la que habia se apilaba encima del pano y era justo lo que lo
  convertia en brasero. Si algun dia vuelve el fuego, necesita un perfil curvo (un solido de
  revolucion): apilando esferas sale un huevo y con cajas afiladas, un avion de papel.

### Icono de Android: adaptativo (2026-09-11)

El usuario probo el APK y dijo **"el icono se ve pequeno"**. Eran dos cosas sumandose, y la
primera no la vio nadie hasta medir el alfa:

1. **`mark_512.png` trae 122 px de alfa vacio ARRIBA** (bbox `(0,122,512,512)`). `_fit` encuadra
   por el lienzo, no por el contenido, asi que el dibujo salia pegado al borde de abajo con una
   banda muerta del 24% encima. `make_android_icons.py` ahora hace `_crop` al alfa **antes** de
   nada. Quien toque `make_icon.py` o `contact.py` tiene el mismo sesgo esperandole.
2. **Un `mipmap` heredado en Android 8+ lo encoge el launcher**: lo mete dentro de su mascara y
   le pone fondo. Por eso hay icono **adaptativo**: `mipmap-anydpi-v26/ic_launcher.xml` con
   `background` + `foreground`, lienzo de **108 dp** y zona segura de **72 dp** (`CANVAS_DP` /
   `SAFE_DP`). El arte se escala a llenar esos 72 dp: lo que llena la zona segura llena el icono.
   Los `mipmap-*dpi/ic_launcher.png` heredados **se quedan** — son los de Android 7 y anteriores
   (minSdk 21) — pero ahora salen tambien del recorte al alfa.

- Fondo: degradado radial del pardo de `Color::baal_dark` (`BG_IN`/`BG_OUT`). Plano se veia
  muerto a 48 dp. No compite con el naranja de Elias ni con el azul de Eliseo.
- **El gate es `make_android_icons.py preview`**, no `contact.py`: en Android lo que se ve NO es
  el PNG, es el PNG recortado por la mascara del launcher de turno. La tira saca las **tres**
  (circulo, redondeado, squircle) a 48 y 96 dp, sobre claro y oscuro, mas el heredado al lado
  para comparar. Verificado el 2026-09-11: con las tres se reconocen las dos cabezas y el manto;
  el circulo solo recorta las puntas del pano y los hombros.
- Lo unico que el ancho del arte (512x390 recortado) sacrifica con mascara circular son esas
  puntas. Si alguna vez hay que salvarlas, se baja `SAFE_DP` — no se reencuadra el render.
