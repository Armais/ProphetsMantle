# `tools/audio/` - sintesis de TODO el audio (`gen_audio.py`)

Lo que el motor hace con los ficheros ya generados esta en `src/audio/CLAUDE.md`.

```
tools/
  audio/gen_audio.py      - sintetizador numpy de TODO el audio -> .ogg via ffmpeg
                            (`python tools/audio/gen_audio.py`; `all wav` deja los .wav).
                            Cuerdas Karplus-Strong, flauta ney, dos buses estereo,
                            humanizacion y compresor de bus. Ver seccion "Audio".
```

### Audio (`tools/audio/gen_audio.py` -> `.ogg`, pasada del 2026-08-25)

**Los 47 ficheros de `assets/audio` son `.ogg` (Vorbis), no `.wav`.** Los mismos sonidos
ocupaban **48 MB** en WAV y no viajaban ni a Web ni a movil; ahora `assets/` entero pesa
**24 MB**. `Audio::load_*` sigue cayendo a `.wav` si falta el `.ogg`, asi que las dos
formas funcionan, pero el `.wav` queda solo como depuracion
(`python tools/audio/gen_audio.py all wav`). La transcodificacion la hace **ffmpeg**, que
es herramienta de compilacion: no se distribuye ni se enlaza nada suyo, y sin el el
generador escribe `.wav` y avisa. SDL3_mixer descodifica Vorbis con **stb_vorbis**
compilado dentro (`SDLMIXER_VORBIS_STB=ON`), sin DLL externa.

Todo el audio es **sintesis propia con numpy** — ni una muestra de terceros. Lo que subio
el liston del sintetizador:

- **`ks_pluck`**: cuerda Karplus-Strong de verdad (ruido en un anillo de retardo que se
  filtra a cada vuelta) en lugar de la pila de senos de `note()`, que sonaba siempre a
  campana. Se calcula **por filas de un periodo**, no muestra a muestra: son `dur*freq`
  iteraciones de numpy en vez de `dur*sr` en Python.
- **`ney`**: flauta de cana con aire soplado y vibrato que *entra* — la voz solista del
  Levante, que no existia. `melody_from_pattern(..., voice='ney')`.
- **`Song` con dos buses**: el estereo ya no se finge al final con `widen`; cada voz se
  coloca al anadirla (`add(..., pan=)`, potencia constante) y cada canal lleva su propio
  juego de retardos de reverb (`ROOM_L` / `ROOM_R`). Aun asi la correlacion L/R se queda
  en ~0,98 porque bajo, bombo y pads van al centro: es **deliberado**, el juego sale a
  movil y ahi todo se reproduce en mono.
- **Humanizacion**: `Song.add` desplaza cada evento unos milisegundos y le varia la
  intensidad. Una rejilla exacta es lo que delata a la musica generada. `tight=True` la
  salta para pedales y notas tenidas, que tienen que caer clavadas en el compas.
- **`compress`**: compresor de bus suave (3:1). Su detector va **diezmado en bloques de
  64 muestras** — no es un detalle de rendimiento: con un filtro por muestra la
  envolvente de relajacion sale a `np.convolve` con ventana de 14.000 muestras sobre
  millones y el generador se cuelga **minutos por pista**.

Guion: `scripts/audio_check.txt` (menu -> prologo -> combate -> muertes con sonido propio
-> milagros -> jefe -> Ascension -> victoria). Su criterio es **negativo**: cero lineas
`Failed to load` en el log. Procedencia y licencias de todo: `assets/CREDITS.md`.

### Musica: Sonic Pi (`tools/audio/sonicpi/`, tanda del 2026-09-13)

Las pistas NO salen de `gen_audio.py` (eso quedo para SFX y voz): salen de Sonic Pi.
**Una partitura por pista, 27 en total**: siete globales (`main_theme`, `menu`, `boss_theme`,
`victory`, `death`, `ascension`, `prologue`) y **dos por capitulo** — `capNN.ogg` para jugar y
`capNN_intro.ogg`, el mismo tema desnudo, para la cinematica y el dialogo de apertura. Antes
habia diez pistas para diez capitulos repartidas entre DOS temas de ambiente.

**Flujo**: `record.py boot` (proceso de larga vida; sin su keep-alive el daemon se suicida en
segundos) -> `record.py capture <pista> <periodo_s>` en background -> mandar la partitura con el
MCP `run_code`. El harness graba 2 periodos, corta el segundo (regimen estacionario: la cola de
reverb del primero ya suena dentro), sella la costura con un crossfade de 64 muestras, pasa la
**mezcla de bus**, normaliza y encodea.

