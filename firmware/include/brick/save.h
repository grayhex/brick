/*
 * Persistent profile: settings and per-game records, stored as a versioned, CRC-protected record in two redundant
 * slots (A/B). Writing always goes to the slot that does NOT hold the newest valid copy, so a power loss during a
 * write leaves the previous profile intact.
 *
 * The on-storage format is explicit little-endian bytes (not a struct dump): it does not depend on the compiler,
 * the host or the MCU, and a newer firmware can migrate older versions. Unknown (newer) versions are ignored and
 * defaults are used, never half-parsed. Mid-game snapshots are intentionally NOT persisted (see
 * docs/FIRMWARE_ARCHITECTURE.md).
 */
#ifndef BRICK_SAVE_H
#define BRICK_SAVE_H

#include "brick/hal.h"

BK_EXTERN_C_BEGIN

#define BK_SAVE_VERSION 2 /* 1 (seven settings bytes) is still read: the three newer settings take their defaults */
#define BK_MAX_RECORDS  96

/* Auto-off after this long without a key press (setting SLEEP): index into the table of the console. */
#define BK_SLEEP_OPTIONS 5 /* OFF, 1, 3, 5, 10 minutes */
#define BK_SLEEP_DEFAULT 3 /* 5 minutes */

typedef struct {
    uint8_t sound_on;    /* 0/1 */
    uint8_t light_level; /* 0..BK_LIGHT_MAX */
    uint8_t last_game;   /* menu position to restore */
    uint8_t last_speed;  /* 0..9 */
    uint8_t last_level;  /* 0..9 */
    uint16_t last_mode;
    uint8_t ble_on;    /* 0/1: the phone link (settings, synchronisation) is requested; off by default to save power */
    uint8_t demo_on;   /* 0/1: the menu preview is animated; off = a still poster frame and no refreshes of the panel */
    uint8_t sleep_idx; /* 0..BK_SLEEP_OPTIONS-1: auto-off timeout */
} bk_settings_t;

typedef struct {
    uint16_t game_uid; /* bk_game_def_t.uid */
    uint16_t mode;
    uint32_t score;
} bk_record_t;

typedef struct {
    bk_settings_t settings;
    uint16_t record_count;
    bk_record_t records[BK_MAX_RECORDS];
    uint32_t seq;      /* sequence number of the stored copy, advanced on every store */
    uint8_t next_slot; /* slot the next store writes to (not persisted) */
} bk_profile_t;

typedef enum { BK_SAVE_OK = 0, BK_SAVE_DEFAULTED = 1, BK_SAVE_IO_ERROR = -1 } bk_save_status_t;

void bk_profile_defaults(bk_profile_t *p);

/* Record accessors. set() keeps the best score; returns true if the stored value changed. */
uint32_t bk_profile_get_record(const bk_profile_t *p, uint16_t game_uid, uint16_t mode);
bool bk_profile_set_record(bk_profile_t *p, uint16_t game_uid, uint16_t mode, uint32_t score);
void bk_profile_clear_records(bk_profile_t *p);

/* Load the newest valid copy; BK_SAVE_DEFAULTED if none is valid (defaults filled in). */
bk_save_status_t bk_save_load(const bk_storage_port_t *st, bk_profile_t *p);
/* Store into the older slot and advance p->seq. */
bk_save_status_t bk_save_store(const bk_storage_port_t *st, bk_profile_t *p);

/* Serialisation, exposed for tests. Returns bytes written / 0 on failure, resp. true if the buffer is a valid record. */
size_t bk_save_encode(const bk_profile_t *p, uint8_t *buf, size_t cap);
bool bk_save_decode(const uint8_t *buf, size_t len, bk_profile_t *p);

uint32_t bk_crc32(const uint8_t *data, size_t len);

BK_EXTERN_C_END

#endif /* BRICK_SAVE_H */
