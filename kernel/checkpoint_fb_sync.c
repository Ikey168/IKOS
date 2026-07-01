/* IKOS Orthogonal Persistence v2 - Framebuffer re-attach adapter (#145)
 *
 * Wires the pure repaint core (checkpoint_fb.c) to the real framebuffer driver:
 * captures the geometry + a plain-RAM shadow of the pixels into the checkpoint
 * (#139 kernel blobs), and registers the display's persist_ops (#143) so that on
 * restore the mode is re-programmed and the persisted pixels are blitted back
 * into fresh video memory. Compile-checked freestanding; the visible repaint is
 * validated in QEMU (the demo GIF).
 */

#include "checkpoint_fb.h"
#include "checkpoint_driver.h"
#include "checkpoint.h"        /* checkpoint_capture_kernel_blob */
#include "framebuffer.h"       /* fb_info_t, fb_get_info, fb_set_mode */

/* Saved geometry and the restored pixel shadow used by the reattach thunk. The
 * capture path fills g_fb_state in-run; the restore path (#147) repopulates both
 * from the reassembled kernel blobs before the registry drives reattach. */
static checkpoint_fb_state_t g_fb_state;
static const void*           g_fb_shadow;

/* Byte copy; the freestanding build has no libc. Screen-sized but only ever runs
 * once per restore. */
static void fb_bytecopy(void* dst, const void* src, uint32_t n) {
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    for (uint32_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

/* ----- hardware ops for the pure core ----- */

static int fb_do_set_mode(void* ctx, uint32_t mode,
                          uint32_t width, uint32_t height, uint32_t bpp) {
    (void)ctx;
    return fb_set_mode((fb_mode_t)mode, width, height, bpp);
}

static int fb_do_blit(void* ctx, const void* pixels, uint32_t bytes) {
    (void)ctx;
    fb_info_t* info = fb_get_info();
    if (!info || !info->buffer) {
        return -1;
    }
    fb_bytecopy(info->buffer, pixels, bytes);
    return 0;
}

/* ----- persist_ops (#143) thunks ----- */

static int fb_persist_quiesce(void* dev) {
    (void)dev;
    return checkpoint_fb_quiesce();
}

static int fb_persist_reattach(void* dev) {
    (void)dev;
    checkpoint_fb_ops_t ops;
    ops.set_mode = fb_do_set_mode;
    ops.blit     = fb_do_blit;
    ops.ctx      = 0;
    return checkpoint_fb_reattach(&g_fb_state, g_fb_shadow, &ops);
}

/* ----- capture ----- */

int checkpoint_fb_capture(void) {
    fb_info_t* info = fb_get_info();
    if (!info || !info->initialized) {
        return -1;
    }

    int rc = checkpoint_fb_capture_state(&g_fb_state, (uint32_t)info->mode,
                                         info->width, info->height, info->bpp,
                                         info->pitch, info->size);
    if (rc != CHECKPOINT_FB_OK) {
        return rc;
    }

    /* The persisted pixel source is the plain-RAM back buffer when double
     * buffered; otherwise read straight from the (readable) video aperture. */
    const void* pixels = info->double_buffered && info->back_buffer
                       ? info->back_buffer : info->buffer;
    if (!pixels) {
        return -1;
    }

    rc = checkpoint_capture_kernel_blob(CHECKPOINT_TAG_FRAMEBUFFER,
                                        &g_fb_state, (uint32_t)sizeof(g_fb_state));
    if (rc != 0) {
        return rc;
    }
    return checkpoint_capture_kernel_blob(CHECKPOINT_TAG_FRAMEBUFFER_PIXELS,
                                          pixels, g_fb_state.size);
}

int checkpoint_fb_register(void) {
    static const checkpoint_persist_ops_t ops = {
        fb_persist_quiesce, fb_persist_reattach, true
    };
    return checkpoint_driver_register(checkpoint_driver_global(), &g_fb_state, &ops);
}

/* ----- restore (driven by #147) ----- */

int checkpoint_fb_restore_state(const void* blob, uint32_t size) {
    if (!blob || size != sizeof(g_fb_state)) {
        return -1;
    }
    fb_bytecopy(&g_fb_state, blob, (uint32_t)sizeof(g_fb_state));
    return checkpoint_fb_validate(&g_fb_state);
}

int checkpoint_fb_restore_pixels(const void* pixels, uint32_t size) {
    if (!pixels || size < g_fb_state.size) {
        return -1;
    }
    g_fb_shadow = pixels;
    return 0;
}
