# `tools/audio/` - sintesis de TODO el audio (`gen_audio.py`)

Lo que el motor hace con los ficheros ya generados esta en `src/audio/CLAUDE.md`.

```
tools/
  audio/gen_audio.py      - sintetizador numpy de los SFX y la voz -> .ogg via ffmpeg
                            (`python tools/audio/gen_audio.py`; `all wav` deja los .wav).
                            Cuerdas Karplus-Strong, flauta ney, dos buses estereo,
                            humanizacion y compresor de bus. Ver seccion "Audio".
  audio/orquesta/         - LA MUSICA (47 pistas): orquesta sintetica propia en numpy/scipy
                            (`python tools/audio/orquesta/render.py [prefijos]`).
                            Ver seccion "Musica: la orquesta sintetica".
  audio/sonicpi/          - HISTORICO: las partituras de Sonic Pi de la tanda 2026-09-13.
                            Ya no generan nada; `analyze.py` sigue siendo el gate.
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

### Musica: la orquesta sintetica (`tools/audio/orquesta/`, tanda del 2026-10-03)

**Es lo que genera hoy las 47 pistas.** El usuario pidio "dar el salto de calidad... a un
nivel ultra profesional": banda sonora cinematografica de Oriente Proximo antiguo (la
referencia es *El Principe de Egipto*, *Assassin's Creed Origins*, *Journey*). Sonic Pi no
llegaba (sintes de fabrica y samples ajenos) y ademas no corre en Linux sin cabeza, asi que
la musica pasa a ser **sintesis propia de principio a fin, sin una sola muestra**: ni
samples, ni soundfonts, ni IR grabadas. Mismos nombres de fichero, mismos `.ogg`, el motor
no cambia.

```bash
python tools/audio/orquesta/render.py                  # las 47 (~10 min en 4 nucleos)
python tools/audio/orquesta/render.py cap03 boss       # por PREFIJO de nombre
python tools/audio/orquesta/render.py menu --wav-only --spectro   # sin encodear + PNG
python tools/audio/orquesta/spectro.py <pista.ogg|wav> [salida.png] [--seam]
```

`render.py` escribe `assets/audio/music/<pista>.ogg` (Vorbis, 32 kHz, `-q:a 3`), deja el
WAV crudo (float32) y el espectrograma en `tools/audio/orquesta/tmp/` (fuera del repo por
.gitignore), decodifica el `.ogg` para medir pico y sonoridad reales y al final pasa
**`tools/audio/sonicpi/analyze.py`** a cada pista. **Determinista**: las semillas salen de
`crc32(nombre de pista, nombre de parte)` — NUNCA de `hash()`, que en Python cambia en
cada proceso para las cadenas (dos pasadas tienen que dar ficheros identicos).

| fichero | que hay |
|---|---|
| `dsp.py` | biquads RBJ, ruido lento, tablas de onda limitadas en banda, IR sinteticas, convolucion circular, BS.1770 (LUFS), compresor, cinta, limitador de pico verdadero |
| `instruments.py` | los modelos (abajo) |
| `compose.py` | modos y maqamat, notacion de melodias, `Score`/`Part`, conduccion de voces, humanizacion |
| `arrange.py` | el MOTIVO, tesituras, secciones, bordones, tanpura |
| `mix.py` | partes -> cuerpos -> buses de reverb -> bus maestro; tabla `CAL` |
| `tracks_global.py`, `tracks_act1.py`, `tracks_act2.py` | las 47 partituras |
| `render.py`, `spectro.py` | CLI (multiproceso) y espectrogramas |

**Los instrumentos** (todos con humanizacion de ataque y dinamica):

- **Cuerdas** (`vln1 vln2 vla vc cb` y solistas `solo_vln solo_vla solo_vc`): tabla de
  onda con el espectro de Helmholtz del arco (1/n con las muescas del punto de contacto),
  **6-8 voces por seccion** con vibrato, deriva y desafinacion independientes (eso es lo
  que la hace "lush"), legato con portamento y bajada de arco, ruido de arco, brillo que
  sigue a la dinamica, tremolo/picado/spiccato, y un **cuerpo de convolucion**: envolvente
  disenada (picos ~285/460/560 Hz y joroba del puente ~2,6 kHz en el violin, mas graves en
  viola/chelo/contrabajo) por encima de una estructura fina de ~70 resonancias aleatorias.
  El vibrato barre esa estructura fina: es el brillo vivo que separa una cuerda de una
  sierra filtrada.
- **Coro** (`choir_s/a/t/b`, `voice` solista): pulso glotal en tabla con jitter, shimmer y
  vibrato por cantante (9-10 voces por cuerda), banco de **formantes** por tesitura y vocal
  (tablas clasicas soprano/contralto/tenor/bajo), morph entre dos vocales, aliento.
- **Vientos** (`duduk ney kaval shofar`): fuente en tabla + ruido de soplo (parte ancha y
  parte enganchada al periodo), cuerpo formantico, entradas desde abajo, vibrato que ENTRA
  tarde en las notas largas, mordentes/trinos/apoyaturas desde la notacion.
- **Metales** (`horns lowbrass`): seccion de 4 con el filtro que se ABRE con la dinamica
  (filtro variable por bloques).
- **Pulsadas** (`oud kanun harp lyre pizz`): Karplus-Strong de verdad con filtro de lazo
  simetrico de 3 coeficientes, calculado por FILAS de un periodo (numpy, no Python por
  muestra) y afinado exacto leyendo la cuerda a velocidad f*N/SR; posicion de pulsacion,
  ruido de pua, cuerpo de convolucion. Cache por (altura, capa, variante).
- **Percusion modal** (`frame daf darbuka riq taiko timp zills shaker clap gong`): modos de
  membrana circular (ceros de Bessel), golpe de centro vs. borde, glissando de tension,
  anillas inarmonicas que repican, redobles.
- **Metalofonos** (`bells celesta crotales`), **bordon** (`tanpura`, con el pico del
  jawari bajando por los armonicos) y **ambientes** (`wind sea fire water`).

**Reverb**: cuatro salas con IR SINTETICAS estereo verdadero (LL/LR/RL/RR): ruido
decorrelado por bandas con su propio T60 (los agudos mueren antes, la piedra los alarga),
reflexiones tempranas dispersas y rampa de entrada. `hall` (2,5 s), `temple` (3,6 s,
reflexiones separadas), `cave` (5 s, oscura) y `room` (0,9 s). Convolucion FFT CIRCULAR.

**El bucle exacto, sin grabar dos vueltas**: todo lo lineal (notas, cuerpos, reverb) se
calcula sobre el "one-shot" y se **pliega modulo el periodo**, que es exactamente el
regimen estacionario de tocarlo en bucle; lo no lineal (compresor, cinta, limitador,
nivelador) se precalienta con el propio final del bucle (`dsp.circular`). La costura empalma
a la muestra (no hace falta crossfade). Es equivalente a "grabar dos periodos y cortar el
segundo", a mitad de coste.

**Bus maestro** (`mix.master`): paso alto 35 Hz -> shelves ADAPTATIVOS de aire (hasta
+7,5 dB desde 2,6 kHz si la pista no llega al 5% sobre 2 kHz) y de cuerpo (si no llega al
11% bajo 200 Hz) -> balance L/R estatico (+-2 dB como mucho) -> **nivelador** (solo si el
arco pasa de 12,5 dB entre el tramo mas fuerte y el mas flojo: "lleva el fader" alrededor
de la mediana con una envolvente lentisima) -> compresor de pegamento 2:1 (~0,5 dB) ->
cinta (tanh x2 sobremuestreada) -> -18 LUFS (BS.1770 propio; `ffmpeg ebur128` lo confirma
sobre el `.ogg`) -> limitador de pico verdadero a -1,5 dBTP. Tras el Vorbis ningun pico
pasa de -1,4 dBFS. Los retornos de sala (`* 0.72` en `render_score`) dejan la relacion
directo/reverb en ~+3 dB: con +6/+7 sonaba a toma de microfono cercano, seca.

**El motivo del juego** (`arrange.MOTIF`, Hijaz en MI: MI FA SOL# SI, la llamada, y la caida
por la segunda aumentada) vuelve transformado: el duduk lo presenta en el titulo, el arpa lo
dice en eolico en el Santuario, las trompas lo convierten en desafio en el Carmelo y lo dicen
en LIDIO en el Llamado, en MAYOR en Samaria y en la victoria, AUMENTADO en la gloria de la
Ascension e INVERTIDO en la derrota. Cada capitulo tiene su modo y su solista, y sus cuatro
pistas no repiten solista ni fondo (tablas en los docstrings de `tracks_act1.py` y
`tracks_act2.py`).

**Notacion de melodias** (`compose.parse_mel`): `"0:1.5^ 1:.5 2:1 4:1g | 3:.75 ..."` =
grado:pulsos + adornos (`^` entra desde abajo, `g`/`h` apoyatura, `m`/`w` mordente, `t`
trino, `v` caida, `~` vibrato ancho, `>` acento, `.` picado, `!` re-articula), `'`/`,`
octava, `#`/`b` semitono y `q`/`d` cuarto de tono (los maqamat `bayati`, `rast`, `saba`
existen). **`check_bars` avisa si un compas no suma 4 pulsos**: un error de duracion no da
excepcion, desplaza todo lo que viene detras y la melodia acaba a contratiempo de la armonia.
Cazo dos en la primera pasada.

