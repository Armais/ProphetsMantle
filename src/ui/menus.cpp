// =============================================================================
// MANTLE - Menu System Implementation
// =============================================================================

#include "ui/menus.h"
#include "core/config.h"
#include <algorithm>
#include "core/i18n.h"
#include "game/items.h"

void MenuSystem::mark_row(int i, f32 y, f32 step) {
    if (i < 0 || i >= MAX_ROW_RECTS) return;
    row_rects[i] = {40.0f, y - 2.0f, static_cast<f32>(Config::GAME_WIDTH) - 80.0f, step};
    if (i + 1 > row_rect_count) row_rect_count = i + 1;
}

void MenuSystem::mark_row_rect(int i, Rect r) {
    if (i < 0 || i >= MAX_ROW_RECTS) return;
    row_rects[i] = r;
    if (i + 1 > row_rect_count) row_rect_count = i + 1;
}

// =============================================================================
// Botones de menu
// =============================================================================
// Hasta aqui una opcion era una linea de texto centrada con "> " delante. En un telefono
// eso no es un boton: no se ve donde empieza y acaba la zona que responde al dedo, ni cual
// esta elegida de un vistazo. Ahora cada opcion es una pieza con forma, contorno y bisel
// (`Renderer::draw_panel`, el mismo panel de 9 trozos que enmarca el mapa), y la elegida
// cambia de COLOR, de TAMANO y lleva un aro de foco: a plena luz en un movil un cambio de
// tono no se ve, y el cambio de tamano si.
// El crecimiento de la elegida es un GUINO, no otra pildora: a 9 por lado volvia a meter
// 18 de aire horizontal justo en el boton que se esta mirando, que es de lo que se quejaba
// la pantalla. Con 4 basta, porque el relleno dorado y el aro ya hacen el grueso del aviso.
static constexpr f32 BTN_GROW = 4.0f;     // lo que crece la elegida por cada lado
static constexpr f32 BTN_HIT_PAD = BTN_GROW * 2.0f + 14.0f;   // margen de la zona del dedo

// El ancho de un boton es el de SU texto. Ni una constante (148 fijas sobre 320 eran una
// pila de pildoras de media pantalla dijera lo que dijera el rotulo) ni el del texto MAS
// LARGO de la pantalla, que es lo que se hizo antes y dejaba "Salir" con media pildora de
// aire a cada lado solo porque "Nueva Partida" estaba tres filas mas arriba. Se mide con
// `measure_text`, como las pestanas del Codice y la placa de rotulo del mapa, y el unico
// aire es el relleno de 7 por lado que pide la esquina redondeada del panel.
static f32 menu_button_width(const Renderer& r, const char* label, f32 text_scale) {
    return std::min(static_cast<f32>(Config::GAME_WIDTH) - 24.0f,
                    r.measure_text(label, text_scale) + 14.0f);
}

static Rect menu_button_rect(int i, int selected, f32 top, f32 pitch, f32 h, f32 btn_w) {
    const f32 w = btn_w + ((i == selected) ? BTN_GROW * 2.0f : 0.0f);
    return {(static_cast<f32>(Config::GAME_WIDTH) - w) * 0.5f, top + i * pitch, w, h};
}

// La zona tactil NO depende de cual este elegida: si creciera con la seleccion, el sitio
// donde hay que tocar se moveria bajo el dedo.
static Rect menu_hit_rect(int i, f32 top, f32 pitch, f32 h, f32 btn_w) {
    const f32 hw = btn_w + BTN_HIT_PAD;
    return {(static_cast<f32>(Config::GAME_WIDTH) - hw) * 0.5f,
            top + i * pitch - (pitch - h) * 0.5f, hw, pitch};
}

// En tactil el ancho de la zona del dedo no sale del texto de cada fila: "Salir" tiene que
// ser tan facil de tocar como "Reintentar capitulo". Alto = paso completo, o sea que las
// filas se tocan entre si y no queda ni una franja muerta entre dos botones.
static Rect menu_hit_rect_wide(int i, f32 top, f32 pitch, f32 h) {
    return {16.0f, top + i * pitch - (pitch - h) * 0.5f,
            static_cast<f32>(Config::GAME_WIDTH) - 32.0f, pitch};
}

// --- Rejilla de dos columnas (titulo en tactil) ------------------------------------
// En apaisado sobra ancho y falta alto: una sola columna de 7 filas obliga a un paso de
// 10,9 unidades, que en un movil de 6,5" son 4 mm de dedo (Android pide 9). En 2x4 la
// celda pasa a 25 unidades ~ 9,2 mm sin robarle sitio a nada.
// El orden es por COLUMNAS (izq. 0..rows-1, dcha. el resto): la lista se lee entera de
// arriba abajo antes de saltar, y el mapeo indice -> accion de Engine::update no se toca.
// Las dos columnas cuelgan del CENTRO, no de los bordes. Con las celdas pegadas al borde
// pasaban las dos cosas a la vez: los botones se echaban encima de los profetas —que en
// esta pantalla flanquean el fondo— y entre columna y columna quedaban 55 unidades de aire
// muerto justo en el medio. Ahora el bloque entero vive en la franja libre entre los dos
// bustos (x 72..245) y el unico hueco es el canal.
static constexpr f32 GRID_COL_W = 92.0f;    // zona del dedo por columna (~34 mm)
static constexpr f32 GRID_GAP   = 12.0f;    // canal entre las dos columnas
static constexpr f32 GRID_PAD   = 4.0f;     // aire entre el boton y el filo interior

static f32 menu_grid_col_x(int i, int rows) {
    const f32 cx = static_cast<f32>(Config::GAME_WIDTH) * 0.5f;
    return (i < rows) ? cx - GRID_GAP * 0.5f - GRID_COL_W : cx + GRID_GAP * 0.5f;
}

static Rect menu_grid_hit(int i, int rows, f32 top, f32 pitch, f32 h) {
    const int row = i % rows;
    return {menu_grid_col_x(i, rows), top + row * pitch - (pitch - h) * 0.5f, GRID_COL_W, pitch};
}

static Rect menu_grid_button(int i, int rows, f32 top, f32 pitch, f32 h, f32 btn_w) {
    const int row = i % rows;
    const f32 x0  = menu_grid_col_x(i, rows);
    // El boton se arrima al centro dentro de su celda (la izquierda por la derecha y al
    // reves), asi que los dos filos interiores quedan alineados y el bloque sale simetrico
    // y compacto. La celda entera sigue respondiendo al dedo, tambien por fuera del boton.
    const f32 x = (i < rows) ? x0 + GRID_COL_W - GRID_PAD - btn_w : x0 + GRID_PAD;
    return {x, top + row * pitch, btn_w, h};
}

