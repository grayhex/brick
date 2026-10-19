#include "brick/console.h"

#include <string.h>

#include "brick/font.h"

/* ---------------------------------------------------------------- helpers */

static const bk_game_def_t *current_def(const bk_console_t *c)
{
    return c->catalog[c->game_index].def;
}

static unsigned wrap(int value, unsigned count)
{
    int m = (int)count;
    return (unsigned)(((value % m) + m) % m);
}

uint8_t bk_buttons_to_keys(bk_buttons_t b)
{
    uint8_t k = 0;
    if (b & BK_BTN_LEFT) {
        k |= BK_KEY_LEFT;
    }
    if (b & BK_BTN_RIGHT) {
        k |= BK_KEY_RIGHT;
    }
    if (b & BK_BTN_UP) {
        k |= BK_KEY_UP;
    }
    if (b & BK_BTN_DOWN) {
        k |= BK_KEY_DOWN;
    }
    if (b & BK_BTN_ACTION) {
        k |= BK_KEY_ROTATE; /* the one big action button */
    }
    return k;
}

static void emit(bk_console_t *c, bk_event_type_t type)
{
    bk_events_push(&c->events, type, 0, 0, 0);
}

static void store_profile(bk_console_t *c)
{
    if (bk_save_store(&c->hal->storage, &c->profile) != BK_SAVE_OK) {
        c->save_errors++;
    }
}

static void enter_screen(bk_console_t *c, bk_screen_t screen)
{
    c->screen = screen;
    c->screen_ticks = 0;
    memset(c->repeat, 0, sizeof c->repeat);
}

static void apply_light(bk_console_t *c)
{
    if (c->hal->system.set_frontlight != NULL) {
        c->hal->system.set_frontlight(c->hal->system.ctx, c->profile.settings.light_level);
    }
}

static void cycle_light(bk_console_t *c, int delta)
{
    c->profile.settings.light_level = (uint8_t)wrap(c->profile.settings.light_level + delta, BK_LIGHT_MAX + 1u);
    apply_light(c);
    store_profile(c);
}

/* ---------------------------------------------------------------- init */

bool bk_console_init(bk_console_t *c, const bk_catalog_entry_t *catalog, size_t count, const bk_hal_t *hal,
                     unsigned cell_dots)
{
    if (catalog == NULL || count == 0 || count > BK_MAX_GAMES || hal == NULL) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        const bk_game_def_t *d = catalog[i].def;
        if (d == NULL || d->create == NULL || d->mode_count == 0 || d->state_size > sizeof c->game_mem.bytes ||
            d->state_align > sizeof(uint64_t) || catalog[i].glyph == NULL) {
            return false;
        }
    }
    memset(c, 0, sizeof *c);
    if (!bk_layout_init(&c->layout, cell_dots)) {
        return false;
    }
    c->catalog = catalog;
    c->catalog_count = count;
    c->hal = hal;

    bk_save_load(&hal->storage, &c->profile);
    bk_audio_init(&c->audio, &hal->audio, c->profile.settings.sound_on != 0);
    apply_light(c);

    /* restore the menu position */
    c->game_index = c->profile.settings.last_game < count ? c->profile.settings.last_game : 0;
    c->mode = c->profile.settings.last_mode < current_def(c)->mode_count ? c->profile.settings.last_mode : 0;
    c->speed = c->profile.settings.last_speed;
    c->level = c->profile.settings.last_level;
    c->focus = BK_MENU_SPEED;

    c->seed_base = hal->system.entropy != NULL ? hal->system.entropy(hal->system.ctx) : BK_DEFAULT_SEED;
    bk_frame_init(&c->frame, c->layout.width, c->layout.height);
    bk_frame_init(&c->shown, c->layout.width, c->layout.height);
    enter_screen(c, BK_SCREEN_SPLASH);
    return true;
}

/* ---------------------------------------------------------------- HOME */

