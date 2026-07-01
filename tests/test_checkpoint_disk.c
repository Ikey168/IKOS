/* Host-side test for the disk (IDE) re-attach reconcile core (#144).
 *
 * Pure logic, no kernel deps. Drives checkpoint_disk_quiesce/reattach against a
 * mock disk: verifies the flush-on-quiesce path, the reprobe-then-verify order
 * on reattach, and each failure mode (flush, reprobe, read, bad magic).
 *
 * Build: gcc -I../include -o test_checkpoint_disk test_checkpoint_disk.c \
 *            ../kernel/checkpoint_disk.c
 */

#include <stdint.h>
#include <stdbool.h>
extern int printf(const char*, ...);

#include "checkpoint_disk.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
    else { printf("  ok:   %s\n", msg); } \
} while (0)

/* Mock disk. sector[] models the store superblock sector; the *_fail flags
 * force each op to error; counters record call order. */
static unsigned char g_sector[512];
static uint64_t      g_store_lba = 2048;
static int g_flush_calls, g_reprobe_calls, g_read_calls;
static uint64_t g_last_read_lba;
static int g_flush_fail, g_reprobe_fail, g_read_fail;

static void mock_reset(void) {
    g_flush_calls = g_reprobe_calls = g_read_calls = 0;
    g_last_read_lba = 0;
    g_flush_fail = g_reprobe_fail = g_read_fail = 0;
    /* Default: a valid store superblock (magic in the first 8 bytes, LE). */
    for (int i = 0; i < 512; i++) g_sector[i] = 0;
    for (int i = 0; i < 8; i++)
        g_sector[i] = (unsigned char)((CHECKPOINT_DISK_SB_MAGIC >> (8 * i)) & 0xFF);
}

static int mock_flush(void* ctx) { (void)ctx; g_flush_calls++; return g_flush_fail ? -1 : 0; }
static int mock_reprobe(void* ctx) { (void)ctx; g_reprobe_calls++; return g_reprobe_fail ? -1 : 0; }
static int mock_read(void* ctx, uint64_t lba, void* buf) {
    (void)ctx;
    g_read_calls++; g_last_read_lba = lba;
    if (g_read_fail) return -1;
    unsigned char* b = (unsigned char*)buf;
    for (int i = 0; i < 512; i++) b[i] = g_sector[i];
    return 0;
}

static checkpoint_disk_ops_t make_ops(void) {
    checkpoint_disk_ops_t o;
    o.flush = mock_flush; o.reprobe = mock_reprobe; o.read_sector = mock_read;
    o.ctx = 0; o.store_lba = g_store_lba;
    return o;
}

int main(void) {
    checkpoint_disk_ops_t o;

    printf("Test 1: read_magic is little-endian\n");
    mock_reset();
    CHECK(checkpoint_disk_read_magic(g_sector) == CHECKPOINT_DISK_SB_MAGIC, "decodes store magic");

    printf("Test 2: quiesce flushes\n");
    mock_reset(); o = make_ops();
    CHECK(checkpoint_disk_quiesce(&o) == CHECKPOINT_DISK_OK, "quiesce ok");
    CHECK(g_flush_calls == 1, "flush called once");

    printf("Test 3: quiesce flush failure\n");
    mock_reset(); g_flush_fail = 1; o = make_ops();
    CHECK(checkpoint_disk_quiesce(&o) == CHECKPOINT_DISK_ERR_FLUSH, "flush failure reported");

    printf("Test 4: reattach re-probes then verifies the store\n");
    mock_reset(); o = make_ops();
    CHECK(checkpoint_disk_reattach(&o) == CHECKPOINT_DISK_OK, "reattach ok");
    CHECK(g_reprobe_calls == 1, "reprobed once");
    CHECK(g_read_calls == 1, "read the store sector once");
    CHECK(g_last_read_lba == g_store_lba, "read at the store LBA");

    printf("Test 5: reprobe failure stops before any read\n");
    mock_reset(); g_reprobe_fail = 1; o = make_ops();
    CHECK(checkpoint_disk_reattach(&o) == CHECKPOINT_DISK_ERR_REPROBE, "reprobe failure reported");
    CHECK(g_read_calls == 0, "store not read after a failed reprobe");

    printf("Test 6: unreadable store region\n");
    mock_reset(); g_read_fail = 1; o = make_ops();
    CHECK(checkpoint_disk_reattach(&o) == CHECKPOINT_DISK_ERR_READ, "read failure reported");

    printf("Test 7: wrong superblock magic\n");
    mock_reset(); g_sector[0] ^= 0xFF; o = make_ops();
    CHECK(checkpoint_disk_reattach(&o) == CHECKPOINT_DISK_ERR_MAGIC, "bad magic rejected");

    printf("Test 8: NULL args\n");
    CHECK(checkpoint_disk_quiesce(0) == CHECKPOINT_DISK_ERR_PARAM, "NULL quiesce rejected");
    CHECK(checkpoint_disk_reattach(0) == CHECKPOINT_DISK_ERR_PARAM, "NULL reattach rejected");
    o = make_ops(); o.read_sector = 0;
    CHECK(checkpoint_disk_reattach(&o) == CHECKPOINT_DISK_ERR_PARAM, "reattach needs read_sector");

    printf("Test 9: NULL flush is a no-op success\n");
    mock_reset(); o = make_ops(); o.flush = 0;
    CHECK(checkpoint_disk_quiesce(&o) == CHECKPOINT_DISK_OK, "no flush op still ok");

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