// Fila que hay bajo el dedo, -1 si no hay ninguna. En movil es lo que sustituye al
// resaltado de seleccion: se ilumina lo que se esta tocando, no lo que un cursor recuerda.
static int pressed_row(bool active, Vec2 p, const Rect* hits, int n) {
    if (!active) return -1;
    for (int i = 0; i < n; i++) if (hits[i].contains(p)) return i;
    return -1;
}

static void draw_menu_button(Renderer& renderer, Rect r, const char* label,
                             bool selected, bool disabled, f32 text_scale = 0.95f) {
    // El texto de la opcion elegida va CLARO, no oscuro. `draw_text` siempre dibuja contorno
    // oscuro alrededor del glifo, asi que letra oscura sobre dorado sale como un borron: se
    // vio en la primera captura y "Nueva Partida" no se leia. Lo que distingue a la elegida
    // es el relleno dorado, el tamano y el aro, que se ven incluso a plena luz en un movil.
    Color fill = {52, 45, 41, 240};
    Color text = {216, 202, 182, 255};
    if (selected) { fill = {188, 132,  38, 255}; text = {255, 249, 234, 255}; }
    if (disabled) { fill = { 34,  30,  28, 210}; text = { 98,  90,  84, 255}; }

    renderer.draw_panel({r.x, r.y + 1.7f, r.w, r.h}, {0, 0, 0, 110}, 4.2f);     // sombra
    if (selected)                                                               // aro de foco
        renderer.draw_panel({r.x - 1.7f, r.y - 1.7f, r.w + 3.4f, r.h + 3.4f}, {255, 214, 120, 205}, 5.6f);
    renderer.draw_panel(r, fill, 4.2f);

    // `draw_text` no recorta: lo que no cabe se planta encima del vecino. Con la rejilla
    // del titulo el ancho lo manda la columna y no el rotulo, asi que la etiqueta larga
    // ("Partida abandonada  +12345") necesita esta red. Mismo apaño que las etiquetas de
    // las celdas de milagro, que ya prueban 0,55 y luego 0,45.
    f32 s  = text_scale;
    f32 lw = renderer.measure_text(label, s);
    if (lw > r.w - 6.0f && lw > 0.0f) {
        s *= (r.w - 6.0f) / lw;
        lw = renderer.measure_text(label, s);
    }
    renderer.draw_text(label, r.x + (r.w - lw) * 0.5f,
                       r.y + (r.h - Renderer::TEXT_LINE_UNITS * s) * 0.5f, text, s);
}

void MenuSystem::draw_button_column(Renderer& renderer, const char* const* labels, int n,
                                    Layout pc, Layout touch, f32 text_scale, int disabled_idx) {
    const Layout L = touch_hints ? touch : pc;
    // En movil NADA sale elegido: no hay cursor que mover, se toca el boton que se quiere.
    // Con sel = -1 el boton tampoco crece, asi que lo unico que cambia de aspecto es lo
    // que esta bajo el dedo.
    const int sel = touch_hints ? -1 : selected_option;
    if (n > MAX_ROW_RECTS) n = MAX_ROW_RECTS;

    // Las zonas se calculan ENTERAS antes de dibujar: probar contra un array a medio
    // llenar encenderia el resalte del dedo segun avanza el bucle, de arriba abajo.
    Rect hits[MAX_ROW_RECTS];
    f32  widths[MAX_ROW_RECTS];
    for (int i = 0; i < n; i++) {
        widths[i] = menu_button_width(renderer, labels[i], text_scale);
        hits[i]   = touch_hints ? menu_hit_rect_wide(i, L.top, L.pitch, L.h)
                                : menu_hit_rect(i, L.top, L.pitch, L.h, widths[i]);
    }
    const int down = pressed_row(press_active, press_pos, hits, n);

    for (int i = 0; i < n; i++) {
        draw_menu_button(renderer, menu_button_rect(i, sel, L.top, L.pitch, L.h, widths[i]),
                         labels[i], i == sel || i == down, i == disabled_idx, text_scale);
        mark_row_rect(i, hits[i]);
    }
}

void MenuSystem::update(Input& input) {
    // En tactil arrastrar mueve la VENTANA, no el cursor. Sin esto, quitar el resaltado en
    // movil dejaria un cursor invisible que el arrastre pasea a ciegas: el jugador arrastra
    // para ver el resto de filas y de paso cambia cual se va a ajustar.
    const bool touch_scroll = touch_hints && current == MenuScreen::Settings;
    if (input.is_action_just_pressed(Action::MoveUp)) {
        if (touch_scroll) settings_scroll = std::max(0, settings_scroll - 1);
        else selected_option = (selected_option - 1 + option_count) % std::max(1, option_count);
    }
    if (input.is_action_just_pressed(Action::MoveDown)) {
        if (touch_scroll) settings_scroll = std::min(std::max(0, option_count - 1), settings_scroll + 1);
        else selected_option = (selected_option + 1) % std::max(1, option_count);
    }

    // --- Toque ---------------------------------------------------------------
    Vec2 p;

    // Pantallas sin filas: tocar en cualquier sitio acepta y sale.
    if (current == MenuScreen::Victory) {
        if (input.take_tap(p)) input.pulse(Action::Confirm);
        return;
    }
    // Controles: un solo boton (jugar el tutorial). Tocar FUERA vuelve, que es lo que esta
    // pantalla hacia siempre; tocarlo a el, lo juega.
    if (current == MenuScreen::Controls) {
        if (rect_screen != current) return;
        if (!input.take_tap(p)) return;
        const bool on_button = row_rect_count > 0 && row_rects[0].contains(p);
        selected_option = 0;
        input.pulse(on_button ? Action::Confirm : Action::Cancel);
        SDL_Log("Menu tap: pantalla=%d fila=%d (%.0f,%.0f)",
                static_cast<int>(current), on_button ? 0 : -1, p.x, p.y);
        return;
    }
    // Los rects se apuntan al PINTAR, asi que el primer frame de una pantalla nueva aun
    // lleva los de la anterior. Sin esta guarda, entrar en Opciones y tocar de inmediato
    // pulsaba la fila que hubiera debajo en el menu de antes. Va ANTES de take_tap: si se
    // consumia primero, el toque se lo tragaba esta pantalla y no lo atendia nadie — el
    // primer toque tras cada cambio de pantalla se perdia. Ahora espera al frame siguiente.
    if (rect_screen != current) return;
    if (!input.take_tap(p)) return;

    int hit_row = -1;
    for (int i = 0; i < row_rect_count && i < option_count; i++) {
        if (!row_rects[i].contains(p)) continue;
        hit_row = i;
        selected_option = i;
        if (current == MenuScreen::Settings && settings_row_at(i) != ROW_BACK) {
            // Mitad izquierda baja, mitad derecha sube. Lo consume Engine::update_settings,
            // que ya sabe que hacer con MoveLeft/MoveRight en cada fila.
            input.pulse(p.x < Config::GAME_WIDTH * 0.5f ? Action::MoveLeft : Action::MoveRight);
        } else {
            input.pulse(Action::Confirm);
        }
        break;
    }
    // Un toque que falla era MUDO, y por eso las coordenadas podridas de un guion pasaban
    // sin que saltara nada. `fila=-1` es el gate negativo de menus_tactil_check.
    SDL_Log("Menu tap: pantalla=%d fila=%d (%.0f,%.0f)",
            static_cast<int>(current), hit_row, p.x, p.y);
}

