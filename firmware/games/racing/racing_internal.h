/*
 * Internal state of the Racing game. Exposed so that tests can set up exact positions (the C counterpart of the reference
 * tests reaching into the game). Not part of the public API.
 */
#ifndef BRICK_RACING_INTERNAL_H
#define BRICK_RACING_INTERNAL_H

#include "brick/games/racing.h"

BK_EXTERN_C_BEGIN

#define BKR_LANES         2  /* one variant only (ADR-036): two lanes, normal field */
#define BKR_MAX_OPPONENTS 16 /* generous: at most a few groups fit on the 20 rows plus the entrance */
#define BKR_LEVELUP_POINT 50u
#define BKR_EXTRA_LIFE_AT 2000u
#define BKR_INTRO_TICKS   90 /* 1500 ms of the "start melody" before every round, no input */
#define BKR_STEER_TICKS   5  /* auto-repeat of a lane change: 90 ms */
#define BKR_ACCEL_TICKS   2  /* auto-repeat of the acceleration: 30 ms */
#define BKR_CAR_Y         1  /* centre of the player's car (donor y counted from the bottom of the road) */
#define BKR_CAR_MIN_Y     (-1)
#define BKR_CAR_MAX_Y     2
#define BKR_BORDER_SPAN   5
#define BKR_BORDER_LIT    3

/* Scene kinds: the start melody before a round, the explosion, the clear animation after a lost life. */
#define BKR_SCENE_INTRO  1u
#define BKR_SCENE_KABOOM 2u
#define BKR_SCENE_CLEAR  3u

typedef struct {
    int8_t x; /* centre column */
    int8_t y; /* centre row, donor order: counted from the bottom of the road */
} bkr_car_t;

typedef struct {
    bk_game_t base; /* must be first */

    uint8_t lane; /* the player's lane index and its centre */
    int8_t car_x;
    uint16_t since_step; /* ticks since the last road step */
    bool odd_move;       /* the border moves every step, the cars every second one */
    uint8_t border_phase;
    uint8_t opp_count;
    bkr_car_t opp[BKR_MAX_OPPONENTS]; /* ascending y: the last is the farthest */
    bool tail_valid; /* centre of the last group while it is on the road (the donor's opponents.getLast()) */
    int16_t tail_y;

    uint32_t steer_left; /* Repeater counters */
    uint32_t steer_right;
    uint32_t accel_up;
    uint32_t accel_rotate;

    uint8_t kaboom_base[BK_CELLS]; /* the field when the explosion began */
    int8_t kaboom_x;               /* screen cell of the explosion */
    int8_t kaboom_y;
} bkr_game_t;

/* Centres of the two lanes, three cells apart. */
extern const uint8_t BKR_LANE_X[BKR_LANES];

/* Ticks between road steps (reference halfStepTicks): the same IEEE double operations in the same order. */
unsigned bkr_half_step_ticks(int speed, int level);

/* Score with the speed-up of the reference: every 50 points the speed grows, past 10 the level does. */
void bkr_set_score(bkr_game_t *s, uint32_t value);

/* Do two cars with centres (ax, ay) and (bx, by) share a lit cell? */
bool bkr_cars_overlap(int ax, int ay, int bx, int by);

BK_EXTERN_C_END

#endif /* BRICK_RACING_INTERNAL_H */
