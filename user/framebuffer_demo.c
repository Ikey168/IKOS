/* IKOS Framebuffer Demo Application
 * Issue #26 - Display (Framebuffer) Driver Demo
 * 
 * User-space demonstration application showing framebuffer capabilities.
 * This would run as a user program and demonstrate the framebuffer API.
 */

#include "../include/framebuffer_user_api.h"

/* ================================
 * Demo Functions
 * ================================ */

/**
 * Simple pixel art drawing demo
 */
void demo_pixel_art(void) {
    /* Get framebuffer info */
    fb_user_info_t info;
    if (fb_user_get_info(&info) != 0) {
        return;
    }
    
    /* Clear screen with black */
    fb_user_color_t black = {.value32 = FB_USER_COLOR_BLACK};
    fb_user_clear(black);
    
    /* Draw a simple house */
    fb_user_color_t brown = fb_user_rgb(139, 69, 19);
    fb_user_color_t red = {.value32 = FB_USER_COLOR_RED};
    fb_user_color_t blue = {.value32 = FB_USER_COLOR_BLUE};
    fb_user_color_t green = {.value32 = FB_USER_COLOR_GREEN};
    fb_user_color_t yellow = {.value32 = FB_USER_COLOR_YELLOW};
    
    /* House base */
    fb_user_rect_t house_base = {100, 120, 120, 80};
    fb_user_fill_rect(house_base, brown);
    
    /* Roof */
    for (int i = 0; i < 60; i++) {
        fb_user_point_t start = {100 + i, 120 - i/2};
        fb_user_point_t end = {220 - i, 120 - i/2};
        fb_user_draw_line(start, end, red);
    }
    
    /* Door */
    fb_user_rect_t door = {140, 160, 20, 40};
    fb_user_fill_rect(door, brown);
    
    /* Windows */
    fb_user_rect_t window1 = {110, 140, 15, 15};
    fb_user_fill_rect(window1, blue);
    
    fb_user_rect_t window2 = {195, 140, 15, 15};
    fb_user_fill_rect(window2, blue);
    
    /* Sun */
    fb_user_point_t sun_center = {50, 50};
    fb_user_fill_circle(sun_center, 20, yellow);
    
    /* Ground */
    fb_user_rect_t ground = {0, 200, info.width, info.height - 200};
    fb_user_fill_rect(ground, green);
}

/**
 * Animated bouncing ball demo
 */
void demo_bouncing_ball(void) {
    fb_user_info_t info;
    if (fb_user_get_info(&info) != 0) {
        return;
    }
    
    /* Ball properties */
    int ball_x = 50;
    int ball_y = 50;
    int ball_dx = 2;
    int ball_dy = 3;
    int ball_radius = 10;
    
    fb_user_color_t black = {.value32 = FB_USER_COLOR_BLACK};
    fb_user_color_t red = {.value32 = FB_USER_COLOR_RED};
    
    /* Animation loop (limited iterations for demo) */
    for (int frame = 0; frame < 100; frame++) {
        /* Clear screen */
        fb_user_clear(black);
        
        /* Draw ball */
        fb_user_point_t ball_center = {ball_x, ball_y};
        fb_user_fill_circle(ball_center, ball_radius, red);
        
        /* Update ball position */
        ball_x += ball_dx;
        ball_y += ball_dy;
        
        /* Bounce off walls */
        if (ball_x - ball_radius <= 0 || ball_x + ball_radius >= (int)info.width) {
            ball_dx = -ball_dx;
        }
        if (ball_y - ball_radius <= 0 || ball_y + ball_radius >= (int)info.height) {
            ball_dy = -ball_dy;
        }
        
        /* Swap buffers if double buffering is available */
        fb_user_swap_buffers();
        
        /* Simple delay (in a real OS, this would use proper timing) */
        for (volatile int i = 0; i < 100000; i++);
    }
}

/**
 * Color palette demo
 */
