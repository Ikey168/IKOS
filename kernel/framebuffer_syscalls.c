/* IKOS Framebuffer System Calls
 * Issue #26 - Display (Framebuffer) Driver
 * 
 * System call implementations for framebuffer operations.
 * Bridges user-space API to kernel framebuffer driver.
 */

#include "../include/framebuffer.h"
#include "../include/framebuffer_user_api.h"
#include "../include/syscalls.h"
#include "../include/memory.h"
#include <string.h>

/* ================================
 * Helper Functions
 * ================================ */

/**
 * Unpack coordinates from packed integer
 */
static void unpack_coordinates(int packed, int32_t* x, int32_t* y) {
    *x = (int32_t)(packed >> 16);
    *y = (int32_t)(packed & 0xFFFF);
    
    /* Handle sign extension for negative coordinates */
    if (*x & 0x8000) *x |= 0xFFFF0000;
    if (*y & 0x8000) *y |= 0xFFFF0000;
}

/**
 * Unpack size from packed integer
 */
static void unpack_size(int packed, uint32_t* width, uint32_t* height) {
    *width = (uint32_t)(packed >> 16);
    *height = (uint32_t)(packed & 0xFFFF);
}

/**
 * Convert user color to kernel color
 */
static fb_color_t user_to_kernel_color(uint32_t user_color) {
    fb_color_t color;
    color.value32 = user_color;
    return color;
}

/**
 * Convert kernel color to user color
 */
static uint32_t kernel_to_user_color(fb_color_t kernel_color) {
    return kernel_color.value32;
}

/* ================================
 * Framebuffer System Calls
 * ================================ */

/**
 * System call: Initialize framebuffer
 */
int syscall_fb_init(void) {
    return fb_init();
}

/**
 * System call: Get framebuffer information
 */
int syscall_fb_get_info(fb_user_info_t* user_info) {
    if (!user_info) {
        return FB_ERROR_INVALID_PARAMS;
    }
    
    /* Validate user pointer */
    /* In a real OS, this would check if the pointer is in valid user space */
    
    fb_info_t* kernel_info = fb_get_info();
    if (!kernel_info) {
        return FB_ERROR_NOT_INITIALIZED;
    }
    
    /* Copy kernel info to user space structure */
    user_info->width = kernel_info->width;
    user_info->height = kernel_info->height;
    user_info->bpp = kernel_info->bpp;
    user_info->pitch = kernel_info->pitch;
    
    return FB_SUCCESS;
}

/**
 * System call: Set framebuffer mode
 */
int syscall_fb_set_mode(int mode, uint32_t width, uint32_t height, uint32_t bpp) {
    fb_mode_t kernel_mode;
    
    /* Convert user mode to kernel mode */
    switch (mode) {
        case FB_USER_MODE_TEXT:
            kernel_mode = FB_MODE_TEXT;
            break;
        case FB_USER_MODE_VGA_GRAPHICS:
            kernel_mode = FB_MODE_VGA_GRAPHICS;
            break;
        case FB_USER_MODE_VESA_LFB:
            kernel_mode = FB_MODE_VESA_LFB;
            break;
        default:
            return FB_ERROR_INVALID_MODE;
    }
    
    return fb_set_mode(kernel_mode, width, height, bpp);
}

/**
 * System call: Clear framebuffer
 */
int syscall_fb_clear(uint32_t color_value) {
    fb_color_t color = user_to_kernel_color(color_value);
    return fb_clear(color);
}

/**
 * System call: Set pixel
 */
int syscall_fb_set_pixel(uint32_t x, uint32_t y, uint32_t color_value) {
    fb_color_t color = user_to_kernel_color(color_value);
    return fb_set_pixel(x, y, color);
}

/**
 * System call: Get pixel
 */
uint32_t syscall_fb_get_pixel(uint32_t x, uint32_t y) {
    fb_color_t color = fb_get_pixel(x, y);
    return kernel_to_user_color(color);
}

/**
 * System call: Draw line
 */
int syscall_fb_draw_line(int start_packed, int end_packed, uint32_t color_value) {
    fb_point_t start, end;
    unpack_coordinates(start_packed, &start.x, &start.y);
    unpack_coordinates(end_packed, &end.x, &end.y);
    
    fb_color_t color = user_to_kernel_color(color_value);
    
    return fb_draw_line(start, end, color);
}

/**
 * System call: Draw rectangle
 */
