#pragma once

// =============================================================================
// MANTLE - Suelo HORNEADO por trozos (2026-10-03)
// =============================================================================
//
// Peticion del usuario: «mejora todo, graficos e imagenes, a un nivel ultra profesional».
// Lo que mas se ve del juego es el suelo, y era una rejilla: cuatro variantes de baldosa
// sorteadas por tile (en el Carmelo, un damero de losas grises), una senda hecha de manchas
// repetidas y charcas en escalera de 16 unidades. Ninguna de las tres cosas se arregla
// pintando mejor la celda del tileset: el defecto es la CELDA.
//
// Aqui el suelo se genera por codigo en texturas de TROZO (16x16 tiles = 256 unidades, a 2 px
// por unidad = 512x512 px) a partir de los tiles de la sala:
//
//   - campos suaves por tipo (senda, agua, hueco, solidos) que se emborronan a nivel de tile
//     y se muestrean con bilineal + ruido: las orillas y los bordes de la senda salen CURVOS y
//     organicos, sin dejar de seguir la colision (el umbral 0,5 cae en la linea entre tiles);
//   - una paleta y un estilo por capitulo (tierra, arena con ondas, roca con grietas, pradera,
//     losas irregulares tipo Voronoi en ciudad e interiores);
//   - agua con profundidad (clara en la orilla, honda dentro), arena mojada y linea de espuma;
//   - sombra de contacto (AO) al pie de rocas, arboles y muros.
//
// Encima se sigue pintando lo de siempre (muros, puentes, vados, decorados, props) y el agua
// lleva destellos animados por encima (la textura es estatica). Sin tileset no se hornea nada.
//
// Memoria: MAX_CHUNKS ranuras de 1 MB que se reciclan (LRU); se generan las visibles y, una por
// frame, las de alrededor. Cada trozo lleva una FIRMA de sus tiles (+ margen): si un tile cambia
// (una charca seca con Sanar Aguas, una grieta abierta) el trozo se rehace solo.
// =============================================================================

#include "core/types.h"

struct Renderer;
struct Room;

// Pinta el suelo horneado de `room` en la vista. Devuelve false si no se pudo (sin trozos
// listos): entonces render_ground cae a las celdas del tileset de siempre.
bool ground_draw_baked(Renderer& renderer, const Room& room, Rect view, f32 time);

// Olvida todos los trozos (cambio de capitulo / de sala): la siguiente pasada los rehace.
void ground_invalidate();

// Para los guiones: cuantos trozos se generaron en total y cuanto tardo el ultimo (ms).
int  ground_chunks_built();
f32  ground_last_build_ms();
