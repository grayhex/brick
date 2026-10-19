/* Persistent profile: encoding, CRC, A/B slots, power-loss behaviour, records. */
#include <string.h>

#include "bk_test.h"
#include "brick/save.h"
#include "fake_hal.h"

static fake_hal_t hal;

static void test_crc32_known_vector(void)
{
    CHECK_EQ(bk_crc32((const uint8_t *)"123456789", 9), 0xCBF43926u); /* the standard CRC-32 check value */
    CHECK_EQ(bk_crc32((const uint8_t *)"", 0), 0);
}

static void test_defaults(void)
{
    bk_profile_t p;
    bk_profile_defaults(&p);
    CHECK_EQ(p.settings.sound_on, 1);
    CHECK_EQ(p.settings.light_level, 0);
    CHECK_EQ(p.record_count, 0);
}

static void test_roundtrip(void)
{
    bk_profile_t p;
    bk_profile_defaults(&p);
    p.seq = 41;
    p.settings.sound_on = 0;
    p.settings.light_level = 2;
    p.settings.last_game = 3;
    p.settings.last_speed = 7;
    p.settings.last_level = 9;
    p.settings.last_mode = 12;
    CHECK(bk_profile_set_record(&p, 1, 0, 12345));
    CHECK(bk_profile_set_record(&p, 2, 4, 9999999));
    uint8_t buf[BK_STORAGE_SLOT_MAX];
    size_t n = bk_save_encode(&p, buf, sizeof buf);
    CHECK_EQ(n, 12 + 7 + 2 + 2 * 8 + 4);
    bk_profile_t q;
    CHECK(bk_save_decode(buf, n, &q));
    CHECK_EQ(q.seq, 41);
    CHECK_EQ(q.settings.sound_on, 0);
    CHECK_EQ(q.settings.light_level, 2);
    CHECK_EQ(q.settings.last_game, 3);
    CHECK_EQ(q.settings.last_speed, 7);
    CHECK_EQ(q.settings.last_level, 9);
    CHECK_EQ(q.settings.last_mode, 12);
    CHECK_EQ(bk_profile_get_record(&q, 1, 0), 12345);
    CHECK_EQ(bk_profile_get_record(&q, 2, 4), 9999999);
    CHECK_EQ(bk_profile_get_record(&q, 2, 0), 0);
}

static void test_explicit_little_endian_layout(void)
{
    bk_profile_t p;
    bk_profile_defaults(&p);
    p.seq = 0x01020304;
    uint8_t buf[BK_STORAGE_SLOT_MAX];
    size_t n = bk_save_encode(&p, buf, sizeof buf);
    CHECK(memcmp(buf, "BKSV", 4) == 0);
    CHECK_EQ(buf[4], BK_SAVE_VERSION);
    CHECK_EQ(buf[5], 0);
    CHECK_EQ(buf[8], 0x04); /* seq, little endian */
    CHECK_EQ(buf[11], 0x01);
    CHECK_EQ(n, 12 + 7 + 2 + 4);
}

static void test_rejects_corruption(void)
{
    bk_profile_t p;
    bk_profile_defaults(&p);
    bk_profile_set_record(&p, 1, 0, 500);
    uint8_t buf[BK_STORAGE_SLOT_MAX];
    size_t n = bk_save_encode(&p, buf, sizeof buf);
    bk_profile_t q;
    CHECK(bk_save_decode(buf, n, &q));
    for (size_t i = 0; i < n; i++) { /* flipping any single bit must be detected */
        buf[i] ^= 0x10;
        CHECK(!bk_save_decode(buf, n, &q));
        buf[i] ^= 0x10;
    }
    CHECK(!bk_save_decode(buf, n - 1, &q)); /* truncated */
    CHECK(!bk_save_decode(buf, 5, &q));
    /* an unknown (newer) version is ignored rather than half-parsed, even with a valid CRC */
    buf[4] = BK_SAVE_VERSION + 1;
    uint32_t crc = bk_crc32(buf, n - 4);
    buf[n - 4] = (uint8_t)crc;
    buf[n - 3] = (uint8_t)(crc >> 8);
    buf[n - 2] = (uint8_t)(crc >> 16);
    buf[n - 1] = (uint8_t)(crc >> 24);
    CHECK(!bk_save_decode(buf, n, &q));
}