void MenuSystem::render(Renderer& renderer) {
    row_rect_count = 0;
    rect_screen    = current;
    switch (current) {
        case MenuScreen::Language:  render_language(renderer); break;
        case MenuScreen::Title:     render_title(renderer); break;
        case MenuScreen::PauseMenu: render_pause(renderer); break;
        case MenuScreen::GameOver:  render_game_over(renderer); break;
        case MenuScreen::Victory:   render_victory(renderer); break;
        case MenuScreen::Settings:  render_settings(renderer); break;
        case MenuScreen::Controls:  render_controls(renderer); break;
        case MenuScreen::TutorialAsk: render_tutorial_ask(renderer); break;
        default: break;
    }
}

// =============================================================================
// Selector de idioma (primer arranque)
// =============================================================================
// La UNICA pantalla del juego que no pasa por `tr()`, y a proposito: quien todavia no ha
// elegido idioma no puede leer una pregunta escrita en el que no es el suyo. El rotulo y
// la ayuda van en los dos a la vez, y cada boton lleva el nombre de su lengua EN esa
// lengua — que es como se reconoce el propio sin entender el resto de la pantalla.
void MenuSystem::render_language(Renderer& renderer) {
    const f32 W = static_cast<f32>(Config::GAME_WIDTH);
    const f32 H = static_cast<f32>(Config::GAME_HEIGHT);

    // Mismo fondo que el titulo: es la primera imagen del juego y no tiene por que ser
    // una pantalla de sistema.
    renderer.clear({10, 8, 16, 255});
    constexpr int BANDS = 30;
    for (int i = 0; i < BANDS; i++) {
        f32 k = 1.0f - static_cast<f32>(i) / BANDS;
        renderer.draw_rect({0, H * i / BANDS, W, H / BANDS + 0.5f},
                           {static_cast<u8>(58 * k), static_cast<u8>(30 * k), static_cast<u8>(34 * k), 255});
    }
    if (title_bg != TEXTURE_NONE)
        renderer.draw_sprite_rect(title_bg, {0, 0, 1280, 720}, {0, 0, W, H}, {255, 255, 255, 255});
    if (renderer.vignette_tex != TEXTURE_NONE)
        renderer.draw_sprite_rect(renderer.vignette_tex, {0, 0, 160, 90}, {-8, -8, W + 16, H + 16}, {0, 0, 0, 165});
    // Velo blando tras la columna central: aqui hay menos filas que en el titulo, pero el
    // orillo de oro del manto cruza justo por detras de los dos botones.
    if (title_bg != TEXTURE_NONE && renderer.shadow_tex != TEXTURE_NONE)
        renderer.draw_sprite_rect(renderer.shadow_tex, {0, 0, 64, 32}, {40, 50, 240, 120}, {8, 6, 14, 175});

    const f32 title_w = renderer.measure_text("MANTLE", 2.4f);
    renderer.draw_text("MANTLE", (W - title_w) * 0.5f + 1.3f, 26.0f + 1.3f, {0, 0, 0, 170}, 2.4f);
    renderer.draw_text("MANTLE", (W - title_w) * 0.5f, 26.0f, Color::elias_gold(), 2.4f);

    renderer.draw_text_centered("Elige idioma", 60, {205, 184, 142, 230}, 0.95f);
    renderer.draw_text_centered("Choose your language", 72, {150, 135, 105, 215}, 0.8f);

    option_count = 2;
    const char* options[] = {"Español", "English"};
    draw_button_column(renderer, options, option_count,
                       {94.0f, 17.0f, 13.0f}, {92.0f, 28.0f, 22.0f}, 1.0f);

    // En tactil las dos pildoras ya ocupan de 89 a 145: los pies bajan a su hueco.
    renderer.draw_text_centered(touch_hints ? "Toca tu idioma  ·  Tap your language"
                                            : "[W/S]  ·  [ENTER]",
                                touch_hints ? 152.0f : 146.0f, {110, 99, 88, 195}, 0.75f);
    // Se dice aqui para que la eleccion no parezca definitiva: la fila del idioma vive en
    // Opciones desde siempre.
    renderer.draw_text_centered("Se cambia en Opciones  ·  Change it in Options",
                                touch_hints ? 164.0f : 160.0f, {96, 87, 79, 175}, 0.66f);
}

