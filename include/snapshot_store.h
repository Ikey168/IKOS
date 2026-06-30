/* IKOS Orthogonal Persistence - On-disk Snapshot Store
 *
 * Implements the persistent checkpoint store described in
 * docs/architecture/orthogonal-persistence.md: a superblock plus two
 * double-buffered checkpoint slots, each holding one checkpoint as a log of
 * (epoch, pid, virt_addr) -> page records, protected by CRC32.
 *
 * Crash consistency: a new checkpoint is always written to the *inactive*
 * slot; the single superblock-sector write that flips the active slot is the
 * one and only commit point. A crash before that write leaves the previous
 * checkpoint fully intact.
 *
 * Issue #114 (epic #121).
 */

#ifndef SNAPSHOT_STORE_H
#define SNAPSHOT_STORE_H

#include <stdint.h>
#include <stdbool.h>
#include "fat.h"   /* fat_block_device_t */

/* Storage geometry */
#define SNAPSHOT_SECTOR_SIZE     512
#define SNAPSHOT_PAGE_SIZE       4096
#define SNAPSHOT_SECTORS_PER_PAGE (SNAPSHOT_PAGE_SIZE / SNAPSHOT_SECTOR_SIZE) /* 8 */
/* Each record on disk: 1 metadata sector + the page's data sectors. */
#define SNAPSHOT_SECTORS_PER_RECORD (1 + SNAPSHOT_SECTORS_PER_PAGE)           /* 9 */

/* On-disk magic numbers */
#define SNAPSHOT_SB_MAGIC        0x494B4F53534E4250ULL /* "IKOSSNBP" */
#define SNAPSHOT_SLOT_MAGIC      0x494B4F53534C4F54ULL /* "IKOSSLOT" */
#define SNAPSHOT_FORMAT_VERSION  1

/* Sentinel for "no valid slot yet" in the superblock. */
#define SNAPSHOT_NO_SLOT         0xFFFFFFFFu

/* Error codes */
#define SNAPSHOT_OK               0
#define SNAPSHOT_ERR_PARAM       -1
#define SNAPSHOT_ERR_IO          -2
#define SNAPSHOT_ERR_NOMEM       -3
#define SNAPSHOT_ERR_FULL        -4   /* slot can't hold another record */
#define SNAPSHOT_ERR_NO_CHECKPOINT -5 /* no valid checkpoint on disk */
#define SNAPSHOT_ERR_CRC         -6   /* CRC mismatch (corrupt slot/superblock) */
#define SNAPSHOT_ERR_STATE       -7   /* called in the wrong order */
#define SNAPSHOT_ERR_VERSION     -8   /* checkpoint's kernel-version stamp mismatches */

/* ----- On-disk structures (each lives in its own 512-byte sector) ----- */

typedef struct {
    uint64_t magic;            /* SNAPSHOT_SB_MAGIC */
    uint32_t version;          /* SNAPSHOT_FORMAT_VERSION */
    uint32_t active_slot;      /* 0, 1, or SNAPSHOT_NO_SLOT */
    uint64_t epoch;            /* epoch stored in the active slot */
    uint32_t slot_crc;         /* crc32 of the active slot's record sectors */
    uint32_t kernel_version;   /* kernel build stamp; restore rejects a mismatch (#139) */
    uint32_t superblock_crc;   /* crc32 over all preceding bytes of this struct */
} snapshot_superblock_t;

typedef struct {
    uint64_t magic;            /* SNAPSHOT_SLOT_MAGIC */
    uint64_t epoch;            /* checkpoint epoch held in this slot */
    uint32_t record_count;     /* number of page records */
    uint32_t reserved;
    uint64_t page_count;       /* informational total of pages persisted */
    uint32_t slot_crc;         /* crc32 over all record sectors */
    uint32_t reserved2;
} snapshot_slot_header_t;

typedef struct {
    uint64_t epoch;            /* epoch this page belongs to */
    uint32_t pid;              /* owning process */
    uint32_t flags;            /* snapshot/region flags */
    uint64_t virt_addr;        /* page-aligned virtual address */
    uint32_t page_bytes;       /* = SNAPSHOT_PAGE_SIZE */
    uint32_t reserved;
} snapshot_record_header_t;