static void start_game(bk_console_t *c)
{
    const bk_game_def_t *def = current_def(c);
    bk_game_options_t o;
    memset(&o, 0, sizeof o);
    c->parties++;
    o.seed = c->seed_base + c->parties * 0x9e3779b1u;
    o.hi_score = bk_profile_get_record(&c->profile, def->uid, c->mode);
    o.speed = (uint8_t)(c->speed + 1u);
    o.level = (uint8_t)(c->level + 1u);
    o.mode = c->mode;
    c->game = def->create(c->game_mem.bytes, &o);

    bk_settings_t *s = &c->profile.settings;
    if (s->last_game != c->game_index || s->last_mode != c->mode || s->last_speed != c->speed || s->last_level != c->level) {
        s->last_game = (uint8_t)c->game_index;
        s->last_mode = c->mode;
        s->last_speed = c->speed;
        s->last_level = c->level;
        store_profile(c);
    }
    bk_game_start(c->game, &c->events);
    c->last_phase = c->game->phase;
    enter_screen(c, BK_SCREEN_GAME);
}

static void home_change_value(bk_console_t *c, int delta)
{
    const bk_game_def_t *def = current_def(c);
    switch (c->focus) {
    case BK_MENU_MODE:
        if (def->mode_count > 1) {
            c->mode = (uint16_t)wrap((int)c->mode + delta, def->mode_count);
            emit(c, BK_EV_MOVE);
        }
        break;
    case BK_MENU_SPEED:
        c->speed = (uint8_t)wrap((int)c->speed + delta, BK_MENU_VALUE_MAX + 1u);
        emit(c, BK_EV_MOVE);
        break;
    default:
        c->level = (uint8_t)wrap((int)c->level + delta, BK_MENU_VALUE_MAX + 1u);
        emit(c, BK_EV_MOVE);
        break;
    }
}

static void home_step(bk_console_t *c, bk_buttons_t pressed, bk_buttons_t held)
{
    bk_input_t in = bk_input_make(bk_buttons_to_keys(pressed), bk_buttons_to_keys(held));
    bool up = bk_repeat_fire(&c->repeat[0], BK_KEY_UP, &in, BK_MENU_REPEAT_DELAY, BK_MENU_REPEAT_INTERVAL);
    bool down = bk_repeat_fire(&c->repeat[1], BK_KEY_DOWN, &in, BK_MENU_REPEAT_DELAY, BK_MENU_REPEAT_INTERVAL);
    bool left = bk_repeat_fire(&c->repeat[2], BK_KEY_LEFT, &in, BK_MENU_REPEAT_DELAY, BK_MENU_REPEAT_INTERVAL);
    bool right = bk_repeat_fire(&c->repeat[3], BK_KEY_RIGHT, &in, BK_MENU_REPEAT_DELAY, BK_MENU_REPEAT_INTERVAL);

    if (pressed & (BK_BTN_ACTION | BK_BTN_FN1)) {
        start_game(c);
        return;
    }
    if (pressed & BK_BTN_FN3) {
        c->cursor = 0;
        c->confirm_clear = false;
        emit(c, BK_EV_UI_CONFIRM);
        enter_screen(c, BK_SCREEN_SETTINGS);
        return;
    }
    if ((left || right) && c->catalog_count > 1) {
        c->game_index = (uint8_t)wrap((int)c->game_index + (right ? 1 : -1), (unsigned)c->catalog_count);
        c->mode = 0; /* the variant starts from the first one when the game changes */
        if (c->focus == BK_MENU_MODE && current_def(c)->mode_count <= 1) {
            c->focus = BK_MENU_SPEED;
        }
        emit(c, BK_EV_MOVE);
    }
    if (pressed & BK_BTN_FN2) {
        do {
            c->focus = (uint8_t)((c->focus + 1u) % 3u);
        } while (c->focus == BK_MENU_MODE && current_def(c)->mode_count <= 1);
        emit(c, BK_EV_MOVE);
    }
    if (up) {
        home_change_value(c, +1);
    }
    if (down) {
        home_change_value(c, -1);
    }
}