void MenuSystem::render_title(Renderer& renderer) {
    const f32 W = static_cast<f32>(Config::GAME_WIDTH);
    const f32 H = static_cast<f32>(Config::GAME_HEIGHT);

    // Vertical gradient (warm ember at the top, night at the bottom) instead of a flat fill
    renderer.clear({10, 8, 16, 255});
    constexpr int BANDS = 30;
    for (int i = 0; i < BANDS; i++) {
        f32 k = 1.0f - static_cast<f32>(i) / BANDS;
        renderer.draw_rect({0, H * i / BANDS, W, H / BANDS + 0.5f},
                           {static_cast<u8>(58 * k), static_cast<u8>(30 * k), static_cast<u8>(34 * k), 255});
    }
    // La estampa (Elias, Eliseo y el manto, misma composicion que el icono) va sobre el
    // degradado y DEBAJO de la vineta, que es lo que la integra con el fondo.
    if (title_bg != TEXTURE_NONE)
        renderer.draw_sprite_rect(title_bg, {0, 0, 1280, 720}, {0, 0, W, H}, {255, 255, 255, 255});

    if (renderer.vignette_tex != TEXTURE_NONE)
        renderer.draw_sprite_rect(renderer.vignette_tex, {0, 0, 160, 90}, {-8, -8, W + 16, H + 16}, {0, 0, 0, 150});

    // Velo elipsoidal suave tras la columna del menu: los bustos flanquean los lados,
    // pero el manto cruza por detras de las ultimas filas y sin esto el texto pelea con
    // el orillo de oro. shadow_tex es justo un degradado blando, no hace falta mas.
    if (title_bg != TEXTURE_NONE && renderer.shadow_tex != TEXTURE_NONE)
        renderer.draw_sprite_rect(renderer.shadow_tex, {0, 0, 64, 32}, {36, 44, 248, 144}, {8, 6, 14, 165});

    // Title at 3x with a drop shadow
    const f32 title_w = renderer.measure_text("MANTLE", 3.0f);
    renderer.draw_text("MANTLE", (W - title_w) * 0.5f + 1.5f, 22.0f + 1.5f, {0, 0, 0, 170}, 3.0f);
    renderer.draw_text("MANTLE", (W - title_w) * 0.5f, 22.0f, Color::elias_gold(), 3.0f);

    // Subtitle. El lema largo NO cabe en tactil: la rejilla de botones empieza justo ahi.
    renderer.draw_text_centered(tr("El Manto del Profeta", "The Prophet's Mantle"), 56, {180, 160, 120, 200});
    if (!touch_hints)
        renderer.draw_text_centered(tr("Desafía reyes. Destruye altares. Hereda el manto.", "Defy kings. Tear down altars. Inherit the mantle."), 67, {120, 105, 80, 200}, 0.75f);

    // Menu options. Con 7 filas hay que apretar el interlineado: a 11 unidades la ultima
    // se comia la linea de ofrendas.
    option_count = 7;
    const char* options[] = {tr("Nueva Partida", "New Game"), tr("Continuar", "Continue"), tr("El Santuario", "The Sanctuary"), tr("El Códice", "The Codex"),
                             tr("Prólogo", "Prologue"), tr("Opciones", "Options"), tr("Controles", "Controls")};

    // Una partida caducada ya no se continua: lo unico que queda de ella son ofrendas. La
    // recompensa va EN el boton: entre las 7 filas, las ofrendas y la ayuda de teclas no
    // queda hueco para una linea suelta en 180 unidades de alto. Se compone ANTES de medir,
    // porque "Partida abandonada  +12345" es con mucho la fila mas larga y medir sin ella
    // dejaria justo ese boton desbordado (que es lo que pasaba con el ancho fijo).
    char expired[64];
    if (has_continue && continue_expired) {
        snprintf(expired, sizeof(expired), tr("Partida abandonada  +%d", "Abandoned run  +%d"), abandoned_offerings);
        options[1] = expired;
    }

    const int  sel      = touch_hints ? -1 : selected_option;   // en movil no hay cursor
    const int  disabled = has_continue ? -1 : 1;
    char offer_buf[48] = {};
    if (offerings > 0)
        snprintf(offer_buf, sizeof(offer_buf), tr("%d ofrendas", "%d offerings"), offerings);

    if (touch_hints) {
        // Rejilla de 2x4. Siete opciones en una columna dan un paso de 10,9 unidades — 4 mm
        // de dedo — y en apaisado el ancho estaba vacio. Aqui la celda mide 92x25 (34x9,2
        // mm) y la septima deja un hueco donde caben las ofrendas.
        constexpr int ROWS = 4;
        const f32 TOP = 68.0f, PITCH = 25.0f, BTN_H = 19.0f;

        // UN ancho para las siete, no uno por fila: en rejilla, medir cada rotulo hace una
        // escalera dentro de la columna. La etiqueta de partida caducada ya esta compuesta
        // (es la mas larga), y si aun asi no cupiera, draw_menu_button baja la escala.
        f32 btn_w = 0.0f;
        for (int i = 0; i < option_count; i++)
            btn_w = std::max(btn_w, renderer.measure_text(options[i], 0.9f) + 14.0f);
        btn_w = std::min(btn_w, GRID_COL_W - GRID_PAD * 2.0f);

        Rect hits[7];
        for (int i = 0; i < option_count; i++) hits[i] = menu_grid_hit(i, ROWS, TOP, PITCH, BTN_H);
        const int down = pressed_row(press_active, press_pos, hits, option_count);

        for (int i = 0; i < option_count; i++) {
            const Rect br = menu_grid_button(i, ROWS, TOP, PITCH, BTN_H, btn_w);
            draw_menu_button(renderer, br, options[i], i == down, i == disabled, 0.9f);
            mark_row_rect(i, hits[i]);
            // El Santuario con algo comprable: una moneda que late en la esquina
            if (i == 2 && sanctuary_ready) {
                renderer.draw_disc({br.right() - 2.0f, br.y + 2.0f}, 3.2f, {255, 206, 90, 255});
                renderer.draw_disc({br.right() - 2.6f, br.y + 1.4f}, 1.1f, {255, 248, 215, 240});
            }
        }
        // Las ofrendas van al hueco de la septima celda, SIN panel: con placa se leerian
        // como un octavo boton. Se centran sobre el hueco del BOTON que ocuparia esa celda
        // y no sobre la celda entera, asi que cuelgan de la columna en vez de quedarse
        // descolgadas hacia el borde.
        if (offer_buf[0]) {
            const Rect hole = menu_grid_button(ROWS * 2 - 1, ROWS, TOP, PITCH, BTN_H, btn_w);
            const f32 ow = renderer.measure_text(offer_buf, 0.8f);
            renderer.draw_text(offer_buf, hole.x + (hole.w - ow) * 0.5f,
                               hole.y + (hole.h - Renderer::TEXT_LINE_UNITS * 0.8f) * 0.5f,
                               {200, 175, 115, 220}, 0.8f);
        }
        return;
    }

    // Siete botones entre el lema (y=70) y la linea de ofrendas (y=160): con 10,9 de paso y
    // 9,5 de alto, el ultimo cierra en 152,9 y no pisa nada.
    const f32 BTN_TOP = 78.0f, BTN_PITCH = 10.9f, BTN_H = 9.5f;
    for (int i = 0; i < option_count; i++) {
        const f32 BTN_W = menu_button_width(renderer, options[i], 0.9f);
        const Rect br = menu_button_rect(i, sel, BTN_TOP, BTN_PITCH, BTN_H, BTN_W);
        draw_menu_button(renderer, br, options[i], i == sel, i == disabled, 0.9f);
        mark_row_rect(i, menu_hit_rect(i, BTN_TOP, BTN_PITCH, BTN_H, BTN_W));
        if (i == 2 && sanctuary_ready) {
            renderer.draw_disc({br.right() + 4.0f, br.y + br.h * 0.5f}, 2.6f, {255, 206, 90, 255});
            renderer.draw_disc({br.right() + 3.4f, br.y + br.h * 0.5f - 0.6f}, 0.9f, {255, 248, 215, 240});
        }
    }

    // Ofrendas acumuladas: es lo que se gasta en El Santuario, y se dice
    if (offer_buf[0]) {
        char line[96];
        snprintf(line, sizeof(line), "%s%s", offer_buf,
                 sanctuary_ready ? tr("  ·  algo te espera en El Santuario", "  ·  something awaits in the Sanctuary") : "");
        renderer.draw_text_centered(line, 160, {200, 175, 115, 220}, 0.75f);
    }
    renderer.draw_text_centered(tr("[ENTER] Seleccionar", "[ENTER] Select"),
                                170, {100, 90, 80, 180}, 0.8f);
}