**`lib.rb` se inyecta por el SPIDER antes del runner**, y la partitura llega despues por el MCP
(puerto de cues). Son dos canales distintos a proposito: los `define` persisten en la sesion,
asi que los helpers no se copian 27 veces y el mensaje del MCP se queda corto.

Lo que subio el liston respecto a la tanda de agosto (medido, no opinado — `analyze.py` sobre
las diez viejas da **0/10 en verde**):

- **Humanizacion determinista** (`hp`/`hc`/`pk` en `lib.rb`): cada evento se desplaza +-9 ms con
  `time_warp` y varia de intensidad. El desplazamiento NO mueve el reloj logico, asi que el
  periodo sigue cuadrando; y el jitter va sembrado con `sd` **al principio de cada vuelta**, de
  modo que el periodo 2 (el que se graba) es identico al 3 y la costura empalma. Sin la semilla
  por vuelta, humanizar rompe el corte del bucle.
- **Forma con secciones y arco**: 32 compases `intro | A | B | A' | salida`, con capas que
  entran y salen. Las diez viejas tenian todas las capas sonando de principio a fin: **0,3-2,9 dB
  de rango dinamico**.
- **Un modo y un solista por capitulo** (`sc(:dorian)`, `:hijaz`, `:lydian`...). Las diez viejas
  compartian frigia dominante en MI y la misma paleta `prophet`+`fm`+`pluck`+`blade`.
- **Mezcla de bus en `record.py`**: high-pass a 55 Hz y **high-shelf de +8 dB desde 2,8 kHz**.
  Esto es lo que arregla el "suena tapado": los samples de percusion aguda si tienen aire
  (`hat_cab` lleva el 98% de su energia sobre 2 kHz) pero a amp 0,06 y 0,16 s no pesan nada al
  lado de un bajo sostenido, y las pistas viejas se quedaban en **el 0-2% de energia sobre
  2 kHz**. El shelf levanta todo el agudo de la mezcla, armonicos incluidos.
- **Sonoridad comun**: `ebur128` mide y se normaliza a **-18 LUFS** con techo en -1,5 dBFS. Antes
  se igualaba el RMS contra `music_backup/`, que solo existe para las diez viejas: cualquier
  pista nueva salia con ganancia 1,0 y a otro volumen. Dos de las viejas **clipean** (pico 1,02).
- **Encode a 32 kHz `-q:a 3`**: con 27 pistas, `assets/` crece ~15 MB en vez de ~26.

**Los WAV de `tools/audio/sonicpi/tmp/` NO se borran** (723 MB, fuera del repo por
.gitignore). Son la grabacion cruda de cada pista —2 periodos en PCM 16 bits, 176 KB/s— y con
ellos `record.py reprocess <pista> <periodo>` rehace la mezcla en SEGUNDOS en vez de regrabar
en minutos: sonoridad, brillo, filtros y crossfade. Solo hay que regrabar si cambia la MUSICA
(una nota, un instrumento, el nivel de una capa).

**El periodo de cada pista** (segundo argumento de `capture` y de `reprocess`; si se falla, el
bucle no cierra):

| pista | periodo | apertura | periodo |
|---|---|---|---|
| cap01 | 96 | cap01_intro | 53.333333 |
| cap02 | 87.272727 | cap02_intro | 54.857143 |
| cap03 | 80 | cap03_intro | 50.526316 |
| cap04 | 128 | cap04_intro | 68.571429 |
| cap05 | 83.478261 | cap05_intro | 48 |
| cap06 | 76.8 | cap06_intro | 45.714286 |
| cap07 | 80 | cap07_intro | 45.714286 |
| cap08 | 101.052632 | cap08_intro | 56.470588 |
| cap09 | 73.846154 | cap09_intro | 43.636364 |
| cap10 | 68.571429 | cap10_intro | 41.739130 |
| main_theme | 96 | menu | 90 |
| boss_theme | 58.181818 | victory | 31.304348 |
| death | 48 | ascension | 75.789474 |
| prologue | 87.272727 | | |

**Dos trampas que costaron varias tomas** y que conviene mirar ANTES de grabar:

- **Una nota por debajo de 55 Hz no existe**: el paso alto de bus se la come. `cap04` tenia
  su drone en la nota 37 (46 Hz) y su bajo en la 25 (35 Hz), y la pista salia con el 93% de
  la energia en los medios. Con tonicas graves (DO#3 y similares) el bajo va a `-1`, no a `-2`.
- **Una campana por debajo de 2 kHz no cuenta como aire**, porque ese es justo el umbral con
  el que se mide. `pretty_bell` en la octava 2 no sube el gate; en la 3, si. Y `dull_bell` es
  apagado por definicion: para dar aire, `pretty_bell`.

**El crossfade de la costura es adaptativo**: 30 ms normalmente, y 1,5 s cuando el salto pasa
de 6 dB. Lo que deja escalon son las capas de cola larga al cruzar el bucle (el drone de
`cap04` vive 15 s), porque el principio del trozo grabado lleva la cola del ciclo anterior y
el final no. Aun asi, un escalon grande se arregla en la PARTITURA, igualando el nivel de la
entrada y el de la salida: si el bucle empieza en una seccion llena y acaba en una vacia, se
oye a cada vuelta.

**El gate es `analyze.py`** (lo llama `capture` solo): rango dinamico, costura, hueco de 2 s,
correlacion L/R, centroide, reparto por bandas, **energia sobre 2 kHz** y densidad de ataques
—este ultimo es el que caza la capa muerta por sample mal nombrado, que no da error al evaluar
y se pierde en silencio—. `python analyze.py assets/audio/music` mide la carpeta entera.

**El `spider.log` NO sirve de gate**: no registra ni jobs ni `puts` (0 lineas "Job" en todo el
historico). Lo unico que dice si la partitura entro es el OSC del harness: `/mantle/started`
frente a `/mantle/error`.

### Voz sintetizada (GDD 6.4, 2026-08-25)

El GDD pide dos cosas y **solo una es compatible con la regla de cero assets de terceros**:
las **exclamaciones cortas de gameplay** se sintetizan aqui; las **cinematicas dobladas
NO**, porque exigen TTS de terceros — dependencia externa y licencia. Eso queda como
decision del usuario, sin hacer.

**Modelo fuente-filtro** (`gen_audio.py`), que es lo que separa una voz de un tono con
envolvente. Son dos cosas independientes:

- la **FUENTE** (`glottal`): el pulso glotal, que da tono y registro. **No es un seno** —
  es un pulso con muchisimos armonicos, aqui un diente de sierra suavizado, con **jitter**
  de tono y **shimmer** de amplitud. Sin esos dos la voz sale plana y robotica, por el
  mismo motivo por el que `Song.add` humaniza la rejilla de la musica.
- el **FILTRO** (`_resonator` x3 sobre `FORMANTS`): el tracto vocal resuena en tres
  formantes fijos, y **son ellos, no el tono, los que hacen que una "a" suene a "a"**.
  Cambiar de vocal es mover los formantes.

`voice(vowel, f0, dur, bend, effort, breath)`. **`bend` (caida del tono) no es un adorno**:
un grito de esfuerzo siempre baja al final y sin eso suena a nota tenida. `effort` aprieta
el aire y sube el 3er formante, que es lo que se oye como "apretar los dientes".

**15 ficheros, 132 KB** (`assets/` sigue en 25 MB). Cuatro exclamaciones — esfuerzo (dash),
dolor (golpe recibido), milagro y jefe — con **tomas propias para cada profeta**: Elias
grave y cerrado (f0 ~106, vocal 'a'/'o'), Eliseo mas alto y abierto (f0 ~138, 'e'/'a').
Se separan por **tono y vocal, no por efectos**: eso un jugador lo distingue a la primera;
dos reverbs distintos, no. Tres variantes por exclamacion, por la misma razon que los
disparos.

- **`gen_audio.py` acepta ahora un PREFIJO** (`python tools/audio/gen_audio.py voz_`) para
  regenerar un grupo sin reescribir los 47 ficheros que ya estan bien.

Guion: `audio_check.txt` ampliado. Dos trampas que costo verlas: el bloque de voz al final
del guion **no sonaba** porque tras la victoria la escena ya no es `Playing` y los comandos
de partida se descartan en silencio; y la voz del milagro se la comia el `voice_cooldown`
de los golpes anteriores. Ademas, para el grito de jefe hay que entrar en su sala con
`goto`, y **el indice depende del capitulo** (`gw/2`: el cap. 1 es 3x3 -> sala 1; el cap. 3
es 4x3 -> sala 2).
