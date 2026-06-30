/* Host-side test for the open-file table serialization core (#141).
 *
 * Pure core, no kernel deps. Verifies serialize/deserialize round-trips
 * descriptors (fd, offset, flags, kind, path) exactly, validation catches
 * malformed tables (unterminated path, duplicate fd), find-by-fd, and bounds.
 *
 * Build: gcc -I../include -o test_checkpoint_filetable \
 *            test_checkpoint_filetable.c ../kernel/checkpoint_filetable.c
 */

#include <stdint.h>
#include <stdbool.h>

typedef __SIZE_TYPE__ size_t;
extern int    printf(const char*, ...);
extern void*  memcpy(void*, const void*, size_t);
extern void*  memset(void*, int, size_t);
extern int    memcmp(const void*, const void*, size_t);
extern size_t strlen(const char*);

#include "checkpoint_filetable.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
    else { printf("  ok:   %s\n", msg); } \
} while (0)

/* Set a record's NUL-terminated path. */
static void set_path(checkpoint_file_record_t* r, const char* s) {
    memset(r->path, 0, CHECKPOINT_FILE_PATH_MAX);
    size_t n = strlen(s);
    if (n >= CHECKPOINT_FILE_PATH_MAX) n = CHECKPOINT_FILE_PATH_MAX - 1;
    memcpy(r->path, s, n);
}

/* fd 3: a regular file with a path; fd 5: a socket (no path). */
static void build(checkpoint_filetable_t* t) {
    memset(t, 0, sizeof(*t));
    t->pid = 7;
    t->count = 2;
    t->records[0].fd = 3; t->records[0].flags = 0x2; t->records[0].offset = 4096;
    t->records[0].kind = 0 /* EXTSTATE_REGULAR_FILE */;
    set_path(&t->records[0], "/etc/motd");
    t->records[1].fd = 5; t->records[1].flags = 0x1; t->records[1].offset = 0;
    t->records[1].kind = 1 /* EXTSTATE_SOCKET */;
    set_path(&t->records[1], "");
}

int main(void) {
    printf("Test 1: round-trip\n");
    checkpoint_filetable_t in, out;
    build(&in);

    static uint8_t buf[64 * 1024];
    uint32_t need = checkpoint_filetable_blob_size(in.count);
    uint32_t w = checkpoint_filetable_serialize(&in, buf, sizeof(buf));
    CHECK(w == need, "serialize returns the computed blob size");
    CHECK(checkpoint_filetable_deserialize(&out, buf, w), "deserialize succeeds");
    CHECK(out.pid == 7 && out.count == 2, "pid + count round-trip");
    CHECK(memcmp(out.records, in.records, in.count * sizeof(checkpoint_file_record_t)) == 0,
          "records round-trip exactly");

    const checkpoint_file_record_t* f3 = checkpoint_filetable_find(&out, 3);
    CHECK(f3 && f3->offset == 4096 && strlen(f3->path) > 0, "find(3): offset + path preserved");
    const checkpoint_file_record_t* f5 = checkpoint_filetable_find(&out, 5);
    CHECK(f5 && f5->kind == 1 && f5->path[0] == '\0', "find(5): socket, no path");
    CHECK(checkpoint_filetable_find(&out, 9) == 0, "find of absent fd is NULL");

    printf("Test 2: validation\n");
    CHECK(checkpoint_filetable_validate(&in), "consistent table validates");

    checkpoint_filetable_t bad = in;
    memset(bad.records[0].path, 'A', CHECKPOINT_FILE_PATH_MAX); /* no NUL */
    CHECK(!checkpoint_filetable_validate(&bad), "unterminated path rejected");

    bad = in; bad.records[1].fd = 3; /* duplicate fd */
    CHECK(!checkpoint_filetable_validate(&bad), "duplicate fd rejected");

    printf("Test 3: bounds\n");
    CHECK(checkpoint_filetable_serialize(&in, buf, need - 1) == 0, "too-small buffer rejected");
    uint8_t tiny[8];
    CHECK(!checkpoint_filetable_deserialize(&out, tiny, sizeof(tiny)), "truncated blob rejected");
    checkpoint_filetable_serialize(&in, buf, sizeof(buf));
    buf[2] ^= 0xFF;
    CHECK(!checkpoint_filetable_deserialize(&out, buf, w), "bad magic rejected");

    printf("Test 4: full table spans multiple pages\n");
    checkpoint_filetable_t big;
    memset(&big, 0, sizeof(big));
    big.pid = 1; big.count = CHECKPOINT_FILETABLE_MAX;
    for (uint32_t i = 0; i < big.count; i++) { big.records[i].fd = i; }
    uint32_t bw = checkpoint_filetable_serialize(&big, buf, sizeof(buf));
    CHECK(bw == checkpoint_filetable_blob_size(big.count), "full-table blob size");
    CHECK(bw > 4096, "full table exceeds one page (exercises blob chunking)");
    CHECK(checkpoint_filetable_deserialize(&out, buf, bw) && out.count == big.count,
          "full table round-trips");

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