/* ----- In-memory handles ----- */

typedef struct {
    fat_block_device_t* dev;
    uint32_t base_sector;      /* sector of the superblock */
    uint32_t slot_sectors;     /* sectors reserved for each slot */
    uint32_t max_records;      /* derived: (slot_sectors - 1) / 9 */
    uint32_t kernel_version;   /* expected kernel build stamp (#139); 0 by default */
    bool     initialized;
} snapshot_store_t;

typedef struct {
    snapshot_store_t* store;
    uint32_t slot;             /* slot being written (the inactive one) */
    uint32_t slot_base;        /* first sector of that slot */
    uint64_t epoch;
    uint32_t record_count;
    uint64_t page_count;
    uint32_t crc;              /* running crc over record sectors */
    bool     active;
} snapshot_writer_t;

typedef struct {
    snapshot_store_t* store;
    uint32_t slot_base;
    uint64_t epoch;
    uint32_t record_count;
    uint32_t next_index;
    bool     valid;
} snapshot_reader_t;

/* One page yielded by the reader. page_data must point at a
 * SNAPSHOT_PAGE_SIZE buffer supplied by the caller. */
typedef struct {
    uint64_t epoch;
    uint32_t pid;
    uint32_t flags;
    uint64_t virt_addr;
    void*    page_data;
} snapshot_page_record_t;

/* ----- API ----- */

/* Bind a store to a block device region. base_sector is where the superblock
 * lives; the two slots follow it. slot_sectors must be >= 1 + 9 so at least
 * one page record fits. Does not write anything. */
int snapshot_store_init(snapshot_store_t* store, fat_block_device_t* dev,
                        uint32_t base_sector, uint32_t slot_sectors);

/* Stamp the store with the running kernel's build version (#139). It is written
 * into the superblock on format/commit, and snapshot_store_load() rejects a
 * checkpoint whose stamp differs (SNAPSHOT_ERR_VERSION), so a kernel upgrade
 * invalidates old checkpoints instead of restoring incompatible kernel state.
 * Defaults to 0 if never called (matches an unstamped store). */
void snapshot_store_set_version(snapshot_store_t* store, uint32_t kernel_version);

/* Write a fresh, empty superblock marking "no valid checkpoint". Use once
 * when provisioning a brand-new store. */
int snapshot_store_format(snapshot_store_t* store);

/* Begin a new checkpoint. Selects the slot NOT currently active, so the last
 * good checkpoint is never touched. */
int snapshot_store_begin(snapshot_store_t* store, uint64_t epoch,
                         snapshot_writer_t* writer);

/* Append one page record to the in-progress checkpoint. */
int snapshot_writer_add_page(snapshot_writer_t* writer, uint32_t pid,
                             uint64_t virt_addr, uint32_t flags,
                             const void* page_data);

/* Finalize: write the slot header + CRC, then flip the superblock. The
 * superblock write is the atomic commit point. */
int snapshot_store_commit(snapshot_writer_t* writer);

/* Open the latest valid checkpoint for reading. Validates the superblock CRC,
 * the slot magic/epoch, and recomputes the slot CRC; returns
 * SNAPSHOT_ERR_NO_CHECKPOINT or SNAPSHOT_ERR_CRC if nothing valid is present. */
int snapshot_store_load(snapshot_store_t* store, snapshot_reader_t* reader);

/* Yield the next page of the loaded checkpoint into out->page_data (caller
 * buffer of SNAPSHOT_PAGE_SIZE). Returns SNAPSHOT_OK with out filled, or
 * SNAPSHOT_ERR_NO_CHECKPOINT when iteration is exhausted. */
int snapshot_reader_next(snapshot_reader_t* reader, snapshot_page_record_t* out);

/* CRC32 (IEEE 802.3, reflected, poly 0xEDB88320). Exposed for tests. */
uint32_t snapshot_crc32(uint32_t crc, const void* data, uint32_t len);

#endif /* SNAPSHOT_STORE_H */
