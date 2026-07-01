/* Host-side durability test for the IDE-backed checkpoint store at boot (#201).
 *
 * Over the REAL checkpoint_ide binding (#130), writes a checkpoint (snapshot
 * store), an input journal (#194), and a keyframe retention ring (#195) onto a
 * MOCK non-volatile IDE disk, then simulates a power cut by discarding all the
 * in-memory handles and re-opening fresh ones over the SAME disk bytes. All
 * three reload intact, proving:
 *   - the stores are bound to (and persist on) the IDE disk, and
 *   - crash consistency holds across the cut (the single superblock-flip commit
 *     of each store survives a hard restart).
 *
 * Build: gcc -I../include -o test_ide_durable_boot test_ide_durable_boot.c \
 *          ../kernel/checkpoint_ide.c ../kernel/snapshot_store.c \
 *          ../kernel/checkpoint_journal.c ../kernel/keyframe_store.c \
 *          ../kernel/keyframe_ring.c
 */

#include <stdint.h>
#include <stdbool.h>

typedef __SIZE_TYPE__ size_t;
extern int   printf(const char*, ...);
extern void* malloc(size_t);
extern void  free(void*);
extern void* memcpy(void*, const void*, size_t);
extern void* memset(void*, int, size_t);

void* kmalloc(size_t s) { return malloc(s); }
void  kfree(void* p) { free(p); }

#include "checkpoint_ide.h"
#include "snapshot_store.h"
#include "checkpoint_journal.h"
#include "keyframe_store.h"

/* ---- Mock NON-VOLATILE IDE disk: an LBA-addressed buffer that survives a
 * simulated power cut (we simply do not clear it). ---- */
#define IDE_SECTORS 2048
#define IDE_SUCCESS 0
static uint8_t g_ide_disk[IDE_SECTORS * 512];

int ide_read_sectors(void* dev, uint8_t drive, uint64_t lba, uint16_t count, void* buf) {
    (void)dev; (void)drive;
    if (lba + count > IDE_SECTORS) return -1;
    memcpy(buf, g_ide_disk + lba * 512, (size_t)count * 512);
    return IDE_SUCCESS;
}
int ide_write_sectors(void* dev, uint8_t drive, uint64_t lba, uint16_t count, const void* buf) {
    (void)dev; (void)drive;
    if (lba + count > IDE_SECTORS) return -1;
    memcpy(g_ide_disk + lba * 512, buf, (size_t)count * 512);
    return IDE_SUCCESS;
}

/* Fresh binding + block device over the IDE disk (as boot would produce). */
static fat_block_device_t* bind_ide(fat_block_device_t* bdev,
                                    checkpoint_ide_binding_t* binding) {
    static int token; /* stands in for ide_device_t* */
    return checkpoint_ide_bind(bdev, binding, &token, 0 /*master*/, IDE_SECTORS);
}

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  [FAIL] %s\n", msg); failures++; } \
    else { printf("  [ ok ] %s\n", msg); } \
} while (0)

/* Store layout on the IDE disk (non-overlapping regions). */
#define CKPT_BASE   0
#define CKPT_SLOT   64
#define JRNL_BASE   200
#define JRNL_SLOT   32
#define KF_BASE     400
#define KF_INDEX    3
#define KF_CAP      4
#define KF_SLOT     10

#define MARK 0xABCDEF0123456789ULL

