#include "brick/save.h"

#include <string.h>

/* Record layout (all little-endian):
 *   "BKSV" | version u16 | payload_len u16 | seq u32 | payload | crc32 u32 (over everything before it)
 * payload: sound u8, light u8, last_game u8, last_speed u8, last_level u8, last_mode u16,
 *          [version 2: ble u8, demo u8, sleep u8,]
 *          record_count u16, record_count x (game_uid u16, mode u16, score u32)
 * Version 1 has no bracketed bytes; it is read, never written.
 */
#define HEADER_LEN      12u
#define SETTINGS_LEN_V1 7u
#define SETTINGS_LEN    10u
#define RECORD_LEN      8u
#define MAX_ENCODED     (HEADER_LEN + SETTINGS_LEN + 2u + BK_MAX_RECORDS * RECORD_LEN + 4u)

_Static_assert(MAX_ENCODED <= BK_STORAGE_SLOT_MAX, "profile does not fit a storage slot");

uint32_t bk_crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t) - (int32_t)(crc & 1u));
        }
    }
    return ~crc;
}

static void put16(uint8_t *b, uint32_t v)
{
    b[0] = (uint8_t)v;
    b[1] = (uint8_t)(v >> 8);
}

static void put32(uint8_t *b, uint32_t v)
{
    put16(b, v);
    put16(b + 2, v >> 16);
}

static uint32_t get16(const uint8_t *b)
{
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8);
}

static uint32_t get32(const uint8_t *b)
{
    return get16(b) | (get16(b + 2) << 16);
}

void bk_profile_defaults(bk_profile_t *p)
{
    memset(p, 0, sizeof *p);
    p->settings.sound_on = 1;
    p->settings.light_level = 0;
    p->settings.ble_on = 0;
    p->settings.demo_on = 1;
    p->settings.sleep_idx = BK_SLEEP_DEFAULT;
    p->next_slot = 0;
}

uint32_t bk_profile_get_record(const bk_profile_t *p, uint16_t game_uid, uint16_t mode)
{
    for (unsigned i = 0; i < p->record_count; i++) {
        if (p->records[i].game_uid == game_uid && p->records[i].mode == mode) {
            return p->records[i].score;
        }
    }
    return 0;
}

bool bk_profile_set_record(bk_profile_t *p, uint16_t game_uid, uint16_t mode, uint32_t score)
{
    for (unsigned i = 0; i < p->record_count; i++) {
        if (p->records[i].game_uid == game_uid && p->records[i].mode == mode) {
            if (score > p->records[i].score) {
                p->records[i].score = score;
                return true;
            }
            return false;
        }
    }
    if (score == 0 || p->record_count >= BK_MAX_RECORDS) {
        return false; /* nothing worth keeping, or full: records are never evicted silently */
    }
    p->records[p->record_count].game_uid = game_uid;
    p->records[p->record_count].mode = mode;
    p->records[p->record_count].score = score;
    p->record_count++;
    return true;
}

void bk_profile_clear_records(bk_profile_t *p)
{
    p->record_count = 0;
    memset(p->records, 0, sizeof p->records);
}

size_t bk_save_encode(const bk_profile_t *p, uint8_t *buf, size_t cap)
{
    size_t payload = SETTINGS_LEN + 2u + (size_t)p->record_count * RECORD_LEN;
    size_t total = HEADER_LEN + payload + 4u;
    if (p->record_count > BK_MAX_RECORDS || cap < total) {
        return 0;
    }
    memcpy(buf, "BKSV", 4);
    put16(buf + 4, BK_SAVE_VERSION);
    put16(buf + 6, (uint32_t)payload);
    put32(buf + 8, p->seq);
    uint8_t *q = buf + HEADER_LEN;
    *q++ = p->settings.sound_on;
    *q++ = p->settings.light_level;
    *q++ = p->settings.last_game;
    *q++ = p->settings.last_speed;
    *q++ = p->settings.last_level;
    put16(q, p->settings.last_mode);
    q += 2;
    *q++ = p->settings.ble_on;
    *q++ = p->settings.demo_on;
    *q++ = p->settings.sleep_idx;
    put16(q, p->record_count);
    q += 2;
    for (unsigned i = 0; i < p->record_count; i++, q += RECORD_LEN) {
        put16(q, p->records[i].game_uid);
        put16(q + 2, p->records[i].mode);
        put32(q + 4, p->records[i].score);
    }
    put32(q, bk_crc32(buf, HEADER_LEN + payload));
    return total;
}

