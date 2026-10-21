#!/usr/bin/env python3
"""Mutation run for the Tank port (docs/CONFORMANCE.md section 5): break ONE thing at a time in the C sources, rebuild the
given test targets, run them; a mutant is KILLED when a test fails. A survivor is either an equivalent mutant (it does not
change behaviour; listed in EQUIVALENT with the reason) or a hole in the tests.

    cmake -S . -B build -G Ninja && cmake --build build
    python3 tools/mutation/mutate_tank.py test_tank_conformance test_tank_rules      # about 8 minutes
    CHECK_ONLY=1 python3 tools/mutation/mutate_tank.py                                 # are all patterns still found (no build)?
    ONLY="weights" python3 tools/mutation/mutate_tank.py test_tank_conformance       # only the mutants whose name contains it

The sources under firmware/games/tank/ are rewritten during the run and restored afterwards (also on Ctrl-C): run it on a
tree without uncommitted edits to those files. BUILD=dir selects the build directory (default: build).
"""
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = str(Path(__file__).resolve().parents[2])
BUILD = os.path.join(ROOT, os.environ.get('BUILD', 'build'))
FILES = {
  'tank': ROOT + '/firmware/games/tank/tank.c',
  'ai': ROOT + '/firmware/games/tank/tank_ai.c',
  'geo': ROOT + '/firmware/games/tank/tank_geometry.c',
  'hdr': ROOT + '/firmware/games/tank/tank_internal.h',
}

# Equivalent mutants: the behaviour does not change, so no test can (or should) tell them apart.
EQUIVALENT = {
    'move delay: swapped': 'MOVE_TICKS and ROTATE_TICKS are both 5 (the donor has 90 / 360 ms; the project shortened the turn)',
    'ms_to_ticks rounding': 'the ten enemy periods are 90 - 23k/3 ticks (thirds, never a half) and every fixed time is a whole number of ticks',
    'shoot dir: vertical sign': 'that branch is taken only when |dy| > |dx| >= 0, so dy is never 0',
    'friend: self counts': 'is_tank_ahead already answers false for the same centre, so the extra check of the tank itself is redundant',
    'shot roll <': 'the chance is a multiple of 1/13 (never a dyadic fraction), so the roll u32 / 2^32 never equals it',
}

M = []
def m(name, f, old, new, count=1):
    """old: the text to replace (exactly once) or a compiled regular expression (robust against clang-format)."""
    M.append((name, f, old, new, count))
def const(name, macro, value):
    """Change the value of a #define of tank_internal.h whatever its alignment."""
    m(name, 'hdr', re.compile(r'(#define\s+BKK_' + macro + r'\s+)\S+'), r'\g<1>' + value)
def found(src, old):
    return old.search(src) is not None if isinstance(old, re.Pattern) else old in src
def mutate(src, old, new, count):
    return old.sub(new, src, count=count) if isinstance(old, re.Pattern) else src.replace(old, new, count)