void demo_color_palette(void) {
    fb_user_info_t info;
    if (fb_user_get_info(&info) != 0) {
        return;
    }
    
    /* Clear screen */
    fb_user_color_t black = {.value32 = FB_USER_COLOR_BLACK};
    fb_user_clear(black);
    
    /* Draw color gradient */
    int palette_width = info.width / 8;
    int palette_height = info.height / 4;
    
    /* Red gradient */
    for (int x = 0; x < palette_width; x++) {
        uint8_t intensity = (x * 255) / palette_width;
        fb_user_color_t color = fb_user_rgb(intensity, 0, 0);
        fb_user_rect_t rect = {x * 8, 0, 8, palette_height};
        fb_user_fill_rect(rect, color);
    }
    
    /* Green gradient */
    for (int x = 0; x < palette_width; x++) {
        uint8_t intensity = (x * 255) / palette_width;
        fb_user_color_t color = fb_user_rgb(0, intensity, 0);
        fb_user_rect_t rect = {x * 8, palette_height, 8, palette_height};
        fb_user_fill_rect(rect, color);
    }
    
    /* Blue gradient */
    for (int x = 0; x < palette_width; x++) {
        uint8_t intensity = (x * 255) / palette_width;
        fb_user_color_t color = fb_user_rgb(0, 0, intensity);
        fb_user_rect_t rect = {x * 8, palette_height * 2, 8, palette_height};
        fb_user_fill_rect(rect, color);
    }
    
    /* Rainbow gradient */
    for (int x = 0; x < info.width; x++) {
        float hue = (float)x / info.width * 360.0f;
        
        /* Simple HSV to RGB conversion */
        uint8_t r, g, b;
        if (hue < 60) {
            r = 255; g = (uint8_t)(hue * 255 / 60); b = 0;
        } else if (hue < 120) {
            r = (uint8_t)((120 - hue) * 255 / 60); g = 255; b = 0;
        } else if (hue < 180) {
            r = 0; g = 255; b = (uint8_t)((hue - 120) * 255 / 60);
        } else if (hue < 240) {
            r = 0; g = (uint8_t)((240 - hue) * 255 / 60); b = 255;
        } else if (hue < 300) {
            r = (uint8_t)((hue - 240) * 255 / 60); g = 0; b = 255;
        } else {
            r = 255; g = 0; b = (uint8_t)((360 - hue) * 255 / 60);
        }
        
        fb_user_color_t color = fb_user_rgb(r, g, b);
        fb_user_rect_t rect = {x, palette_height * 3, 1, palette_height};
        fb_user_fill_rect(rect, color);
    }
}

/**
 * Pattern drawing demo
 */
void demo_patterns(void) {
    fb_user_info_t info;
    if (fb_user_get_info(&info) != 0) {
        return;
    }
    
    /* Clear screen */
    fb_user_color_t white = {.value32 = FB_USER_COLOR_WHITE};
    fb_user_clear(white);
    
    /* Checkerboard pattern */
    fb_user_color_t black = {.value32 = FB_USER_COLOR_BLACK};
    int square_size = 20;
    
    for (int y = 0; y < (int)info.height; y += square_size) {
        for (int x = 0; x < (int)info.width; x += square_size) {
            if ((x / square_size + y / square_size) % 2 == 0) {
                fb_user_rect_t square = {x, y, square_size, square_size};
                fb_user_fill_rect(square, black);
            }
        }
    }
    
    /* Overlay some circles */
    fb_user_color_t red = {.value32 = FB_USER_COLOR_RED};
    fb_user_color_t blue = {.value32 = FB_USER_COLOR_BLUE};
    fb_user_color_t green = {.value32 = FB_USER_COLOR_GREEN};
    
    fb_user_point_t center1 = {(int)info.width / 4, (int)info.height / 2};
    fb_user_draw_circle(center1, 50, red);
    
    fb_user_point_t center2 = {(int)info.width / 2, (int)info.height / 2};
    fb_user_draw_circle(center2, 50, blue);
    
    fb_user_point_t center3 = {3 * (int)info.width / 4, (int)info.height / 2};
    fb_user_draw_circle(center3, 50, green);
}

/**
 * Text display demo (for text mode)
 */