int main(void) {
    printf("=== IDE durable checkpoint store at boot (#201) ===\n");
    memset(g_ide_disk, 0, sizeof(g_ide_disk));

    fat_block_device_t bdev;
    checkpoint_ide_binding_t binding;
    fat_block_device_t* dev = bind_ide(&bdev, &binding);
    CHECK(dev != 0, "checkpoint_ide_bind produced a block device");
    CHECK(dev->read_sectors && dev->write_sectors && dev->total_sectors == IDE_SECTORS,
          "block device wired to the IDE drive");

    /* ---- Write a checkpoint, a journal, and keyframes onto the IDE disk ---- */
    {
        snapshot_store_t ss;
        CHECK(snapshot_store_init(&ss, dev, CKPT_BASE, CKPT_SLOT) == SNAPSHOT_OK, "checkpoint store init");
        CHECK(snapshot_store_format(&ss) == SNAPSHOT_OK, "checkpoint store format");
        snapshot_writer_t w;
        CHECK(snapshot_store_begin(&ss, 7, &w) == SNAPSHOT_OK, "checkpoint begin epoch 7");
        static uint8_t page[SNAPSHOT_PAGE_SIZE];
        memset(page, 0, sizeof(page));
        uint64_t mark = MARK; memcpy(page, &mark, sizeof(mark));
        CHECK(snapshot_writer_add_page(&w, 1, 0x1000, 0, page) == SNAPSHOT_OK, "checkpoint add page");
        CHECK(snapshot_store_commit(&w) == SNAPSHOT_OK, "checkpoint commit (superblock flip)");

        journal_store_t js;
        CHECK(journal_store_init(&js, dev, JRNL_BASE, JRNL_SLOT) == JOURNAL_OK, "journal store init");
        CHECK(journal_store_format(&js) == JOURNAL_OK, "journal store format");
        journal_writer_t jw;
        CHECK(journal_store_begin(&js, 7, 0, &jw) == JOURNAL_OK, "journal begin epoch 7");
        CHECK(journal_writer_append(&jw, JOURNAL_EV_KEY, 3, 0x42) == JOURNAL_OK, "journal append event");
        CHECK(journal_store_commit(&jw) == JOURNAL_OK, "journal commit (superblock flip)");

        keyframe_store_t ks;
        CHECK(keyframe_store_init(&ks, dev, KF_BASE, KF_INDEX, KF_CAP, KF_SLOT) == KEYFRAME_STORE_OK, "keyframe store init");
        CHECK(keyframe_store_format(&ks) == KEYFRAME_STORE_OK, "keyframe store format");
        for (uint64_t e = 5; e <= 8; e++) {
            snapshot_writer_t kw;
            CHECK(keyframe_store_begin(&ks, e, &kw) == KEYFRAME_STORE_OK, "keyframe begin");
            static uint8_t kp[SNAPSHOT_PAGE_SIZE];
            memset(kp, 0, sizeof(kp));
            memcpy(kp, &e, sizeof(e));
            snapshot_writer_add_page(&kw, 0, 0x2000, 0, kp);
            CHECK(keyframe_store_commit(&ks, &kw, e) == KEYFRAME_STORE_OK, "keyframe commit");
        }
    }

    /* ---- Simulate a POWER CUT: drop every handle; keep only the disk bytes.
     * Re-open fresh handles over the SAME g_ide_disk (a cold boot). ---- */
    printf("-- power cut: re-opening stores over the same IDE disk --\n");
    fat_block_device_t bdev2;
    checkpoint_ide_binding_t binding2;
    fat_block_device_t* dev2 = bind_ide(&bdev2, &binding2);

    /* Checkpoint reloads. */
    {
        snapshot_store_t ss;
        CHECK(snapshot_store_init(&ss, dev2, CKPT_BASE, CKPT_SLOT) == SNAPSHOT_OK, "reopen checkpoint store");
        snapshot_reader_t r;
        CHECK(snapshot_store_load(&ss, &r) == SNAPSHOT_OK, "checkpoint survived the power cut");
        CHECK(r.epoch == 7, "checkpoint epoch is 7");
        static uint8_t page[SNAPSHOT_PAGE_SIZE];
        snapshot_page_record_t rec; rec.page_data = page;
        CHECK(snapshot_reader_next(&r, &rec) == SNAPSHOT_OK, "checkpoint page reloads");
        uint64_t got = 0; memcpy(&got, page, sizeof(got));
        CHECK(got == MARK, "checkpoint page contents intact");
    }

    /* Journal reloads. */
    {
        journal_store_t js;
        CHECK(journal_store_init(&js, dev2, JRNL_BASE, JRNL_SLOT) == JOURNAL_OK, "reopen journal store");
        journal_reader_t r;
        CHECK(journal_store_load(&js, &r) == JOURNAL_OK, "journal survived the power cut");
        CHECK(r.epoch == 7, "journal epoch is 7");
        journal_event_t ev;
        CHECK(journal_reader_next(&r, &ev) == JOURNAL_OK && ev.type == JOURNAL_EV_KEY && ev.value == 0x42,
              "journal event reloads intact");
    }

    /* Keyframe ring reloads and can still select a retained keyframe by epoch. */
    {
        keyframe_store_t ks;
        CHECK(keyframe_store_init(&ks, dev2, KF_BASE, KF_INDEX, KF_CAP, KF_SLOT) == KEYFRAME_STORE_OK, "reopen keyframe store");
        CHECK(keyframe_store_load_index(&ks) == KEYFRAME_STORE_OK, "keyframe ring index survived the power cut");
        const keyframe_ring_t* ring = keyframe_store_ring(&ks);
        uint64_t oldest = 0, newest = 0;
        keyframe_ring_oldest(ring, &oldest); keyframe_ring_newest(ring, &newest);
        CHECK(keyframe_ring_count(ring) == KF_CAP && oldest == 5 && newest == 8,
              "retained window is epochs 5..8 after the cut");
        snapshot_reader_t r; uint64_t sel = 0;
        CHECK(keyframe_store_load_epoch(&ks, 7, &r, &sel) == KEYFRAME_STORE_OK && sel == 7,
              "a retained keyframe still restores by epoch");
        static uint8_t kp[SNAPSHOT_PAGE_SIZE];
        snapshot_page_record_t rec; rec.page_data = kp;
        snapshot_reader_next(&r, &rec);
        uint64_t got = 0; memcpy(&got, kp, sizeof(got));
        CHECK(got == 7, "keyframe 7 contents intact");
    }

    printf("%s\n", failures ? "FAILED" : "PASSED: checkpoint + journal + keyframe ring durable on the IDE disk");
    return failures ? 1 : 0;
}