# ---- constants
const('MOVE_TICKS 5->4', 'MOVE_TICKS', '4')
const('ROTATE_TICKS 5->6', 'ROTATE_TICKS', '6')
const('FIRE_TICKS 14->13', 'FIRE_TICKS', '13')
const('BULLET_TICKS 7->8', 'BULLET_TICKS', '8')
const('START_TICKS 90->89', 'START_TICKS', '89')
const('CLEAR_WIN 314->313', 'CLEAR_WIN_TICKS', '313')
const('CLEAR_LOSE 72->71', 'CLEAR_LOSE_TICKS', '71')
const('KILLS 25->24', 'KILLS_PER_LEVEL', '24')
const('EXTRA_LIFE 500->400', 'EXTRA_LIFE_AT', '400u')
const('PLAYER_BULLETS 4->3', 'PLAYER_BULLETS', '3')
const('SPEED_FIRST 1500->1400', 'SPEED_FIRST_MS', '1400')
const('SPEED_TENTH 350->360', 'SPEED_TENTH_MS', '360')
m('PLAYER_START_Y -1', 'hdr', re.compile(r'(#define\s+BKK_PLAYER_START_Y\s+)\(BKK_H / 2 - 3\)'), r'\g<1>(BKK_H / 2 - 2)')
# ---- bullets
m('shot event for enemies too', 'tank', '    if (by_player) { /* only the player', '    if (true) { /* only the player')
m('player bullet kills player', 'tank', 'if (hit_player && !by_player) {', 'if (hit_player) {')
m('enemy bullet destroys enemy', 'tank', '            if (by_player) {\n                destroy_enemy(s, hit_enemy);', '            if (true) {\n                destroy_enemy(s, hit_enemy);')
m('obstacle not erased', 'tank', '            s->obstacles[cell] = 0;\n        }\n    }\n    return true;', '            (void)cell;\n        }\n    }\n    return true;')
m('player bullet removes enemy bullet branch', 'tank', '} else if (player_shot >= 0 && !by_player) {', '} else if (player_shot >= 0) {')
m('no hit event', 'tank', '    bk_game_emit(&s->base, BK_EV_HIT, 0, 0, 0);\n    bullet->live = false;', '    bullet->live = false;')
m('enemy bullet meeting skipped', 'tank', '    if (enemy_shot >= 0) {\n        s->enemy_bullet[enemy_shot].live = false;', '    if (false) {\n        s->enemy_bullet[enemy_shot].live = false;')
m('score per kill 2', 'tank', 'bk_game_add_score(&s->base, 1);', 'bk_game_add_score(&s->base, 2);')
m('kill counter not incremented', 'tank', '    s->killed++;', '    (void)0;')
m('on_board not decremented', 'tank', '    s->on_board--;', '    (void)0;')
m('no explode event', 'tank', '    bk_game_emit(&s->base, BK_EV_EXPLODE, 0, 0, 0);', '    (void)0;')
m('bullets: player first', 'tank', '''    for (int i = 0; i < BKK_ENEMIES; i++) {
        if (!flight_of_bullet(s, false, i)) {
            return;
        }
    }
    for (int i = 0; i < BKK_PLAYER_BULLETS; i++) {
        if (!flight_of_bullet(s, true, i)) {
            return;
        }
    }''', '''    for (int i = 0; i < BKK_PLAYER_BULLETS; i++) {
        if (!flight_of_bullet(s, true, i)) {
            return;
        }
    }
    for (int i = 0; i < BKK_ENEMIES; i++) {
        if (!flight_of_bullet(s, false, i)) {
            return;
        }
    }''')
m('hit on player does not stop the rest', 'tank', '''        if (!flight_of_bullet(s, false, i)) {
            return;
        }''', '''        if (!flight_of_bullet(s, false, i)) {
            continue;
        }''')
m('bullet leaves field not removed', 'tank', '    if (!in_board(x, y)) {\n        bullet->live = false;\n        return true;', '    if (!in_board(x, y)) {\n        return true;')
m('fire: no immediate step', 'tank', '    slots[index].live = true;\n    flight_of_bullet(s, by_player, index);', '    slots[index].live = true;')
m('fire: last free slot', 'tank', '''        if (!slots[i].live) {
            index = i;
            break;
        }''', '''        if (!slots[i].live) {
            index = i;
        }''')
