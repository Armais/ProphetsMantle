#pragma once

// =============================================================================
// MANTLE - El tutorial: se aprende jugando, en su propio mapa
// =============================================================================
//
// Decisiones del usuario (2026-09-12), para que no se re-discutan:
//
//   - Se ofrece al pulsar NUEVA PARTIDA, con una pregunta de dos botones. No vive dentro
//     del capitulo 1: tiene MAPA PROPIO y no toca el mundo fijo de ningun capitulo.
//   - TRES salas encadenadas: interior de combate -> interior de fe -> EXTERIOR de mundo.
//     La puerta del norte se abre al cerrar el ultimo paso de la sala.
//   - El profeta lleva TODO puesto (tres milagros, arma secundaria y consumible): es la
//     unica forma de que "explicalo todo" y "se valida haciendolo" quepan juntos.
//   - Los enemigos pegan de verdad pero el tutorial NO se puede perder: la vida no baja
//     de 1 (Engine::update_tutorial).
//   - NINGUN paso se cierra solo (2026-09-13): si el cartel nombra una accion, esa accion
//     es lo que lo cierra. No quedan relojes. Lo puramente informativo (que es la vida, que
//     es la fe, sus grados) dejo de ser un paso: no es una accion, es una explicacion, y va
//     al FINAL en forma de dialogo (`story_tutorial_outro`), que es ademas la pantalla que
//     falta entre el tutorial y Acab. El mundo no se congela en ningun momento.
//   - CRUZAR EL ARCO termina el tutorial: el arco de la sala 3 es la salida de verdad.
//   - Se salta entero en cualquier momento, con el mismo boton de SALTAR de las escenas.
//   - Al terminar (o al saltar) arranca la partida de verdad: el equipo del tutorial NO
//     viaja, y el guardado se restaura tal como estaba (Engine::finish_tutorial). Pero
//     llamado desde la pantalla de Controles vuelve al TITULO (`TutorialThen`): quien solo
//     iba a repasar los controles no puede perder la partida que tenia a medias.
//
// VOZ: dos lineas por cartel. Arriba la instruccion en seco, abajo una linea corta en el
// registro del juego. Ese sabor es INVENTADO y esta escrito a la manera de la
// Reina-Valera 1960 (la regla de voz de `docs/encuentros-y-lore.md`), pero NO finge ser
// versiculo: sin comillas angulares y sin referencia, que en esta capa se reservan para
// la cita de verdad.
//
// =============================================================================

#include "core/types.h"
#include "core/i18n.h"

struct Room;

// =============================================================================
// Los pasos
// =============================================================================
// El orden de este enum ES el orden del tutorial. `TUTORIAL_STEPS` (tutorial.cpp) dice en
// que sala vive cada uno y cuantas veces hay que hacerlo.
enum class TutorialGoal : u8 {
    // Sala 1 - el cuerpo
    Move = 0,       // moverse
    Attack,         // romper dos tinajas a tiros
    Secondary,      // disparar el arma secundaria
    Dash,           // esquivar (entra un arquero: esquivar con alguien disparandote)
    Consumable,     // beberse el consumible (el mordisco del arquero deja algo que curar)

    // Sala 2 - la fe
    Miracle1,       // lanzar el milagro del primer slot
    Miracle2,       // ... el del segundo
    Miracle3,       // ... el del tercero
    Pray,           // orar en el altar

    // Sala 3 - el mundo
    Secrets,        // abrir dos escondrijos (matorral, tinaja)
    Chest,          // abrir el cofre (la carta de 1 de 3 se explica ESTANDO abierta)
    Offering,       // recoger una ofrenda
    Map,            // abrir el mapa
    Pause,          // abrir la pausa (y salir de ella)
    CrossExit,      // cruzar el arco: ahi se acaba el tutorial

    COUNT
};

struct TutorialStep {
    TutorialGoal goal;
    int  room;      // 0, 1 o 2: en que sala se juega
    int  target;    // > 1: hay que repetir la accion N veces
};

// Tabla del tutorial (tutorial.cpp). `TUTORIAL_STEP_COUNT` = numero de pasos.
extern const TutorialStep TUTORIAL_STEPS[];
extern const int TUTORIAL_STEP_COUNT;

