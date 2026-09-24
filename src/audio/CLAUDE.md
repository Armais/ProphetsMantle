# `src/audio/` - reproduccion (SDL3_mixer)

Reglas transversales a todo `src/`: `../CLAUDE.md`.
**La sintesis de toda la musica, los SFX y la voz esta en `tools/audio/CLAUDE.md`**; aqui
solo vive lo que hace el motor con los ficheros ya generados.

La API es la de SDL3_mixer 3.1.2 (`MIX_*` en mayusculas, NO el `Mix_*` de SDL2): ver
"Important: SDL3 API differences from SDL2" en el `CLAUDE.md` de la raiz. `Audio::load_*`
cae `.ogg` -> `.wav`.

## Musica: cuatro pistas por capitulo

`MusicID` tiene **siete globales y cuatro bloques CONTIGUOS de diez**
(`ChapterTheme1..10`, `ChapterIntro1..10`, `ChapterCineA1..10`, `ChapterCineB1..10`).
Los bloques se indexan con aritmetica desde el numero de capitulo —`chapter_theme(c)`,
`chapter_intro(c)`, `chapter_cine_a(c)`, `chapter_cine_b(c)`, en `audio.h`—, igual que
las variantes consecutivas de SFX: **no los reordenes ni metas un ID en medio**.

La apertura de un capitulo se oye en cuatro tramos seguidos y **cada uno estrena pista**:

| tramo | pista | fichero |
|---|---|---|
| dialogo de apertura (escena guionizada) | `ChapterIntro` | `capNN_intro.ogg` |
| cinematica, estampa A (rotulo y versiculo) | `ChapterCineA` | `capNN_cine1.ogg` |
| cinematica, estampa B (narracion y objetivo) | `ChapterCineB` | `capNN_cine2.ogg` |
| carta de apertura y a jugar | `ChapterIntro` / `ChapterTheme` | `capNN_intro.ogg` / `capNN.ogg` |

- Lo elige `Engine::update_music` (`core/engine.cpp`) leyendo `state.current_chapter`,
  `cinematic.scene` (0 = estampa A) y `scene_id` (la escena guionizada de apertura).
- **El 2026-09-14 esto eran dos pistas y sonaba a una.** `ChapterIntro` era el tema del
  capitulo desnudo —la misma melodia, el mismo solista, la misma progresion—, las dos
  estampas de la cinematica pedian la misma, y el dialogo de apertura sonaba ya con el
  tema de juego. O sea: el titulo, la apertura y el capitulo, lo mismo. Si vuelves a
  tocar esto, la regla es que **dentro de un capitulo ningun tramo repite fondo ni
  solista**; el modo y el centro tonal si se comparten, y es lo que los hace del mismo
  sitio.
- Antes de esto, `update_music` mandaba `Exploration` en todo el Acto I y `EliseoCombat`
  en todo el Acto II: **diez capitulos con dos pistas**. `EliasCombat` y `Menu` se
  cargaban y no las pedia nadie. Ahora el Santuario usa `Menu` y el titulo `MainTheme`.
- **`load_music_or(id, path, fallback)`**: si el `.ogg` de un capitulo no esta, carga el
  de respaldo y lo dice por log. Un capitulo mudo se nota mucho mas que un capitulo con
  la pista de al lado, y durante una tanda de musica a medias eso pasa por definicion.
- La musica va con `predecode = false`, o sea **streaming**: 27 pistas cargadas no cuestan
  RAM, solo descriptores.
- **`play_music` loguea `Music: <idx> '<fichero>'`** en cada cambio. Sin eso, un guion de
  autoplay puede comprobar que el fichero se CARGO pero no que suene el del capitulo en
  el que esta (`scripts/musica_check.txt`).

Como se generan las pistas: `tools/audio/CLAUDE.md`.

## Variantes y ducking

**Variacion por disparo**: `Audio::play_sfx_variant(first, count, vol)` sortea entre
`count` tomas **consecutivas** en el enum (como la terna `CayadoSwing1..3` de siempre).
Hay tres de cada proyectil, tres de `EnemyHit` y dos de `EnemyDeath`. Sin esto, mantener
el ataque suena a ametralladora. **Ese sorteo usa un RNG propio (`Audio::variant_rng`),
nunca `rand()`**: `rand()` es el generador que siembra los mapas (`core/types.h: randf`),
asi que sortear ahi haria que el AUDIO moviera la partida — dos partidas con la misma
semilla divergirian segun cuantos sonidos hubieran sonado.

**Ducking**: un SFX con volumen >= 0,85 agacha la musica (`Audio::duck_music`) y
`Audio::update(dt)` la deja subir. Se aplica **solo** en `set_music_volume`, sobre la
ganancia de la pista: `music_volume` sigue siendo exactamente lo que el jugador puso en
Opciones. El Engine solo tiene que llamar a `audio.update(dt)`.

## Voz

- **`Engine::play_voice` con `voice_cooldown` de 2,2 s** es lo unico que separa "el profeta
  se queja" de "el profeta no calla": con i-frames de 1 s se oiria un gemido por segundo.
  El grito de **jefe se salta la espera** — suena una vez por capitulo y es justo el
  momento en el que tiene que oirse.
- **Solo los milagros de tier 3+ llevan voz.** Si el profeta gritara tambien al curarse con
  los cuervos, la exclamacion dejaria de significar nada.
- `SDL_Log("Voice: ...")` por exclamacion: sin el, `audio_check` solo podia decir que el
  fichero se habia CARGADO, no que se hubiera oido.
