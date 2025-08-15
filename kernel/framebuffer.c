/* IKOS Framebuffer Driver Implementation
 * Issue #26 - Display (Framebuffer) Driver
 * 
 * Implements framebuffer-based display driver for graphical output.
 * Provides basic pixel drawing functions and hardware abstraction.
 */

#include "../include/framebuffer.h"
#include "../include/memory.h"
#include <string.h>

/* ================================
 * Global Variables
 * ================================ */

static fb_info_t g_fb_info;
static fb_stats_t g_fb_stats;
static bool g_fb_initialized = false;

/* Built-in 8x16 VGA font data */
static const uint8_t vga_font_8x16[] = {
    /* Character 0x20 (space) */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    
    /* Character 0x21 (!) */
    0x00, 0x00, 0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00,
    0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    
    /* More characters would be defined here... */
    /* For brevity, only showing a few sample characters */
    
    /* Character 0x41 (A) */
    0x00, 0x00, 0x3C, 0x66, 0x66, 0x7E, 0x66, 0x66,
    0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    
    /* Character 0x42 (B) */
    0x00, 0x00, 0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66,
    0x7C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

const fb_font_t fb_font_8x16 = {
    .width = 8,
    .height = 16,
    .data = vga_font_8x16
};

/* ================================
 * Helper Functions
 * ================================ */

/**
 * Check if coordinates are within framebuffer bounds
 */
static bool fb_bounds_check(uint32_t x, uint32_t y) {
    return (x < g_fb_info.width && y < g_fb_info.height);
}

/**
 * Calculate memory offset for given coordinates
 */
static uint32_t fb_calc_offset(uint32_t x, uint32_t y) {
    return y * g_fb_info.pitch + x * (g_fb_info.bpp / 8);
}

/**
 * Write pixel value to framebuffer memory
 */
static void fb_write_pixel_raw(uint32_t offset, fb_color_t color) {
    uint8_t* buffer = (uint8_t*)g_fb_info.buffer;
    
    switch (g_fb_info.bpp) {
        case 8:
            buffer[offset] = color.value8;
            break;
        case 16:
            *(uint16_t*)(buffer + offset) = color.rgb16.value;
            break;
        case 24:
            buffer[offset] = color.bgra.b;
            buffer[offset + 1] = color.bgra.g;
            buffer[offset + 2] = color.bgra.r;
            break;
        case 32:
            *(uint32_t*)(buffer + offset) = color.value32;
            break;
    }
}

/**
 * Read pixel value from framebuffer memory
 */
static fb_color_t fb_read_pixel_raw(uint32_t offset) {
    fb_color_t color = {0};
    uint8_t* buffer = (uint8_t*)g_fb_info.buffer;
    
    switch (g_fb_info.bpp) {
        case 8:
            color.value8 = buffer[offset];
            break;
        case 16:
            color.rgb16.value = *(uint16_t*)(buffer + offset);
            break;
        case 24:
            color.bgra.b = buffer[offset];
            color.bgra.g = buffer[offset + 1];
            color.bgra.r = buffer[offset + 2];
            color.bgra.a = 0xFF;
            break;
        case 32:
            color.value32 = *(uint32_t*)(buffer + offset);
            break;
    }
    
    return color;
}

/**
 * Set up VGA text mode
 */
static int fb_setup_vga_text(void) {
    g_fb_info.width = VGA_TEXT_WIDTH;
    g_fb_info.height = VGA_TEXT_HEIGHT;
    g_fb_info.bpp = 16; /* 2 bytes per character (char + attribute) */
    g_fb_info.pitch = VGA_TEXT_WIDTH * 2;
    g_fb_info.size = VGA_TEXT_WIDTH * VGA_TEXT_HEIGHT * 2;
    g_fb_info.buffer = (void*)VGA_TEXT_MEMORY;
    g_fb_info.mode = FB_MODE_TEXT;
    g_fb_info.format = FB_FORMAT_INDEXED;
    
    return FB_SUCCESS;
}

/**
 * Set up VGA graphics mode 13h (320x200x8)
 */
static int fb_setup_vga_graphics(void) {
    g_fb_info.width = VGA_GRAPHICS_WIDTH;
    g_fb_info.height = VGA_GRAPHICS_HEIGHT;
    g_fb_info.bpp = 8;
    g_fb_info.pitch = VGA_GRAPHICS_WIDTH;
    g_fb_info.size = VGA_GRAPHICS_WIDTH * VGA_GRAPHICS_HEIGHT;
    g_fb_info.buffer = (void*)VGA_GRAPHICS_MEMORY;
    g_fb_info.mode = FB_MODE_VGA_GRAPHICS;
    g_fb_info.format = FB_FORMAT_INDEXED;
    
    return FB_SUCCESS;
}

/**
 * Set up VESA linear framebuffer
 */
static int fb_setup_vesa_lfb(uint32_t width, uint32_t height, uint32_t bpp) {
    /* For now, use default VESA settings */
    /* In a real implementation, this would query VESA BIOS */
    
    g_fb_info.width = width ? width : VESA_DEFAULT_WIDTH;
    g_fb_info.height = height ? height : VESA_DEFAULT_HEIGHT;
    g_fb_info.bpp = bpp ? bpp : VESA_DEFAULT_BPP;
    g_fb_info.pitch = g_fb_info.width * (g_fb_info.bpp / 8);
    g_fb_info.size = g_fb_info.pitch * g_fb_info.height;
    g_fb_info.buffer = (void*)VESA_LFB_BASE;
    g_fb_info.mode = FB_MODE_VESA_LFB;
    g_fb_info.format = FB_FORMAT_RGB;
    
    /* Set up color masks for RGB extraction */
    if (g_fb_info.bpp == 32) {
        g_fb_info.red_mask = 0x00FF0000;
        g_fb_info.green_mask = 0x0000FF00;
        g_fb_info.blue_mask = 0x000000FF;
        g_fb_info.alpha_mask = 0xFF000000;
        g_fb_info.red_shift = 16;
        g_fb_info.green_shift = 8;
        g_fb_info.blue_shift = 0;
        g_fb_info.alpha_shift = 24;
    } else if (g_fb_info.bpp == 16) {
        g_fb_info.red_mask = 0xF800;
        g_fb_info.green_mask = 0x07E0;
        g_fb_info.blue_mask = 0x001F;
        g_fb_info.red_shift = 11;
        g_fb_info.green_shift = 5;
        g_fb_info.blue_shift = 0;
    }
    
    return FB_SUCCESS;
}

/* ================================
 * Core API Implementation
 * ================================ */

/**
 * Initialize framebuffer driver
 */
int fb_init(void) {
    if (g_fb_initialized) {
        return FB_SUCCESS;
    }
    
    /* Initialize framebuffer info structure */
    memset(&g_fb_info, 0, sizeof(fb_info_t));
    memset(&g_fb_stats, 0, sizeof(fb_stats_t));
    
    /* Start with VGA text mode as default */
    int result = fb_setup_vga_text();
    if (result != FB_SUCCESS) {
        return result;
    }
    
    g_fb_info.initialized = true;
    g_fb_info.double_buffered = false;
    g_fb_info.back_buffer = NULL;
    
    g_fb_initialized = true;
    
    return FB_SUCCESS;
}

/**
 * Shutdown framebuffer driver
 */
int fb_shutdown(void) {
    if (!g_fb_initialized) {
        return FB_ERROR_NOT_INITIALIZED;
    }
    
    /* Clean up double buffer if allocated */
    if (g_fb_info.back_buffer) {
        kfree(g_fb_info.back_buffer);
        g_fb_info.back_buffer = NULL;
    }
    
    g_fb_initialized = false;
    g_fb_info.initialized = false;
    
    return FB_SUCCESS;
}

/**
 * Set framebuffer mode
 */
int fb_set_mode(fb_mode_t mode, uint32_t width, uint32_t height, uint32_t bpp) {
    if (!g_fb_initialized) {
        return FB_ERROR_NOT_INITIALIZED;
    }
    
    int result;
    
    switch (mode) {
        case FB_MODE_TEXT:
            result = fb_setup_vga_text();
            break;
        case FB_MODE_VGA_GRAPHICS:
            result = fb_setup_vga_graphics();
            break;
        case FB_MODE_VESA_LFB:
            result = fb_setup_vesa_lfb(width, height, bpp);
            break;
        default:
            return FB_ERROR_INVALID_MODE;
    }
    
    if (result == FB_SUCCESS) {
        /* Update stats */
        g_fb_stats.current_width = g_fb_info.width;
        g_fb_stats.current_height = g_fb_info.height;
        g_fb_stats.current_bpp = g_fb_info.bpp;
        g_fb_stats.current_mode = g_fb_info.mode;
    }
    
    return result;
}

/**
 * Get framebuffer information
 */
fb_info_t* fb_get_info(void) {
    if (!g_fb_initialized) {
        return NULL;
    }
    
    return &g_fb_info;
}

/**
 * Check if mode is supported
 */
bool fb_is_mode_supported(fb_mode_t mode, uint32_t width, uint32_t height, uint32_t bpp) {
    switch (mode) {
        case FB_MODE_TEXT:
            return (width == VGA_TEXT_WIDTH && height == VGA_TEXT_HEIGHT);
        case FB_MODE_VGA_GRAPHICS:
            return (width == VGA_GRAPHICS_WIDTH && height == VGA_GRAPHICS_HEIGHT && bpp == 8);
        case FB_MODE_VESA_LFB:
            return (width <= FB_MAX_WIDTH && height <= FB_MAX_HEIGHT && 
                    (bpp == 16 || bpp == 24 || bpp == 32));
        default:
            return false;
    }
}

/* ================================
 * Drawing Operations
 * ================================ */

/**
 * Clear framebuffer with specified color
 */
int fb_clear(fb_color_t color) {
    if (!g_fb_initialized) {
        return FB_ERROR_NOT_INITIALIZED;
    }
    
    if (g_fb_info.mode == FB_MODE_TEXT) {
        /* Clear text mode screen */
        uint16_t* buffer = (uint16_t*)g_fb_info.buffer;
        uint16_t char_attr = (color.value8 << 8) | ' ';
        
        for (int i = 0; i < VGA_TEXT_WIDTH * VGA_TEXT_HEIGHT; i++) {
            buffer[i] = char_attr;
        }
    } else {
        /* Clear graphics mode */
        for (uint32_t y = 0; y < g_fb_info.height; y++) {
            for (uint32_t x = 0; x < g_fb_info.width; x++) {
                uint32_t offset = fb_calc_offset(x, y);
                fb_write_pixel_raw(offset, color);
            }
        }
    }
    
    return FB_SUCCESS;
}

/**
 * Set pixel at specified coordinates
 */
int fb_set_pixel(uint32_t x, uint32_t y, fb_color_t color) {
    if (!g_fb_initialized) {
        return FB_ERROR_NOT_INITIALIZED;
    }
    
    if (!fb_bounds_check(x, y)) {
        return FB_ERROR_OUT_OF_BOUNDS;
    }
    
    if (g_fb_info.mode == FB_MODE_TEXT) {
        return FB_ERROR_OPERATION_FAILED; /* Can't set individual pixels in text mode */
    }
    
    uint32_t offset = fb_calc_offset(x, y);
    fb_write_pixel_raw(offset, color);
    
    g_fb_stats.pixels_drawn++;
    
    return FB_SUCCESS;
}

/**
 * Get pixel at specified coordinates
 */
fb_color_t fb_get_pixel(uint32_t x, uint32_t y) {
    fb_color_t color = {0};
    
    if (!g_fb_initialized || !fb_bounds_check(x, y)) {
        return color;
    }
    
    if (g_fb_info.mode == FB_MODE_TEXT) {
        return color; /* Can't get individual pixels in text mode */
    }
    
    uint32_t offset = fb_calc_offset(x, y);
    return fb_read_pixel_raw(offset);
}

/**
 * Draw line using Bresenham's algorithm
 */
int fb_draw_line(fb_point_t start, fb_point_t end, fb_color_t color) {
    if (!g_fb_initialized) {
        return FB_ERROR_NOT_INITIALIZED;
    }
    
    int dx = abs(end.x - start.x);
    int dy = abs(end.y - start.y);
    int sx = (start.x < end.x) ? 1 : -1;
    int sy = (start.y < end.y) ? 1 : -1;
    int err = dx - dy;
    
    int x = start.x;
    int y = start.y;
    
    while (true) {
        if (fb_bounds_check(x, y)) {
            fb_set_pixel(x, y, color);
        }
        
        if (x == end.x && y == end.y) {
            break;
        }
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
    
    g_fb_stats.lines_drawn++;
    
    return FB_SUCCESS;
}

/**
 * Draw rectangle outline
 */
int fb_draw_rect(fb_rect_t rect, fb_color_t color) {
    if (!g_fb_initialized) {
        return FB_ERROR_NOT_INITIALIZED;
    }
    
    /* Draw four lines forming rectangle */
    fb_point_t tl = {rect.x, rect.y};
    fb_point_t tr = {rect.x + rect.width - 1, rect.y};
    fb_point_t bl = {rect.x, rect.y + rect.height - 1};
    fb_point_t br = {rect.x + rect.width - 1, rect.y + rect.height - 1};
    
    fb_draw_line(tl, tr, color); /* Top */
    fb_draw_line(tr, br, color); /* Right */
    fb_draw_line(br, bl, color); /* Bottom */
    fb_draw_line(bl, tl, color); /* Left */
    
    g_fb_stats.rects_drawn++;
    
    return FB_SUCCESS;
}

/**
 * Fill rectangle with solid color
 */
int fb_fill_rect(fb_rect_t rect, fb_color_t color) {
    if (!g_fb_initialized) {
        return FB_ERROR_NOT_INITIALIZED;
    }
    
    for (uint32_t y = rect.y; y < rect.y + rect.height; y++) {
        for (uint32_t x = rect.x; x < rect.x + rect.width; x++) {
            if (fb_bounds_check(x, y)) {
                fb_set_pixel(x, y, color);
            }
        }
    }
    
    g_fb_stats.rects_drawn++;
    
    return FB_SUCCESS;
}

/**
 * Draw circle using midpoint circle algorithm
 */
int fb_draw_circle(fb_point_t center, uint32_t radius, fb_color_t color) {
    if (!g_fb_initialized) {
        return FB_ERROR_NOT_INITIALIZED;
    }
    
    int x = radius;
    int y = 0;
    int err = 0;
    
    while (x >= y) {
        /* Draw 8 symmetric points */
        fb_set_pixel(center.x + x, center.y + y, color);
        fb_set_pixel(center.x + y, center.y + x, color);
        fb_set_pixel(center.x - y, center.y + x, color);
        fb_set_pixel(center.x - x, center.y + y, color);
        fb_set_pixel(center.x - x, center.y - y, color);
        fb_set_pixel(center.x - y, center.y - x, color);
        fb_set_pixel(center.x + y, center.y - x, color);
        fb_set_pixel(center.x + x, center.y - y, color);
        
        if (err <= 0) {
            y += 1;
            err += 2*y + 1;
        }
        
        if (err > 0) {
            x -= 1;
            err -= 2*x + 1;
        }
    }
    
    return FB_SUCCESS;
}

/**
 * Fill circle with solid color
 */
int fb_fill_circle(fb_point_t center, uint32_t radius, fb_color_t color) {
    if (!g_fb_initialized) {
        return FB_ERROR_NOT_INITIALIZED;
    }
    
    for (int y = -radius; y <= (int)radius; y++) {
        for (int x = -radius; x <= (int)radius; x++) {
            if (x*x + y*y <= (int)(radius*radius)) {
                fb_set_pixel(center.x + x, center.y + y, color);
            }
        }
    }
    
    return FB_SUCCESS;
}

/* ================================
 * Text Rendering
 * ================================ */

/**
 * Draw single character
 */
int fb_draw_char(uint32_t x, uint32_t y, char c, fb_color_t fg_color, 
                 fb_color_t bg_color, const fb_font_t* font) {
    if (!g_fb_initialized) {
        return FB_ERROR_NOT_INITIALIZED;
    }
    
    if (!font) {
        font = &fb_font_8x16;
    }
    
    /* Simple character rendering for demonstration */
    /* In a real implementation, this would use the font bitmap data */
    
    if (g_fb_info.mode == FB_MODE_TEXT) {
        /* Text mode rendering */
        uint16_t* buffer = (uint16_t*)g_fb_info.buffer;
        uint32_t pos = (y * VGA_TEXT_WIDTH + x);
        
        if (pos < VGA_TEXT_WIDTH * VGA_TEXT_HEIGHT) {
            uint16_t char_attr = (bg_color.value8 << 12) | (fg_color.value8 << 8) | c;
            buffer[pos] = char_attr;
            g_fb_stats.chars_drawn++;
        }
    } else {
        /* Graphics mode rendering - simple 8x16 block */
        fb_rect_t char_rect = {x, y, font->width, font->height};
        fb_fill_rect(char_rect, bg_color);
        
        /* Draw simple character representation */
        if (c >= 'A' && c <= 'Z') {
            /* Draw a simple pattern for letters */
            fb_rect_t fg_rect = {x + 1, y + 1, font->width - 2, font->height - 2};
            fb_fill_rect(fg_rect, fg_color);
        }
        
        g_fb_stats.chars_drawn++;
    }
    
    return FB_SUCCESS;
}

/**
 * Draw string
 */
int fb_draw_string(uint32_t x, uint32_t y, const char* str, fb_color_t fg_color, 
                   fb_color_t bg_color, const fb_font_t* font) {
    if (!g_fb_initialized || !str) {
        return FB_ERROR_NOT_INITIALIZED;
    }
    
    if (!font) {
        font = &fb_font_8x16;
    }
    
    uint32_t char_x = x;
    uint32_t char_y = y;
    
    while (*str) {
        if (*str == '\n') {
            char_x = x;
            char_y += font->height;
        } else {
            fb_draw_char(char_x, char_y, *str, fg_color, bg_color, font);
            char_x += font->width;
        }
        str++;
    }
    
    return FB_SUCCESS;
}

/* ================================
 * Color Utilities
 * ================================ */

/**
 * Create RGB color
 */
fb_color_t fb_rgb(uint8_t r, uint8_t g, uint8_t b) {
    fb_color_t color;
    color.rgba.r = r;
    color.rgba.g = g;
    color.rgba.b = b;
    color.rgba.a = 0xFF;
    return color;
}

/**
 * Create RGBA color
 */
fb_color_t fb_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    fb_color_t color;
    color.rgba.r = r;
    color.rgba.g = g;
    color.rgba.b = b;
    color.rgba.a = a;
    return color;
}

/**
 * Pack color according to framebuffer format
 */
fb_color_t fb_pack_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a, const fb_info_t* info) {
    fb_color_t color = {0};
    
    if (!info) {
        info = &g_fb_info;
    }
    
    if (info->bpp == 32) {
        color.value32 = (a << info->alpha_shift) |
                       (r << info->red_shift) |
                       (g << info->green_shift) |
                       (b << info->blue_shift);
    } else if (info->bpp == 16) {
        color.rgb16.value = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    } else {
        color.value8 = (r + g + b) / 3; /* Simple grayscale for 8-bit */
    }
    
    return color;
}