m('bullet_at ignores live', 'tank', 'if (list[i].live && list[i].x == x && list[i].y == y) {', 'if (list[i].x == x && list[i].y == y) {')
m('enemy_covering ignores live', 'tank', 'if (s->enemy_live[i] && bkk_covers(BKK_ENEMY, &s->enemy[i], x, y)) {', 'if (bkk_covers(BKK_ENEMY, &s->enemy[i], x, y)) {')
# ---- enemies
m('spawn chance inverted', 'tank', 'if (bk_rng_int(&s->base.rng, 4) == 0 || !respawn(s, index)) {', 'if (bk_rng_int(&s->base.rng, 4) != 0 || !respawn(s, index)) {')
m('newcomer shot chance', 'tank', '(uint32_t)(10 + 11 - s->base.level)', '(uint32_t)(10 + 10 - s->base.level)')
m('newcomer shoots always when born', 'tank', 'bkk_tank_t next = (shot == move && !born_with_gun)', 'bkk_tank_t next = (shot == move)')
m('shoot only on the move', 'tank', 'if (born_with_gun || bkk_wants_to_shoot(board, s, index, &s->base.rng)) {', 'if (born_with_gun) {')
m('cap off by one', 'tank', 'if (s->killed + s->on_board >= BKK_KILLS_PER_LEVEL) {', 'if (s->killed + s->on_board > BKK_KILLS_PER_LEVEL) {')
m('respawn tries 3->4', 'tank', '    int tries = 3;\n    do {\n        uint32_t p', '    int tries = 4;\n    do {\n        uint32_t p')
m('respawn tries 3->2', 'tank', '    int tries = 3;\n    do {\n        uint32_t p', '    int tries = 2;\n    do {\n        uint32_t p')
m('respawn: no x shift', 'tank', '        if (x + min_x < 0) {\n            x -= min_x;', '        if (false) {\n            x -= min_x;')
m('respawn: no y shift', 'tank', '        if (y + min_y < 0) {\n            y -= min_y;', '        if (false) {\n            y -= min_y;')
m('respawn: box test off', 'tank', '} while (bkk_hits_board(board, BKK_BOX, BKK_UP, x, y) && tries > 0);', '} while (false);')
m('respawn dir = up', 'tank', 's->enemy[index].dir = (uint8_t)bkk_shoot_direction(x, y, &s->player);', 's->enemy[index].dir = BKK_UP;')
m('spawn point table', 'tank', '{0, 0}, {BKK_W - 1, 0}, {0, BKK_H / 2}', '{0, 0}, {BKK_W - 1, 0}, {0, BKK_H / 2 + 1}')
m('on_board not incremented', 'tank', '        s->on_board++;\n        if (bk_rng_int', '        if (bk_rng_int')
m('round robin no wrap', 'tank', '        if (s->enemy_index >= BKK_ENEMIES) {\n            s->enemy_index = 0;', '        if (s->enemy_index >= BKK_ENEMIES + 5) {\n            s->enemy_index = 0;')
m('round robin tries', 'tank', '    int tries = BKK_ENEMIES;\n    bool done;', '    int tries = 1;\n    bool done;')
m('round robin does not advance', 'tank', 'done = ai_turn(s, s->enemy_index++);', 'done = ai_turn(s, s->enemy_index);')
m('enemy_index kept on restart', 'tank', '    s->enemy_index = 0;\n    load_new_level(s);', '    load_new_level(s);')
# ---- level / round
m('ai_left init 1->2', 'tank', '    s->ai_left = 1;\n}', '    s->ai_left = 2;\n}')
m('start_left not reset', 'tank', '    s->start_left = BKK_START_TICKS; /* the enemies wait', '    /* s->start_left = BKK_START_TICKS; the enemies wait')
m('bullet timer not reset', 'tank', '    s->bullet_left = BKK_BULLET_TICKS;\n    memset(s->key_delay', '    memset(s->key_delay')
m('key delays not reset', 'tank', '    memset(s->key_delay, 0, sizeof s->key_delay);\n    s->start_left', '    s->start_left')
m('bullets not cleared on level', 'tank', '    memset(s->player_bullet, 0, sizeof s->player_bullet);\n    memset(s->enemy_bullet', '    memset(s->enemy_bullet')
m('reload resets kills', 'tank', '        init_level(s); /* the same level again', '        load_new_level(s); /* the same level again')
m('win: kills not reset', 'tank', '        bk_game_next_level(g);\n        load_new_level(s);', '        bk_game_next_level(g);\n        init_level(s);')
m('win: no next level', 'tank', '        bk_game_next_level(g);\n        load_new_level(s);', '        load_new_level(s);')
m('win: no event', 'tank', '    bk_game_emit(&s->base, BK_EV_WIN, 0, 0, 0);\n', '')
m('win scene length', 'tank', 'bk_game_cutscene(&s->base, BKK_SCENE_CLEAR_WIN, BKK_CLEAR_WIN_TICKS);', 'bk_game_cutscene(&s->base, BKK_SCENE_CLEAR_WIN, BKK_CLEAR_WIN_TICKS / 2);')
m('lose: no crash event', 'tank', '    bk_game_emit(g, BK_EV_CRASH, 0, 0, 0);\n    tank_draw', '    tank_draw')
m('kaboom y not flipped', 'tank', 's->kaboom_y = (int8_t)(BKK_H - 1 - s->player.y);', 's->kaboom_y = s->player.y;')
m('kaboom base after', 'tank', '    tank_draw(g, s->kaboom_base); /* the explosion paints', '    memset(s->kaboom_base, 0, BK_CELLS); /* the explosion paints')
m('scene frame: lose clear length', 'tank', 'bk_game_paint_clear(g, BKK_CLEAR_LOSE_TICKS, elapsed);', 'bk_game_paint_clear(g, BKK_CLEAR_WIN_TICKS, elapsed);')
m('lives at start', 'tank', 'bk_game_set_lives(g, BK_MAX_LIVES);', 'bk_game_set_lives(g, BK_MAX_LIVES - 1);')
m('extra life threshold not set', 'tank', '    g->scores_to_extra_life = BKK_EXTRA_LIFE_AT;', '    (void)0;')
# ---- keys
m('delay decrement removed', 'tank', '        if (s->key_delay[i] > 0) {\n            s->key_delay[i]--;', '        if (false) {\n            s->key_delay[i]--;')
m('press does not reset delay', 'tank', '        if ((in->pressed & MOVE_KEYS[i].bit) != 0u) {\n            s->key_delay[MOVE_KEYS[i].key] = 0;', '        if (false) {\n            s->key_delay[MOVE_KEYS[i].key] = 0;')
m('fire during intro', 'tank', 'if (s->start_left == 0 && fire_held', 'if (fire_held')
m('fire ignores key delay', 'tank', '&& s->key_delay[BKK_KEY_FIRE] == 0 && fire(s, &s->player, true)', '&& fire(s, &s->player, true)')
m('fire key only ROTATE', 'tank', '(in->held & (BK_KEY_ROTATE | BK_KEY_FIRE)) != 0u', '(in->held & BK_KEY_ROTATE) != 0u')
m('fire key only FIRE', 'tank', '(in->held & (BK_KEY_ROTATE | BK_KEY_FIRE)) != 0u', '(in->held & BK_KEY_FIRE) != 0u')
m('move delay: swapped', 'tank', 's->key_delay[key] = same ? BKK_MOVE_TICKS : BKK_ROTATE_TICKS;', 's->key_delay[key] = same ? BKK_ROTATE_TICKS : BKK_MOVE_TICKS;')
m('move order reversed', 'tank', '''    {BKK_KEY_LEFT, BK_KEY_LEFT, BKK_LEFT},
    {BKK_KEY_RIGHT, BK_KEY_RIGHT, BKK_RIGHT},''', '''    {BKK_KEY_RIGHT, BK_KEY_RIGHT, BKK_RIGHT},
    {BKK_KEY_LEFT, BK_KEY_LEFT, BKK_LEFT},''')