// Texto de un paso: `head` es la instruccion y `flavor` la linea en voz del juego. En
// tactil la instruccion habla de dedos y no de teclas (la regla de `src/input/CLAUDE.md`:
// un boton que anuncia una tecla que el aparato no tiene es peor que uno mudo).
struct TutorialText {
    const char* head;
    const char* flavor;
};
TutorialText tutorial_text(TutorialGoal goal, bool touch, bool lefty = false);

// Cartel mientras la puerta del norte esta abierta y queda cruzarla.
TutorialText tutorial_door_text();

// Titulo de la carta de "elige 1 de 3" cuando la abre el cofre del tutorial: la explicacion
// se da MIENTRAS la carta esta en pantalla, no en un cartel posterior con la carta ya cerrada.
const char* tutorial_choice_title();

// Cuantos de los SEIS escondrijos que siembra `tutorial_build_room` en la sala 3 estan ya
// abiertos. Por posicion de tile y no contando props de la sala: asi ni el matorral
// decorativo del bioma ni un Fuego del Cielo lanzado a otra cosa cierran el paso.
int tutorial_secrets_opened(const Room& room);

// Monta la sala `index` (0..2) del tutorial sobre una Room reutilizada. Determinista: las
// tres salas son identicas en todas las partidas, como el resto del mundo del juego.
void tutorial_build_room(Room& room, int index, int chapter);

// Siembra las cinco tinajas de la sala 1 en las casillas libres. La llama `tutorial_build_room`
// al montar la sala y `tutorial_setup_step` al abrirse el paso de atacar, porque quien rompio
// las tinajas ANTES de que se las pidieran dejaba el paso imposible (no hay timeout).
void tutorial_plant_jars(Room& room);

// Abre la puerta del muro norte (el ultimo paso de la sala se ha cerrado).
void tutorial_open_north_door(Room& room);

// Donde nace el profeta en esa sala, en tiles.
void tutorial_spawn_tile(int index, int& tx, int& ty);

// =============================================================================
// Estado
// =============================================================================
// A donde se vuelve al terminar el tutorial. Lo fija quien lo lanza: `NewRun` desde la
// pregunta de Nueva Partida y `Title` desde la pantalla de Controles.
enum class TutorialThen : u8 { NewRun, Title };

struct TutorialSystem {
    bool active = false;
    TutorialThen then = TutorialThen::NewRun;
    int  step   = 0;      // indice en TUTORIAL_STEPS
    int  room   = -1;     // sala montada ahora mismo (-1 = ninguna)
    f32  step_t = 0.0f;   // tiempo dentro del paso actual
    f32  done_t = 0.0f;   // > 0: el paso se cumplio y se esta mostrando el visto bueno
    f32  acc    = 0.0f;   // acumulador del paso (hoy: distancia recorrida en `Move`)
    int  progress = 0;    // veces que se ha hecho la accion (pasos con `target` > 1)
    // El paso siguiente vive en OTRA sala: la puerta del norte esta abierta y no se evalua
    // nada hasta cruzarla. Sin esto, el cartel de la sala 2 saldria estando aun en la 1.
    bool waiting_door = false;
    bool door_open = false;

    // Sondas del frame anterior: el tutorial mira el mundo en vez de que medio motor
    // tenga que avisarle. Lo unico que el Engine le cuenta a mano son las ofrendas
    // (`offerings_picked`), porque su contador se consume antes de llegar aqui.
    f32  prev_secondary_cd = 0.0f;
    f32  prev_cooldown[3]  = {0.0f, 0.0f, 0.0f};
    int  prev_props = 0;          // tinajas + matorral que quedaban en la sala
    int  prev_charges = 0;        // Player::consumable_charges
    int  offerings_picked = 0;
    bool pause_seen   = false;    // se abrio la pausa desde el juego (paso `Pause`)
    bool exit_reached = false;    // se piso el arco de salida (paso `CrossExit`)

    const TutorialStep& current() const {
        const int i = (step < 0) ? 0 : (step >= TUTORIAL_STEP_COUNT ? TUTORIAL_STEP_COUNT - 1 : step);
        return TUTORIAL_STEPS[i];
    }
    bool finished() const { return step >= TUTORIAL_STEP_COUNT; }
};
