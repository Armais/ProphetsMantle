# `tools/` - los tres pipelines que generan `assets/`

Ni un asset se edita a mano: todo sale de aqui y se copia a `assets/`.

| Pipeline | Que genera | Reglas |
|---|---|---|
| `tools/blender/mantle_sprites/` | hojas de personaje, tileset, efectos, retratos, PNJ y los 22 paneles cinematicos | `tools/blender/CLAUDE.md` |
| `tools/audio/gen_audio.py` | los 47 `.ogg` de musica, SFX y voz (sintesis numpy) | `tools/audio/CLAUDE.md` |
| `tools/branding/` | `mantle.ico` y `mark_256.png` | `tools/branding/CLAUDE.md` |