void MenuSystem::render_pause(Renderer& renderer) {
    // Semi-transparent overlay
    renderer.draw_rect({0, 0,
        static_cast<f32>(Config::GAME_WIDTH),
        static_cast<f32>(Config::GAME_HEIGHT)}, {0, 0, 0, 150});

    renderer.draw_text_centered(tr("PAUSA", "PAUSED"), touch_hints ? 22.0f : 44.0f, Color::white(), 2);

    option_count = 5;
    // En el tutorial la ultima fila es SU salida y lo dice: en PC no hay boton de SALTAR
    // (ESC ya es esta pausa, que el paso 14 enseña) y esta es la unica puerta.
    // "Equipo" (2026-10-03): que llevas y que hace cada cosa, con las cifras.
    const char* options[] = {tr("Reanudar", "Resume"), tr("Equipo", "Equipment"), tr("El Códice", "The Codex"),
                             tr("Opciones", "Options"),
                             in_tutorial ? tr("Dejar el tutorial", "Leave the tutorial") : tr("Salir", "Quit")};

    // En tactil las cinco pildoras van de 50 a 170. El pie "Toca una opcion" sobra en cuanto
    // cada opcion tiene el tamaño de un dedo: explicaba lo obvio.
    draw_button_column(renderer, options, option_count,
                       {72.0f, 15.0f, 12.0f}, {50.0f, 24.0f, 19.0f}, 0.95f);
}

void MenuSystem::render_game_over(Renderer& renderer) {
    renderer.clear({15, 8, 8, 255});

    renderer.draw_text_centered(tr("DERROTA", "DEFEAT"), 6, {192, 57, 43, 255}, 1.6f);

    char buf[96];
    snprintf(buf, sizeof(buf), tr("El profeta ha caído  ·  capítulo %d  ·  %d:%02d", "The prophet has fallen  ·  chapter %d  ·  %d:%02d"),
             stats_chapter, static_cast<int>(stats_time) / 60, static_cast<int>(stats_time) % 60);
    renderer.draw_text_centered(buf, 27, {170, 140, 130, 240}, 0.65f);
    // (el desglose de ofrendas lo pinta el Engine entre y=38 y y=112: render_run_summary)

    // Menu de verdad: morir y volver al menu por error costaba la partida entera. "El
    // Santuario" va en medio: gastar lo que acabas de ganar y reintentar ya con ello.
    option_count = 3;
    char sanct[48];
    snprintf(sanct, sizeof(sanct), "%s%s", tr("El Santuario", "The Sanctuary"), sanctuary_ready ? "  (!)" : "");
    const char* options[] = {tr("Reintentar capítulo", "Retry chapter"), sanct, tr("Volver al menú", "Back to menu")};
    draw_button_column(renderer, options, option_count,
                       {121.0f, 13.5f, 11.0f}, {117.0f, 20.5f, 17.0f}, 0.9f);

    if (!touch_hints) {
        renderer.draw_text_centered(tr("Tu partida sigue guardada al principio del capítulo", "Your run is still saved at the chapter start"),
                                    163, {125, 110, 98, 210}, 0.6f);
        renderer.draw_text_centered(tr("[W/S] elegir   [ENTER] aceptar   [R] reintentar", "[W/S] choose   [ENTER] accept   [R] retry"),
                                    171, {100, 90, 80, 190}, 0.6f);
    }
}

void MenuSystem::render_victory(Renderer& renderer) {
    renderer.clear({18, 24, 44, 255});

    renderer.draw_text_centered(tr("EL LEGADO", "THE LEGACY"), 6, {245, 225, 150, 255}, 1.6f);
    renderer.draw_text_centered(tr("Israel ha visto la mano de Dios: Elías y Eliseo cumplieron su llamado",
                                   "Israel has seen the hand of God: Elijah and Elisha fulfilled their calling"),
                                27, {190, 195, 220, 240}, 0.6f);

    char buf[64];
    snprintf(buf, sizeof(buf), tr("%d enemigos  ·  tiempo %d:%02d", "%d enemies  ·  time %d:%02d"),
             stats_kills, static_cast<int>(stats_time) / 60, static_cast<int>(stats_time) % 60);
    renderer.draw_text_centered(buf, 35, {170, 180, 200, 230}, 0.6f);
    // (el desglose de ofrendas lo pinta el Engine: render_run_summary)
    if (victory_note)
        renderer.draw_text_centered(victory_note, 150, {190, 160, 110, 225}, 0.65f);
    renderer.draw_text_centered(touch_hints ? tr("Toca para volver al menú", "Tap to go back to the menu")
                                            : tr("[ENTER] Volver al menú", "[ENTER] Back to menu"),
                                164, {120, 130, 150, 220}, 0.8f);
}

