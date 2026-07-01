/* IKOS Orthogonal Persistence v2 - Framebuffer re-attach + repaint (#145)
 *
 * The headline whole-machine moment: on restore, re-initialize the video mode
 * and repaint the screen from the persisted pixel buffer, so the desktop comes
 * back exactly as it was before the power cut (orthogonal-persistence-v2.md
 * section 4).
 *
 * Video memory (the MMIO aperture at fb_info.buffer) is a hardware region and is
 * NOT persisted. Instead a plain-RAM shadow of the pixels is captured into the
 * checkpoint as kernel state (#139/#140) alongside the framebuffer geometry.
 * On restore this core re-programs the mode from the saved geometry and blits
 * the persisted shadow back into the freshly re-initialized video memory.
 *
 * The mode-set and blit are injected through a vtable so the reconcile logic is
 * a pure, host-testable core. The kernel adapter (checkpoint_fb_sync.c) wires
 * the real framebuffer.c calls in, captures the shadow, and registers the
 * display into the global driver registry (#143).
 */

#ifndef CHECKPOINT_FB_H
#define CHECKPOINT_FB_H

#include <stdint.h>

/* Kernel-blob tags (see checkpoint_capture_kernel_blob, #139). 1..3 are taken
 * by the proc/file/ipc tables; the framebuffer uses 4 (geometry) and 5 (pixels). */
#define CHECKPOINT_TAG_FRAMEBUFFER        4
#define CHECKPOINT_TAG_FRAMEBUFFER_PIXELS 5

#define CHECKPOINT_FB_OK          0
#define CHECKPOINT_FB_ERR_PARAM  -1
#define CHECKPOINT_FB_ERR_STATE  -2   /* saved geometry is inconsistent */
#define CHECKPOINT_FB_ERR_MODE   -3   /* re-init of the video mode failed */
#define CHECKPOINT_FB_ERR_BLIT   -4   /* repaint into video memory failed */

/* Portable framebuffer geometry captured at checkpoint time and persisted as a
 * kernel-state blob. `size` must equal pitch * height and bounds the shadow. */
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t pitch;    /* bytes per scanline */
    uint32_t size;     /* total pixel bytes = pitch * height */
    uint32_t mode;     /* fb_mode_t value to restore */
    uint32_t valid;    /* nonzero once a checkpoint captured a framebuffer */
    uint32_t reserved;
} checkpoint_fb_state_t;

/* Hardware operations, injected so the reconcile logic is host-testable.
 * Each returns 0 on success. */
typedef struct {
    int (*set_mode)(void* ctx, uint32_t mode,
                    uint32_t width, uint32_t height, uint32_t bpp);
    int (*blit)(void* ctx, const void* pixels, uint32_t bytes); /* shadow -> video */
    void* ctx;
} checkpoint_fb_ops_t;

/* Fill *st from framebuffer geometry, marking it valid. Returns
 * CHECKPOINT_FB_OK, or CHECKPOINT_FB_ERR_PARAM / _STATE on bad geometry. */
int checkpoint_fb_capture_state(checkpoint_fb_state_t* st,
                                uint32_t mode, uint32_t width, uint32_t height,
                                uint32_t bpp, uint32_t pitch, uint32_t size);

/* Validate a (deserialized) geometry blob: size == pitch*height, all nonzero. */
int checkpoint_fb_validate(const checkpoint_fb_state_t* st);

/* quiesce: the pixel buffer is plain memory persisted with kernel state, so
 * there is nothing in flight to stop. Always succeeds (kept for symmetry). */
int checkpoint_fb_quiesce(void);

/* reattach: re-init the video mode from *st, then blit the persisted `shadow`
 * (st->size bytes) into video memory via ops. An invalid/absent capture is a
 * no-op success (nothing was persisted to repaint). */
int checkpoint_fb_reattach(const checkpoint_fb_state_t* st,
                           const void* shadow,
                           const checkpoint_fb_ops_t* ops);

/* Kernel adapter (checkpoint_fb_sync.c): capture the current framebuffer
 * geometry + pixel shadow into the checkpoint, and register the display's
 * persist_ops (#143) into the global driver registry. Return 0, or negative. */
int checkpoint_fb_capture(void);
int checkpoint_fb_register(void);

/* Restore path (driven by #147): hand the reassembled kernel blobs back to the
 * adapter so the registered reattach can repaint. checkpoint_fb_restore_state
 * deserializes the geometry blob; checkpoint_fb_restore_pixels caches the
 * persisted pixel shadow the reattach will blit. Return 0, or negative. */
int checkpoint_fb_restore_state(const void* blob, uint32_t size);
int checkpoint_fb_restore_pixels(const void* pixels, uint32_t size);

#endif /* CHECKPOINT_FB_H */
