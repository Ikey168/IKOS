/* Host-side test for the IPC serialization core (#142).
 *
 * Pure core, no kernel deps. Verifies serialize/deserialize round-trips
 * channels + messages (incl. payloads) exactly, validation catches malformed
 * tables (dangling/duplicate/zero channel ids, oversized payload), find, bounds.
 *
 * Build: gcc -I../include -o test_checkpoint_ipc \
 *            test_checkpoint_ipc.c ../kernel/checkpoint_ipc.c
 */

#include <stdint.h>
#include <stdbool.h>

typedef __SIZE_TYPE__ size_t;
extern int   printf(const char*, ...);
extern void* memcpy(void*, const void*, size_t);
extern void* memset(void*, int, size_t);
extern int   memcmp(const void*, const void*, size_t);

#include "checkpoint_ipc.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
    else { printf("  ok:   %s\n", msg); } \
} while (0)

static checkpoint_ipc_t in, out;
static uint8_t buf[256 * 1024];

/* Channels 10 and 20; three messages across them with distinct payloads. */
static void build(void) {
    memset(&in, 0, sizeof(in));
    in.channel_count = 2;
    in.channels[0] = (checkpoint_ipc_channel_t){ .channel_id=10, .owner_pid=3, .permissions=0x7, .active=1 };
    in.channels[1] = (checkpoint_ipc_channel_t){ .channel_id=20, .owner_pid=4, .permissions=0x1, .active=1 };
    in.message_count = 3;
    for (int i = 0; i < 3; i++) {
        checkpoint_ipc_message_t* m = &in.messages[i];
        m->channel_id = (i == 2) ? 20 : 10;
        m->type = 1; m->src_pid = 3; m->dst_pid = 4; m->message_id = 100 + i;
        m->data_size = 64 + i;
        m->timestamp = 5000 + i;
        for (uint32_t k = 0; k < m->data_size; k++) m->data[k] = (uint8_t)(i * 40 + k);
    }
}

int main(void) {
    printf("Test 1: round-trip\n");
    build();
    uint32_t need = checkpoint_ipc_blob_size(in.channel_count, in.message_count);
    uint32_t w = checkpoint_ipc_serialize(&in, buf, sizeof(buf));
    CHECK(w == need, "serialize returns the computed blob size");
    CHECK(checkpoint_ipc_deserialize(&out, buf, w), "deserialize succeeds");
    CHECK(out.channel_count == 2 && out.message_count == 3, "counts round-trip");
    CHECK(memcmp(out.channels, in.channels, 2 * sizeof(checkpoint_ipc_channel_t)) == 0,
          "channels round-trip");
    CHECK(memcmp(out.messages, in.messages, 3 * sizeof(checkpoint_ipc_message_t)) == 0,
          "messages (incl. payloads) round-trip exactly");

    const checkpoint_ipc_channel_t* c20 = checkpoint_ipc_find_channel(&out, 20);
    CHECK(c20 && c20->owner_pid == 4, "find channel 20");
    CHECK(checkpoint_ipc_find_channel(&out, 99) == 0, "find of absent channel is NULL");
    CHECK(out.messages[2].channel_id == 20 && out.messages[2].data[0] == (uint8_t)(2*40),
          "message 2 payload preserved");

    printf("Test 2: validation\n");
    CHECK(checkpoint_ipc_validate(&in), "consistent table validates");

    checkpoint_ipc_t bad = in;
    bad.messages[0].channel_id = 77; /* dangling */
    CHECK(!checkpoint_ipc_validate(&bad), "message on absent channel rejected");

    bad = in; bad.channels[1].channel_id = 10; /* duplicate */
    CHECK(!checkpoint_ipc_validate(&bad), "duplicate channel id rejected");

    bad = in; bad.channels[0].channel_id = 0;
    CHECK(!checkpoint_ipc_validate(&bad), "channel id 0 rejected");

    bad = in; bad.messages[0].data_size = CHECKPOINT_IPC_DATA_MAX + 1;
    CHECK(!checkpoint_ipc_validate(&bad), "oversized payload rejected");

    printf("Test 3: bounds\n");
    CHECK(checkpoint_ipc_serialize(&in, buf, need - 1) == 0, "too-small buffer rejected");
    uint8_t tiny[8];
    CHECK(!checkpoint_ipc_deserialize(&out, tiny, sizeof(tiny)), "truncated blob rejected");
    checkpoint_ipc_serialize(&in, buf, sizeof(buf));
    buf[1] ^= 0xFF;
    CHECK(!checkpoint_ipc_deserialize(&out, buf, w), "bad magic rejected");

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