void MenuSystem::render_settings(Renderer& renderer) {
    // Background depends on origin
    if (settings_origin == MenuScreen::PauseMenu) {
        // Semi-transparent overlay (game is visible behind)
        renderer.draw_rect({0, 0,
            static_cast<f32>(Config::GAME_WIDTH),
            static_cast<f32>(Config::GAME_HEIGHT)}, {0, 0, 0, 180});
    } else {
        renderer.clear({15, 12, 25, 255});
    }

    renderer.draw_text_centered(tr("OPCIONES", "OPTIONS"), 26, Color::elias_gold(), 2);

    // Las filas se montan en una TABLA de ids y no por indice fijo: la del stick derecho
    // solo existe en tactil, y sin esto anadir o quitar una fila desplazaba el significado
    // de todas las de abajo tanto al pintar como al ajustar (update_settings).
    option_count = 0;
    for (int r = 0; r < ROW_COUNT; r++) {
        // Las filas de los controles tactiles (stick derecho, tamaño, opacidad, zurdo,
        // ahorro de bateria) solo existen en tactil: con teclado serian ruido.
        if (is_touch_row(r) && !settings_show_rstick) continue;
        // La fila del cayado solo aparece cuando hay MAS DE UNA skin desbloqueada: una
        // fila que solo ofrece lo que ya llevas puesto no es una opcion, es ruido.
        if (r == ROW_SKIN && (settings_skins_mask & ~1u) == 0) continue;
        settings_rows[option_count++] = r;
    }
    if (selected_option >= option_count) selected_option = option_count - 1;

    // A 15 unidades de paso, 8 filas desde y=52 acaban en 157 y dejan la ayuda en 166.
    // En tactil la placa sube a 19 de alto (7,8 mm de dedo) y por eso ya no caben todas:
    // se pinta una VENTANA de 6 filas que se recorre arrastrando. Con 9 filas (stick
    // derecho + cayado desbloqueado) la ultima se salia de la pantalla incluso a 15.
    const bool win   = touch_hints;
    const int  VIS   = win ? std::min(option_count, 6) : option_count;
    const f32  TOP   = win ? 48.0f : 52.0f;
    const f32  PITCH = win ? 21.0f : 15.0f;
    const f32  PH    = win ? 19.0f : 14.0f;
    const f32  PX    = win ? 16.0f : 26.0f;
    settings_scroll = win ? std::max(0, std::min(settings_scroll, option_count - VIS)) : 0;

    // Las filas fuera de la ventana no pueden responder al dedo: se apuntan lejos de la
    // pantalla en vez de a cero, porque un rect nulo en el origen contiene el punto (0,0).
    for (int i = 0; i < option_count && i < MAX_ROW_RECTS; i++)
        mark_row_rect(i, {-1000.0f, -1000.0f, 0.0f, 0.0f});

    // Primero las zonas, luego el dibujo: el resalte del dedo tiene que conocerlas todas.
    Rect hits[MAX_ROW_RECTS];
    for (int k = 0; k < VIS; k++) {
        const f32 y = TOP + k * PITCH;
        hits[k] = {PX, y - (PITCH - Renderer::TEXT_LINE_UNITS) * 0.5f,
                   static_cast<f32>(Config::GAME_WIDTH) - 2.0f * PX, PITCH};
    }
    const int down = pressed_row(press_active, press_pos, hits, VIS);

    for (int k = 0; k < VIS; k++) {
        const int i = settings_scroll + k;
        const int row = settings_rows[i];
        // En movil no hay cursor: la fila no se "elige", se toca. Lo unico que cambia de
        // aspecto es la que esta bajo el dedo.
        const bool cursor  = !touch_hints && (i == selected_option);
        const bool lit     = cursor || (k == down);
        // Misma razon que en draw_menu_button: el contorno oscuro del glifo obliga a poner
        // texto CLARO sobre la placa dorada de la fila elegida.
        Color c = lit ? Color{255, 249, 234, 255} : Color{206, 194, 176, 255};
        f32 y = TOP + k * PITCH;
        const char* prefix = cursor ? "> " : "  ";
        const char* label =
            row == ROW_MUSIC  ? tr("Música", "Music") :
            row == ROW_SFX    ? tr("Efectos", "Effects") :
            row == ROW_AIM    ? tr("Auto-mira", "Auto-aim") :
            row == ROW_SENS   ? tr("Sensibilidad", "Sensitivity") :
            row == ROW_RUMBLE ? tr("Vibración", "Rumble") :
            row == ROW_RSTICK ? tr("Stick dcho.", "Right stick") :
            row == ROW_TSIZE  ? tr("Tam. botones", "Button size") :
            row == ROW_TOPACITY ? tr("Opacidad", "Opacity") :
            row == ROW_LEFTY  ? tr("Zurdo", "Left-handed") :
            row == ROW_BATTERY ? tr("Ahorro bat.", "Battery saver") :
            row == ROW_SKIN   ? tr("Cayado", "Staff") :
            row == ROW_LANG   ? tr("Idioma", "Language") :
                                tr("Volver", "Back");
        const char* si = tr("  SÍ  ", "  ON  ");
        const char* no = tr("  NO  ", " OFF  ");
        char buf[96];

        if (row == ROW_MUSIC || row == ROW_SFX) {
            // Barra de volumen: "Etiqueta  [||||......] XX%"
            f32 v = (row == ROW_MUSIC) ? settings_music_vol : settings_sfx_vol;
            int pct = static_cast<int>(v * 100.0f + 0.5f);
            int filled = static_cast<int>(v * 10.0f + 0.5f);
            char bar[12];
            for (int b = 0; b < 10; b++) bar[b] = (b < filled) ? '|' : '.';
            bar[10] = '\0';
            snprintf(buf, sizeof(buf), "%s%-12s [%s] %3d%%", prefix, label, bar, pct);
        } else if (row == ROW_AIM) {
            // Tres estados (GDD 4.2). "Asistido" no es un punto medio decorativo: es el
            // unico modo en el que el disparo sigue siendo del jugador y aun asi perdona
            // el error de apuntar.
            const char* mode = (settings_aim_mode == 2) ? si
                             : (settings_aim_mode == 1) ? tr("ASIST.", "ASSIST") : no;
            snprintf(buf, sizeof(buf), "%s%-12s [%s]", prefix, label, mode);
        } else if (row == ROW_SENS) {
            char bar[12];
            for (int b = 0; b < 10; b++) bar[b] = (b < settings_sensitivity) ? '|' : '.';
            bar[10] = '\0';
            snprintf(buf, sizeof(buf), "%s%-12s [%s] %2d", prefix, label, bar, settings_sensitivity);
        } else if (row == ROW_RUMBLE) {
            snprintf(buf, sizeof(buf), "%s%-12s [%s]", prefix, label, settings_rumble ? si : no);
        } else if (row == ROW_RSTICK) {
            snprintf(buf, sizeof(buf), "%s%-12s [%s]", prefix, label, settings_rstick ? si : no);
        } else if (row == ROW_TSIZE || row == ROW_TOPACITY) {
            // Barra como la del volumen: 10 marcas sobre el recorrido del ajuste
            const int v  = (row == ROW_TSIZE) ? settings_touch_size : settings_touch_opacity;
            const int lo = (row == ROW_TSIZE) ? 70 : 30, hi = (row == ROW_TSIZE) ? 150 : 100;
            const int filled = 1 + (v - lo) * 9 / std::max(1, hi - lo);
            char bar[12];
            for (int b = 0; b < 10; b++) bar[b] = (b < filled) ? '|' : '.';
            bar[10] = '\0';
            snprintf(buf, sizeof(buf), "%s%-12s [%s] %3d%%", prefix, label, bar, v);
        } else if (row == ROW_LEFTY) {
            snprintf(buf, sizeof(buf), "%s%-12s [%s]", prefix, label, settings_lefty ? si : no);
        } else if (row == ROW_BATTERY) {
            // Encendido dice lo que cuesta: la pantalla baja a 30 fps (el juego sigue a 60)
            snprintf(buf, sizeof(buf), "%s%-12s [%s]", prefix, label,
                     settings_battery ? tr(" SÍ · 30 FPS ", " ON · 30 FPS ") : no);
        } else if (row == ROW_SKIN) {
            StaffSkin sk = get_staff_skin(static_cast<u32>(settings_skin));
            snprintf(buf, sizeof(buf), "%s%-12s [%s]", prefix, label, tr(sk.name, sk.name_en));
        } else if (row == ROW_LANG) {
            snprintf(buf, sizeof(buf), "%s%-12s [%s]", prefix, label,
                     I18N::language_label(static_cast<Language>(settings_language)));
        } else {
            snprintf(buf, sizeof(buf), "%s%s", prefix, label);
        }
        // En Opciones la fila no es un boton (se ajusta tocando su mitad izquierda o
        // derecha), pero sin una pieza detras tampoco se ve cual esta elegida ni hasta
        // donde llega la zona que responde al dedo.
        const Rect plate = {PX, y - (PH - Renderer::TEXT_LINE_UNITS) * 0.5f,
                            static_cast<f32>(Config::GAME_WIDTH) - 2.0f * PX, PH};
        renderer.draw_panel({plate.x, plate.y + 1.4f, plate.w, plate.h}, {0, 0, 0, 95}, 3.8f);
        renderer.draw_panel(plate, lit ? Color{188, 132, 38, 255}
                                       : Color{46, 40, 37, 224}, 3.8f);
        // La MITAD que hay bajo el dedo, un punto mas clara: es la que va a ajustar, y
        // enseña "izquierda baja, derecha sube" sin gastar una linea de texto en decirlo.
        if (k == down && settings_row_at(i) != ROW_BACK) {
            const bool right = press_pos.x >= Config::GAME_WIDTH * 0.5f;
            renderer.draw_panel({right ? plate.x + plate.w * 0.5f : plate.x, plate.y,
                                 plate.w * 0.5f, plate.h}, {228, 176, 74, 210}, 3.8f);
        }
        renderer.draw_text_centered(buf, y, c);
        mark_row_rect(i, hits[k]);
    }

    // Flechas en el margen izquierdo, como en El Santuario: centradas pisarian el titulo.
    if (win && settings_scroll > 0)
        renderer.draw_text("^", 5.0f, TOP, {180, 160, 120, 210}, 0.7f);
    if (win && settings_scroll + VIS < option_count)
        renderer.draw_text("v", 5.0f, TOP + (VIS - 1) * PITCH, {180, 160, 120, 210}, 0.7f);

    renderer.draw_text_centered(touch_hints ? tr("Toca izquierda/derecha de la fila para ajustar  ·  arrastra para recorrer",
                                                 "Tap the left/right half of a row to adjust  ·  drag to scroll")
                                            : tr("[</>] Ajustar   [ENTER] Volver",
                                                 "[</>] Adjust   [ENTER] Back"),
                                touch_hints ? 174.0f : 168.0f, {100, 90, 80, 180},
                                touch_hints ? 0.62f : 1.0f);
}

