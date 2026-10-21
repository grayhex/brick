/*
 * The enemy tanks' decisions: where to drive, where and when to shoot. Port of ApolloBrickGames
 * src/brick/games/tanks/ai.ts (getDirectionForMovement, getDirectionForShoot, checkForShot, isTankAhead,
 * distanceToObstacle and descShakerSort of the donor n0Live/BrickGame, MIT): the same weights, thresholds and order of the
 * dice, in the same IEEE double operations. Build with -ffp-contract=off (docs/CONFORMANCE.md).
 */
#include "tank_internal.h"

/* No libc here: the simulator builds the core freestanding for WebAssembly. */
static inline int abs_int(int v)
{
    return v < 0 ? -v : v;
}

/* Direction in which an enemy shoots at the player: along the larger of the two coordinate differences. */
int bkk_shoot_direction(int x, int y, const bkk_tank_t *player)
{
    if (abs_int(x - player->x) < abs_int(y - player->y)) {
        return y - player->y > 0 ? BKK_DOWN : BKK_UP;
    }
    return x - player->x > 0 ? BKK_LEFT : BKK_RIGHT;
}

/* Directions by descending weight: the donor's "shaker" sort (the order of equal weights is the donor's). */
void bkk_sort_by_weight(const double weights[4], int out[4])
{
    out[0] = BKK_UP;
    out[1] = BKK_RIGHT;
    out[2] = BKK_DOWN;
    out[3] = BKK_LEFT;
    int left = 0;
    int right = 3;
    do {
        for (int i = right; i > left; i--) {
            if (weights[out[i]] > weights[out[i - 1]]) {
                int t = out[i];
                out[i] = out[i - 1];
                out[i - 1] = t;
            }
        }
        left++;
        for (int i = left; i < right; i++) {
            if (weights[out[i]] < weights[out[i + 1]]) {
                int t = out[i];
                out[i] = out[i + 1];
                out[i + 1] = t;
            }
        }
        right--;
    } while (left <= right);
}

/* Distance from (x, y) to the nearest occupied cell in direction `dir` (1: the next cell), -1 if there is none. */
int bkk_distance_to_obstacle(const uint8_t board[BK_CELLS], int x, int y, int dir)
{
    const int distances[4] = {BKK_H - y, BKK_W - x, y, x}; /* up, right, down, left */
    int distance = distances[dir];
    for (int i = 1; i < distance; i++) {
        if (board[(y + i * BKK_DY[dir]) * BKK_W + (x + i * BKK_DX[dir])] != 0) {
            return i;
        }
    }
    return -1;
}

/*
 * Does `other` stand in direction `view` from (sx, sy) within `distance` (isTankAhead)? The donor passes the distance with
 * its sign (the difference of the enemy's and the player's coordinates), so everything behind the shooter inside a
 * tank-wide strip also counts as "ahead"; this behaviour of the donor is kept as it is.
 */
static bool is_tank_ahead(int sx, int sy, int distance, int view, const bkk_tank_t *other)
{
    if (sx == other->x && sy == other->y) {
        return false;
    }
    const int half = BKK_TANK_SIZE / 2;
    if (view == BKK_RIGHT || view == BKK_LEFT) {
        return abs_int(other->y - sy) <= half && (other->x - sx) * (view == BKK_LEFT ? -1 : 1) <= distance;
    }
    return abs_int(other->x - sx) <= half && (other->y - sy) * (view == BKK_DOWN ? -1 : 1) <= distance;
}

static bool is_friend_ahead(const bkk_game_t *s, int self, int distance, int view)
{
    const bkk_tank_t *tank = &s->enemy[self];
    for (int i = 0; i < BKK_ENEMIES; i++) {
        if (s->enemy_live[i] && i != self && is_tank_ahead(tank->x, tank->y, distance, view, &s->enemy[i])) {
            return true;
        }
    }
    return false;
}

#define MIN_SHOT_DISTANCE 4
#define MAX_SHOT_DISTANCE 9

