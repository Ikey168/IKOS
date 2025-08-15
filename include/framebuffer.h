/* IKOS Framebuffer Driver
 * Issue #26 - Display (Framebuffer) Driver Implementation
 * 
 * Provides a framebuffer-based display driver for graphical output.
 * Supports basic pixel drawing functions and user-space API.
 */

#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>
#include <stdbool.h>

/* ================================
 * Framebuffer Constants
 * ================================ */

/* VGA Text Mode Base Address */
#define VGA_TEXT_MEMORY     0xB8000
#define VGA_TEXT_WIDTH      80
#define VGA_TEXT_HEIGHT     25

/* VGA Graphics Mode 13h (320x200x8) */
#define VGA_GRAPHICS_MEMORY 0xA0000
#define VGA_GRAPHICS_WIDTH  320
#define VGA_GRAPHICS_HEIGHT 200

/* VESA Linear Framebuffer (typical addresses) */
#define VESA_LFB_BASE       0xE0000000
#define VESA_DEFAULT_WIDTH  1024
#define VESA_DEFAULT_HEIGHT 768
#define VESA_DEFAULT_BPP    32

/* Maximum supported resolution */
#define FB_MAX_WIDTH        1920
#define FB_MAX_HEIGHT       1080
#define FB_MAX_BPP          32

/* Color depths */
#define FB_BPP_8            8
#define FB_BPP_16           16
#define FB_BPP_24           24
#define FB_BPP_32           32

/* Framebuffer modes */
typedef enum {
    FB_MODE_TEXT,           /* VGA text mode 80x25 */
    FB_MODE_VGA_GRAPHICS,   /* VGA graphics mode 320x200x8 */
    FB_MODE_VESA_LFB,       /* VESA linear framebuffer */
    FB_MODE_UNKNOWN
} fb_mode_t;

/* Color format types */
typedef enum {
    FB_FORMAT_RGB,          /* Red-Green-Blue */
    FB_FORMAT_BGR,          /* Blue-Green-Red */
    FB_FORMAT_INDEXED,      /* Palette-based */
    FB_FORMAT_GRAYSCALE
} fb_color_format_t;

/* ================================
 * Framebuffer Structures
 * ================================ */

/* Color structure for different bit depths */
typedef union {
    struct {
        uint8_t b, g, r, a;     /* 32-bit BGRA */
    } bgra;
    struct {
        uint8_t r, g, b, a;     /* 32-bit RGBA */
    } rgba;
    struct {
        uint16_t value;         /* 16-bit color */
    } rgb16;
    uint32_t value32;           /* 32-bit value */
    uint8_t value8;             /* 8-bit indexed */
} fb_color_t;

/* Framebuffer information structure */
typedef struct {
    uint32_t width;             /* Width in pixels */
    uint32_t height;            /* Height in pixels */
    uint32_t bpp;               /* Bits per pixel */
    uint32_t pitch;             /* Bytes per line */
    uint32_t size;              /* Total framebuffer size */
    
    void* buffer;               /* Framebuffer memory address */
    fb_mode_t mode;             /* Current mode */
    fb_color_format_t format;   /* Color format */
    
    /* Color masks for RGB extraction */
    uint32_t red_mask;
    uint32_t green_mask;
    uint32_t blue_mask;
    uint32_t alpha_mask;
    
    /* Shift values for RGB packing */
    uint8_t red_shift;
    uint8_t green_shift;
    uint8_t blue_shift;
    uint8_t alpha_shift;
    
    bool initialized;           /* Initialization state */
    bool double_buffered;       /* Double buffering support */
    void* back_buffer;          /* Back buffer for double buffering */
} fb_info_t;

/* Rectangle structure for drawing operations */
typedef struct {
    int32_t x, y;               /* Top-left corner */
    uint32_t width, height;     /* Dimensions */
} fb_rect_t;

/* Point structure */
typedef struct {
    int32_t x, y;
} fb_point_t;

/* Font structure for text rendering */
typedef struct {
    uint8_t width;              /* Character width */
    uint8_t height;             /* Character height */
    const uint8_t* data;        /* Font bitmap data */
} fb_font_t;

/* ================================
 * Framebuffer Driver API
 * ================================ */

/* Core initialization and management */
int fb_init(void);
int fb_shutdown(void);
int fb_set_mode(fb_mode_t mode, uint32_t width, uint32_t height, uint32_t bpp);
fb_info_t* fb_get_info(void);

/* Mode detection and capability queries */
bool fb_is_mode_supported(fb_mode_t mode, uint32_t width, uint32_t height, uint32_t bpp);
int fb_detect_modes(fb_mode_t* modes, int max_modes);
int fb_get_current_mode(fb_mode_t* mode, uint32_t* width, uint32_t* height, uint32_t* bpp);