void MenuSystem::render_controls(Renderer& renderer) {
    renderer.clear({15, 12, 25, 255});

    renderer.draw_text_centered(tr("CONTROLES", "CONTROLS"), 20, Color::elias_gold(), 2);

    Color label = {200, 180, 140, 255};
    Color value = {140, 130, 120, 255};

    // En un telefono no hay ni una sola de las teclas de abajo: la pantalla de ayuda tiene
    // que contar los gestos que existen de verdad, o es peor que no tenerla.
    // El boton del tutorial es la UNICA fila de esta pantalla (y por eso `option_count` es
    // 1): tocarlo lo juega, y tocar fuera vuelve — que es lo que la pantalla hacia siempre.
    // El rect sale de la misma formula que lo pinta, como en el resto de la capa.
    option_count = 1;
    const char* play = tr("Jugar el tutorial", "Play the tutorial");
    const f32 btn_w = menu_button_width(renderer, play, 0.9f);
    const f32 btn_y = touch_hints ? 150.0f : 152.0f;
    const f32 btn_h = touch_hints ? 20.0f : 14.0f;
    const Rect btn  = {(static_cast<f32>(Config::GAME_WIDTH) - btn_w) * 0.5f, btn_y, btn_w, btn_h};
    const bool down = press_active && btn.contains(press_pos);

    if (touch_hints) {
        // Con el ajuste de zurdo las dos mitades se cambian (Input::in_move_half)
        renderer.draw_text_centered(tr("Moverse", "Move"), 38, label, 0.95f);
        renderer.draw_text_centered(touch_lefty ? tr("Pulgar en la mitad derecha: el stick nace ahí y te sigue",
                                                     "Right half: the stick appears under your thumb and follows it")
                                                : tr("Pulgar en la mitad izquierda: el stick nace ahí y te sigue",
                                                     "Left half: the stick appears under your thumb and follows it"),
                                    48, value, 0.75f);

        renderer.draw_text_centered(tr("Atacar y apuntar", "Attack and aim"), 60, label, 0.95f);
        renderer.draw_text_centered(touch_lefty ? tr("Toca la mitad izquierda; mantén y arrastra para apuntar", "Tap the left half; hold and drag to aim")
                                                : tr("Toca la mitad derecha; mantén y arrastra para apuntar", "Tap the right half; hold and drag to aim"),
                                    70, value, 0.75f);

        renderer.draw_text_centered(tr("Botones", "Buttons"), 82, label, 0.95f);
        renderer.draw_text_centered(tr("DASH en la esquina; USAR · ARMA · ORAR alrededor (los que apliquen)",
                                       "DASH in the corner; USE · WEAPON · PRAY around it (those that apply)"), 92, value, 0.75f);

        renderer.draw_text_centered(tr("Milagros", "Miracles"), 104, label, 0.95f);
        renderer.draw_text_centered(tr("Toca su botón para lanzarlo; mantén para ver su ficha", "Tap its button to cast; hold to read its card"), 114, value, 0.75f);

        renderer.draw_text_centered(tr("Mapa: toca el minimapa  ·  Pausa: II o atrás  ·  Tamaño y zurdo: Opciones",
                                       "Map: tap the minimap  ·  Pause: II or back  ·  Size and left-handed: Options"), 128, value, 0.7f);
        renderer.draw_text_centered(tr("Objetivo: explora, halla al JEFE y cruza su arco", "Goal: explore, find the BOSS, cross its arch"), 140, label, 0.72f);

        draw_menu_button(renderer, btn, play, down, false, 0.9f);
        mark_row_rect(0, {16.0f, btn_y - 4.0f, static_cast<f32>(Config::GAME_WIDTH) - 32.0f, btn_h + 8.0f});
        renderer.draw_text_centered(tr("Toca fuera para volver", "Tap outside to go back"), 176, {100, 90, 80, 180}, 0.7f);
        return;
    }

    renderer.draw_text_centered(tr("Movimiento", "Movement"), 46, label);
    renderer.draw_text_centered(tr("WASD / Flechas", "WASD / Arrows"), 58, value);

    renderer.draw_text_centered(tr("Ataque", "Attack"), 70, label);
    renderer.draw_text_centered(tr("K (mantener) — dispara hacia donde te mueves", "K (hold) — shoots where you move"), 82, value, 0.9f);

    renderer.draw_text_centered(tr("Arma secundaria", "Secondary weapon"), 94, label);
    renderer.draw_text_centered(tr("I / clic derecho — Honda o Vara, si la llevas", "I / right click — Sling or Rod, if you have one"), 106, value, 0.9f);

    renderer.draw_text_centered(tr("Dash: ESPACIO / J     Consumible: R     Orar: P", "Dash: SPACE / J     Consumable: R     Pray: P"), 122, value, 0.8f);
    renderer.draw_text_centered(tr("Milagros: Q / E / F     Mapa: TAB     Pausa: ESC", "Miracles: Q / E / F     Map: TAB     Pause: ESC"), 132, value, 0.8f);
    renderer.draw_text_centered(tr("Objetivo: explora, halla al JEFE y cruza su arco", "Goal: explore, find the BOSS, cross its arch"), 138, label, 0.75f);

    draw_menu_button(renderer, btn, play, true, false, 0.9f);
    mark_row_rect(0, menu_hit_rect(0, btn_y, btn_h + 6.0f, btn_h, btn_w));
    // 170, y no 174: `draw_text` ancla por arriba y una linea a 0,8 ocupa ~9 unidades, asi
    // que a 174 la ultima fila se salia por el filo de las 180 (se vio en la captura).
    renderer.draw_text_centered(tr("[ENTER] jugarlo    [ESC] volver", "[ENTER] play it    [ESC] back"),
                                170, {120, 108, 96, 200}, 0.8f);
}

