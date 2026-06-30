/* Host-side test for the IDE-backed checkpoint store adapter (#130).
 *
 * checkpoint_ide_bind() adapts an IDE drive to fat_block_device_t. This test
 * binds a MOCK ide backend (an in-memory disk) and drives a real snapshot store
 * through it - begin/add_page/commit then load/next - proving the adapter
 * correctly bridges store sector I/O to ide_read_sectors/ide_write_sectors,
 * including the sector->LBA and count mapping.
 *
 * Build: gcc -I../include -o test_checkpoint_ide test_checkpoint_ide.c \
 *            ../kernel/checkpoint_ide.c ../kernel/snapshot_store.c
 */

#include <stdint.h>
#include <stdbool.h>

typedef __SIZE_TYPE__ size_t;
extern int   printf(const char*, ...);
extern void* malloc(size_t);
extern void  free(void*);
extern void* memcpy(void*, const void*, size_t);
extern void* memset(void*, int, size_t);
extern int   memcmp(const void*, const void*, size_t);

void* kmalloc(size_t s) { return malloc(s); }
void  kfree(void* p) { free(p); }

#include "checkpoint_ide.h"
#include "snapshot_store.h"

/* ---- Mock IDE backend: an in-memory disk addressed by LBA. ---- */
#define IDE_SECTORS 2048
#define IDE_SUCCESS 0
static uint8_t g_ide_disk[IDE_SECTORS * 512];
static uint64_t g_last_lba;      /* records the mapping the adapter produced */
static uint16_t g_last_count;

/* These symbols match the IDE driver's signatures; checkpoint_ide.c calls them.
 * The first arg is ide_device_t* in the kernel; here it is an opaque token. */
int ide_read_sectors(void* dev, uint8_t drive, uint64_t lba, uint16_t count, void* buf) {
    (void)dev; (void)drive;
    g_last_lba = lba; g_last_count = count;
    if (lba + count > IDE_SECTORS) return -1;
    memcpy(buf, g_ide_disk + lba * 512, (size_t)count * 512);
    return IDE_SUCCESS;
}
int ide_write_sectors(void* dev, uint8_t drive, uint64_t lba, uint16_t count, const void* buf) {
    (void)dev; (void)drive;
    g_last_lba = lba; g_last_count = count;
    if (lba + count > IDE_SECTORS) return -1;
    memcpy(g_ide_disk + lba * 512, buf, (size_t)count * 512);
    return IDE_SUCCESS;
}

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
    else { printf("  ok:   %s\n", msg); } \
} while (0)

int main(void) {
    printf("Test: IDE-backed checkpoint store adapter\n");

    memset(g_ide_disk, 0, sizeof(g_ide_disk));

    /* Bind a mock IDE drive as a block device. */
    int dummy_ide; /* stands in for ide_device_t */
    fat_block_device_t bdev;
    checkpoint_ide_binding_t binding;
    fat_block_device_t* dev = checkpoint_ide_bind(&bdev, &binding, &dummy_ide, 0, IDE_SECTORS);
    CHECK(dev == &bdev, "bind returns the block device");
    CHECK(dev->sector_size == 512, "sector size is 512");
    CHECK(dev->private_data == &binding, "binding stored in private_data");
    CHECK(checkpoint_ide_bind(&bdev, &binding, 0, 0, IDE_SECTORS) == 0, "NULL ide rejected");

    /* A read goes through to ide_read_sectors with sector mapped to LBA. */
    uint8_t sec[512];
    dev->read_sectors(dev->private_data, 17, 1, sec);
    CHECK(g_last_lba == 17 && g_last_count == 1, "sector maps to LBA/count");

    /* Drive a full checkpoint through the adapter. */
    snapshot_store_t store;
    CHECK(snapshot_store_init(&store, dev, 0, 256) == SNAPSHOT_OK, "store init over IDE");
    CHECK(snapshot_store_format(&store) == SNAPSHOT_OK, "store format");

    snapshot_writer_t w;
    CHECK(snapshot_store_begin(&store, 42, &w) == SNAPSHOT_OK, "begin checkpoint");
    uint8_t page[SNAPSHOT_PAGE_SIZE];
    for (int i = 0; i < SNAPSHOT_PAGE_SIZE; i++) page[i] = (uint8_t)(i * 7 + 3);
    CHECK(snapshot_writer_add_page(&w, 5, 0x400000, 0, page) == SNAPSHOT_OK, "add page");
    CHECK(snapshot_store_commit(&w) == SNAPSHOT_OK, "commit");

    /* Read it back through the adapter. */
    snapshot_reader_t r;
    CHECK(snapshot_store_load(&store, &r) == SNAPSHOT_OK, "load from IDE-backed store");
    CHECK(r.epoch == 42 && r.record_count == 1, "epoch + record count");
    uint8_t out[SNAPSHOT_PAGE_SIZE];
    snapshot_page_record_t rec; rec.page_data = out;
    CHECK(snapshot_reader_next(&r, &rec) == SNAPSHOT_OK, "read the page back");
    CHECK(rec.pid == 5 && rec.virt_addr == 0x400000, "metadata round-trips");
    CHECK(memcmp(out, page, SNAPSHOT_PAGE_SIZE) == 0, "page contents round-trip through IDE");

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
