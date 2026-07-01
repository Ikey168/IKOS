/* Host-side unit test for the external-state policy (#118).
 *
 * Verifies that:
 *   1. Classification is correct: regular files survive a checkpoint; sockets,
 *      DMA buffers, devices, and pipes do not (and unknown kinds fail safe).
 *   2. extstate_sever() only severs open, non-persistable handles, and is
 *      idempotent.
 *   3. extstate_sever_all() severs exactly the non-persistable open handles in
 *      a mixed set (e.g. a process holding a file + a socket).
 *   4. A severed handle reports EXTSTATE_SEVERED so the app can re-establish it,
 *      while survivors and the still-usable handles report EXTSTATE_OK.
 *
 * Build: gcc -I../include -o test_checkpoint_extstate \
 *            test_checkpoint_extstate.c ../kernel/checkpoint_extstate.c
 */

#include <stdint.h>
#include <stdbool.h>

extern int printf(const char*, ...);

#include "checkpoint_extstate.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
    else { printf("  ok:   %s\n", msg); } \
} while (0)

static extstate_resource_t open_res(extstate_kind_t kind) {
    extstate_resource_t r;
    r.kind = kind;
    r.flags = EXTSTATE_FLAG_OPEN;
    return r;
}

int main(void) {
    /* === 1. Classification === */
    printf("Test 1: classification\n");
    CHECK(extstate_survives_checkpoint(EXTSTATE_REGULAR_FILE) == true,  "regular file survives");
    CHECK(extstate_survives_checkpoint(EXTSTATE_SOCKET)       == false, "socket does not survive");
    CHECK(extstate_survives_checkpoint(EXTSTATE_DMA_BUFFER)   == false, "DMA buffer does not survive");
    CHECK(extstate_survives_checkpoint(EXTSTATE_DEVICE)       == false, "device does not survive");
    CHECK(extstate_survives_checkpoint(EXTSTATE_PIPE)         == false, "pipe does not survive");
    CHECK(extstate_survives_checkpoint((extstate_kind_t)999)  == false, "unknown kind fails safe");

    /* === 2. sever() rules === */
    printf("Test 2: sever rules\n");
    {
        extstate_resource_t sock = open_res(EXTSTATE_SOCKET);
        CHECK(extstate_sever(&sock) == true, "open socket is severed");
        CHECK((sock.flags & EXTSTATE_FLAG_SEVERED) != 0, "severed flag set");
        CHECK(extstate_sever(&sock) == false, "second sever is a no-op (idempotent)");

        extstate_resource_t file = open_res(EXTSTATE_REGULAR_FILE);
        CHECK(extstate_sever(&file) == false, "open file is not severed");
        CHECK((file.flags & EXTSTATE_FLAG_SEVERED) == 0, "file keeps its handle");

        extstate_resource_t closed = {EXTSTATE_SOCKET, 0 /* not open */};
        CHECK(extstate_sever(&closed) == false, "closed handle is not severed");

        CHECK(extstate_sever(0) == false, "NULL is safe");
    }

    /* === 3. sever_all over a process's mixed handle set === */
    printf("Test 3: sever_all\n");
    {
        extstate_resource_t fds[5];
        fds[0] = open_res(EXTSTATE_REGULAR_FILE); /* survives */
        fds[1] = open_res(EXTSTATE_SOCKET);       /* severed */
        fds[2] = open_res(EXTSTATE_DMA_BUFFER);   /* severed */
        fds[3] = open_res(EXTSTATE_DEVICE);       /* severed */
        fds[4].kind = EXTSTATE_SOCKET; fds[4].flags = 0; /* closed: untouched */

        int n = extstate_sever_all(fds, 5);
        CHECK(n == 3, "exactly 3 non-persistable open handles severed");
        CHECK(extstate_status(&fds[0]) == EXTSTATE_OK,       "file still usable");
        CHECK(extstate_status(&fds[1]) == EXTSTATE_SEVERED,  "socket reported severed");
        CHECK(extstate_status(&fds[2]) == EXTSTATE_SEVERED,  "DMA reported severed");
        CHECK(extstate_status(&fds[3]) == EXTSTATE_SEVERED,  "device reported severed");
        CHECK(extstate_status(&fds[4]) == EXTSTATE_OK,       "closed handle not severed");

        CHECK(extstate_sever_all(0, 5) == 0, "NULL array is safe");
        CHECK(extstate_sever_all(fds, 0) == 0, "zero count is safe");
    }

    /* === 4. File-descriptor flags integration (#128) === */
    printf("Test 4: fd-flags severing\n");
    {
        /* Kind round-trips through the flags word without clobbering low bits. */
        uint32_t f = 0x00000002; /* e.g. an O_RDWR-ish open flag in the low bits */
        extstate_fd_set_kind(&f, EXTSTATE_SOCKET);
        CHECK(extstate_kind_from_fd_flags(f) == EXTSTATE_SOCKET, "kind stored/read back");
        CHECK((f & 0xFFFF) == 0x0002, "low (open) flag bits preserved");

        /* A socket fd severs and then reports severed. */
        CHECK(extstate_sever_fd(&f) == true, "socket fd severed on restore");
        CHECK(extstate_fd_is_severed(f) == true, "socket fd reports severed");
        CHECK(extstate_sever_fd(&f) == false, "second sever is a no-op");

        /* A regular-file fd is untouched and usable. */
        uint32_t file = 0;
        extstate_fd_set_kind(&file, EXTSTATE_REGULAR_FILE);
        CHECK(extstate_sever_fd(&file) == false, "regular-file fd not severed");
        CHECK(extstate_fd_is_severed(file) == false, "regular-file fd usable");

        /* Device and DMA fds also sever. */
        uint32_t dev = 0, dma = 0;
        extstate_fd_set_kind(&dev, EXTSTATE_DEVICE);
        extstate_fd_set_kind(&dma, EXTSTATE_DMA_BUFFER);
        CHECK(extstate_sever_fd(&dev) == true, "device fd severed");
        CHECK(extstate_sever_fd(&dma) == true, "DMA fd severed");

        CHECK(extstate_sever_fd(0) == false, "NULL flags safe");
    }

    /* === 5. DMA / IPC endpoints + reconnection contract (#146) === */
    printf("Test 5: DMA / IPC endpoint severing\n");
    {
        /* A process holding an in-flight DMA mapping, a device handle, an IPC
         * socket endpoint, and a regular-file-backed mapping. */
        extstate_endpoint_t eps[4];
        eps[0].kind = EXTSTATE_DMA_BUFFER; eps[0].flags = EXTSTATE_FLAG_OPEN; eps[0].owner = 7; eps[0].generation = 0;
        eps[1].kind = EXTSTATE_DEVICE;     eps[1].flags = EXTSTATE_FLAG_OPEN; eps[1].owner = 7; eps[1].generation = 0;
        eps[2].kind = EXTSTATE_SOCKET;     eps[2].flags = EXTSTATE_FLAG_OPEN; eps[2].owner = 7; eps[2].generation = 0;
        eps[3].kind = EXTSTATE_REGULAR_FILE; eps[3].flags = EXTSTATE_FLAG_OPEN; eps[3].owner = 7; eps[3].generation = 0;

        CHECK(extstate_endpoint_persistable(&eps[0]) == false, "DMA endpoint non-persistable");
        CHECK(extstate_endpoint_persistable(&eps[3]) == true,  "file-backed endpoint persistable");

        int n = extstate_endpoint_sever_all(eps, 4);
        CHECK(n == 3, "the DMA, device, and socket endpoints are severed");
        CHECK(extstate_endpoint_status(&eps[0]) == EXTSTATE_SEVERED, "DMA reports severed");
        CHECK(extstate_endpoint_status(&eps[3]) == EXTSTATE_OK,      "file-backed endpoint untouched");
        CHECK(extstate_endpoint_needs_reconnect(&eps[2]) == true,    "socket needs reconnect");

        /* Idempotent: a second restore pass severs nothing new. */
        CHECK(extstate_endpoint_sever_all(eps, 4) == 0, "second sever pass is a no-op");
        CHECK(extstate_endpoint_sever(0) == false, "NULL endpoint safe");
        CHECK(extstate_endpoint_sever_all(0, 4) == 0, "NULL array safe");
    }

    printf("Test 6: reconnection contract\n");
    {
        extstate_endpoint_t dma;
        dma.kind = EXTSTATE_DMA_BUFFER; dma.flags = EXTSTATE_FLAG_OPEN; dma.owner = 9; dma.generation = 0;

        /* Restore severs it; the owner observes the defined error. */
        CHECK(extstate_endpoint_sever(&dma) == true, "DMA severed on restore");
        CHECK(extstate_endpoint_status(&dma) == EXTSTATE_SEVERED, "owner sees EXTSTATE_SEVERED");

        /* Owner re-establishes the mapping, then calls reconnect. */
        CHECK(extstate_endpoint_reconnect(&dma) == true, "reconnect transitions from severed");
        CHECK(extstate_endpoint_status(&dma) == EXTSTATE_OK, "endpoint usable again");
        CHECK(extstate_endpoint_needs_reconnect(&dma) == false, "no longer needs reconnect");
        CHECK((dma.flags & EXTSTATE_FLAG_RECONNECTED) != 0, "reconnected flag set");
        CHECK(dma.generation == 1, "generation bumped so stale refs can tell");

        /* A second reconnect is a safe no-op. */
        CHECK(extstate_endpoint_reconnect(&dma) == false, "double reconnect is a no-op");
        CHECK(dma.generation == 1, "generation not bumped twice");

        /* A fresh checkpoint can sever the reconnected endpoint again. */
        CHECK(extstate_endpoint_sever(&dma) == true, "reconnected endpoint can be re-severed");
        CHECK((dma.flags & EXTSTATE_FLAG_RECONNECTED) == 0, "re-sever clears the reconnected flag");

        CHECK(extstate_endpoint_reconnect(0) == false, "NULL reconnect safe");
    }

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