**`CAL` (mix.py) es la calibracion de cada modelo**: con ella una nota tenida a dinamica 0,6
sale a ~-20 LUFS (pulsadas -22, campanas -24, percusion medida por patron), y las ganancias
de las partituras hablan de EQUILIBRIO musical. **Si se toca un modelo, hay que remedir su
`CAL`**, o la pieza entera se descompensa sin que ningun gate lo note.

**Trampas que costaron una pasada (mirarlas ANTES de tocar nada):**

- **Una nota larga en el compas 0 se pliega al FINAL.** Si su ataque humanizado cae unos ms
  antes del cero, el modulo la manda entera a la cola del buffer; con una cola fija mas
  corta que la nota, el final del bucle se quedaba sin ella (el contrabajo de 16 s de una
  estampa desaparecia en sus ultimos 2 s). La cola se dimensiona ahora con los eventos.
- **Karplus-Strong con ruido blanco sale CHILLON.** Excitacion plana = todos los armonicos
  igual de fuertes al pulsar: el oud tenia su maximo en 2 kHz y el fundamental 11 dB por
  debajo. Paso bajo en la excitacion y posicion de pulsacion realista (oud 0,22, junto a la
  roseta) lo dejan con el maximo en el fundamental.
- **Cuerpos modales aleatorios: +-10 dB por banda entre semillas.** Con resonancias al azar
  el violin segundo salia 9 dB por debajo del primero. La envolvente se disena; el azar solo
  pone la estructura fina, con profundidad 0,55 en secciones y 0,95 en solistas.