void demo_text_display(void) {
    /* Set text mode */
    fb_user_set_mode(FB_USER_MODE_TEXT, 80, 25, 16);
    
    /* Clear screen with blue background */
    fb_user_color_t blue_bg = {.value8 = 0x01};
    fb_user_clear(blue_bg);
    
    /* Display title */
    fb_user_color_t white_fg = {.value8 = 0x0F};
    fb_user_color_t blue_bg_clear = {.value8 = 0x00};
    
    fb_user_draw_string(28, 2, "IKOS FRAMEBUFFER", white_fg, blue_bg_clear);
    fb_user_draw_string(32, 3, "USER DEMO", white_fg, blue_bg_clear);
    fb_user_draw_string(20, 4, "========================", white_fg, blue_bg_clear);
    
    /* Display menu */
    fb_user_color_t yellow_fg = {.value8 = 0x0E};
    fb_user_draw_string(10, 7, "Demonstration Features:", yellow_fg, blue_bg_clear);
    fb_user_draw_string(12, 9, "1. Pixel Art Drawing", white_fg, blue_bg_clear);
    fb_user_draw_string(12, 10, "2. Bouncing Ball Animation", white_fg, blue_bg_clear);
    fb_user_draw_string(12, 11, "3. Color Palette Display", white_fg, blue_bg_clear);
    fb_user_draw_string(12, 12, "4. Pattern Generation", white_fg, blue_bg_clear);
    fb_user_draw_string(12, 13, "5. Text Mode Display", white_fg, blue_bg_clear);
    
    /* Display info */
    fb_user_info_t info;
    if (fb_user_get_info(&info) == 0) {
        fb_user_color_t green_fg = {.value8 = 0x0A};
        fb_user_draw_string(10, 16, "Current Framebuffer:", green_fg, blue_bg_clear);
        
        /* Note: In a real implementation, we'd format these numbers properly */
        fb_user_draw_string(12, 17, "Width: (see info structure)", green_fg, blue_bg_clear);
        fb_user_draw_string(12, 18, "Height: (see info structure)", green_fg, blue_bg_clear);
        fb_user_draw_string(12, 19, "BPP: (see info structure)", green_fg, blue_bg_clear);
    }
    
    /* Footer */
    fb_user_color_t cyan_fg = {.value8 = 0x0B};
    fb_user_draw_string(15, 22, "IKOS Operating System - Issue #26", cyan_fg, blue_bg_clear);
    fb_user_draw_string(20, 23, "Framebuffer Driver Demo", cyan_fg, blue_bg_clear);
}

/* ================================
 * Main Demo Program
 * ================================ */

/**
 * Main framebuffer demo program
 * This would be the entry point for a user-space application
 */
int main(void) {
    /* Initialize framebuffer */
    if (fb_user_init() != 0) {
        return -1;
    }
    
    /* Run text mode demo first */
    demo_text_display();
    
    /* Wait for user input or timer (simulated) */
    for (volatile int i = 0; i < 1000000; i++);
    
    /* Switch to graphics mode for graphical demos */
    fb_user_set_mode(FB_USER_MODE_VGA_GRAPHICS, 320, 200, 8);
    
    /* Run graphics demos */
    demo_pixel_art();
    for (volatile int i = 0; i < 1000000; i++); /* Pause */
    
    demo_color_palette();
    for (volatile int i = 0; i < 1000000; i++); /* Pause */
    
    demo_patterns();
    for (volatile int i = 0; i < 1000000; i++); /* Pause */
    
    demo_bouncing_ball();
    
    /* Return to text mode */
    fb_user_set_mode(FB_USER_MODE_TEXT, 80, 25, 16);
    
    /* Display completion message */
    fb_user_color_t black = {.value32 = FB_USER_COLOR_BLACK};
    fb_user_clear(black);
    
    fb_user_color_t green_fg = {.value8 = 0x0A};
    fb_user_color_t black_bg = {.value8 = 0x00};
    
    fb_user_draw_string(25, 12, "Demo Complete!", green_fg, black_bg);
    fb_user_draw_string(15, 14, "Framebuffer driver working correctly.", green_fg, black_bg);
    
    return 0;
}

/* ================================
 * Alternative Entry Points
 * ================================ */

/**
 * Simple test function that can be called from kernel
 */
void run_framebuffer_user_demo(void) {
    main();
}