double bkk_shot_chance(const uint8_t board[BK_CELLS], const bkk_game_t *s, int self)
{
    const double rate = 1.0 / (double)(MIN_SHOT_DISTANCE + MAX_SHOT_DISTANCE);
    const bkk_tank_t *tank = &s->enemy[self];
    const bkk_tank_t *player = &s->player;
    int horizontal = tank->x - player->x;
    int vertical = tank->y - player->y;
    int nearer = abs_int(horizontal) < abs_int(vertical) ? abs_int(horizontal) : abs_int(vertical);
    int farther = abs_int(horizontal) > abs_int(vertical) ? abs_int(horizontal) : abs_int(vertical);

    double chance = 1.0 - (double)(nearer + farther - 1) * rate;
    chance = chance > 0.0 ? chance : 0.0;
    if ((horizontal < 0 && player->dir == BKK_LEFT) || (horizontal > 0 && player->dir == BKK_RIGHT) ||
        (vertical < 0 && player->dir == BKK_DOWN) || (vertical > 0 && player->dir == BKK_UP)) {
        chance += rate; /* the player drives towards the enemy */
    }

    int shot = bkk_shoot_direction(tank->x, tank->y, player);
    int distance = (shot == BKK_LEFT || shot == BKK_RIGHT) ? horizontal : vertical;
    if (is_friend_ahead(s, self, distance, shot)) {
        chance -= rate * 2.0;
    } else if (nearer <= BKK_TANK_SIZE / 2) {
        chance += rate * 10.0; /* point blank */
    } else {
        int found = bkk_distance_to_obstacle(board, tank->x, tank->y, shot);
        if (found > 0 && found <= 5) {
            chance += rate * (double)(9 - found);
        }
    }
    double low = chance > rate ? chance : rate;
    double high = 1.0 - rate;
    return low < high ? low : high;
}

bool bkk_wants_to_shoot(const uint8_t board[BK_CELLS], const bkk_game_t *s, int self, bk_rng_t *rng)
{
    double roll = (double)bk_rng_next_u32(rng) / 4294967296.0;
    return roll <= bkk_shot_chance(board, s, self);
}

void bkk_move_weights(const uint8_t board[BK_CELLS], const bkk_tank_t *tank, const bkk_tank_t *player, double weights[4])
{
    bool blocked[4] = {false, false, false, false};
    for (int i = 0; i < 4; i++) {
        weights[i] = 0.25;
    }
    int horizontal = tank->x - player->x;
    int vertical = tank->y - player->y;
    bool horizontal_is_nearer = abs_int(horizontal) < abs_int(vertical);
    int preferred_horizontal = horizontal < 0 ? BKK_RIGHT : BKK_LEFT;
    int preferred_vertical = vertical < 0 ? BKK_UP : BKK_DOWN;

    for (int dir = 0; dir < 4; dir++) {
        /* the near point is a step of one cell, the far one a whole tank length */
        if (!bkk_can_place(board, BKK_ENEMY, dir, tank->x + BKK_DX[dir], tank->y + BKK_DY[dir])) {
            weights[dir] -= 0.5;
            blocked[dir] = true;
            if (!bkk_can_place(board, BKK_ENEMY, dir, tank->x + BKK_DX[dir] * BKK_TANK_SIZE,
                               tank->y + BKK_DY[dir] * BKK_TANK_SIZE)) {
                weights[dir] -= 0.25;
            }
        } else {
            weights[dir] += 0.25;
        }
    }
    if (!blocked[tank->dir]) {
        weights[tank->dir] += 0.25;
    }

    /* towards the player: along the shorter axis the weight grows more when the way is not closed */
    if (horizontal != 0) {
        weights[preferred_horizontal] += (horizontal_is_nearer && !blocked[preferred_horizontal]) ? 0.5 : 0.25;
    }
    if (vertical != 0) {
        weights[preferred_vertical] += (!horizontal_is_nearer && !blocked[preferred_vertical]) ? 0.5 : 0.25;
    }
    /* not across the player's course */
    weights[(player->dir == BKK_LEFT || player->dir == BKK_RIGHT) ? preferred_vertical : preferred_horizontal] -= 0.25;
}

int bkk_choose_move(const uint8_t board[BK_CELLS], const bkk_tank_t *tank, const bkk_tank_t *player, bk_rng_t *rng)
{
    double weights[4];
    int ordered[4];
    bkk_move_weights(board, tank, player, weights);
    bkk_sort_by_weight(weights, ordered);
    for (int i = 0; i < 4; i++) {
        double w = weights[ordered[i]];
        double chance = w > 0.0 ? w : 0.0;
        chance = chance < 0.95 ? chance : 0.95;
        double roll = (double)bk_rng_next_u32(rng) / 4294967296.0;
        if (roll <= chance) {
            return ordered[i];
        }
    }
    return ordered[0];
}