// =============================================================================
// "¿Quieres el tutorial?" (game/tutorial.h)
// =============================================================================
// Sale al pulsar Nueva Partida, y es lo unico que separa al jugador de su primera sala: dos
// botones y fuera. Se pregunta SIEMPRE, sin bit en el guardado — decision del usuario: quien
// repite sabe decir que no, y un tutorial que se decide solo sorprende a quien lo queria.
void MenuSystem::render_tutorial_ask(Renderer& renderer) {
    const f32 W = static_cast<f32>(Config::GAME_WIDTH);
    const f32 H = static_cast<f32>(Config::GAME_HEIGHT);

    // Mismo fondo que el titulo: se viene de ahi y no hay por que cortar la estampa.
    renderer.clear({10, 8, 16, 255});
    constexpr int BANDS = 30;
    for (int i = 0; i < BANDS; i++) {
        f32 k = 1.0f - static_cast<f32>(i) / BANDS;
        renderer.draw_rect({0, H * i / BANDS, W, H / BANDS + 0.5f},
                           {static_cast<u8>(58 * k), static_cast<u8>(30 * k), static_cast<u8>(34 * k), 255});
    }
    if (title_bg != TEXTURE_NONE)
        renderer.draw_sprite_rect(title_bg, {0, 0, 1280, 720}, {0, 0, W, H}, {255, 255, 255, 255});
    if (renderer.vignette_tex != TEXTURE_NONE)
        renderer.draw_sprite_rect(renderer.vignette_tex, {0, 0, 160, 90}, {-8, -8, W + 16, H + 16}, {0, 0, 0, 165});
    if (title_bg != TEXTURE_NONE && renderer.shadow_tex != TEXTURE_NONE)
        renderer.draw_sprite_rect(renderer.shadow_tex, {0, 0, 64, 32}, {40, 44, 240, 126}, {8, 6, 14, 180});

    // En tactil todo sube cuatro unidades y los botones crecen: el presupuesto vertical
    // aqui es duro por los dos extremos. El orillo del manto de la estampa cruza desde
    // y=146 y se come el texto apagado, y abajo las zonas del dedo de los dos botones
    // ocupan 75..127 — con el reparto de antes ({92, 28, 22}) el pie caia en y=130,
    // DENTRO del panel y encima del rotulo "No, al capitulo 1", letra sobre letra.
    renderer.draw_text_centered(tr("ANTES DE EMPEZAR", "BEFORE YOU BEGIN"),
                                touch_hints ? 38.0f : 40.0f, Color::elias_gold(), 1.3f);
    renderer.draw_text_centered(tr("¿Quieres jugar el tutorial?", "Do you want to play the tutorial?"),
                                touch_hints ? 56.0f : 60.0f, {205, 184, 142, 235}, 0.95f);
    renderer.draw_text_centered(tr("Enseña los controles, la fe, los milagros y las ofrendas",
                                   "It teaches the controls, faith, miracles and offerings"),
                                touch_hints ? 68.0f : 72.0f, {150, 135, 105, 215}, 0.7f);

    option_count = 2;
    const char* options[] = {tr("Sí, enséñame", "Yes, teach me"), tr("No, al capítulo 1", "No, to chapter 1")};
    // Paso 26 y alto 20 (antes 28 y 22): 26 unidades de 180 siguen siendo ~10,4 mm de dedo
    // en un movil de 6,5" apaisado, por encima de los 9 que pide Android.
    draw_button_column(renderer, options, option_count,
                       {94.0f, 17.0f, 13.0f}, {78.0f, 26.0f, 20.0f}, 1.0f);

    // Los dos pies van ARRIBA de donde iban: a 146/160 el orillo del manto de la estampa
    // cruza justo por detras y el texto apagado se perdia encima (se vio en la captura).
    renderer.draw_text_centered(touch_hints ? tr("Se puede dejar a medias cuando quieras",
                                                 "You can quit it whenever you like")
                                            : tr("[W/S]  ·  [ENTER]", "[W/S]  ·  [ENTER]"),
                                touch_hints ? 131.0f : 132.0f, {130, 118, 104, 215}, 0.75f);
    renderer.draw_text_centered(tr("Se repite desde Controles, en el menú",
                                   "Replay it from Controls, in the menu"),
                                touch_hints ? 139.0f : 143.0f, {116, 105, 94, 195}, 0.66f);
}
