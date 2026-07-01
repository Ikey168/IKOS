/* Host-side test for the framebuffer repaint reconcile core (#145).
 *
 * Pure logic, no kernel deps. Verifies geometry capture/validation, the
 * quiesce no-op, and that reattach re-programs the mode then blits the persisted
 * pixel shadow into a mock "video memory" so it ends up byte-identical, plus
 * the failure modes (bad geometry, mode-set failure, blit failure, no capture).
 *
 * Build: gcc -I../include -o test_checkpoint_fb test_checkpoint_fb.c \
 *            ../kernel/checkpoint_fb.c
 */

#include <stdint.h>
#include <stdbool.h>
extern int printf(const char*, ...);

#include "checkpoint_fb.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
    else { printf("  ok:   %s\n", msg); } \
} while (0)

/* Mock display. video[] models the MMIO aperture the blit repaints into. */
#define VMEM 4096
static unsigned char g_video[VMEM];
static int g_setmode_calls, g_blit_calls;
static uint32_t g_last_mode, g_last_w, g_last_h, g_last_bpp, g_last_blit_bytes;
static int g_setmode_fail, g_blit_fail;

static void mock_reset(void) {
    g_setmode_calls = g_blit_calls = 0;
    g_last_mode = g_last_w = g_last_h = g_last_bpp = g_last_blit_bytes = 0;
    g_setmode_fail = g_blit_fail = 0;
    for (int i = 0; i < VMEM; i++) g_video[i] = 0;
}

static int mock_set_mode(void* ctx, uint32_t mode, uint32_t w, uint32_t h, uint32_t bpp) {
    (void)ctx;
    g_setmode_calls++;
    g_last_mode = mode; g_last_w = w; g_last_h = h; g_last_bpp = bpp;
    return g_setmode_fail ? -1 : 0;
}
static int mock_blit(void* ctx, const void* pixels, uint32_t bytes) {
    (void)ctx;
    g_blit_calls++; g_last_blit_bytes = bytes;
    if (g_blit_fail) return -1;
    const unsigned char* s = (const unsigned char*)pixels;
    for (uint32_t i = 0; i < bytes && i < VMEM; i++) g_video[i] = s[i];
    return 0;
}

static checkpoint_fb_ops_t make_ops(void) {
    checkpoint_fb_ops_t o;
    o.set_mode = mock_set_mode; o.blit = mock_blit; o.ctx = 0;
    return o;
}

int main(void) {
    /* A small persisted framebuffer: 16 x 8 x 32bpp, pitch 64, size 512. */
    unsigned char shadow[512];
    for (int i = 0; i < 512; i++) shadow[i] = (unsigned char)(i * 7 + 3);

    checkpoint_fb_state_t st;
    checkpoint_fb_ops_t o;

    printf("Test 1: capture + validate geometry\n");
    CHECK(checkpoint_fb_capture_state(&st, 1, 16, 8, 32, 64, 512) == CHECKPOINT_FB_OK,
          "capture ok");
    CHECK(st.valid && st.size == 512 && st.pitch == 64, "fields recorded");
    CHECK(checkpoint_fb_validate(&st) == CHECKPOINT_FB_OK, "validate ok");

    printf("Test 2: inconsistent geometry rejected\n");
    checkpoint_fb_state_t bad = st; bad.size = 500; /* != pitch*height */
    CHECK(checkpoint_fb_validate(&bad) == CHECKPOINT_FB_ERR_STATE, "size mismatch rejected");
    bad = st; bad.bpp = 0;
    CHECK(checkpoint_fb_validate(&bad) == CHECKPOINT_FB_ERR_STATE, "zero bpp rejected");

    printf("Test 3: quiesce is a no-op success\n");
    CHECK(checkpoint_fb_quiesce() == CHECKPOINT_FB_OK, "quiesce ok");

    printf("Test 4: reattach re-programs mode then repaints\n");
    mock_reset(); o = make_ops();
    CHECK(checkpoint_fb_reattach(&st, shadow, &o) == CHECKPOINT_FB_OK, "reattach ok");
    CHECK(g_setmode_calls == 1 && g_blit_calls == 1, "mode set then blit, once each");
    CHECK(g_last_mode == 1 && g_last_w == 16 && g_last_h == 8 && g_last_bpp == 32,
          "mode re-programmed from saved geometry");
    CHECK(g_last_blit_bytes == 512, "blitted the full pixel buffer");
    int identical = 1;
    for (int i = 0; i < 512; i++) if (g_video[i] != shadow[i]) identical = 0;
    CHECK(identical, "video memory now byte-identical to the persisted shadow");

    printf("Test 5: mode-set failure stops before blit\n");
    mock_reset(); g_setmode_fail = 1; o = make_ops();
    CHECK(checkpoint_fb_reattach(&st, shadow, &o) == CHECKPOINT_FB_ERR_MODE, "mode failure reported");
    CHECK(g_blit_calls == 0, "no repaint after a failed mode set");

    printf("Test 6: blit failure reported\n");
    mock_reset(); g_blit_fail = 1; o = make_ops();
    CHECK(checkpoint_fb_reattach(&st, shadow, &o) == CHECKPOINT_FB_ERR_BLIT, "blit failure reported");

    printf("Test 7: no capture is a no-op success\n");
    mock_reset(); o = make_ops();
    checkpoint_fb_state_t none; none.valid = 0;
    CHECK(checkpoint_fb_reattach(&none, 0, &o) == CHECKPOINT_FB_OK, "absent capture skipped");
    CHECK(g_setmode_calls == 0 && g_blit_calls == 0, "nothing touched when nothing persisted");

    printf("Test 8: valid geometry but missing pixels rejected\n");
    o = make_ops();
    CHECK(checkpoint_fb_reattach(&st, 0, &o) == CHECKPOINT_FB_ERR_PARAM, "null shadow rejected");

    printf("Test 9: NULL args\n");
    o = make_ops();
    CHECK(checkpoint_fb_reattach(0, shadow, &o) == CHECKPOINT_FB_ERR_PARAM, "null state rejected");
    CHECK(checkpoint_fb_reattach(&st, shadow, 0) == CHECKPOINT_FB_ERR_PARAM, "null ops rejected");

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