/* ---------------------------------------------------------------- SETTINGS and friends */

static void settings_activate(bk_console_t *c, int delta)
{
    bk_settings_t *s = &c->profile.settings;
    switch (c->cursor) {
    case BK_SET_SOUND:
        s->sound_on = s->sound_on ? 0 : 1;
        bk_audio_set_enabled(&c->audio, s->sound_on != 0);
        store_profile(c);
        emit(c, BK_EV_UI_CONFIRM); /* audible only when sound has just been switched on */
        break;
    case BK_SET_LIGHT:
        cycle_light(c, delta);
        emit(c, BK_EV_UI_CONFIRM);
        break;
    case BK_SET_RECORDS:
        if (!c->confirm_clear) {
            c->confirm_clear = true;
            emit(c, BK_EV_MOVE);
        } else {
            bk_profile_clear_records(&c->profile);
            store_profile(c);
            c->confirm_clear = false;
            emit(c, BK_EV_UI_CONFIRM);
        }
        break;
    case BK_SET_BUTTONS:
        emit(c, BK_EV_UI_CONFIRM);
        enter_screen(c, BK_SCREEN_BUTTON_TEST);
        break;
    default:
        emit(c, BK_EV_UI_CONFIRM);
        enter_screen(c, BK_SCREEN_ABOUT);
        break;
    }
}

static void settings_step(bk_console_t *c, bk_buttons_t pressed, bk_buttons_t held)
{
    bk_input_t in = bk_input_make(bk_buttons_to_keys(pressed), bk_buttons_to_keys(held));
    bool up = bk_repeat_fire(&c->repeat[0], BK_KEY_UP, &in, BK_MENU_REPEAT_DELAY, BK_MENU_REPEAT_INTERVAL);
    bool down = bk_repeat_fire(&c->repeat[1], BK_KEY_DOWN, &in, BK_MENU_REPEAT_DELAY, BK_MENU_REPEAT_INTERVAL);

    if (pressed & BK_BTN_FN3) {
        emit(c, BK_EV_UI_BACK);
        enter_screen(c, BK_SCREEN_HOME);
        return;
    }
    if (up || down) {
        c->cursor = (uint8_t)wrap((int)c->cursor + (down ? 1 : -1), BK_SET_COUNT);
        c->confirm_clear = false;
        emit(c, BK_EV_MOVE);
    }
    if (pressed & BK_BTN_LEFT) {
        settings_activate(c, -1);
    } else if (pressed & (BK_BTN_RIGHT | BK_BTN_ACTION | BK_BTN_FN1)) {
        settings_activate(c, +1);
    }
}

static void button_test_step(bk_console_t *c, bk_buttons_t held)
{
    const bk_buttons_t chord = BK_BTN_FN1 | BK_BTN_FN2 | BK_BTN_FN3;
    if ((held & chord) == chord) {
        emit(c, BK_EV_UI_BACK);
        enter_screen(c, BK_SCREEN_SETTINGS);
    }
}

static void about_step(bk_console_t *c, bk_buttons_t pressed)
{
    if (pressed & (BK_BTN_FN3 | BK_BTN_ACTION | BK_BTN_FN1)) {
        emit(c, BK_EV_UI_BACK);
        enter_screen(c, BK_SCREEN_SETTINGS);
    }
}

/* ---------------------------------------------------------------- GAME */

/* Keep the best score of this game and mode in the profile (also when leaving before game over). */
static void commit_record(bk_console_t *c)
{
    if (c->game != NULL && bk_profile_set_record(&c->profile, current_def(c)->uid, c->game->mode, c->game->hi_score)) {
        store_profile(c);
    }
}

