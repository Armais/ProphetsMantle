# Créditos y licencias de los assets de MANTLE

Registro de procedencia de **todo** lo que hay en `assets/`. La regla del proyecto es que
un asset sin procedencia anotada aquí es un asset que hay que sacar, así que este fichero
se actualiza **al meter** el asset, no después.

Estado a 2026-08-26: **no hay ningún asset de terceros con obligaciones, salvo la
tipografía (OFL).** La música hornea samples de percusión que Sonic Pi trae de serie, en
**CC0 1.0 (dominio público, sin atribución obligatoria)**; todo lo demás se genera con las
herramientas del propio repositorio. El juego se puede distribuir comercialmente sin más
trámite que respetar la OFL de la fuente.

## Audio — 100 % generado por el proyecto

| Qué | Dónde | Cómo se genera |
|---|---|---|
| 27 pistas de música | `assets/audio/music/*.ogg` | `tools/audio/sonicpi/*.rb` + `record.py` (Sonic Pi) |
| 37 efectos de sonido | `assets/audio/sfx/*.ogg` | `tools/audio/gen_audio.py` |
| 15 exclamaciones de voz | `assets/audio/sfx/voz_*.ogg` | `tools/audio/gen_audio.py` (132 KB) |

**Música (2026-09-13): compuesta por el proyecto en Sonic Pi.** Las 27 partituras viven en
`tools/audio/sonicpi/*.rb` y son composición original: **siete globales y dos por capítulo**
—el tema de juego y su variación desnuda para la cinemática y el diálogo de apertura—, cada
capítulo con su propio modo, su solista y su percusión. `record.py` arranca el daemon de Sonic
Pi sin GUI, graba la salida de
SuperCollider bit a bit, corta el periodo exacto del bucle y encodea el `.ogg`. Sonic Pi y
SuperCollider son **herramientas de generación** (como Blender o ffmpeg): no se distribuye ni
se enlaza nada suyo con el juego. La percusión usa samples que Sonic Pi trae de serie
(tablas, bombos, platos, cabasa, y también `ambi_choir` para las voces, `misc_crow` para los
cuervos de Querit y `drum_cowbell` para las yuntas del cap. 5), publicados bajo **CC0 1.0
(dominio público)**, sin obligación de atribución; el resto es síntesis de los sintetizadores
de Sonic Pi. Las versiones anteriores
(síntesis numpy) quedan en `tools/audio/music_backup/` y `gen_audio.py` sigue pudiendo
regenerarlas.

Los SFX y las voces siguen siendo síntesis propia con numpy: cuerdas Karplus-Strong, flauta
de caña (*ney*), percusión sintetizada, sala tipo Schroeder y compresor de bus. **En SFX y
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

# Música (Sonic Pi): arrancar el daemon y grabar cada pista
python tools/audio/sonicpi/record.py boot            # proceso de larga vida
python tools/audio/sonicpi/record.py capture <pista> <periodo_s>   # y mandar el .rb por el MCP
python tools/audio/sonicpi/record.py batch <pista>:<periodo> ...  # en cadena, sin MCP
python tools/audio/sonicpi/record.py reprocess <pista> <periodo>  # rehacer la mezcla sin regrabar
python tools/audio/sonicpi/analyze.py assets/audio/music          # gates de calidad
python tools/audio/sonicpi/check_names.py                         # nombres de sample/synth
```

La transcodificación a Vorbis usa **ffmpeg** como herramienta externa de compilación
(no se distribuye con el juego, no se enlaza nada suyo). Sin ffmpeg el generador escribe
`.wav` y avisa.

**Titularidad**: obra del proyecto. Uso comercial sin restricciones.

## Sprites — 100 % render propio

| Qué | Dónde | Cómo se genera |
|---|---|---|
| 12 hojas de personaje | `assets/sprites/*.png` | `tools/blender/mantle_sprites/` (Blender 4.1) |
| Tileset de terreno | `assets/sprites/tiles.png` | `tools/blender/mantle_sprites/tiles.py` |
| 8 estampas del prólogo + 10 intros de capítulo + 4 estampas del Ascenso (x3 capas) | `assets/sprites/cine/` | `tools/blender/mantle_sprites/cine.py` |
| 22 retratos y viñetas | `assets/sprites/portraits.png` | `tools/blender/mantle_sprites/portraits.py` |
| Hoja de efectos | `assets/sprites/vfx.png` | `tools/blender/mantle_sprites/vfx.py` |
| 32 iconos de objeto y milagro (8x4, celda 128 px) | `assets/sprites/icons.png` | `tools/blender/mantle_sprites/icons.py` (Blender 4.2, 2026-09-24) |
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
