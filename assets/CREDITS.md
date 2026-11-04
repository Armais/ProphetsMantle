# Créditos y licencias de los assets de MANTLE

Registro de procedencia de **todo** lo que hay en `assets/`. La regla del proyecto es que
un asset sin procedencia anotada aquí es un asset que hay que sacar, así que este fichero
se actualiza **al meter** el asset, no después.

Estado a 2026-10-03: **no hay ningún asset de terceros, salvo la tipografía (OFL).** Todo
el audio —música, efectos y voz— es síntesis propia, sin muestras de terceros; todo lo demás
se genera con las herramientas del propio repositorio. El juego se puede distribuir
comercialmente sin más trámite que respetar la OFL de la fuente. El texto bíblico que va
dentro del juego es de dominio público (ver «Texto bíblico», más abajo).

## Audio — 100 % generado por el proyecto

| Qué | Dónde | Cómo se genera |
|---|---|---|
| 47 pistas de música | `assets/audio/music/*.ogg` | `tools/audio/orquesta/` (`render.py`, síntesis numpy/scipy) |
| 37 efectos de sonido | `assets/audio/sfx/*.ogg` | `tools/audio/gen_audio.py` |
| 15 exclamaciones de voz | `assets/audio/sfx/voz_*.ogg` | `tools/audio/gen_audio.py` (132 KB) |

**Música (2026-10-03): compuesta y sintetizada por el proyecto, sin una sola muestra.** Las
47 partituras son composición original y viven como código en `tools/audio/orquesta/`
(`tracks_global.py`, `tracks_act1.py`, `tracks_act2.py`): **siete globales y cuatro por
capítulo** —el tema de juego, el del diálogo y la carta de apertura, y una por cada una de
las dos estampas de la cinemática—, cada capítulo con su propio modo y su solista, y cada
tramo de la apertura con instrumentos que no se repiten entre ellos. La orquesta entera es
**síntesis propia en numpy/scipy**: cuerdas por secciones con cuerpos de resonancia
sintéticos, coro de pulso glotal y formantes, duduk, ney, kaval y shofar con ruido de soplo,
oud, kanun, arpa y lira por Karplus-Strong, percusión modal (tar, daf, darbuka, riq, taikos,
timbales, zils), campanas, tanpura y ambientes de ruido filtrado, todo en salas cuya
respuesta al impulso también se SINTETIZA (ruido decorrelado con caída por bandas). **No hay
samples, ni soundfonts, ni respuestas al impulso grabadas, ni material con licencia ajena.**
La transcodificación la hace ffmpeg (herramienta de compilación, ver abajo).

Historia: entre el 2026-09-13 y el 2026-10-02 las pistas salían de **Sonic Pi**
(`tools/audio/sonicpi/`, conservado como histórico), que horneaba en ellas samples que Sonic
Pi trae de serie (CC0 1.0: tablas, platos, cabasa, `ambi_choir`, `misc_crow`,
`drum_cowbell`). **Ninguna pista actual contiene ya esas muestras.** Las versiones
anteriores a Sonic Pi (síntesis numpy) quedan en `tools/audio/music_backup/`.

Los SFX y las voces son síntesis propia con numpy: cuerdas Karplus-Strong, flauta de caña
(*ney*), percusión sintetizada, sala tipo Schroeder y compresor de bus. **En música, SFX y
voz no hay muestras de terceros, ni samples de bibliotecas, ni material con licencia ajena.**

**Las voces también son síntesis, no grabaciones ni TTS.** Modelo fuente-filtro: pulso
glotal (diente de sierra suavizado, con *jitter* de tono y *shimmer* de amplitud) pasado
por tres formantes resonantes. Ni una voz humana grabada, ni un servicio de terceros: eso
habría metido una dependencia externa y una licencia ajena, y este repo no las tiene.

Regenerar con:

```
python tools/audio/gen_audio.py            # SFX y voz, salida .ogg
python tools/audio/gen_audio.py all wav    # .wav sin comprimir (depuración)
python tools/audio/gen_audio.py voz_       # solo un grupo, por prefijo de nombre

# Música: la orquesta sintética (las 47 pistas, ~9 min en 4 núcleos)
python tools/audio/orquesta/render.py                # todas -> assets/audio/music/*.ogg
python tools/audio/orquesta/render.py cap03 boss     # solo las que empiezan por esos prefijos
python tools/audio/sonicpi/analyze.py assets/audio/music          # gates de calidad
```

La transcodificación a Vorbis usa **ffmpeg** como herramienta externa de compilación
(no se distribuye con el juego, no se enlaza nada suyo). Sin ffmpeg el generador escribe
`.wav` y avisa.

**Titularidad**: obra del proyecto. Uso comercial sin restricciones.

## Sprites — 100 % render propio