- **El formante no puede dejar fuera al fundamental**: los cantantes suben F1 con la nota
  (sintonia de formante), y el banco lo hace (`F1 >= 1,12 f0`) mas un camino de "pecho".
- **Costura**: el bucle empalma a la muestra, pero `analyze` mide el NIVEL de los ultimos 2 s
  contra los 2 primeros. Lo que la rompia: un crescendo hasta el final de una estampa (vuelve
  a empezar en piano), un arpegio que sube y se apaga cada compas de 4 s (el final siempre
  cae en la parte floja) y el viento con rafagas demasiado profundas. Los gestos se escriben
  por medio compas o en arco (sube y vuelve).
- **Chasquidos de fuego de una muestra = clic digital** (factor de cresta 120, el limitador
  bajaba 6 dB toda la pista). Son rafagas de 1-4 ms con saturacion suave.
- Las texturas oscuras (duduk, tremolo, coro en 'u') no llegan al 4% sobre 2 kHz ni con
  +10 dB de shelf, y ese shelf solo sube el soplido. El aire se ORQUESTA (crotalos, anillas,
  shaker, ruido de arco, el viento o el agua del sitio) y el shelf queda en +7,5 dB como
  mucho. `cap05_cine1`, `cap08_cine1` y `cap09_intro` son las que van mas justas (4,6-6%).
- **La cuerda grave tiraba la mezcla a la derecha** (hasta 8,6 dB en una ventana de 4 s en
  las intros de `cap06`): la energia grave manda en el RMS. Contrabajos y chelos van casi al
  centro, como en una mezcla de cine, y el maestro corrige el balance global.
- **Fraseo automatico** (`Part._shape_phrases`): cada `mel()` hace un arco de dinamica por
  frase, las notas agudas pesan un poco mas y la nota final se suelta. Sin eso, todas las
  notas de una melodia salian a la misma dinamica: secuenciador. `shape=False` lo apaga
  (riffs, cantos en ostinato).
- **Timbre medido por octavas, no a ojo**: el violin de seccion salia con la banda de 2 kHz
  tan fuerte como el fundamental (zumbido de sierra) hasta bajar la joroba del puente a
  +2,5 dB y cortar por encima de 4,8 kHz. Para comprobarlo: renderizar una escala de cada
  instrumento y medir la energia por octava (el fundamental o el 2o armonico deben mandar;
  4 kHz, 10-15 dB por debajo).

