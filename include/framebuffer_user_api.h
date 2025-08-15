/* IKOS Framebuffer User-Space API
 * Issue #26 - Display (Framebuffer) Driver
 * 
 * User-space interface for applications to draw to the screen
 * using the framebuffer driver.
 */

#ifndef FRAMEBUFFER_USER_API_H
#define FRAMEBUFFER_USER_API_H

#include <stdint.h>
#include <stdbool.h>

/* ================================
 * User-Space Framebuffer API
 * ================================ */

/* Mirror basic types from kernel framebuffer.h for user space */
typedef union {
    struct {
        uint8_t b, g, r, a;
    } bgra;
    struct {
        uint8_t r, g, b, a;
    } rgba;
    struct {
        uint16_t value;
    } rgb16;
    uint32_t value32;
    uint8_t value8;
} fb_user_color_t;

typedef struct {
    int32_t x, y;
} fb_user_point_t;

typedef struct {
    int32_t x, y;
    uint32_t width, height;
} fb_user_rect_t;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t pitch;
} fb_user_info_t;

/* Framebuffer system call numbers */
#define SYSCALL_FB_INIT         0x80
#define SYSCALL_FB_GET_INFO     0x81
#define SYSCALL_FB_SET_MODE     0x82
#define SYSCALL_FB_CLEAR        0x83
#define SYSCALL_FB_SET_PIXEL    0x84
#define SYSCALL_FB_GET_PIXEL    0x85
#define SYSCALL_FB_DRAW_LINE    0x86
#define SYSCALL_FB_DRAW_RECT    0x87
#define SYSCALL_FB_FILL_RECT    0x88
#define SYSCALL_FB_DRAW_CIRCLE  0x89
#define SYSCALL_FB_FILL_CIRCLE  0x8A
#define SYSCALL_FB_DRAW_CHAR    0x8B
#define SYSCALL_FB_DRAW_STRING  0x8C
#define SYSCALL_FB_SWAP_BUFFERS 0x8D

/* Mode constants for user space */
#define FB_USER_MODE_TEXT        0
#define FB_USER_MODE_VGA_GRAPHICS 1
#define FB_USER_MODE_VESA_LFB    2

/* Predefined colors for user space */
#define FB_USER_COLOR_BLACK     0x00000000
#define FB_USER_COLOR_WHITE     0x00FFFFFF
#define FB_USER_COLOR_RED       0x00FF0000
#define FB_USER_COLOR_GREEN     0x0000FF00
#define FB_USER_COLOR_BLUE      0x000000FF
#define FB_USER_COLOR_YELLOW    0x00FFFF00
#define FB_USER_COLOR_CYAN      0x0000FFFF
#define FB_USER_COLOR_MAGENTA   0x00FF00FF

/* ================================
 * User-Space API Functions
 * ================================ */

/**
 * Initialize framebuffer for user application
 */
int fb_user_init(void);

/**
 * Get framebuffer information
 */
int fb_user_get_info(fb_user_info_t* info);

/**
 * Set framebuffer mode
 */
int fb_user_set_mode(int mode, uint32_t width, uint32_t height, uint32_t bpp);

/**
 * Clear screen with specified color
 */
int fb_user_clear(fb_user_color_t color);

/**
 * Set pixel at specified coordinates
 */
int fb_user_set_pixel(uint32_t x, uint32_t y, fb_user_color_t color);

/**
 * Get pixel at specified coordinates
 */
fb_user_color_t fb_user_get_pixel(uint32_t x, uint32_t y);

/**
 * Draw line between two points
 */
int fb_user_draw_line(fb_user_point_t start, fb_user_point_t end, fb_user_color_t color);

/**
 * Draw rectangle outline
 */
int fb_user_draw_rect(fb_user_rect_t rect, fb_user_color_t color);

/**
 * Fill rectangle with solid color
 */
int fb_user_fill_rect(fb_user_rect_t rect, fb_user_color_t color);

/**
 * Draw circle outline
 */
int fb_user_draw_circle(fb_user_point_t center, uint32_t radius, fb_user_color_t color);

/**
 * Fill circle with solid color
 */
int fb_user_fill_circle(fb_user_point_t center, uint32_t radius, fb_user_color_t color);

/**
 * Draw single character
 */
int fb_user_draw_char(uint32_t x, uint32_t y, char c, fb_user_color_t fg_color, fb_user_color_t bg_color);

/**
 * Draw string
 */
int fb_user_draw_string(uint32_t x, uint32_t y, const char* str, fb_user_color_t fg_color, fb_user_color_t bg_color);

/**
 * Swap front and back buffers (if double buffering is enabled)
 */
int fb_user_swap_buffers(void);

/**
 * Color utility functions
 */
fb_user_color_t fb_user_rgb(uint8_t r, uint8_t g, uint8_t b);
fb_user_color_t fb_user_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

#endif /* FRAMEBUFFER_USER_API_H */
