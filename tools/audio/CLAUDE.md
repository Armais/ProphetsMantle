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
