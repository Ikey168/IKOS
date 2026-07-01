/* IKOS Orthogonal Persistence - Input Journal (#161, epic #159)
 *
 * The deterministic-replay input journal: a ring log written alongside the
 * checkpoint slots that captures every nondeterministic input between two
 * checkpoints (keystrokes, disk-completion events, timer and cycle reads,
 * entropy), each tagged with the checkpoint epoch and a logical clock. A
 * keyframe records the whole system state at an epoch; this journal records
 * the delta between keyframes, so the replay engine (#165) can boot the
 * nearest keyframe and re-drive execution forward to any past moment.
 *
 * The nondeterminism sources this journal exists to capture are catalogued in
 * docs/architecture/time-travel.md (#160).
 *
 * On-disk layout mirrors the snapshot store (snapshot_store.h): a superblock
 * sector plus two double-buffered slots. A new journal is always written to the
 * INACTIVE slot; the single superblock-sector write that flips the active slot
 * is the one and only commit point, so a crash before that write leaves the
 * previous journal fully intact. Every slot is protected by CRC32.
 *
 * Like checkpoint_disk.c, this core is dependency-free and host-testable: it
 * talks to storage only through a fat_block_device_t and carries its own sector
 * buffers, so it needs no allocator.
 */

#ifndef CHECKPOINT_JOURNAL_H
#define CHECKPOINT_JOURNAL_H

#include <stdint.h>
#include <stdbool.h>
#include "fat.h"   /* fat_block_device_t */

/* Storage geometry */
#define JOURNAL_SECTOR_SIZE       512
#define JOURNAL_EVENT_SIZE        32
#define JOURNAL_EVENTS_PER_SECTOR (JOURNAL_SECTOR_SIZE / JOURNAL_EVENT_SIZE) /* 16 */

/* On-disk magic numbers */
#define JOURNAL_SB_MAGIC          0x494B4F534A524E4CULL /* "IKOSJRNL" */
#define JOURNAL_SLOT_MAGIC        0x494B4F534A534C54ULL /* "IKOSJSLT" */
#define JOURNAL_FORMAT_VERSION    1

/* Sentinel for "no valid journal yet" in the superblock. */
#define JOURNAL_NO_SLOT           0xFFFFFFFFu

/* Event types. value carries the small inline datum for each; bulk payloads
 * (for example a run of entropy bytes) are recorded as a sequence of 8-byte
 * JOURNAL_EV_ENTROPY events. */
#define JOURNAL_EV_KEY            1  /* keyboard scancode in value */
#define JOURNAL_EV_DISK           2  /* disk-completion token/status in value */
#define JOURNAL_EV_TIMER          3  /* timer or cycle read (RDTSC) in value */
#define JOURNAL_EV_ENTROPY        4  /* up to 8 bytes of entropy in value */

/* Error codes */
#define JOURNAL_OK                0
#define JOURNAL_ERR_PARAM        -1
#define JOURNAL_ERR_IO           -2
#define JOURNAL_ERR_FULL         -3   /* slot cannot hold another event */
#define JOURNAL_ERR_NO_JOURNAL   -4   /* no valid journal on disk / iteration done */
#define JOURNAL_ERR_CRC          -5   /* CRC mismatch (corrupt slot/superblock) */
#define JOURNAL_ERR_STATE        -6   /* called in the wrong order */

/* ----- On-disk structures (fields ordered for natural alignment) ----- */

typedef struct {
    uint64_t magic;            /* JOURNAL_SB_MAGIC */
    uint32_t version;          /* JOURNAL_FORMAT_VERSION */
    uint32_t active_slot;      /* 0, 1, or JOURNAL_NO_SLOT */
    uint64_t epoch;            /* epoch held in the active slot */
    uint32_t event_count;      /* events in the active slot */
    uint32_t slot_crc;         /* crc32 over the active slot's event sectors */
    uint32_t superblock_crc;   /* crc32 over all preceding bytes of this struct */
    uint32_t reserved;
} journal_superblock_t;

typedef struct {
    uint64_t magic;            /* JOURNAL_SLOT_MAGIC */
    uint64_t epoch;            /* epoch this journal covers */
    uint32_t event_count;      /* number of events */
    uint32_t slot_crc;         /* crc32 over the event sectors */
    uint64_t base_lclock;      /* logical clock at epoch open (informational) */
} journal_slot_header_t;