m('move: no break (all keys act)', 'tank', '            move_player(s, MOVE_KEYS[i].dir);\n            break;', '            move_player(s, MOVE_KEYS[i].dir);')
m('move_player: own tank not skipped', 'tank', 'bkk_build_board(s, &s->player, board);\n    s->player = bkk_move_tank', 'bkk_build_board(s, NULL, board);\n    s->player = bkk_move_tank')
m('intro blocks driving', 'tank', '    process_keys(s, in);\n    if (s->start_left > 0) {', '    if (s->start_left == 0) {\n        process_keys(s, in);\n    }\n    if (s->start_left > 0) {')
m('intro keeps bullets moving', 'tank', '    if (s->start_left > 0) {\n        s->start_left--;\n        return;', '    if (s->start_left > 0) {\n        s->start_left--;\n        if (s->start_left > 0) {\n            return;\n        }')
m('dead check after win', 'tank', '''    if (s->dead) {
        lose_life(s);
        return;
    }
    if (s->killed >= BKK_KILLS_PER_LEVEL) {
        win_level(s);
        return;
    }''', '''    if (s->killed >= BKK_KILLS_PER_LEVEL) {
        win_level(s);
        return;
    }
    if (s->dead) {
        lose_life(s);
        return;
    }''')
m('bullet timer period', 'tank', '        s->bullet_left = BKK_BULLET_TICKS;\n        flight_of_bullets(s);', '        s->bullet_left = BKK_BULLET_TICKS - 1;\n        flight_of_bullets(s);')
m('no second death check', 'tank', '    ai_round(s);\n    if (s->dead) {\n        lose_life(s);\n    }', '    ai_round(s);')
m('ai_left reload', 'tank', 's->ai_left = (int16_t)bkk_enemy_ticks(g->speed);', 's->ai_left = (int16_t)bkk_enemy_ticks(g->level);')
m('build_board: no player bullets', 'tank', '    for (int i = 0; i < BKK_PLAYER_BULLETS; i++) {\n        if (s->player_bullet[i].live && in_board', '    for (int i = 0; i < 0; i++) {\n        if (s->player_bullet[i].live && in_board')
m('build_board: no enemy bullets', 'tank', '    for (int i = 0; i < BKK_ENEMIES; i++) {\n        if (s->enemy_bullet[i].live && in_board', '    for (int i = 0; i < 0; i++) {\n        if (s->enemy_bullet[i].live && in_board')
m('build_board: skip ignored', 'tank', 'if (&s->player != skip) {', 'if (true) {')
m('draw not flipped', 'tank', 'cells[(BKK_H - 1 - y) * BKK_W + x] = BK_CELL_ON;', 'cells[y * BKK_W + x] = BK_CELL_ON;')
m('ms_to_ticks rounding', 'tank', 'return x - (double)i >= 0.5 ? i + 1 : i;', 'return x - (double)i > 0.5 ? i + 1 : i;')
# ---- AI
m('shoot dir: <=', 'ai', 'if (abs_int(x - player->x) < abs_int(y - player->y)) {', 'if (abs_int(x - player->x) <= abs_int(y - player->y)) {')
m('shoot dir: vertical sign', 'ai', 'return y - player->y > 0 ? BKK_DOWN : BKK_UP;', 'return y - player->y >= 0 ? BKK_DOWN : BKK_UP;')
m('shoot dir: horizontal sign', 'ai', 'return x - player->x > 0 ? BKK_LEFT : BKK_RIGHT;', 'return x - player->x >= 0 ? BKK_LEFT : BKK_RIGHT;')
m('sort: first pass >=', 'ai', 'if (weights[out[i]] > weights[out[i - 1]]) {', 'if (weights[out[i]] >= weights[out[i - 1]]) {')
m('sort: second pass <=', 'ai', 'if (weights[out[i]] < weights[out[i + 1]]) {', 'if (weights[out[i]] <= weights[out[i + 1]]) {')
m('distance: <=', 'ai', 'for (int i = 1; i < distance; i++) {', 'for (int i = 1; i <= distance - 1 && i < 3; i++) {')
m('tank ahead: half', 'ai', 'const int half = BKK_TANK_SIZE / 2;', 'const int half = 0;')
m('tank ahead: same cell', 'ai', 'if (sx == other->x && sy == other->y) {\n        return false;', 'if (false) {\n        return false;')
m('tank ahead: sign LEFT', 'ai', '(other->x - sx) * (view == BKK_LEFT ? -1 : 1) <= distance', '(other->x - sx) * (view == BKK_LEFT ? 1 : 1) <= distance')
m('tank ahead: sign DOWN', 'ai', '(other->y - sy) * (view == BKK_DOWN ? -1 : 1) <= distance', '(other->y - sy) * (view == BKK_DOWN ? 1 : 1) <= distance')
m('friend: dead friends count', 'ai', 'if (s->enemy_live[i] && i != self && is_tank_ahead', 'if (i != self && is_tank_ahead')
m('friend: self counts', 'ai', 'if (s->enemy_live[i] && i != self && is_tank_ahead', 'if (s->enemy_live[i] && is_tank_ahead')
m('shot rate 13->12', 'ai', '#define MAX_SHOT_DISTANCE 9', '#define MAX_SHOT_DISTANCE 8')
m('chance floor removed', 'ai', '    chance = chance > 0.0 ? chance : 0.0;', '    (void)chance;')
m('facing bonus: horizontal', 'ai', '(horizontal < 0 && player->dir == BKK_LEFT) ||', '(horizontal < 0 && player->dir == BKK_RIGHT) ||')
m('facing bonus: vertical', 'ai', '(vertical < 0 && player->dir == BKK_DOWN) || (vertical > 0 && player->dir == BKK_UP)', '(vertical < 0 && player->dir == BKK_UP) || (vertical > 0 && player->dir == BKK_UP)')
m('friend penalty', 'ai', 'chance -= rate * 2.0;', 'chance -= rate * 1.0;')
m('point blank', 'ai', 'chance += rate * 10.0; /* point blank */', 'chance += rate * 9.0; /* point blank */')
m('point blank limit', 'ai', '} else if (nearer <= BKK_TANK_SIZE / 2) {', '} else if (nearer < BKK_TANK_SIZE / 2) {')
m('obstacle range', 'ai', 'if (found > 0 && found <= 5) {', 'if (found > 0 && found <= 4) {')
m('obstacle bonus', 'ai', 'chance += rate * (double)(9 - found);', 'chance += rate * (double)(8 - found);')
m('chance clamp low', 'ai', 'double low = chance > rate ? chance : rate;', 'double low = chance;')
m('chance clamp high', 'ai', 'double high = 1.0 - rate;', 'double high = 1.0;')
m('shot roll <', 'ai', 'return roll <= bkk_shot_chance(board, s, self);', 'return roll < bkk_shot_chance(board, s, self);')
m('weights base', 'ai', '        weights[i] = 0.25;', '        weights[i] = 0.5;')
m('weights blocked', 'ai', 'weights[dir] -= 0.5;\n            blocked[dir] = true;', 'weights[dir] -= 0.25;\n            blocked[dir] = true;')
m('weights far blocked', 'ai', '                weights[dir] -= 0.25;\n            }\n        } else {', '                weights[dir] -= 0.0;\n            }\n        } else {')
m('weights free bonus', 'ai', '        } else {\n            weights[dir] += 0.25;', '        } else {\n            weights[dir] += 0.0;')
m('weights far step uses 2', 'ai', 'tank->x + BKK_DX[dir] * BKK_TANK_SIZE,', 'tank->x + BKK_DX[dir] * 2,')
m('weights current dir', 'ai', '    if (!blocked[tank->dir]) {\n        weights[tank->dir] += 0.25;', '    if (true) {\n        weights[tank->dir] += 0.25;')
m('weights horizontal pref', 'ai', 'int preferred_horizontal = horizontal < 0 ? BKK_RIGHT : BKK_LEFT;', 'int preferred_horizontal = horizontal <= 0 ? BKK_RIGHT : BKK_LEFT;')
m('weights vertical pref', 'ai', 'int preferred_vertical = vertical < 0 ? BKK_UP : BKK_DOWN;', 'int preferred_vertical = vertical <= 0 ? BKK_UP : BKK_DOWN;')
m('weights nearer axis', 'ai', 'bool horizontal_is_nearer = abs_int(horizontal) < abs_int(vertical);', 'bool horizontal_is_nearer = abs_int(horizontal) <= abs_int(vertical);')
m('weights horizontal bonus', 'ai', '(horizontal_is_nearer && !blocked[preferred_horizontal]) ? 0.5 : 0.25', '(horizontal_is_nearer && !blocked[preferred_horizontal]) ? 0.5 : 0.5')
m('weights vertical bonus', 'ai', '(!horizontal_is_nearer && !blocked[preferred_vertical]) ? 0.5 : 0.25', '(!horizontal_is_nearer && !blocked[preferred_vertical]) ? 0.25 : 0.25')
m('weights zero-axis skip', 'ai', '    if (horizontal != 0) {\n        weights[preferred_horizontal]', '    if (true) {\n        weights[preferred_horizontal]')
m('weights cross course', 'ai', 'weights[(player->dir == BKK_LEFT || player->dir == BKK_RIGHT) ? preferred_vertical : preferred_horizontal] -= 0.25;', 'weights[(player->dir == BKK_LEFT || player->dir == BKK_RIGHT) ? preferred_horizontal : preferred_vertical] -= 0.25;')
m('choose cap 0.95', 'ai', 'chance = chance < 0.95 ? chance : 0.95;', 'chance = chance < 1.0 ? chance : 1.0;')
m('choose floor', 'ai', 'double chance = w > 0.0 ? w : 0.0;', 'double chance = w > 0.0 ? w : 0.05;')
m('choose roll <', 'ai', 'if (roll <= chance) {\n            return ordered[i];', 'if (roll < chance) {\n            return ordered[i];')
m('choose fallback', 'ai', '    return ordered[0];\n}', '    return ordered[3];\n}')
# ---- geometry
m('enemy mask: back cell', 'geo', 'static const int8_t ENEMY_UP[6][2] = {{-1, -1}, {1, -1}, {-1, 0}', 'static const int8_t ENEMY_UP[6][2] = {{-1, -1}, {0, -1}, {-1, 0}')
m('player mask barrel', 'geo', '{1, 0}, {0, 1}};\nstatic const int8_t ENEMY_UP', '{1, 0}, {0, 0}};\nstatic const int8_t ENEMY_UP')
m('turn right', 'geo', '        out[0] = (int8_t)y;\n        out[1] = (int8_t)-x;', '        out[0] = (int8_t)-y;\n        out[1] = (int8_t)x;')
m('turn down', 'geo', '        out[0] = (int8_t)-x;\n        out[1] = (int8_t)-y;', '        out[0] = (int8_t)-x;\n        out[1] = (int8_t)y;')
m('out_of_board upper', 'geo', 'y + max_y >= BKK_H', 'y + max_y > BKK_H')
m('out_of_board left', 'geo', 'x + min_x < 0 ||', 'x + min_x < -1 ||')
m('hits_board ignores outside', 'geo', 'if (in_board(bx, by) && board[cell_index(bx, by)] != 0) {\n            return true;', 'if (board[(by < 0 ? 0 : by) % BK_ROWS * BKK_W + (bx < 0 ? 0 : bx) % BKK_W] != 0) {\n            return true;')
m('covers: wrong cell', 'geo', 'if (tank->x + cells[i][0] == x && tank->y + cells[i][1] == y) {', 'if (tank->x + cells[i][0] == x && tank->y + cells[i][1] == y + 0 && i != 0) {')
m('move: ladder off', 'geo', 'if (bkk_hits_board(board, kind, dir, x, y)) {\n            x += BKK_DX[dir];', 'if (false) {\n            x += BKK_DX[dir];')
m('move: turn also steps', 'geo', '    if (rotation_only || tank.dir != dir) {', '    if (rotation_only) {')
m('move: rotation_only ignored', 'geo', '    if (rotation_only || tank.dir != dir) {', '    if (tank.dir != dir) {')
m('move: edge check off', 'geo', '    if (bkk_out_of_board(kind, dir, x, y) || bkk_hits_board(board, kind, dir, x, y)) {\n        return tank;', '    if (bkk_hits_board(board, kind, dir, x, y)) {\n        return tank;')
m('level 5 table', 'geo', 'LEVEL_5[] = {O(1, BKK_UP, 1, 3), O(1, BKK_DOWN, 6, 15)', 'LEVEL_5[] = {O(1, BKK_UP, 1, 3), O(1, BKK_DOWN, 6, 14)')
m('level 10 table', 'geo', re.compile(r'(LEVEL_10\[\][^;]*O\(2, BKK_UP, 3, )11'), r'\g<1>12')
m('obstacle block', 'geo', '{{0, 0}, {1, 0}, {2, 0}, {0, 1}, {1, 1}, {2, 1}};', '{{0, 0}, {1, 0}, {2, 0}, {0, 1}, {1, 1}, {2, 0}};')
m('obstacle corner', 'geo', '{{1, 0}, {0, 0}, {0, 1}}', '{{1, 0}, {0, 0}, {1, 1}}')
m('obstacle shift', 'geo', '        out[i][0] = (int8_t)(out[i][0] - min_x);', '        out[i][0] = (int8_t)(out[i][0] - 0);')

