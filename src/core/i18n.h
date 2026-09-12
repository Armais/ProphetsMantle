#pragma once

// =============================================================================
// MANTLE - Localizacion (i18n)
// =============================================================================
//
// El juego se escribio entero en espanol, con los literales metidos en el sitio donde se
// pintan y dentro de las tablas `switch` de contenido (items, milagros, enemigos, logros,
// mejoras, lore, encuentros). Son unas 600 cadenas visibles.
//
// FORMA ELEGIDA: `tr("es", "en")` — las dos versiones VIVEN JUNTAS en el punto de uso.
// La alternativa clasica (un enum StringID + una tabla por idioma) se descarto a
// proposito. Las dos dan el error en COMPILACION, que es lo que pedia el proyecto; pero
// con ids habria que inventar ~600 nombres, mantener enum y tabla sincronizados, y cada
// sitio del codigo pasaria de leerse "Nueva Partida" a leerse "StringID::MenuNewGame".
// Con `tr` el error de compilacion es aun mas duro —falta un ARGUMENTO, no una entrada en
// una tabla— y quien lee o revisa el codigo ve el original y su traduccion en la misma
// linea, que es donde se detecta que una traduccion esta mal.
//
// REGLAS
//  * `Scripture::ref` ("1 Reyes 17:4") NO se pinta en ningun sitio, ni en espanol ni
//    traducido. Esa regla no cambia con el idioma.
//  * Ojo con `Renderer::wrap_text`, que TRUNCA EN SILENCIO lo que no cabe: el ingles casi
//    siempre es mas corto que el espanol, pero no siempre. Lo verifica
//    `scripts/i18n_check.txt` fotografiando las mismas pantallas en los dos idiomas.
//  * El atlas de glifos se hornea con Lilita One (latino). ES/EN/PT/IT/FR entran sin tocar
//    nada; cirilico, griego y CJK **no** — harian falta otra fuente y otro atlas.
//
// =============================================================================

#include "core/types.h"

enum class Language : u8 {
    ES = 0,   // base: el idioma en el que esta escrito el juego
    EN = 1,
    COUNT
};

namespace I18N {

// Idioma activo. Vive en una variable global y no colgando del Engine porque lo lee
// literalmente cada llamada de dibujo; pasarlo por parametro habria tocado todas.
extern Language current;

inline const char* language_name(Language l) {
    return (l == Language::EN) ? "English" : "Espanol";   // solo para el log: ASCII
}

// Nombre del idioma tal y como se ensena en Opciones (en su propio idioma, que es como se
// hace: quien no entiende la interfaz actual tiene que reconocer el suyo).
inline const char* language_label(Language l) {
    return (l == Language::EN) ? "English" : "Español";
}

}  // namespace I18N

// Devuelve la cadena en el idioma activo. Con `en` vacio o nulo cae al espanol, para que
// una traduccion a medias ensene el original en vez de un hueco.
inline const char* tr(const char* es, const char* en) {
    return (I18N::current == Language::EN && en && *en) ? en : es;
}
