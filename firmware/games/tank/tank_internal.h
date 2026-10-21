/*
 * Internal state and helpers of the Tank game. Exposed so that tests can set up exact positions (the C counterpart of the
 * reference tests reaching into the game). Not part of the public API.
 *
 * Coordinates are the donor's: x to the right, y UP from the bottom row of the field. They are turned into screen rows
 * (row 0 = top) only when the field is drawn and for the centre of the explosion.
 */
#ifndef BRICK_TANK_INTERNAL_H
#define BRICK_TANK_INTERNAL_H

#include "brick/games/tank.h"

BK_EXTERN_C_BEGIN

#define BKK_W 10
#define BKK_H 20

#define BKK_ENEMIES          3 /* one variant (ADR-036): donor type 1, three enemy tanks, ready-made fields */
#define BKK_PLAYER_BULLETS   4
#define BKK_KILLS_PER_LEVEL  25
#define BKK_EXTRA_LIFE_AT    500u
#define BKK_MOVE_TICKS       5    /* pause of a direction key after a step: 90 ms */
#define BKK_ROTATE_TICKS     5    /* ... after a turn on the spot (the donor's 360 ms made shorter, docs/GAME_TANK.md) */
#define BKK_FIRE_TICKS       14   /* pause of the gun: 240 ms */
#define BKK_BULLET_TICKS     7    /* every bullet flies one cell per 120 ms */
#define BKK_START_TICKS      90   /* 1500 ms before every level: the player may drive, enemies and guns wait */
#define BKK_CLEAR_LOSE_TICKS 72   /* clear scene after a lost life, 1200 ms */
#define BKK_CLEAR_WIN_TICKS  314  /* clear scene after a won level, 5240 ms */
#define BKK_SPEED_FIRST_MS   1500 /* period of an enemy move on speed 1 and on speed 10 */
#define BKK_SPEED_TENTH_MS   350
#define BKK_PLAYER_START_X   (BKK_W / 2 - 1)
#define BKK_PLAYER_START_Y   (BKK_H / 2 - 3)
#define BKK_TANK_SIZE        3

/* Directions as the donor's RotationAngle: 0 up, 1 right, 2 down, 3 left. */
enum { BKK_UP = 0, BKK_RIGHT = 1, BKK_DOWN = 2, BKK_LEFT = 3 };
/* Tank shapes: the player's (7 cells), an enemy (6 cells), a 3 x 3 box (to test whether a spawn place is free). */
enum { BKK_PLAYER = 0, BKK_ENEMY = 1, BKK_BOX = 2 };

/* Scene kinds. */
#define BKK_SCENE_KABOOM     1u
#define BKK_SCENE_CLEAR_LOSE 2u
#define BKK_SCENE_CLEAR_WIN  3u

typedef struct {
    int8_t x; /* centre */
    int8_t y;
    uint8_t dir;
} bkk_tank_t;

typedef struct {
    int8_t x;
    int8_t y;
    uint8_t dir;
    bool live;
} bkk_bullet_t;

/* Keys with a pause of their own (GameUtils.setKeyDelay): the four directions and the gun. */
enum { BKK_KEY_LEFT = 0, BKK_KEY_RIGHT = 1, BKK_KEY_UP = 2, BKK_KEY_DOWN = 3, BKK_KEY_FIRE = 4, BKK_KEYS = 5 };

typedef struct {
    bk_game_t base; /* must be first */

    bkk_tank_t player;
    bkk_tank_t enemy[BKK_ENEMIES];
    bool enemy_live[BKK_ENEMIES];
    bkk_bullet_t enemy_bullet[BKK_ENEMIES];
    bkk_bullet_t player_bullet[BKK_PLAYER_BULLETS];
    uint8_t obstacles[BK_CELLS]; /* 1: occupied; index y * 10 + x, donor y */

    uint8_t killed;   /* enemies destroyed on this level */
    uint8_t on_board; /* enemies standing on the field now */
    bool dead;        /* the player was hit: explosion and a lost life at the next step */
    uint8_t enemy_index;
    int16_t ai_left;     /* ticks to the next enemy move */
    int16_t start_left;  /* ticks of the start melody left */
    uint8_t bullet_left; /* the shared bullet timer (Countdown) */
    uint8_t key_delay[BKK_KEYS];

    uint8_t kaboom_base[BK_CELLS]; /* the field when the explosion began */
    int8_t kaboom_x;               /* screen cell of the explosion */
    int8_t kaboom_y;
} bkk_game_t;

/* ---- geometry (tank_geometry.c) ---- */

/* Cells of a tank shape turned to `dir`, relative to its centre. Returns how many (7, 6 or 9). */
int bkk_mask(int kind, int dir, int8_t cells[9][2]);
bool bkk_out_of_board(int kind, int dir, int x, int y);
bool bkk_hits_board(const uint8_t board[BK_CELLS], int kind, int dir, int x, int y);
bool bkk_can_place(const uint8_t board[BK_CELLS], int kind, int dir, int x, int y);
void bkk_put_tank(uint8_t board[BK_CELLS], int kind, const bkk_tank_t *tank);
bool bkk_covers(int kind, const bkk_tank_t *tank, int x, int y);
/* TanksGame.moveTank: `board` is the field WITHOUT the tank itself. */
bkk_tank_t bkk_move_tank(const uint8_t board[BK_CELLS], int kind, bkk_tank_t tank, int dir, bool rotation_only);

/* Ready-made obstacles of a level (0 or 1: none) into `board` (cleared first). */
void bkk_preset_obstacles(int level, uint8_t board[BK_CELLS]);

extern const int8_t BKK_DX[4];
extern const int8_t BKK_DY[4];

/* ---- enemy AI (tank_ai.c) ---- */

int bkk_shoot_direction(int x, int y, const bkk_tank_t *player);
void bkk_sort_by_weight(const double weights[4], int out[4]);
int bkk_distance_to_obstacle(const uint8_t board[BK_CELLS], int x, int y, int dir);
/* `board` is the field without the shooting tank, `self` its index among the enemies (the tank itself is not a "friend"). */
double bkk_shot_chance(const uint8_t board[BK_CELLS], const bkk_game_t *s, int self);
void bkk_move_weights(const uint8_t board[BK_CELLS], const bkk_tank_t *tank, const bkk_tank_t *player, double weights[4]);
int bkk_choose_move(const uint8_t board[BK_CELLS], const bkk_tank_t *tank, const bkk_tank_t *player, bk_rng_t *rng);
bool bkk_wants_to_shoot(const uint8_t board[BK_CELLS], const bkk_game_t *s, int self, bk_rng_t *rng);

/* ---- game ---- */

/* Ticks between two enemy moves on a speed (reference genuineTicks): the same IEEE double operations in the same order. */
unsigned bkk_enemy_ticks(int speed);
/* The whole field in donor coordinates: obstacles, tanks (except `skip`, compared by address) and bullets. */
void bkk_build_board(const bkk_game_t *s, const bkk_tank_t *skip, uint8_t board[BK_CELLS]);

BK_EXTERN_C_END

#endif /* BRICK_TANK_INTERNAL_H */
