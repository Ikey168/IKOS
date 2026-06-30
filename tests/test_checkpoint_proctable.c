/* Host-side test for the process-table serialization core (#140).
 *
 * Pure core, no kernel deps. Verifies serialize/deserialize round-trips the
 * table (pids, relationships, scheduler membership) exactly, that referential
 * integrity validation catches broken tables, find-by-pid, and buffer bounds.
 *
 * Build: gcc -I../include -o test_checkpoint_proctable \
 *            test_checkpoint_proctable.c ../kernel/checkpoint_proctable.c
 */

#include <stdint.h>
#include <stdbool.h>

typedef __SIZE_TYPE__ size_t;
extern int   printf(const char*, ...);
extern void* memcpy(void*, const void*, size_t);
extern void* memset(void*, int, size_t);
extern int   memcmp(const void*, const void*, size_t);

#include "checkpoint_proctable.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
    else { printf("  ok:   %s\n", msg); } \
} while (0)

/* Build a small, consistent table: pid 1 (init) has child 2; 2 and 3 are
 * siblings; 3's parent is 1. */
static void build(checkpoint_proctable_t* t) {
    memset(t, 0, sizeof(*t));
    t->count = 3;
    t->procs[0] = (checkpoint_proc_record_t){ .pid=1, .ppid=0, .state=2, .priority=1,
        .alarm_time=0, .first_child_pid=2, .next_sibling_pid=0, .on_ready_queue=1, .ready_order=0 };
    t->procs[1] = (checkpoint_proc_record_t){ .pid=2, .ppid=1, .state=2, .priority=2,
        .alarm_time=500, .first_child_pid=0, .next_sibling_pid=3, .on_ready_queue=1, .ready_order=1 };
    t->procs[2] = (checkpoint_proc_record_t){ .pid=3, .ppid=1, .state=3, .priority=0,
        .alarm_time=0, .first_child_pid=0, .next_sibling_pid=0, .on_ready_queue=0, .ready_order=0 };
}

int main(void) {
    /* === 1. round-trip === */
    printf("Test 1: serialize/deserialize round-trip\n");
    checkpoint_proctable_t in, out;
    build(&in);

    uint32_t need = checkpoint_proctable_blob_size(in.count);
    static uint8_t buf[64 * 1024];
    uint32_t w = checkpoint_proctable_serialize(&in, buf, sizeof(buf));
    CHECK(w == need, "serialize returns the computed blob size");

    CHECK(checkpoint_proctable_deserialize(&out, buf, w), "deserialize succeeds");
    CHECK(out.count == in.count, "count round-trips");
    CHECK(memcmp(out.procs, in.procs, in.count * sizeof(checkpoint_proc_record_t)) == 0,
          "all records round-trip exactly");

    /* spot-check a couple of fields */
    const checkpoint_proc_record_t* p2 = checkpoint_proctable_find(&out, 2);
    CHECK(p2 && p2->ppid == 1 && p2->next_sibling_pid == 3 && p2->alarm_time == 500,
          "find(2): parent/sibling/alarm preserved");
    CHECK(checkpoint_proctable_find(&out, 1)->first_child_pid == 2, "find(1): child preserved");
    CHECK(checkpoint_proctable_find(&out, 99) == 0, "find of absent pid is NULL");

    /* === 2. validation === */
    printf("Test 2: validation\n");
    CHECK(checkpoint_proctable_validate(&in), "consistent table validates");

    checkpoint_proctable_t bad = in;
    bad.procs[1].ppid = 77; /* references an absent pid */
    CHECK(!checkpoint_proctable_validate(&bad), "dangling ppid rejected");

    bad = in; bad.procs[2].pid = 1; /* duplicate pid */
    CHECK(!checkpoint_proctable_validate(&bad), "duplicate pid rejected");

    bad = in; bad.procs[0].pid = 0; /* pid 0 */
    CHECK(!checkpoint_proctable_validate(&bad), "pid 0 rejected");

    bad = in; bad.procs[0].first_child_pid = 42; /* dangling child */
    CHECK(!checkpoint_proctable_validate(&bad), "dangling first_child rejected");

    /* === 3. bounds === */
    printf("Test 3: bounds\n");
    CHECK(checkpoint_proctable_serialize(&in, buf, need - 1) == 0, "too-small buffer rejected");
    uint8_t tiny[4];
    CHECK(!checkpoint_proctable_deserialize(&out, tiny, sizeof(tiny)), "truncated blob rejected");

    /* corrupt magic */
    checkpoint_proctable_serialize(&in, buf, sizeof(buf));
    buf[0] ^= 0xFF;
    CHECK(!checkpoint_proctable_deserialize(&out, buf, w), "bad magic rejected");

    /* === 4. a full table spans multiple pages (kernel blob will chunk it) === */
    printf("Test 4: large table\n");
    checkpoint_proctable_t big;
    memset(&big, 0, sizeof(big));
    big.count = CHECKPOINT_PROCTABLE_MAX;
    for (uint32_t i = 0; i < big.count; i++) big.procs[i].pid = i + 1;
    uint32_t bw = checkpoint_proctable_serialize(&big, buf, sizeof(buf));
    CHECK(bw == checkpoint_proctable_blob_size(big.count), "full-table blob size");
    CHECK(bw > 4096, "full table exceeds one page (exercises blob chunking)");
    CHECK(checkpoint_proctable_deserialize(&out, buf, bw) && out.count == big.count,
          "full table round-trips");

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
