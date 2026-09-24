# `tools/` - los tres pipelines que generan `assets/`

Ni un asset se edita a mano: todo sale de aqui y se copia a `assets/`.

| Pipeline | Que genera | Reglas |
|---|---|---|
| `tools/blender/mantle_sprites/` | hojas de personaje, tileset, efectos, retratos, PNJ y los 22 paneles cinematicos | `tools/blender/CLAUDE.md` |
| `tools/audio/sonicpi/` | las 47 pistas de musica (Sonic Pi headless + `record.py`) | `tools/audio/CLAUDE.md` |
| `tools/audio/gen_audio.py` | los 52 `.ogg` de SFX y voz (sintesis numpy) | `tools/audio/CLAUDE.md` |
| `tools/branding/` | `mantle.ico` y `mark_256.png` | `tools/branding/CLAUDE.md` |

Y uno que no genera assets: **`tools/release/`** sube el APK a GitHub como release
(`make_release.ps1`, sin `gh` CLI). Reglas en `tools/release/CLAUDE.md`.