**Periodos** (todos son bucles; las estampas duran 12-16 s y lo que importa pasa en los
primeros 10):

| pista | BPM x compases | s | | pista | BPM x compases | s |
|---|---|---|---|---|---|---|
| main_theme | 80 x 32 | 96 | | menu | 64 x 24 | 90 |
| boss_theme | 132 x 32 | 58,2 | | victory | 80 x 16 | 48 |
| death | 60 x 12 | 48 | | ascension | 76 x 24 | 75,8 |
| prologue | 66 x 24 | 87,3 | | | | |

| cap | tema | apertura | estampa A | estampa B |
|---|---|---|---|---|
| 01 | 80 x 32 = 96 | 60 x 12 = 48 | 60 x 4 = 16 | 80 x 4 = 12 |
| 02 | 88 x 32 = 87,3 | 72 x 16 = 53,3 | 16 | 72 x 4 = 13,3 |
| 03 | 96 x 32 = 80 | 80 x 16 = 48 | 16 | 12 |
| 04 | 60 x 28 = 112 | 60 x 12 = 48 | 16 | 60 x 4 = 16 |
| 05 | 92 x 32 = 83,5 | 80 x 16 = 48 | 16 | 12 |
| 06 | 100 x 32 = 76,8 | 48 | 16 | 12 |
| 07 | 96 x 32 = 80 | 48 | 16 | 12 |
| 08 | 76 x 32 = 101,1 | 48 | 16 | 12 |
| 09 | 104 x 32 = 73,8 | 48 | 16 | 12 |
| 10 | 112 x 32 = 68,6 | 48 | 16 | 12 |

### Musica: Sonic Pi (HISTORICO — `tools/audio/sonicpi/`, tanda del 2026-09-13)

> **Ya no genera las pistas** (desde el 2026-10-03 las genera `tools/audio/orquesta/`).
> Se conserva como historia del pipeline anterior y porque `analyze.py` sigue siendo el
> gate de la orquesta. Lo de abajo describe como ERA.

Las pistas NO salen de `gen_audio.py` (eso quedo para SFX y voz): salen de Sonic Pi.
**Una partitura por pista, 27 en total**: siete globales (`main_theme`, `menu`, `boss_theme`,
`victory`, `death`, `ascension`, `prologue`) y **dos por capitulo** — `capNN.ogg` para jugar y
`capNN_intro.ogg` para el dialogo y la carta de apertura, y `capNN_cine1.ogg` /
`capNN_cine2.ogg`, una por estampa de la cinematica. Dentro de un capitulo **ningun tramo
repite fondo ni solista**: cuando `capNN_intro` era el tema desnudo (misma melodia, mismo
ney) el jugador oia la misma pista desde el menu hasta el primer enemigo. Antes
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
- **Encode a 32 kHz `-q:a 3`**: con 47 pistas, `assets/` crece ~18 MB en vez de ~31.

**Los WAV de `tools/audio/sonicpi/tmp/` NO se borran** (723 MB, fuera del repo por
.gitignore). Son la grabacion cruda de cada pista —2 periodos en PCM 16 bits, 176 KB/s— y con
ellos `record.py reprocess <pista> <periodo>` rehace la mezcla en SEGUNDOS en vez de regrabar
en minutos: sonoridad, brillo, filtros y crossfade. Solo hay que regrabar si cambia la MUSICA
(una nota, un instrumento, el nivel de una capa).

**El periodo de cada pista** (segundo argumento de `capture` y de `reprocess`; si se falla, el
bucle no cierra):

| capitulo | tema (jugando) | apertura (dialogo y carta) | estampa A | estampa B |
|---|---|---|---|---|
| cap01 | 96 | 48 | 16 | 12 |
| cap02 | 87.272727 | 53.333333 | 16 | 13.3333 |
| cap03 | 80 | 48 | 16 | 10.6667 |
| cap04 | 128 | 48 | 16 | 16 |
| cap05 | 83.478261 | 48 | 16 | 10.6667 |
| cap06 | 76.8 | 48 | 16 | 10 |
| cap07 | 80 | 48 | 16 | 10 |
| cap08 | 101.052632 | 48 | 16 | 12 |
| cap09 | 73.846154 | 48 | 16 | 10 |
| cap10 | 68.571429 | 48 | 16 | 10 |

Las de cinematica son cortas a proposito: la estampa A dura 10,5 s en pantalla y la B 9 s
(`ui/cinematic.cpp`), asi que **del bucle solo se oye el arranque**. Lo que tenga que pasar,
que pase en los primeros diez segundos.