static void test_values_are_clamped_on_load(void)
{
    bk_profile_t p;
    bk_profile_defaults(&p);
    p.settings.light_level = 200;
    p.settings.last_speed = 77;
    p.settings.last_level = 55;
    p.settings.sound_on = 9;
    uint8_t buf[BK_STORAGE_SLOT_MAX];
    size_t n = bk_save_encode(&p, buf, sizeof buf);
    bk_profile_t q;
    CHECK(bk_save_decode(buf, n, &q));
    CHECK_EQ(q.settings.light_level, BK_LIGHT_MAX);
    CHECK_EQ(q.settings.last_speed, 9);
    CHECK_EQ(q.settings.last_level, 9);
    CHECK_EQ(q.settings.sound_on, 1);
}

static void test_slots_alternate_and_newest_wins(void)
{
    fake_hal_init(&hal);
    bk_profile_t p;
    CHECK_EQ(bk_save_load(&hal.hal.storage, &p), BK_SAVE_DEFAULTED); /* blank storage */
    bk_profile_set_record(&p, 1, 0, 10);
    CHECK_EQ(bk_save_store(&hal.hal.storage, &p), BK_SAVE_OK);
    CHECK_EQ(hal.slot_len[0] > 0, 1);
    CHECK_EQ(hal.slot_len[1], 0);
    bk_profile_set_record(&p, 1, 0, 20);
    CHECK_EQ(bk_save_store(&hal.hal.storage, &p), BK_SAVE_OK);
    CHECK_EQ(hal.slot_len[1] > 0, 1);
    bk_profile_set_record(&p, 1, 0, 30);
    CHECK_EQ(bk_save_store(&hal.hal.storage, &p), BK_SAVE_OK); /* back to slot 0, replacing the oldest */

    bk_profile_t q;
    CHECK_EQ(bk_save_load(&hal.hal.storage, &q), BK_SAVE_OK);
    CHECK_EQ(bk_profile_get_record(&q, 1, 0), 30);
    CHECK_EQ(q.seq, 3);
    /* continuing from the loaded profile writes the OTHER slot */
    bk_profile_set_record(&q, 1, 0, 40);
    CHECK_EQ(bk_save_store(&hal.hal.storage, &q), BK_SAVE_OK);
    bk_profile_t r;
    CHECK_EQ(bk_save_load(&hal.hal.storage, &r), BK_SAVE_OK);
    CHECK_EQ(bk_profile_get_record(&r, 1, 0), 40);
}

static void test_power_loss_keeps_previous_copy(void)
{
    fake_hal_init(&hal);
    bk_profile_t p;
    bk_save_load(&hal.hal.storage, &p);
    bk_profile_set_record(&p, 1, 0, 111);
    CHECK_EQ(bk_save_store(&hal.hal.storage, &p), BK_SAVE_OK);
    bk_profile_set_record(&p, 1, 0, 222);
    CHECK_EQ(bk_save_store(&hal.hal.storage, &p), BK_SAVE_OK);

    /* a write that is cut short corrupts only the slot being written (here: the older one) */
    bk_profile_set_record(&p, 1, 0, 333);
    CHECK_EQ(bk_save_store(&hal.hal.storage, &p), BK_SAVE_OK);
    hal.slot[p.next_slot ^ 1u][20] ^= 0xFF; /* tear the copy that was just written */
    bk_profile_t q;
    CHECK_EQ(bk_save_load(&hal.hal.storage, &q), BK_SAVE_OK);
    CHECK_EQ(bk_profile_get_record(&q, 1, 0), 222); /* previous copy survives */

    /* a failing driver is reported and changes nothing */
    hal.fail_writes = 1;
    bk_profile_set_record(&q, 1, 0, 444);
    CHECK_EQ(bk_save_store(&hal.hal.storage, &q), BK_SAVE_IO_ERROR);
    hal.fail_writes = 0;
    bk_profile_t r;
    CHECK_EQ(bk_save_load(&hal.hal.storage, &r), BK_SAVE_OK);
    CHECK_EQ(bk_profile_get_record(&r, 1, 0), 222);
}

