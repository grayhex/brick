/*
 * Geometry of Tank: tank shapes and their turns, collision tests, the tank's move and the ready-made obstacles of the
 * levels. Port of ApolloBrickGames src/brick/games/tanks/geometry.ts and levels.ts (TankShape, Obstacle and ObstacleUtils
 * of the donor n0Live/BrickGame, MIT). Coordinates are the donor's: y counts UP from the bottom row.
 */
#include <string.h>

#include "tank_internal.h"

const int8_t BKK_DX[4] = {0, 1, 0, -1};
const int8_t BKK_DY[4] = {1, 0, -1, 0};

/* ---------------------------------------------------------------- shapes */

/* TankShape.charactersTable, direction "up"; the barrel is the cell (0, 1). */
static const int8_t PLAYER_UP[7][2] = {{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {0, 0}, {1, 0}, {0, 1}};
static const int8_t ENEMY_UP[6][2] = {{-1, -1}, {1, -1}, {-1, 0}, {0, 0}, {1, 0}, {0, 1}}; /* no cell behind the centre */
static const int8_t BOX_UP[9][2] = {{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {0, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}};

/* Shape.rotateRight / turnAround / rotateLeft of the donor. */
static void turn(int dir, int x, int y, int8_t out[2])
{
    switch (dir) {
    case BKK_RIGHT:
        out[0] = (int8_t)y;
        out[1] = (int8_t)-x;
        break;
    case BKK_DOWN:
        out[0] = (int8_t)-x;
        out[1] = (int8_t)-y;
        break;
    case BKK_LEFT:
        out[0] = (int8_t)-y;
        out[1] = (int8_t)x;
        break;
    default:
        out[0] = (int8_t)x;
        out[1] = (int8_t)y;
        break;
    }
}

int bkk_mask(int kind, int dir, int8_t cells[9][2])
{
    const int8_t(*src)[2] = PLAYER_UP;
    int n = 7;
    if (kind == BKK_ENEMY) {
        src = ENEMY_UP;
        n = 6;
    } else if (kind == BKK_BOX) {
        src = BOX_UP;
        n = 9;
    }
    for (int i = 0; i < n; i++) {
        turn(dir, src[i][0], src[i][1], cells[i]);
    }
    return n;
}

/* Smallest and largest offsets of the shape (every shape and turn spans -1..1, but the code does not rely on it). */
static void bounds(int kind, int dir, int *min_x, int *max_x, int *min_y, int *max_y)
{
    int8_t cells[9][2];
    int n = bkk_mask(kind, dir, cells);
    *min_x = *max_x = cells[0][0];
    *min_y = *max_y = cells[0][1];
    for (int i = 1; i < n; i++) {
        *min_x = cells[i][0] < *min_x ? cells[i][0] : *min_x;
        *max_x = cells[i][0] > *max_x ? cells[i][0] : *max_x;
        *min_y = cells[i][1] < *min_y ? cells[i][1] : *min_y;
        *max_y = cells[i][1] > *max_y ? cells[i][1] : *max_y;
    }
}

static inline bool in_board(int x, int y)
{
    return x >= 0 && x < BKK_W && y >= 0 && y < BKK_H;
}

static inline int cell_index(int x, int y)
{
    return y * BKK_W + x;
}

/* ---------------------------------------------------------------- collisions */

/* checkBoardCollision: does the shape with its centre at (x, y) stick out of the field? */
bool bkk_out_of_board(int kind, int dir, int x, int y)
{
    int min_x, max_x, min_y, max_y;
    bounds(kind, dir, &min_x, &max_x, &min_y, &max_y);
    return x + min_x < 0 || x + max_x >= BKK_W || y + max_y >= BKK_H || y + min_y < 0;
}

/* checkCollision: does the shape touch an occupied cell (cells outside the field are skipped)? */
bool bkk_hits_board(const uint8_t board[BK_CELLS], int kind, int dir, int x, int y)
{
    int8_t cells[9][2];
    int n = bkk_mask(kind, dir, cells);
    for (int i = 0; i < n; i++) {
        int bx = x + cells[i][0];
        int by = y + cells[i][1];
        if (in_board(bx, by) && board[cell_index(bx, by)] != 0) {
            return true;
        }
    }
    return false;
}

bool bkk_can_place(const uint8_t board[BK_CELLS], int kind, int dir, int x, int y)
{
    return !bkk_out_of_board(kind, dir, x, y) && !bkk_hits_board(board, kind, dir, x, y);
}

void bkk_put_tank(uint8_t board[BK_CELLS], int kind, const bkk_tank_t *tank)
{
    int8_t cells[9][2];
    int n = bkk_mask(kind, tank->dir, cells);
    for (int i = 0; i < n; i++) {
        int bx = tank->x + cells[i][0];
        int by = tank->y + cells[i][1];
        if (in_board(bx, by)) {
            board[cell_index(bx, by)] = 1;
        }
    }
}

/* checkTwoShapeCollision with a one-cell bullet: does the tank's drawing cover (x, y)? */
bool bkk_covers(int kind, const bkk_tank_t *tank, int x, int y)
{
    int8_t cells[9][2];
    int n = bkk_mask(kind, tank->dir, cells);
    for (int i = 0; i < n; i++) {
        if (tank->x + cells[i][0] == x && tank->y + cells[i][1] == y) {
            return true;
        }
    }
    return false;
}

bkk_tank_t bkk_move_tank(const uint8_t board[BK_CELLS], int kind, bkk_tank_t tank, int dir, bool rotation_only)
{
    int x = tank.x;
    int y = tank.y;
    if (rotation_only || tank.dir != dir) {
        /* the turned shape would touch something: a step forward with the turn */
        if (bkk_hits_board(board, kind, dir, x, y)) {
            x += BKK_DX[dir];
            y += BKK_DY[dir];
        }
    } else {
        x += BKK_DX[dir];
        y += BKK_DY[dir];
    }
    if (bkk_out_of_board(kind, dir, x, y) || bkk_hits_board(board, kind, dir, x, y)) {
        return tank;
    }
    bkk_tank_t moved = {(int8_t)x, (int8_t)y, (uint8_t)dir};
    return moved;
}

/* ---------------------------------------------------------------- obstacles */

/* Obstacle.charactersTable: 0 a single cell, 1 a corner, 2 a block of 3 x 2. */
static const int8_t OBSTACLE_SQUARE[1][2] = {{0, 0}};
static const int8_t OBSTACLE_CORNER[3][2] = {{1, 0}, {0, 0}, {0, 1}};
static const int8_t OBSTACLE_BLOCK[6][2] = {{0, 0}, {1, 0}, {2, 0}, {0, 1}, {1, 1}, {2, 1}};

/* The cells of an obstacle turned and shifted so that the smallest x and y are zero (Obstacle.setType). */
static int obstacle_cells(int type, int dir, int8_t out[6][2])
{
    const int8_t(*src)[2] = OBSTACLE_SQUARE;
    int n = 1;
    if (type == 1) {
        src = OBSTACLE_CORNER;
        n = 3;
    } else if (type == 2) {
        src = OBSTACLE_BLOCK;
        n = 6;
    }
    int min_x = 0;
    int min_y = 0;
    for (int i = 0; i < n; i++) {
        turn(dir, src[i][0], src[i][1], out[i]);
        min_x = (i == 0 || out[i][0] < min_x) ? out[i][0] : min_x;
        min_y = (i == 0 || out[i][1] < min_y) ? out[i][1] : min_y;
    }
    for (int i = 0; i < n; i++) {
        out[i][0] = (int8_t)(out[i][0] - min_x);
        out[i][1] = (int8_t)(out[i][1] - min_y);
    }
    return n;
}

typedef struct {
    uint8_t type;
    uint8_t dir;
    uint8_t x; /* lower left corner of the obstacle */
    uint8_t y;
} placed_t;

#define O(type, dir, x, y)                                                                                                  \
    {                                                                                                                       \
        type, dir, x, y                                                                                                     \
    }
#define LEVEL_END                                                                                                           \
    {                                                                                                                       \
        0xff, 0, 0, 0                                                                                                       \
    }

/* ObstacleUtils.tanksObstacles: ready-made fields by level (0 and 1 have none). */
static const placed_t LEVEL_2[] = {O(1, BKK_UP, 1, 3), O(1, BKK_LEFT, 6, 3), O(2, BKK_LEFT, 8, 8), LEVEL_END};
static const placed_t LEVEL_3[] = {O(1, BKK_RIGHT, 1, 15), O(1, BKK_DOWN, 6, 15), O(2, BKK_LEFT, 0, 9), LEVEL_END};
static const placed_t LEVEL_4[] = {O(1, BKK_RIGHT, 1, 15), O(1, BKK_LEFT, 6, 3), O(2, BKK_UP, 3, 11), LEVEL_END};
static const placed_t LEVEL_5[] = {O(1, BKK_UP, 1, 3), O(1, BKK_DOWN, 6, 15), LEVEL_END};
static const placed_t LEVEL_6[] = {O(1, BKK_RIGHT, 1, 15), O(1, BKK_DOWN, 6, 15), O(2, BKK_LEFT, 0, 9), O(2, BKK_LEFT, 8, 9),
                                   LEVEL_END};
static const placed_t LEVEL_7[] = {O(1, BKK_UP, 1, 3),   O(1, BKK_LEFT, 6, 3), O(1, BKK_DOWN, 6, 15),
                                   O(2, BKK_LEFT, 0, 8), O(2, BKK_UP, 3, 11),  LEVEL_END};
static const placed_t LEVEL_8[] = {O(1, BKK_UP, 1, 3), O(1, BKK_LEFT, 6, 3), O(1, BKK_RIGHT, 1, 15), O(2, BKK_UP, 3, 11),
                                   LEVEL_END};
static const placed_t LEVEL_9[] = {O(1, BKK_UP, 1, 3),
                                   O(1, BKK_RIGHT, 1, 15),
                                   O(1, BKK_DOWN, 6, 15),
                                   O(2, BKK_LEFT, 0, 9),
                                   O(2, BKK_LEFT, 8, 9),
                                   O(2, BKK_UP, 3, 11),
                                   LEVEL_END};
static const placed_t LEVEL_10[] = {O(1, BKK_UP, 1, 3),
                                    O(1, BKK_LEFT, 6, 3),
                                    O(1, BKK_RIGHT, 1, 15),
                                    O(1, BKK_DOWN, 6, 15),
                                    O(2, BKK_LEFT, 0, 8),
                                    O(2, BKK_UP, 3, 11),
                                    LEVEL_END};

static const placed_t *const LEVELS[11] = {NULL,    NULL,    LEVEL_2, LEVEL_3, LEVEL_4, LEVEL_5,
                                           LEVEL_6, LEVEL_7, LEVEL_8, LEVEL_9, LEVEL_10};

void bkk_preset_obstacles(int level, uint8_t board[BK_CELLS])
{
    memset(board, 0, BK_CELLS);
    if (level < 0 || level > 10 || LEVELS[level] == NULL) {
        return;
    }
    for (const placed_t *p = LEVELS[level]; p->type != 0xff; p++) {
        int8_t cells[6][2];
        int n = obstacle_cells(p->type, p->dir, cells);
        for (int i = 0; i < n; i++) {
            int bx = p->x + cells[i][0];
            int by = p->y + cells[i][1];
            if (in_board(bx, by)) {
                board[cell_index(bx, by)] = 1;
            }
        }
    }
}