bool bk_save_decode(const uint8_t *buf, size_t len, bk_profile_t *p)
{
    if (len < HEADER_LEN + SETTINGS_LEN_V1 + 2u + 4u || memcmp(buf, "BKSV", 4) != 0) {
        return false;
    }
    const uint32_t version = get16(buf + 4);
    if (version != BK_SAVE_VERSION && version != 1u) {
        return false; /* unknown version: use defaults rather than guess */
    }
    const size_t settings_len = version == 1u ? SETTINGS_LEN_V1 : SETTINGS_LEN;
    size_t payload = get16(buf + 6);
    if (HEADER_LEN + payload + 4u != len || payload < settings_len + 2u) {
        return false;
    }
    if (get32(buf + HEADER_LEN + payload) != bk_crc32(buf, HEADER_LEN + payload)) {
        return false;
    }
    const uint8_t *q = buf + HEADER_LEN;
    uint32_t count = get16(q + settings_len);
    if (count > BK_MAX_RECORDS || payload != settings_len + 2u + count * RECORD_LEN) {
        return false;
    }
    bk_profile_defaults(p);
    p->seq = get32(buf + 8);
    p->settings.sound_on = q[0] ? 1 : 0;
    p->settings.light_level = q[1] > BK_LIGHT_MAX ? BK_LIGHT_MAX : q[1];
    p->settings.last_game = q[2];
    p->settings.last_speed = q[3] > 9 ? 9 : q[3];
    p->settings.last_level = q[4] > 9 ? 9 : q[4];
    p->settings.last_mode = (uint16_t)get16(q + 5);
    if (version != 1u) { /* a version 1 profile keeps the defaults of the settings it did not have */
        p->settings.ble_on = q[7] ? 1 : 0;
        p->settings.demo_on = q[8] ? 1 : 0;
        p->settings.sleep_idx = q[9] >= BK_SLEEP_OPTIONS ? BK_SLEEP_DEFAULT : q[9];
    }
    p->record_count = (uint16_t)count;
    q += settings_len + 2u;
    for (uint32_t i = 0; i < count; i++, q += RECORD_LEN) {
        p->records[i].game_uid = (uint16_t)get16(q);
        p->records[i].mode = (uint16_t)get16(q + 2);
        p->records[i].score = get32(q + 4);
    }
    return true;
}

bk_save_status_t bk_save_load(const bk_storage_port_t *st, bk_profile_t *p)
{
    bk_profile_defaults(p);
    if (st == NULL || st->read == NULL) {
        return BK_SAVE_DEFAULTED;
    }
    bool have = false;
    bk_profile_t best;
    unsigned best_slot = 0;
    for (unsigned slot = 0; slot < BK_STORAGE_SLOTS; slot++) {
        uint8_t buf[BK_STORAGE_SLOT_MAX];
        size_t len = 0;
        if (st->read(st->ctx, slot, buf, sizeof buf, &len) != 0 || len == 0 || len > sizeof buf) {
            continue;
        }
        bk_profile_t cand;
        if (!bk_save_decode(buf, len, &cand)) {
            continue;
        }
        /* newest by sequence number, comparing with wrap-around */
        if (!have || (int32_t)(cand.seq - best.seq) > 0) {
            best = cand;
            best_slot = slot;
            have = true;
        }
    }
    if (!have) {
        return BK_SAVE_DEFAULTED;
    }
    *p = best;
    p->next_slot = (uint8_t)((best_slot + 1u) % BK_STORAGE_SLOTS);
    return BK_SAVE_OK;
}

bk_save_status_t bk_save_store(const bk_storage_port_t *st, bk_profile_t *p)
{
    if (st == NULL || st->write == NULL) {
        return BK_SAVE_IO_ERROR;
    }
    uint8_t buf[BK_STORAGE_SLOT_MAX];
    uint32_t next_seq = p->seq + 1u;
    bk_profile_t copy = *p;
    copy.seq = next_seq;
    size_t n = bk_save_encode(&copy, buf, sizeof buf);
    if (n == 0 || st->write(st->ctx, p->next_slot % BK_STORAGE_SLOTS, buf, n) != 0) {
        return BK_SAVE_IO_ERROR;
    }
    p->seq = next_seq;
    p->next_slot = (uint8_t)((p->next_slot + 1u) % BK_STORAGE_SLOTS);
    return BK_SAVE_OK;
}