static void game_step(bk_console_t *c, bk_buttons_t pressed, bk_buttons_t held)
{
    bk_game_t *g = c->game;
    bk_buttons_t consumed = 0;

    if (pressed & BK_BTN_FN2) {
        cycle_light(c, +1);
    }

    bool play = (pressed & BK_BTN_FN1) != 0u || ((pressed & BK_BTN_ACTION) != 0u && g->phase != BK_PHASE_PLAYING);
    if (play) {
        consumed |= BK_BTN_ACTION;
        if (g->phase == BK_PHASE_PLAYING) {
            bk_game_pause(g, &c->events);
        } else if (g->phase == BK_PHASE_PAUSED) {
            bk_game_resume(g, &c->events);
        } else if (g->phase == BK_PHASE_OVER || g->phase == BK_PHASE_READY) {
            c->parties++;
            bk_game_start(g, &c->events); /* a fresh party of the same game; the RNG simply continues */
        }
    } else if (pressed & BK_BTN_FN3) {
        if (g->phase == BK_PHASE_PLAYING) {
            bk_game_pause(g, &c->events);
        } else {
            commit_record(c);
            c->game = NULL;
            emit(c, BK_EV_UI_BACK);
            enter_screen(c, BK_SCREEN_HOME);
            return;
        }
    }

    if (g->phase == BK_PHASE_PLAYING) {
        bk_input_t in = bk_input_make(bk_buttons_to_keys((bk_buttons_t)(pressed & ~consumed)),
                                      bk_buttons_to_keys((bk_buttons_t)(held & ~consumed)));
        bk_game_step(g, &in, &c->events);
    }

    if (g->phase == BK_PHASE_OVER && c->last_phase != BK_PHASE_OVER) {
        commit_record(c);
    }
    c->last_phase = g->phase;
}

/* ---------------------------------------------------------------- step */

void bk_console_step(bk_console_t *c, bk_buttons_t held)
{
    bk_buttons_t pressed = bk_edge_update(&c->edge, held);
    c->held = held;
    c->ticks++;
    c->screen_ticks++;
    bk_events_clear(&c->events);

    switch (c->screen) {
    case BK_SCREEN_SPLASH:
        if (c->screen_ticks >= BK_SPLASH_TICKS || pressed != 0u) {
            enter_screen(c, BK_SCREEN_HOME);
        }
        break;
    case BK_SCREEN_HOME:
        home_step(c, pressed, held);
        break;
    case BK_SCREEN_SETTINGS:
        settings_step(c, pressed, held);
        break;
    case BK_SCREEN_BUTTON_TEST:
        button_test_step(c, held);
        break;
    case BK_SCREEN_ABOUT:
        about_step(c, pressed);
        break;
    case BK_SCREEN_GAME:
        game_step(c, pressed, held);
        break;
    }

    const bool in_menu = c->screen != BK_SCREEN_GAME;
    for (unsigned i = 0; i < c->events.count; i++) {
        bk_audio_play(&c->audio, bk_audio_cue_for_event(&c->events.items[i], in_menu));
    }
    bk_audio_tick(&c->audio);
}

/* ---------------------------------------------------------------- rendering */

static void put_cells(uint8_t *field, const char *const *rows, int nrows, int x0, int y0, uint8_t value)
{
    for (int r = 0; r < nrows; r++) {
        for (int col = 0; rows[r][col] != '\0'; col++) {
            int x = x0 + col;
            int y = y0 + r;
            if (rows[r][col] == '#' && x >= 0 && x < BK_COLS && y >= 0 && y < BK_ROWS) {
                field[y * BK_COLS + x] = value;
            }
        }
    }
}