/* ================================
 * Statistics and Debug
 * ================================ */

/**
 * Get framebuffer statistics
 */
void fb_get_stats(fb_stats_t* stats) {
    if (stats) {
        *stats = g_fb_stats;
    }
}

/**
 * Reset framebuffer statistics
 */
void fb_reset_stats(void) {
    memset(&g_fb_stats, 0, sizeof(fb_stats_t));
    g_fb_stats.current_width = g_fb_info.width;
    g_fb_stats.current_height = g_fb_info.height;
    g_fb_stats.current_bpp = g_fb_info.bpp;
    g_fb_stats.current_mode = g_fb_info.mode;
}

/**
 * Double buffering support (basic implementation)
 */
int fb_enable_double_buffer(void) {
    if (!g_fb_initialized) {
        return FB_ERROR_NOT_INITIALIZED;
    }
    
    if (g_fb_info.double_buffered) {
        return FB_SUCCESS; /* Already enabled */
    }
    
    /* Allocate back buffer */
    g_fb_info.back_buffer = kmalloc(g_fb_info.size);
    if (!g_fb_info.back_buffer) {
        return FB_ERROR_ALLOCATION_FAILED;
    }
    
    g_fb_info.double_buffered = true;
    
    return FB_SUCCESS;
}

/**
 * Swap front and back buffers
 */
int fb_swap_buffers(void) {
    if (!g_fb_initialized || !g_fb_info.double_buffered) {
        return FB_ERROR_NOT_INITIALIZED;
    }
    
    /* Copy back buffer to front buffer */
    memcpy(g_fb_info.buffer, g_fb_info.back_buffer, g_fb_info.size);
    
    g_fb_stats.buffer_swaps++;
    
    return FB_SUCCESS;
}

/**
 * Get current mode information
 */
int fb_get_current_mode(fb_mode_t* mode, uint32_t* width, uint32_t* height, uint32_t* bpp) {
    if (!g_fb_initialized) {
        return FB_ERROR_NOT_INITIALIZED;
    }
    
    if (mode) *mode = g_fb_info.mode;
    if (width) *width = g_fb_info.width;
    if (height) *height = g_fb_info.height;
    if (bpp) *bpp = g_fb_info.bpp;
    
    return FB_SUCCESS;
}
