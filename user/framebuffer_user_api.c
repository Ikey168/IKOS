/* IKOS Framebuffer User-Space API Implementation
 * Issue #26 - Display (Framebuffer) Driver
 * 
 * User-space implementation for applications to draw to the screen.
 */

#include "../include/framebuffer_user_api.h"
#include "../include/syscalls.h"

/* ================================
 * System Call Wrappers
 * ================================ */

/**
 * Make system call with no parameters
 */
static inline int syscall0(int syscall_num) {
    int result;
    __asm__ volatile (
        "int $0x80"
        : "=a" (result)
        : "a" (syscall_num)
        : "memory"
    );
    return result;
}

/**
 * Make system call with one parameter
 */
static inline int syscall1(int syscall_num, int arg1) {
    int result;
    __asm__ volatile (
        "int $0x80"
        : "=a" (result)
        : "a" (syscall_num), "b" (arg1)
        : "memory"
    );
    return result;
}

/**
 * Make system call with two parameters
 */
static inline int syscall2(int syscall_num, int arg1, int arg2) {
    int result;
    __asm__ volatile (
        "int $0x80"
        : "=a" (result)
        : "a" (syscall_num), "b" (arg1), "c" (arg2)
        : "memory"
    );
    return result;
}

/**
 * Make system call with three parameters
 */
static inline int syscall3(int syscall_num, int arg1, int arg2, int arg3) {
    int result;
    __asm__ volatile (
        "int $0x80"
        : "=a" (result)
        : "a" (syscall_num), "b" (arg1), "c" (arg2), "d" (arg3)
        : "memory"
    );
    return result;
}

/**
 * Make system call with four parameters
 */
static inline int syscall4(int syscall_num, int arg1, int arg2, int arg3, int arg4) {
    int result;
    __asm__ volatile (
        "int $0x80"
        : "=a" (result)
        : "a" (syscall_num), "b" (arg1), "c" (arg2), "d" (arg3), "S" (arg4)
        : "memory"
    );
    return result;
}

/**
 * Make system call with five parameters
 */
static inline int syscall5(int syscall_num, int arg1, int arg2, int arg3, int arg4, int arg5) {
    int result;
    __asm__ volatile (
        "int $0x80"
        : "=a" (result)
        : "a" (syscall_num), "b" (arg1), "c" (arg2), "d" (arg3), "S" (arg4), "D" (arg5)
        : "memory"
    );
    return result;
}

/* ================================
 * User-Space API Implementation
 * ================================ */

/**
 * Initialize framebuffer for user application
 */
int fb_user_init(void) {
    return syscall0(SYSCALL_FB_INIT);
}

/**
 * Get framebuffer information
 */
int fb_user_get_info(fb_user_info_t* info) {
    if (!info) {
        return -1;
    }
    
    return syscall1(SYSCALL_FB_GET_INFO, (int)info);
}

/**
 * Set framebuffer mode
 */
int fb_user_set_mode(int mode, uint32_t width, uint32_t height, uint32_t bpp) {
    return syscall4(SYSCALL_FB_SET_MODE, mode, (int)width, (int)height, (int)bpp);
}

/**
 * Clear screen with specified color
 */
int fb_user_clear(fb_user_color_t color) {
    return syscall1(SYSCALL_FB_CLEAR, (int)color.value32);
}

/**
 * Set pixel at specified coordinates
 */
int fb_user_set_pixel(uint32_t x, uint32_t y, fb_user_color_t color) {
    return syscall3(SYSCALL_FB_SET_PIXEL, (int)x, (int)y, (int)color.value32);
}

/**
 * Get pixel at specified coordinates
 */
fb_user_color_t fb_user_get_pixel(uint32_t x, uint32_t y) {
    fb_user_color_t color;
    color.value32 = syscall2(SYSCALL_FB_GET_PIXEL, (int)x, (int)y);
    return color;
}