typedef struct {
    uint64_t epoch;            /* checkpoint epoch this event belongs to */
    uint64_t lclock;           /* logical clock: monotonic within the epoch */
    uint32_t type;             /* JOURNAL_EV_* */
    uint32_t len;              /* reserved for variable payloads; 0 in v1 */
    uint64_t value;            /* inline datum (scancode, timer value, ...) */
} journal_event_t;

/* ----- In-memory handles (caller-allocated; no dynamic memory) ----- */

typedef struct {
    fat_block_device_t* dev;
    uint32_t base_sector;      /* sector of the superblock */
    uint32_t slot_sectors;     /* sectors reserved per slot (>= 2) */
    uint32_t max_events;       /* derived: (slot_sectors - 1) * 16 */
    bool     initialized;
} journal_store_t;

typedef struct {
    journal_store_t* store;
    uint32_t slot;             /* slot being written (the inactive one) */
    uint32_t slot_base;        /* first sector of that slot */
    uint64_t epoch;
    uint64_t base_lclock;
    uint32_t event_count;
    uint32_t crc;              /* running crc over completed event sectors */
    uint32_t buf_events;       /* events pending in sector_buf */
    uint32_t event_sector;     /* next event-sector index within the slot */
    uint8_t  sector_buf[JOURNAL_SECTOR_SIZE];
    bool     active;
} journal_writer_t;

typedef struct {
    journal_store_t* store;
    uint32_t slot_base;
    uint64_t epoch;
    uint32_t event_count;
    uint32_t next_index;
    uint32_t buf_sector;       /* event-sector index currently in sector_buf */
    uint8_t  sector_buf[JOURNAL_SECTOR_SIZE];
    bool     valid;
} journal_reader_t;

/* ----- API ----- */

/* Bind a journal to a block-device region. base_sector holds the superblock;
 * the two slots follow it. slot_sectors must be >= 2 (one header sector plus at
 * least one event sector). Writes nothing. */
int journal_store_init(journal_store_t* store, fat_block_device_t* dev,
                       uint32_t base_sector, uint32_t slot_sectors);

/* Write a fresh, empty superblock marking "no valid journal". Use once when
 * provisioning a brand-new journal region. */
int journal_store_format(journal_store_t* store);

/* Begin a journal for one epoch. Selects the slot NOT currently active, so the
 * last good journal is never touched. base_lclock is the logical-clock value at
 * which the epoch opened (informational; events carry their own lclock). */
int journal_store_begin(journal_store_t* store, uint64_t epoch,
                        uint64_t base_lclock, journal_writer_t* writer);

/* Append one input event in order. type is a JOURNAL_EV_* code, lclock is the
 * event's logical-clock stamp, value is the inline datum. */
int journal_writer_append(journal_writer_t* writer, uint32_t type,
                          uint64_t lclock, uint64_t value);

/* Like journal_writer_append but records `len` in the event's len field, for
 * variable-length payloads packed into value (for example an entropy run of
 * 1..8 bytes). journal_writer_append is this with len == 0. */
int journal_writer_append_len(journal_writer_t* writer, uint32_t type,
                              uint64_t lclock, uint64_t value, uint32_t len);

/* Finalize: flush the last partial sector, write the slot header + CRC, then
 * flip the superblock. The superblock write is the atomic commit point. */
int journal_store_commit(journal_writer_t* writer);

/* Open the latest valid journal for reading. Validates the superblock CRC, the
 * slot magic/epoch/count, and recomputes the slot CRC; returns
 * JOURNAL_ERR_NO_JOURNAL or JOURNAL_ERR_CRC if nothing valid is present. */
int journal_store_load(journal_store_t* store, journal_reader_t* reader);

/* Yield the next event of the loaded journal in recorded order. Returns
 * JOURNAL_OK with *out filled, or JOURNAL_ERR_NO_JOURNAL when exhausted. */
int journal_reader_next(journal_reader_t* reader, journal_event_t* out);

/* CRC32 (IEEE 802.3, reflected, poly 0xEDB88320). Seed with 0. Exposed for
 * tests. Kept local to this module so the core stays dependency-free. */
uint32_t journal_crc32(uint32_t crc, const void* data, uint32_t len);

#endif /* CHECKPOINT_JOURNAL_H */