/* Classic Brick Game menu art on the 10 x 20 matrix: big game number / letter, two-digit mode number, illustration. */
static void draw_home_field(const bk_console_t *c, uint8_t *field)
{
    const bk_catalog_entry_t *e = &c->catalog[c->game_index];
    memset(field, 0, BK_CELLS);

    const char *g = e->glyph;
    if (g[0] != '\0' && g[1] == '\0') {
        const char *const *big = bk_big_glyph(g[0]);
        if (big != NULL) {
            put_cells(field, big, BK_BIG_H, 4, 0, BK_CELL_ON); /* columns 4-7, as on the photographed original */
        }
    } else if (g[0] != '\0') {
        const char *const *a = bk_big_glyph(g[0]);
        const char *const *b = bk_big_glyph(g[1]);
        if (a != NULL) {
            put_cells(field, a, BK_BIG_H, 1, 0, BK_CELL_ON);
        }
        if (b != NULL) {
            put_cells(field, b, BK_BIG_H, 6, 0, BK_CELL_ON);
        }
    }

    unsigned number = (unsigned)c->mode + 1u; /* shown from 1, two digits */
    uint8_t value = c->focus == BK_MENU_MODE ? BK_CELL_BLINK : BK_CELL_ON;
    const char *const *tens = bk_small_digit((int)((number / 10u) % 10u));
    const char *const *units = bk_small_digit((int)(number % 10u));
    if (tens != NULL) {
        put_cells(field, tens, BK_GLYPH_H, 2, 8, value);
    }
    if (units != NULL) {
        put_cells(field, units, BK_GLYPH_H, 7, 8, value);
    }

    if (e->def->thumbnail != NULL && e->def->thumb_frames > 0) {
        uint8_t frame = 0;
        if (e->def->thumb_frame_ticks > 0) {
            frame = (uint8_t)((c->screen_ticks / e->def->thumb_frame_ticks) % e->def->thumb_frames);
        }
        uint8_t art[BK_COLS * 5];
        e->def->thumbnail(frame, art);
        memcpy(&field[15 * BK_COLS], art, sizeof art);
    }
}

