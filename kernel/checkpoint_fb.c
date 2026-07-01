/* IKOS Orthogonal Persistence v2 - Framebuffer re-attach + repaint (#145)
 *
 * Pure reconcile core. No kernel deps. See include/checkpoint_fb.h.
 */

#include "checkpoint_fb.h"

int checkpoint_fb_validate(const checkpoint_fb_state_t* st) {
    if (!st) {
        return CHECKPOINT_FB_ERR_PARAM;
    }
    if (!st->valid) {
        /* No framebuffer was captured; not an error, just nothing to repaint. */
        return CHECKPOINT_FB_OK;
    }
    if (st->width == 0 || st->height == 0 || st->bpp == 0 ||
        st->pitch == 0 || st->size == 0) {
        return CHECKPOINT_FB_ERR_STATE;
    }
    if (st->size != st->pitch * st->height) {
        return CHECKPOINT_FB_ERR_STATE;
    }
    return CHECKPOINT_FB_OK;
}

int checkpoint_fb_capture_state(checkpoint_fb_state_t* st,
                                uint32_t mode, uint32_t width, uint32_t height,
                                uint32_t bpp, uint32_t pitch, uint32_t size) {
    if (!st) {
        return CHECKPOINT_FB_ERR_PARAM;
    }
    st->width    = width;
    st->height   = height;
    st->bpp      = bpp;
    st->pitch    = pitch;
    st->size     = size;
    st->mode     = mode;
    st->valid    = 1;
    st->reserved = 0;
    return checkpoint_fb_validate(st);
}

int checkpoint_fb_quiesce(void) {
    /* The pixel buffer is plain RAM captured with kernel state; nothing to stop. */
    return CHECKPOINT_FB_OK;
}

int checkpoint_fb_reattach(const checkpoint_fb_state_t* st,
                           const void* shadow,
                           const checkpoint_fb_ops_t* ops) {
    if (!st || !ops || !ops->set_mode || !ops->blit) {
        return CHECKPOINT_FB_ERR_PARAM;
    }
    if (!st->valid) {
        return CHECKPOINT_FB_OK; /* nothing was persisted to repaint */
    }

    int vc = checkpoint_fb_validate(st);
    if (vc != CHECKPOINT_FB_OK) {
        return vc;
    }
    if (!shadow) {
        return CHECKPOINT_FB_ERR_PARAM; /* valid geometry but no pixels to blit */
    }

    /* Re-program the video mode the display had at checkpoint time. The MMIO
     * aperture and controller registers are meaningless after a power cut. */
    if (ops->set_mode(ops->ctx, st->mode, st->width, st->height, st->bpp) != 0) {
        return CHECKPOINT_FB_ERR_MODE;
    }
    /* Repaint: blit the persisted pixel shadow back into fresh video memory. */
    if (ops->blit(ops->ctx, shadow, st->size) != 0) {
        return CHECKPOINT_FB_ERR_BLIT;
    }
    return CHECKPOINT_FB_OK;
}
