#pragma once

// =============================================================================
// MANTLE - Ambiente: sombras de nubes, luz y motas (2026-10-03)
// =============================================================================
//
// Peticion del usuario: «mejora todo, graficos e imagenes, a un nivel ultra profesional».
// Con el suelo horneado y el tileset al doble, el mundo ya no tiene rejilla, pero seguia
// QUIETO: una foto bien pintada. Lo que le da vida en un juego de vista cenital son tres
// capas baratas que no tocan el gameplay:
//
//   - SOMBRAS DE NUBES que pasan despacio por el suelo (y por encima de todo lo que hay en
//     el, personajes incluidos: una nube no distingue). Textura de ruido periodica que se
//     repite en el mundo y se desplaza con el viento. Solo en el exterior.
//   - LUZ: un degradado de pantalla por capitulo, calido desde arriba a la izquierda (de
//     donde vienen las sombras de los sprites) y fresco en la esquina contraria. Se suma
//     (ADD) la luz y se mezcla la penumbra; los dos muy flojos.
//   - MOTAS: polvo en el desierto, vilanos en la pradera, ceniza en las ciudades corruptas,
//     pavesas en el Carmelo y polvo quieto en los interiores. Deterministas por indice y
//     tiempo (sin estado): viven en el mundo con envoltura alrededor de la camara, asi que
//     no se van con el profeta.
//
// Todo son texturas procedurales creadas en `ambience_init` (regla de assets/: no crece).
// Sin ellas no se dibuja nada y el juego sigue igual.
// =============================================================================

#include "core/types.h"

struct Renderer;
struct Room;

void ambience_init(Renderer& renderer);

// Dentro de la pasada de MUNDO, justo despues de draw_sorted: las sombras de nubes.
void ambience_draw_world(Renderer& renderer, const Room& room, int chapter, Rect view, f32 time);

// En PANTALLA (despues de end_world, antes de la vineta): luz y motas.
void ambience_draw_screen(Renderer& renderer, const Room& room, int chapter, Vec2 camera, f32 time);