/* Basic drawing operations */
int fb_clear(fb_color_t color);
int fb_set_pixel(uint32_t x, uint32_t y, fb_color_t color);
fb_color_t fb_get_pixel(uint32_t x, uint32_t y);

/* Advanced drawing functions */
int fb_draw_line(fb_point_t start, fb_point_t end, fb_color_t color);
int fb_draw_rect(fb_rect_t rect, fb_color_t color);
int fb_fill_rect(fb_rect_t rect, fb_color_t color);
int fb_draw_circle(fb_point_t center, uint32_t radius, fb_color_t color);
int fb_fill_circle(fb_point_t center, uint32_t radius, fb_color_t color);

/* Bitmap and image operations */
int fb_draw_bitmap(uint32_t x, uint32_t y, uint32_t width, uint32_t height, 
                   const uint8_t* bitmap, fb_color_t fg_color, fb_color_t bg_color);
int fb_copy_region(fb_rect_t src, fb_rect_t dest);
int fb_scroll(int32_t dx, int32_t dy);

/* Text rendering */
int fb_draw_char(uint32_t x, uint32_t y, char c, fb_color_t fg_color, 
                 fb_color_t bg_color, const fb_font_t* font);
int fb_draw_string(uint32_t x, uint32_t y, const char* str, fb_color_t fg_color, 
                   fb_color_t bg_color, const fb_font_t* font);

/* Double buffering support */
int fb_enable_double_buffer(void);
int fb_disable_double_buffer(void);
int fb_swap_buffers(void);
void* fb_get_back_buffer(void);

/* Color utility functions */
fb_color_t fb_rgb(uint8_t r, uint8_t g, uint8_t b);
fb_color_t fb_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
fb_color_t fb_pack_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a, const fb_info_t* info);
void fb_unpack_color(fb_color_t color, uint8_t* r, uint8_t* g, uint8_t* b, uint8_t* a, const fb_info_t* info);

/* Memory management */
int fb_map_memory(uint32_t physical_address, uint32_t size);
int fb_unmap_memory(void);

/* Statistics and debugging */
typedef struct {
    uint64_t pixels_drawn;
    uint64_t lines_drawn;
    uint64_t rects_drawn;
    uint64_t chars_drawn;
    uint64_t buffer_swaps;
    uint32_t current_width;
    uint32_t current_height;
    uint32_t current_bpp;
    fb_mode_t current_mode;
} fb_stats_t;

void fb_get_stats(fb_stats_t* stats);
void fb_reset_stats(void);

/* ================================
 * Predefined Colors
 * ================================ */

#define FB_COLOR_BLACK      ((fb_color_t){.value32 = 0x00000000})
#define FB_COLOR_WHITE      ((fb_color_t){.value32 = 0x00FFFFFF})
#define FB_COLOR_RED        ((fb_color_t){.value32 = 0x00FF0000})
#define FB_COLOR_GREEN      ((fb_color_t){.value32 = 0x0000FF00})
#define FB_COLOR_BLUE       ((fb_color_t){.value32 = 0x000000FF})
#define FB_COLOR_YELLOW     ((fb_color_t){.value32 = 0x00FFFF00})
#define FB_COLOR_CYAN       ((fb_color_t){.value32 = 0x0000FFFF})
#define FB_COLOR_MAGENTA    ((fb_color_t){.value32 = 0x00FF00FF})
#define FB_COLOR_GRAY       ((fb_color_t){.value32 = 0x00808080})
#define FB_COLOR_DARK_GRAY  ((fb_color_t){.value32 = 0x00404040})
#define FB_COLOR_LIGHT_GRAY ((fb_color_t){.value32 = 0x00C0C0C0})

/* ================================
 * Error Codes
 * ================================ */

#define FB_SUCCESS                  0
#define FB_ERROR_INIT_FAILED       -1
#define FB_ERROR_INVALID_MODE       -2
#define FB_ERROR_INVALID_PARAMS     -3
#define FB_ERROR_MEMORY_MAP_FAILED  -4
#define FB_ERROR_NOT_INITIALIZED    -5
#define FB_ERROR_MODE_NOT_SUPPORTED -6
#define FB_ERROR_OUT_OF_BOUNDS      -7
#define FB_ERROR_ALLOCATION_FAILED  -8
#define FB_ERROR_OPERATION_FAILED   -9

/* ================================
 * Built-in Font
 * ================================ */

/* 8x16 VGA font */
extern const fb_font_t fb_font_8x16;

/* ================================
 * Hardware Detection
 * ================================ */

/* VESA BIOS Extensions support */
typedef struct {
    uint16_t mode_number;
    uint16_t width;
    uint16_t height;
    uint8_t bpp;
    bool linear_framebuffer;
    uint32_t framebuffer_address;
} vesa_mode_info_t;

int fb_detect_vesa_modes(vesa_mode_info_t* modes, int max_modes);
bool fb_vesa_available(void);

#endif /* FRAMEBUFFER_H */
