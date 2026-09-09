# `tools/branding/` - icono y logo

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
