#include "tetris_shapes.h"

typedef struct {
    const char *name;
    bkt_coord_t c[BKT_PIECE_CELLS];
} shape_t;

/* Data of the reference FIGURES entries 3..9, in the donor's order. */
// clang-format off
static const shape_t SHAPES[BKT_SHAPE_COUNT] = {
    {"Z", {{-1, 1}, {-1, 0}, {0, 0}, {0, -1}}},
    {"S", {{-1, -1}, {-1, 0}, {0, 0}, {0, 1}}},
    {"Line", {{0, -1}, {0, 0}, {0, 1}, {0, 2}}},
    {"T", {{-1, 0}, {0, 0}, {1, 0}, {0, 1}}},
    {"Square", {{0, 0}, {1, 0}, {0, 1}, {1, 1}}},
    {"L", {{0, -1}, {1, 0}, {1, -1}, {1, 1}}},
    {"MirroredL", {{0, -1}, {1, -1}, {0, 0}, {0, 1}}},
};
// clang-format on

const char *bkt_shape_name(int shape)
{
    return SHAPES[((shape % BKT_SHAPE_COUNT) + BKT_SHAPE_COUNT) % BKT_SHAPE_COUNT].name;
}

static void bounds(bkt_piece_t *p)
{
    p->min_x = p->max_x = p->cells[0].x;
    p->min_y = p->max_y = p->cells[0].y;
    for (unsigned i = 1; i < BKT_PIECE_CELLS; i++) {
        if (p->cells[i].x < p->min_x) {
            p->min_x = p->cells[i].x;
        }
        if (p->cells[i].x > p->max_x) {
            p->max_x = p->cells[i].x;
        }
        if (p->cells[i].y < p->min_y) {
            p->min_y = p->cells[i].y;
        }
        if (p->cells[i].y > p->max_y) {
            p->max_y = p->cells[i].y;
        }
    }
}

bkt_piece_t bkt_piece_make(int shape, int angle)
{
    int s = ((shape % BKT_SHAPE_COUNT) + BKT_SHAPE_COUNT) % BKT_SHAPE_COUNT;
    int a = ((angle % 4) + 4) % 4;

    bkt_piece_t p;
    p.shape = (uint8_t)s;
    p.angle = (uint8_t)a;
    for (unsigned i = 0; i < BKT_PIECE_CELLS; i++) {
        int x = SHAPES[s].c[i].x;
        int y = SHAPES[s].c[i].y;
        int rx;
        int ry;
        switch (a) {
        case 1:
            rx = y;
            ry = -x;
            break;
        case 2:
            rx = -x;
            ry = -y;
            break;
        case 3:
            rx = -y;
            ry = x;
            break;
        default:
            rx = x;
            ry = y;
            break;
        }
        p.cells[i].x = (int8_t)rx;
        p.cells[i].y = (int8_t)ry;
    }
    bounds(&p);

    /* Alignment after rotation, as TetrisShape.setShape: shift right (not the line), then up. */
    if (s != BKT_SHAPE_LINE && (p.min_x < -1 || p.max_x <= 0)) {
        for (unsigned i = 0; i < BKT_PIECE_CELLS; i++) {
            p.cells[i].x++;
        }
        bounds(&p);
    }
    if (p.min_y < 0 || p.max_y <= 0) {
        for (unsigned i = 0; i < BKT_PIECE_CELLS; i++) {
            p.cells[i].y++;
        }
        bounds(&p);
    }
    return p;
}

bkt_piece_t bkt_piece_rotate(const bkt_piece_t *piece)
{
    if (piece->shape == BKT_SHAPE_SQUARE) {
        return *piece;
    }
    return bkt_piece_make(piece->shape, piece->angle + 1);
}