def run(cmd, **kw):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True, **kw)

targets = sys.argv[1:] or ['test_tank_conformance']
if os.environ.get('CHECK_ONLY'):  # only check that every pattern is still found in the sources (after a refactoring or clang-format)
    stale = [name for name, f, old, new, count in M if not found(open(FILES[f]).read(), old)]
    print(f'{len(M)} mutants, {len(stale)} with a stale pattern')
    for name in stale:
        print('  STALE:', name)
    sys.exit(1 if stale else 0)
orig = {k: open(v).read() for k, v in FILES.items()}
killed, survived, invalid = [], [], []
only = os.environ.get('ONLY')
try:
    for name, f, old, new, count in M:
        if only and only not in name: continue
        src = orig[f]
        if not found(src, old):
            invalid.append((name, 'pattern not found')); continue
        for k, v in FILES.items(): open(v, 'w').write(orig[k])
        open(FILES[f], 'w').write(mutate(src, old, new, count))
        b = run(f'cmake --build {BUILD} --target ' + ' '.join(targets) + ' 2>&1')
        if b.returncode != 0:
            invalid.append((name, 'build failed: ' + [l for l in b.stdout.splitlines() if 'error' in l][:1].__repr__())); continue
        dead = False
        for t in targets:
            fixture = {'test_tank_conformance': ' ' + ROOT + '/tests/fixtures/tank/tank.conformance.json'}.get(t, '')
            if run(f'cd {ROOT} && {BUILD}/tests/{t}{fixture} >/dev/null 2>&1').returncode != 0:
                dead = True
                break
        (killed if dead else survived).append(name)
finally:
    for k, v in FILES.items(): open(v, 'w').write(orig[k])
print(f'killed {len(killed)}, survived {len(survived)}, invalid {len(invalid)}')
for n in survived: print('  SURVIVED:', n, '(equivalent: ' + EQUIVALENT[n] + ')' if n in EQUIVALENT else '(NOT documented as equivalent: a hole in the tests)')
for n, why in invalid: print('  INVALID :', n, '-', why)
sys.exit(0 if all(n in EQUIVALENT for n in survived) and not invalid else 1)
