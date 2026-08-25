# Créditos y licencias de los assets de MANTLE

Registro de procedencia de **todo** lo que hay en `assets/`. La regla del proyecto es que
un asset sin procedencia anotada aquí es un asset que hay que sacar, así que este fichero
se actualiza **al meter** el asset, no después.

Estado a 2026-08-25: **no hay ni un asset de terceros salvo la tipografía.** Todo lo
demás se genera con las herramientas del propio repositorio, así que el juego se puede
distribuir comercialmente sin más trámite que respetar la OFL de la fuente.

## Audio — 100 % generado por el proyecto

| Qué | Dónde | Cómo se genera |
|---|---|---|
| 10 pistas de música | `assets/audio/music/*.ogg` | `tools/audio/gen_audio.py` |
| 37 efectos de sonido | `assets/audio/sfx/*.ogg` | `tools/audio/gen_audio.py` |
| 15 exclamaciones de voz | `assets/audio/sfx/voz_*.ogg` | `tools/audio/gen_audio.py` (132 KB) |

Síntesis propia con numpy: cuerdas Karplus-Strong, flauta de caña (*ney*), percusión
sintetizada, sala tipo Schroeder y compresor de bus. **No hay muestras de terceros, ni
samples de bibliotecas, ni material con licencia ajena.**

**Las voces también son síntesis, no grabaciones ni TTS.** Modelo fuente-filtro: pulso
glotal (diente de sierra suavizado, con *jitter* de tono y *shimmer* de amplitud) pasado
por tres formantes resonantes. Ni una voz humana grabada, ni un servicio de terceros: eso
habría metido una dependencia externa y una licencia ajena, y este repo no las tiene.

Regenerar con:

```
python tools/audio/gen_audio.py            # todo, salida .ogg
python tools/audio/gen_audio.py all wav    # .wav sin comprimir (depuración)
python tools/audio/gen_audio.py voz_       # solo un grupo, por prefijo de nombre
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
| 8 estampas del prólogo (x3 capas) | `assets/sprites/cine/` | `tools/blender/mantle_sprites/cine.py` |
| 22 retratos y viñetas | `assets/sprites/portraits.png` | `tools/blender/mantle_sprites/portraits.py` |
| Hoja de efectos | `assets/sprites/vfx.png` | `tools/blender/mantle_sprites/vfx.py` |

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