int syscall_fb_draw_rect(int pos_packed, int size_packed, uint32_t color_value) {
    fb_rect_t rect;
    unpack_coordinates(pos_packed, &rect.x, &rect.y);
    unpack_size(size_packed, &rect.width, &rect.height);
    
    fb_color_t color = user_to_kernel_color(color_value);
    
    return fb_draw_rect(rect, color);
}

/**
 * System call: Fill rectangle
 */
int syscall_fb_fill_rect(int pos_packed, int size_packed, uint32_t color_value) {
    fb_rect_t rect;
    unpack_coordinates(pos_packed, &rect.x, &rect.y);
    unpack_size(size_packed, &rect.width, &rect.height);
    
    fb_color_t color = user_to_kernel_color(color_value);
    
    return fb_fill_rect(rect, color);
}

/**
 * System call: Draw circle
 */
int syscall_fb_draw_circle(int center_packed, uint32_t radius, uint32_t color_value) {
    fb_point_t center;
    unpack_coordinates(center_packed, &center.x, &center.y);
    
    fb_color_t color = user_to_kernel_color(color_value);
    
    return fb_draw_circle(center, radius, color);
}

/**
 * System call: Fill circle
 */
int syscall_fb_fill_circle(int center_packed, uint32_t radius, uint32_t color_value) {
    fb_point_t center;
    unpack_coordinates(center_packed, &center.x, &center.y);
    
    fb_color_t color = user_to_kernel_color(color_value);
    
    return fb_fill_circle(center, radius, color);
}

/**
 * System call: Draw character
 */
int syscall_fb_draw_char(int pos_packed, char c, uint32_t colors_packed) {
    int32_t x, y;
    unpack_coordinates(pos_packed, &x, &y);
    
    /* Unpack foreground and background colors */
    fb_color_t fg_color = user_to_kernel_color(colors_packed & 0xFFFFFF);
    fb_color_t bg_color = user_to_kernel_color((colors_packed >> 24) & 0xFF);
    
    return fb_draw_char(x, y, c, fg_color, bg_color, NULL);
}

/**
 * System call: Draw string
 */
int syscall_fb_draw_string(int pos_packed, const char* str, uint32_t colors_packed) {
    if (!str) {
        return FB_ERROR_INVALID_PARAMS;
    }
    
    int32_t x, y;
    unpack_coordinates(pos_packed, &x, &y);
    
    /* Unpack foreground and background colors */
    fb_color_t fg_color = user_to_kernel_color(colors_packed & 0xFFFFFF);
    fb_color_t bg_color = user_to_kernel_color((colors_packed >> 24) & 0xFF);
    
    /* Validate string pointer */
    /* In a real OS, this would validate the user space string */
    
    return fb_draw_string(x, y, str, fg_color, bg_color, NULL);
}

/**
 * System call: Swap buffers
 */
int syscall_fb_swap_buffers(void) {
    return fb_swap_buffers();
}

/* ================================
 * System Call Handler Registration
 * ================================ */

/**
 * Register framebuffer system calls with the kernel
 */
void register_framebuffer_syscalls(void) {
    /* This would register the system calls with the kernel's syscall table */
    /* For now, this is a placeholder that would be called during kernel init */
    
    /* Example registration (pseudo-code):
     * register_syscall(SYSCALL_FB_INIT, syscall_fb_init);
     * register_syscall(SYSCALL_FB_GET_INFO, syscall_fb_get_info);
     * register_syscall(SYSCALL_FB_SET_MODE, syscall_fb_set_mode);
     * register_syscall(SYSCALL_FB_CLEAR, syscall_fb_clear);
     * register_syscall(SYSCALL_FB_SET_PIXEL, syscall_fb_set_pixel);
     * register_syscall(SYSCALL_FB_GET_PIXEL, syscall_fb_get_pixel);
     * register_syscall(SYSCALL_FB_DRAW_LINE, syscall_fb_draw_line);
     * register_syscall(SYSCALL_FB_DRAW_RECT, syscall_fb_draw_rect);
     * register_syscall(SYSCALL_FB_FILL_RECT, syscall_fb_fill_rect);
     * register_syscall(SYSCALL_FB_DRAW_CIRCLE, syscall_fb_draw_circle);
     * register_syscall(SYSCALL_FB_FILL_CIRCLE, syscall_fb_fill_circle);
     * register_syscall(SYSCALL_FB_DRAW_CHAR, syscall_fb_draw_char);
     * register_syscall(SYSCALL_FB_DRAW_STRING, syscall_fb_draw_string);
     * register_syscall(SYSCALL_FB_SWAP_BUFFERS, syscall_fb_swap_buffers);
     */
}
