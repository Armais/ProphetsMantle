# `tools/` - los tres pipelines que generan `assets/`

Ni un asset se edita a mano: todo sale de aqui y se copia a `assets/`.

| Pipeline | Que genera | Reglas |
|---|---|---|
| `tools/blender/mantle_sprites/` | hojas de personaje, tileset, efectos, retratos, PNJ y los 22 paneles cinematicos | `tools/blender/CLAUDE.md` |
| `tools/audio/sonicpi/` | las 47 pistas de musica (Sonic Pi headless + `record.py`) | `tools/audio/CLAUDE.md` |
| `tools/audio/gen_audio.py` | los 52 `.ogg` de SFX y voz (sintesis numpy) | `tools/audio/CLAUDE.md` |
| `tools/branding/` | `mantle.ico` y `mark_256.png` | `tools/branding/CLAUDE.md` |
| `tools/estampas/` | las 46 estampas de los relatos (`assets/sprites/estampas/ev_NN.png`) | seccion de abajo |

Y uno que no genera assets: **`tools/release/`** sube el APK a GitHub como release
(`make_release.ps1`, sin `gh` CLI). Reglas en `tools/release/CLAUDE.md`.

## Estampas de los relatos: `tools/estampas/` (2026-10-03)

Peticion del usuario: «los eventos secundarios deben de tener un grafico representativo de
cada uno». Una lamina por id de `Ev::` (39 relatos del pozo + 4 paradas del Ascenso + 3 del
epilogo), 768x384, PNG de paleta con tramado (el grano ya va en la lamina). La pinta el motor
en `draw_dialogue` enmarcada arriba, con el titulo en una cinta (ver `src/ui/CLAUDE.md`).

Dos pasos, y el primero necesita `bpy` (el mismo venv que `tools/blender`):

```bash
EGL_PLATFORM=surfaceless <venv-bpy>/bin/python tools/estampas/figuras.py   # -> figs/ (gitignored)
python3 tools/estampas/render.py [FILTRO ...] [--contact]                  # -> assets/sprites/estampas
```

- **Fondos en 2D, figuras en 3D.** `core.py` pinta con campos de distancia (SDF) en numpy:
  degradados, sombreado de cojin, luz de borde hacia `cv.sun`, niebla (`cv.haze`), bloom,
  vineta y grano. `props.py` es el vocabulario (cielos, cordilleras, paredes de canon con
  cornisas `wall_fn`, rios con orillas Catmull-Rom, interiores, camas, tinajas, altar de doce
  piedras, cuadrupedos con anatomia, multitudes...). Las PERSONAS no se dibujan en SDF (un
  trapecio con una bola encima): `figuras.py` posa los munecos del juego (characters.py,
  bosses.py y variantes de reparto en `EXTRA`) y los renderiza en Workbench; `core.cutout`
  los pega por los PIES con la escala de su distancia (`human_h` = lo que mide 1,75 m ahi) y
  les pone la luz de la lamina (`backlit`, `rim`, `tint`, `haze`).
- **El brazo alzado es el IZQUIERDO**: el derecho lleva el cayado y alzarlo lo vuelca (regla de
  cine.py). Por eso las poses de brazos arriba se ruedan mirando a la izquierda y se voltean
  en la lamina (`flip=True`).
- Composicion, la misma en las 46: cielo y luz primero, de lejos a cerca con niebla, UN foco
  (la figura del pasaje a 40-44 de alto, pies en y~98) y el primer plano oscuro enmarcando.
- **Cada lamina se mira** en las hojas de contacto (`--contact`) antes de darla por buena.
  Lo que costo pasadas: las paredes del canon en zigzag eran carton (perfil con cornisas,
  `wall_fn`); los bueyes 3D de cine.py son cajas (van en 2D); las figuras SDF de multitud
  salian fantasmas (`crowd_cut` usa munecos); `hood=False` por defecto rompia el turbante.