static void test_both_slots_corrupt_gives_defaults(void)
{
    fake_hal_init(&hal);
    bk_profile_t p;
    bk_save_load(&hal.hal.storage, &p);
    bk_profile_set_record(&p, 1, 0, 5);
    bk_save_store(&hal.hal.storage, &p);
    bk_save_store(&hal.hal.storage, &p);
    hal.slot[0][15] ^= 1;
    hal.slot[1][15] ^= 1;
    bk_profile_t q;
    CHECK_EQ(bk_save_load(&hal.hal.storage, &q), BK_SAVE_DEFAULTED);
    CHECK_EQ(q.record_count, 0);
    CHECK_EQ(q.settings.sound_on, 1);
}

static void test_sequence_wraparound(void)
{
    fake_hal_init(&hal);
    bk_profile_t p;
    bk_profile_defaults(&p);
    p.seq = 0xFFFFFFFEu;
    CHECK_EQ(bk_save_store(&hal.hal.storage, &p), BK_SAVE_OK); /* seq 0xFFFFFFFF in slot 0 */
    CHECK_EQ(bk_save_store(&hal.hal.storage, &p), BK_SAVE_OK); /* seq 0 in slot 1: newer despite the smaller number */
    bk_profile_t q;
    CHECK_EQ(bk_save_load(&hal.hal.storage, &q), BK_SAVE_OK);
    CHECK_EQ(q.seq, 0);
}

static void test_records(void)
{
    bk_profile_t p;
    bk_profile_defaults(&p);
    CHECK(!bk_profile_set_record(&p, 1, 0, 0)); /* nothing worth keeping */
    CHECK(bk_profile_set_record(&p, 1, 0, 100));
    CHECK(!bk_profile_set_record(&p, 1, 0, 50));  /* lower: unchanged */
    CHECK(!bk_profile_set_record(&p, 1, 0, 100)); /* equal: unchanged */
    CHECK(bk_profile_set_record(&p, 1, 0, 150));
    CHECK(bk_profile_set_record(&p, 1, 1, 7)); /* other mode: separate record */
    CHECK_EQ(bk_profile_get_record(&p, 1, 0), 150);
    CHECK_EQ(bk_profile_get_record(&p, 1, 1), 7);
    for (unsigned i = p.record_count; i < BK_MAX_RECORDS; i++) {
        CHECK(bk_profile_set_record(&p, 9, (uint16_t)i, 1));
    }
    CHECK(!bk_profile_set_record(&p, 77, 0, 1)); /* full: reported, never silently evicts */
    CHECK_EQ(p.record_count, BK_MAX_RECORDS);
    uint8_t buf[BK_STORAGE_SLOT_MAX];
    CHECK(bk_save_encode(&p, buf, sizeof buf) > 0); /* a full profile still fits a slot */
    bk_profile_clear_records(&p);
    CHECK_EQ(p.record_count, 0);
    CHECK_EQ(bk_profile_get_record(&p, 1, 0), 0);
}

int main(void)
{
    TEST_RUN(test_crc32_known_vector);
    TEST_RUN(test_defaults);
    TEST_RUN(test_roundtrip);
    TEST_RUN(test_explicit_little_endian_layout);
    TEST_RUN(test_rejects_corruption);
    TEST_RUN(test_values_are_clamped_on_load);
    TEST_RUN(test_slots_alternate_and_newest_wins);
    TEST_RUN(test_power_loss_keeps_previous_copy);
    TEST_RUN(test_both_slots_corrupt_gives_defaults);
    TEST_RUN(test_sequence_wraparound);
    TEST_RUN(test_records);
    return bk_test_finish("save");
}
