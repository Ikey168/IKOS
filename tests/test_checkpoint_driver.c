/* Host-side test for the driver re-attach framework (#143).
 *
 * Pure ordering engine, no kernel deps. Verifies quiesce/reattach run in
 * registration order, the ABORT vs CONTINUE failure policy, severing on a
 * failed CONTINUE reattach, NULL-op handling, and registry bounds.
 *
 * Build: gcc -I../include -o test_checkpoint_driver test_checkpoint_driver.c \
 *            ../kernel/checkpoint_driver.c
 */

#include <stdint.h>
#include <stdbool.h>
extern int printf(const char*, ...);

#include "checkpoint_driver.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
    else { printf("  ok:   %s\n", msg); } \
} while (0)

/* Mock driver instrumentation. dev points at an int id. */
static int g_order[16];
static int g_order_n;
static int g_quiesce_calls;
static int g_reattach_calls;
static int g_fail_reattach_id = -1;

static void reset(void) { g_order_n = 0; g_quiesce_calls = 0; g_reattach_calls = 0; }

static int mock_quiesce(void* dev) {
    g_order[g_order_n++] = *(int*)dev; g_quiesce_calls++; return 0;
}
static int mock_reattach(void* dev) {
    int id = *(int*)dev;
    g_order[g_order_n++] = id; g_reattach_calls++;
    return (id == g_fail_reattach_id) ? -1 : 0;
}

int main(void) {
    int ids[4] = {0, 1, 2, 3};
    checkpoint_persist_ops_t ops = { mock_quiesce, mock_reattach, true };

    /* === registration + order === */
    printf("Test 1: register + quiesce order\n");
    checkpoint_driver_registry_t r;
    checkpoint_driver_registry_init(&r);
    CHECK(checkpoint_driver_register(&r, &ids[0], &ops) == 0, "register 0");
    CHECK(checkpoint_driver_register(&r, &ids[1], &ops) == 0, "register 1");
    CHECK(checkpoint_driver_register(&r, &ids[2], &ops) == 0, "register 2");
    CHECK(checkpoint_driver_count(&r) == 3, "count is 3");
    CHECK(checkpoint_driver_register(&r, 0, 0) == -1, "NULL ops rejected");

    reset();
    CHECK(checkpoint_driver_quiesce_all(&r) == 3, "all 3 quiesced");
    CHECK(g_order[0] == 0 && g_order[1] == 1 && g_order[2] == 2, "quiesced in registration order");

    /* === reattach: all succeed === */
    printf("Test 2: reattach all ok\n");
    reset(); g_fail_reattach_id = -1;
    CHECK(checkpoint_driver_reattach_all(&r, CHECKPOINT_REATTACH_CONTINUE) == 3, "3 reattached");
    CHECK(g_reattach_calls == 3, "all attempted");
    CHECK(!checkpoint_driver_is_severed(&r, 0) && !checkpoint_driver_is_severed(&r, 1)
          && !checkpoint_driver_is_severed(&r, 2), "none severed");

    /* === CONTINUE: a failure severs that device, others proceed === */
    printf("Test 3: CONTINUE severs the failed one\n");
    reset(); g_fail_reattach_id = 1;
    int n = checkpoint_driver_reattach_all(&r, CHECKPOINT_REATTACH_CONTINUE);
    CHECK(n == 2, "2 reattached, 1 severed");
    CHECK(g_reattach_calls == 3, "all 3 attempted despite the failure");
    CHECK(checkpoint_driver_is_severed(&r, 1), "device 1 severed");
    CHECK(!checkpoint_driver_is_severed(&r, 0) && !checkpoint_driver_is_severed(&r, 2),
          "devices 0 and 2 not severed");

    /* === ABORT: first failure stops the rest === */
    printf("Test 4: ABORT stops at the first failure\n");
    checkpoint_driver_registry_init(&r);
    checkpoint_driver_register(&r, &ids[0], &ops);
    checkpoint_driver_register(&r, &ids[1], &ops);
    checkpoint_driver_register(&r, &ids[2], &ops);
    reset(); g_fail_reattach_id = 1;
    CHECK(checkpoint_driver_reattach_all(&r, CHECKPOINT_REATTACH_ABORT) == CHECKPOINT_DRIVER_ABORTED,
          "reattach aborts");
    CHECK(g_reattach_calls == 2, "device 2 was not attempted after the abort");
    CHECK(g_order[0] == 0 && g_order[1] == 1, "attempted 0 then 1, then stopped");

    /* === NULL ops are no-op successes === */
    printf("Test 5: NULL quiesce/reattach\n");
    checkpoint_driver_registry_init(&r);
    checkpoint_persist_ops_t noop = { 0, 0, true };
    checkpoint_driver_register(&r, &ids[0], &noop);
    reset();
    CHECK(checkpoint_driver_quiesce_all(&r) == 1, "NULL quiesce counts as success");
    CHECK(checkpoint_driver_reattach_all(&r, CHECKPOINT_REATTACH_ABORT) == 1,
          "NULL reattach counts as reattached");

    /* === bounds === */
    printf("Test 6: registry full\n");
    checkpoint_driver_registry_init(&r);
    int full_ok = 1;
    for (uint32_t i = 0; i < CHECKPOINT_DRIVER_MAX; i++) {
        if (checkpoint_driver_register(&r, &ids[0], &ops) != 0) full_ok = 0;
    }
    CHECK(full_ok, "register up to MAX succeeds");
    CHECK(checkpoint_driver_register(&r, &ids[0], &ops) == -1, "one past MAX rejected");

    /* === global singleton exists === */
    CHECK(checkpoint_driver_global() != 0, "global registry available");

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