| Qué | Dónde | Cómo se genera |
|---|---|---|
| 12 hojas de personaje | `assets/sprites/*.png` | `tools/blender/mantle_sprites/` (Blender 4.1) |
| 46 estampas de relato (768x384, paleta 256) | `assets/sprites/estampas/ev_NN.png` | `tools/estampas/` (numpy + figuras de Blender 4.2, 2026-10-03) |
| 9 hojas de jefe (`boss_*.png`, mismo formato) | `assets/sprites/boss_*.png` | `tools/blender/mantle_sprites/bosses.py` (Blender 4.2, 2026-10-03) |
| Tileset de terreno | `assets/sprites/tiles.png` | `tools/blender/mantle_sprites/tiles.py` |
| 8 estampas del prólogo + 10 intros de capítulo + 4 estampas del Ascenso (x3 capas) | `assets/sprites/cine/` | `tools/blender/mantle_sprites/cine.py` |
| 22 retratos y viñetas | `assets/sprites/portraits.png` | `tools/blender/mantle_sprites/portraits.py` |
| Hoja de efectos | `assets/sprites/vfx.png` | `tools/blender/mantle_sprites/vfx.py` |
| 51 iconos: objetos, milagros y las 19 mejoras de El Santuario (8x7, celda 128 px) | `assets/sprites/icons.png` | `tools/blender/mantle_sprites/icons.py` (Blender 4.2, 2026-09-24; Santuario 2026-10-03) |
| Carro de Fuego del milagro (3 direcciones x 4 fotogramas x 2 capas, celda 256 px) | `assets/sprites/chariot.png` | `tools/blender/mantle_sprites/chariot.py` (Blender 4.2, 2026-09-24) |
| Fondo del menú principal | `assets/sprites/menu_bg.png` | `tools/blender/mantle_sprites/menu.py` |
| Logo, marca e icono | `assets/branding/` | `tools/blender/mantle_sprites/logo.py` + `tools/branding/make_icon.py` |

Modelado y animación procedurales en Python sobre Blender; no se importa ningún modelo,
textura ni HDRI de terceros. Los `.blend` fuente viven en `tools/blender/blend/` y no son
assets de ejecución.

**Titularidad**: obra del proyecto. Uso comercial sin restricciones.

## Tipografía — el único asset de terceros

| Fuente | Fichero | Licencia | Autoría |
|---|---|---|---|
| Lilita One | `assets/fonts/LilitaOne-Regular.ttf` | **SIL Open Font License 1.1 (OFL)** | Juan Pablo del Peral (Huerta Tipográfica) |

La OFL permite uso comercial e incrustación en el juego. Obliga a: (a) no vender la
fuente por separado, (b) mantener el aviso de licencia, y (c) no usar los Nombres
Reservados de la fuente en una versión modificada. MANTLE la usa sin modificar, solo la
hornea en atlas de glifos en tiempo de arranque (`Renderer::bake_atlas`).

## Texto bíblico — de terceros, en el código (no en `assets/`)

Las citas viven en `src/game/lore.h`, `src/game/scenes.h`, `src/game/events.h` y
`src/systems/progression.cpp`, no en `assets/`, pero se distribuyen con el juego.

| Idioma | Traducción | Derechos |
|---|---|---|
| Español | Reina-Valera 1909, con la ortografía al día («a» por «á», «fue» por «fué», Acab por Achâb, Nabot por Naboth) | Dominio público. Texto de referencia: eBible.org, `spaRV1909` |
| Inglés | King James Version (1611) | Dominio público, salvo en el Reino Unido: allí los derechos son de la Corona y los administra Cambridge University Press |

**Una cita nueva se copia de la 1909, nunca de la 1960.** La Reina-Valera 1960 tiene
derechos (© Sociedades Bíblicas en América Latina, 1960; renovado © Sociedades Bíblicas
Unidas, 1988) y el juego la citaba hasta el 2026-11-04. Ese día se cotejaron todas las citas
con la 1909 y se cambiaron. Al poner la ortografía al día solo se tocan acentos y la
grafía de los nombres; el orden y las palabras son los de 1909 («Ruégote», «Hete
encontrado», «sajábanse»). Si la cita es larga se recorta, pero no se reescribe.

El registro arcaizante del texto inventado no plantea ningún problema: imitar un estilo no
copia ningún texto.

## Bibliotecas enlazadas (no son assets, pero cuentan para la distribución)

| Biblioteca | Licencia |
|---|---|
| SDL3, SDL3_image, SDL3_mixer, SDL3_ttf | zlib |
| FreeType (vendorizada por SDL3_ttf) | FreeType License / GPLv2 a elección — se usa la **FreeType License** |
| stb_vorbis (dentro de SDL3_mixer) | dominio público / MIT |

## Al añadir un asset nuevo

1. Solo entra con licencia apta para uso comercial: **CC0, CC-BY con atribución, OFL o
   compra**. Nada de "de momento" con licencia sin verificar.
2. Se anota **aquí mismo, en el mismo commit**: qué es, de dónde sale, autoría y licencia.
3. Si es CC-BY, la atribución tiene que ser exactamente la que pida el autor.