static char *fmt_uint(char *dst, unsigned v)
{
    char tmp[12];
    int n = 0;
    do {
        tmp[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v != 0u);
    while (n > 0) {
        *dst++ = tmp[--n];
    }
    *dst = '\0';
    return dst;
}

static void render_text_screen(bk_console_t *c)
{
    static const char *const light_names[BK_LIGHT_MAX + 1] = {"OFF", "LOW", "HIGH"};
    static const char *const button_names[8] = {"UP", "DOWN", "LEFT", "RIGHT", "ACTION", "FN1", "FN2", "FN3"};
    static const bk_button_t button_bits[8] = {BK_BTN_UP,     BK_BTN_DOWN, BK_BTN_LEFT, BK_BTN_RIGHT,
                                               BK_BTN_ACTION, BK_BTN_FN1,  BK_BTN_FN2,  BK_BTN_FN3};
    bk_text_row_t rows[BK_SET_COUNT + 4];
    char line[6][20];

    if (c->screen == BK_SCREEN_SETTINGS) {
        rows[BK_SET_SOUND].label = "SOUND";
        rows[BK_SET_SOUND].value = c->profile.settings.sound_on ? "ON" : "OFF";
        rows[BK_SET_LIGHT].label = "LIGHT";
        rows[BK_SET_LIGHT].value = light_names[c->profile.settings.light_level];
        rows[BK_SET_RECORDS].label = "RECORDS";
        rows[BK_SET_RECORDS].value = c->confirm_clear ? "SURE?" : "CLEAR";
        rows[BK_SET_BUTTONS].label = "BUTTONS";
        rows[BK_SET_BUTTONS].value = "TEST";
        rows[BK_SET_ABOUT].label = "ABOUT";
        rows[BK_SET_ABOUT].value = NULL;
        bk_render_text_page(&c->layout, "SETTINGS", rows, BK_SET_COUNT, c->cursor, "UP DOWN  OK FN1  BACK FN3", &c->frame);
    } else if (c->screen == BK_SCREEN_BUTTON_TEST) {
        for (int i = 0; i < 8; i++) {
            rows[i].label = button_names[i];
            rows[i].value = (c->held & button_bits[i]) ? "ON" : "--";
        }
        bk_render_text_page(&c->layout, "BUTTONS", rows, 8, -1, "EXIT FN1+FN2+FN3", &c->frame);
    } else {
        char *p = line[0];
        strcpy(p, "BRICK");
        rows[0].label = line[0];
        rows[0].value = BK_VERSION_STRING;
        strcpy(line[1], "CELL ");
        char *end = fmt_uint(line[1] + 5, c->layout.cell);
        strcpy(end, " DOTS");
        rows[1].label = line[1];
        rows[1].value = NULL;
        rows[2].label = "PROTOTYPE";
        rows[2].value = NULL;
        rows[3].label = "NO HW APPROVED";
        rows[3].value = NULL;
        bk_render_text_page(&c->layout, "ABOUT", rows, 4, -1, "BACK FN3", &c->frame);
    }
}

const bk_frame_t *bk_console_render(bk_console_t *c)
{
    if (c->screen == BK_SCREEN_SETTINGS || c->screen == BK_SCREEN_BUTTON_TEST || c->screen == BK_SCREEN_ABOUT) {
        render_text_screen(c);
        return &c->frame;
    }

    bk_scene_t *scene = &c->scene;
    memset(scene, 0, sizeof *scene);
    scene->sound_on = c->profile.settings.sound_on != 0;
    scene->tick = c->ticks;

    if (c->screen == BK_SCREEN_SPLASH) {
        /* power-on test pattern: every dot group lit, as on the classic toys (also shows dead pixels) */
        scene->over_fill = BK_ROWS;
        scene->hi_score = scene->score = 8888888u;
        scene->show_score = true;
        scene->speed = scene->level = 8;
        scene->paused = true;
        scene->has_preview = true;
        memset(scene->preview, BK_CELL_ON, sizeof scene->preview);
    } else if (c->screen == BK_SCREEN_HOME) {
        const bk_game_def_t *def = current_def(c);
        draw_home_field(c, scene->cells);
        scene->hi_score = bk_profile_get_record(&c->profile, def->uid, c->mode);
        scene->speed = c->speed;
        scene->level = c->level;
        scene->focus =
            c->focus == BK_MENU_SPEED ? BK_FOCUS_SPEED : (c->focus == BK_MENU_LEVEL ? BK_FOCUS_LEVEL : BK_FOCUS_NONE);
        scene->caption1 = def->title;
        c->label[0] = '\0';
        if (def->mode_label != NULL) {
            def->mode_label(c->mode, c->label, sizeof c->label);
        }
        scene->caption2 = c->label[0] != '\0' ? c->label : NULL;
    } else {
        const bk_game_t *g = c->game;
        bk_game_cells(g, scene->cells);
        scene->has_preview = bk_game_preview(g, scene->preview);
        scene->score = g->score;
        scene->hi_score = g->hi_score;
        scene->show_score = true;
        scene->speed = (uint8_t)(g->speed > 0 ? g->speed - 1 : 0);
        scene->level = (uint8_t)(g->level > 0 ? g->level - 1 : 0);
        scene->over_fill = g->over_fill;
        scene->paused = g->phase == BK_PHASE_PAUSED;
        if (g->phase == BK_PHASE_OVER) {
            scene->over_fill = BK_ROWS;
        }
    }
    bk_render_scene(&c->layout, scene, &c->frame);
    return &c->frame;
}

void bk_console_tick(bk_console_t *c)
{
    bk_buttons_t held = 0;
    if (c->hal->input.read_buttons != NULL) {
        held = c->hal->input.read_buttons(c->hal->input.ctx);
    }
    bk_console_step(c, held);
    bk_console_render(c);
    if (c->hal->display.present != NULL && (!c->shown_valid || !bk_frame_equal(&c->frame, &c->shown))) {
        c->hal->display.present(c->hal->display.ctx, &c->frame);
        memcpy(c->shown.bits, c->frame.bits, bk_frame_bytes(&c->frame));
        c->shown.width = c->frame.width;
        c->shown.height = c->frame.height;
        c->shown.stride = c->frame.stride;
        c->shown_valid = true;
    }
}