/**
 * Draw line between two points
 */
int fb_user_draw_line(fb_user_point_t start, fb_user_point_t end, fb_user_color_t color) {
    /* Pack coordinates into integers for system call */
    int start_packed = (start.x << 16) | (start.y & 0xFFFF);
    int end_packed = (end.x << 16) | (end.y & 0xFFFF);
    
    return syscall3(SYSCALL_FB_DRAW_LINE, start_packed, end_packed, (int)color.value32);
}

/**
 * Draw rectangle outline
 */
int fb_user_draw_rect(fb_user_rect_t rect, fb_user_color_t color) {
    /* Pack rectangle coordinates */
    int pos_packed = (rect.x << 16) | (rect.y & 0xFFFF);
    int size_packed = (rect.width << 16) | (rect.height & 0xFFFF);
    
    return syscall3(SYSCALL_FB_DRAW_RECT, pos_packed, size_packed, (int)color.value32);
}

/**
 * Fill rectangle with solid color
 */
int fb_user_fill_rect(fb_user_rect_t rect, fb_user_color_t color) {
    /* Pack rectangle coordinates */
    int pos_packed = (rect.x << 16) | (rect.y & 0xFFFF);
    int size_packed = (rect.width << 16) | (rect.height & 0xFFFF);
    
    return syscall3(SYSCALL_FB_FILL_RECT, pos_packed, size_packed, (int)color.value32);
}

/**
 * Draw circle outline
 */
int fb_user_draw_circle(fb_user_point_t center, uint32_t radius, fb_user_color_t color) {
    int center_packed = (center.x << 16) | (center.y & 0xFFFF);
    
    return syscall3(SYSCALL_FB_DRAW_CIRCLE, center_packed, (int)radius, (int)color.value32);
}

/**
 * Fill circle with solid color
 */
int fb_user_fill_circle(fb_user_point_t center, uint32_t radius, fb_user_color_t color) {
    int center_packed = (center.x << 16) | (center.y & 0xFFFF);
    
    return syscall3(SYSCALL_FB_FILL_CIRCLE, center_packed, (int)radius, (int)color.value32);
}

/**
 * Draw single character
 */
int fb_user_draw_char(uint32_t x, uint32_t y, char c, fb_user_color_t fg_color, fb_user_color_t bg_color) {
    int pos_packed = (x << 16) | (y & 0xFFFF);
    int colors_packed = (fg_color.value32 & 0xFFFFFF) | ((bg_color.value32 & 0xFF) << 24);
    
    return syscall3(SYSCALL_FB_DRAW_CHAR, pos_packed, (int)c, colors_packed);
}

/**
 * Draw string
 */
int fb_user_draw_string(uint32_t x, uint32_t y, const char* str, fb_user_color_t fg_color, fb_user_color_t bg_color) {
    if (!str) {
        return -1;
    }
    
    int pos_packed = (x << 16) | (y & 0xFFFF);
    int colors_packed = (fg_color.value32 & 0xFFFFFF) | ((bg_color.value32 & 0xFF) << 24);
    
    return syscall4(SYSCALL_FB_DRAW_STRING, pos_packed, (int)str, colors_packed, 0);
}

/**
 * Swap front and back buffers
 */
int fb_user_swap_buffers(void) {
    return syscall0(SYSCALL_FB_SWAP_BUFFERS);
}

/**
 * Create RGB color
 */
fb_user_color_t fb_user_rgb(uint8_t r, uint8_t g, uint8_t b) {
    fb_user_color_t color;
    color.rgba.r = r;
    color.rgba.g = g;
    color.rgba.b = b;
    color.rgba.a = 0xFF;
    return color;
}

/**
 * Create RGBA color
 */
fb_user_color_t fb_user_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    fb_user_color_t color;
    color.rgba.r = r;
    color.rgba.g = g;
    color.rgba.b = b;
    color.rgba.a = a;
    return color;
}