| global | periodo | global | periodo |
|---|---|---|---|
| main_theme | 96 | menu | 90 |
| boss_theme | 58.181818 | victory | 31.304348 |
| death | 48 | ascension | 75.789474 |
| prologue | 87.272727 | | |

**Cuatro trampas que costaron varias tomas** y que conviene mirar ANTES de grabar:

- **Solo `:blade` tiene vibrato.** `:dsaw`, `:saw`, `:tri`, `:pluck` y `:recorder` aceptan
  `vibrato_rate` y `vibrato_depth` sin dar ningun error y los IGNORAN: no existen en su
  synthdef. Comprobar los opts de verdad es mirar el `.scsyndef` compilado
  (`etc/synthdefs/compiled/sonic-pi-<synth>.scsyndef`), donde los nombres de los argumentos
  estan en texto plano. `:dsaw` tampoco tiene `res`. Para un violin, la unica via es doblar
  `:blade` (vibrato) con `:dsaw` (el rascado del arco) y ligar las notas con
  `control <nodo>, note:` + `note_slide`, que es lo que hay en `prologue.rb`.
- **Un `rate:` bajo en un sample lo manda a los graves.** `ambi_choir` a `rate: 0.38` suena
  mas de una octava por debajo y cuenta entero como energia grave; en `cap04_cine1` el coro y
  el drone de `:hollow` se llevaban el 72% de la mezcla y la pista se quedaba en el 0,9% de
  aire por muchas campanas que tuviera encima. Para que un coro haga de coro, `rate` entre
  0,75 y 1,0.
- **Un drone gana siempre a una campana.** El drone suena los cuatro compases (sustain 5 +
  release 4) y la campana son toques sueltos: en energia integrada, que es como mide el
  analizador, no hay color. Si una pista de textura no da el gate de aire, lo que hay que
  bajar es el drone, no subir la campana.
- **La `:kalimba` no sirve de aire.** Concentra casi toda su energia en la fundamental, asi
  que en la octava 2 no pasa de los 2 kHz y la pista se queda en el 0-2% por mucho que la
  oigas tintinear. En la 3 ya cuenta, y si el gate aprieta, detras va una `pretty_bell`.
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

**Antes de grabar, `python check_names.py`**: comprueba la SINTAXIS de las 47 partituras con
el `ruby -c` que trae Sonic Pi (no hace falta que el daemon este arrancado) y que los nombres
de sample y de synth existen. Una partitura con un parentesis mal puesto no da error visible:
el harness graba dos periodos de silencio o, peor, la cola de la pista anterior, y los
numeros que salen no dicen nada. Lo que NO comprueba son los opts de cada synth, por la
trampa de arriba.

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

- **El grito de guerra del milagro** (`grito_guerra`, 2026-09-24): la toma vieja era una 'a'
  sostenida y casi cantada, y se oia como una queja. Lo que la hace rugido: el tono SUBE con el
  esfuerzo y solo cae al final, doblado de periodo SUAVE (grunido a f0/2: si es fuerte, el
  oido coge la octava de abajo y suena a monstruo), raspado de aire, vocal que se mueve
  (cerrada -> 'a' gritada -> 'o'), saturacion y doble toma. Sus formantes son **paso-banda de
  verdad** (`_formant_bp`): el `_resonator` de las otras voces normaliza la ganancia en
  continua y deja pasar el fundamental por cada formante, por eso esas voces salen oscuras
  (3% de energia por encima de 1 kHz). Medido con autocorrelacion: Elias 98 -> 113 -> 90 Hz.
  Usa un rng PROPIO, asi que regenerarlo no mueve ningun otro fichero.
- **`gen_audio.py` acepta ahora un PREFIJO** (`python tools/audio/gen_audio.py voz_`) para
  regenerar un grupo sin reescribir los 47 ficheros que ya estan bien.

Guion: `audio_check.txt` ampliado. Dos trampas que costo verlas: el bloque de voz al final
del guion **no sonaba** porque tras la victoria la escena ya no es `Playing` y los comandos
de partida se descartan en silencio; y la voz del milagro se la comia el `voice_cooldown`
de los golpes anteriores. Ademas, para el grito de jefe hay que entrar en su sala con
`goto`, y **el indice depende del capitulo** (`gw/2`: el cap. 1 es 3x3 -> sala 1; el cap. 3
es 4x3 -> sala 2).
