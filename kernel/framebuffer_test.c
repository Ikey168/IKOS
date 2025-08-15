/* IKOS Framebuffer Driver Test Program
 * Issue #26 - Display (Framebuffer) Driver Test
 * 
 * Comprehensive test program demonstrating framebuffer functionality
 * including pixel drawing, shapes, text rendering, and user-space API.
 */

#include "../include/framebuffer.h"
#include "../include/framebuffer_user_api.h"
#include "../include/memory.h"
#include <string.h>

/* ================================
 * Test Framework
 * ================================ */

static uint32_t g_tests_run = 0;
static uint32_t g_tests_passed = 0;
static uint32_t g_tests_failed = 0;

static void debug_print(const char* format, ...) {
    /* Placeholder for debug printing */
    (void)format;
}

#define TEST_ASSERT(condition, message) \
    do { \
        g_tests_run++; \
        if (condition) { \
            g_tests_passed++; \
            debug_print("[PASS] %s\n", message); \
        } else { \
            g_tests_failed++; \
            debug_print("[FAIL] %s\n", message); \
        } \
    } while(0)

#define TEST_START(name) \
    debug_print("\n=== Test: %s ===\n", name)

#define TEST_END() \
    debug_print("Tests: %u, Passed: %u, Failed: %u\n", \
                g_tests_run, g_tests_passed, g_tests_failed)

/* ================================
 * Kernel Framebuffer Tests
 * ================================ */

static void test_framebuffer_init(void) {
    TEST_START("Framebuffer Initialization");
    
    int result = fb_init();
    TEST_ASSERT(result == FB_SUCCESS, "Framebuffer initialization should succeed");
    
    fb_info_t* info = fb_get_info();
    TEST_ASSERT(info != NULL, "Should get valid framebuffer info");
    TEST_ASSERT(info->initialized == true, "Framebuffer should be marked as initialized");
    
    debug_print("FB: Initial mode: %dx%d, %d bpp\n", 
                info->width, info->height, info->bpp);
}

static void test_framebuffer_modes(void) {
    TEST_START("Framebuffer Mode Setting");
    
    /* Test text mode */
    int result = fb_set_mode(FB_MODE_TEXT, 0, 0, 0);
    TEST_ASSERT(result == FB_SUCCESS, "Setting text mode should succeed");
    
    fb_info_t* info = fb_get_info();
    TEST_ASSERT(info->mode == FB_MODE_TEXT, "Mode should be set to text");
    TEST_ASSERT(info->width == VGA_TEXT_WIDTH, "Text mode width should be 80");
    TEST_ASSERT(info->height == VGA_TEXT_HEIGHT, "Text mode height should be 25");
    
    /* Test VGA graphics mode */
    result = fb_set_mode(FB_MODE_VGA_GRAPHICS, 0, 0, 0);
    TEST_ASSERT(result == FB_SUCCESS, "Setting VGA graphics mode should succeed");
    
    info = fb_get_info();
    TEST_ASSERT(info->mode == FB_MODE_VGA_GRAPHICS, "Mode should be set to VGA graphics");
    TEST_ASSERT(info->width == VGA_GRAPHICS_WIDTH, "VGA graphics width should be 320");
    TEST_ASSERT(info->height == VGA_GRAPHICS_HEIGHT, "VGA graphics height should be 200");
    
    /* Test mode support checking */
    bool supported = fb_is_mode_supported(FB_MODE_TEXT, VGA_TEXT_WIDTH, VGA_TEXT_HEIGHT, 16);
    TEST_ASSERT(supported == true, "Text mode should be supported");
    
    supported = fb_is_mode_supported(FB_MODE_VGA_GRAPHICS, 640, 480, 8);
    TEST_ASSERT(supported == false, "640x480 VGA graphics should not be supported");
}

static void test_basic_drawing(void) {
    TEST_START("Basic Drawing Operations");
    
    /* Set to VGA graphics mode for pixel testing */
    fb_set_mode(FB_MODE_VGA_GRAPHICS, 0, 0, 0);
    
    /* Test clearing */
    fb_color_t blue = FB_COLOR_BLUE;
    int result = fb_clear(blue);
    TEST_ASSERT(result == FB_SUCCESS, "Clearing framebuffer should succeed");
    
    /* Test pixel setting */
    fb_color_t red = FB_COLOR_RED;
    result = fb_set_pixel(10, 10, red);
    TEST_ASSERT(result == FB_SUCCESS, "Setting pixel should succeed");
    
    /* Test pixel getting */
    fb_color_t pixel = fb_get_pixel(10, 10);
    TEST_ASSERT(pixel.value32 == red.value32, "Retrieved pixel should match set pixel");
    
    /* Test bounds checking */
    result = fb_set_pixel(1000, 1000, red);
    TEST_ASSERT(result == FB_ERROR_OUT_OF_BOUNDS, "Out-of-bounds pixel should fail");
    
    /* Test line drawing */
    fb_point_t start = {20, 20};
    fb_point_t end = {100, 50};
    fb_color_t green = FB_COLOR_GREEN;
    result = fb_draw_line(start, end, green);
    TEST_ASSERT(result == FB_SUCCESS, "Drawing line should succeed");
}

static void test_shape_drawing(void) {
    TEST_START("Shape Drawing");
    
    /* Test rectangle drawing */
    fb_rect_t rect = {50, 50, 80, 60};
    fb_color_t yellow = FB_COLOR_YELLOW;
    int result = fb_draw_rect(rect, yellow);
    TEST_ASSERT(result == FB_SUCCESS, "Drawing rectangle outline should succeed");
    
    /* Test filled rectangle */
    fb_rect_t fill_rect = {150, 50, 80, 60};
    fb_color_t cyan = FB_COLOR_CYAN;
    result = fb_fill_rect(fill_rect, cyan);
    TEST_ASSERT(result == FB_SUCCESS, "Filling rectangle should succeed");
    
    /* Test circle drawing */
    fb_point_t center = {200, 150};
    uint32_t radius = 30;
    fb_color_t magenta = FB_COLOR_MAGENTA;
    result = fb_draw_circle(center, radius, magenta);
    TEST_ASSERT(result == FB_SUCCESS, "Drawing circle outline should succeed");
    
    /* Test filled circle */
    fb_point_t fill_center = {100, 150};
    result = fb_fill_circle(fill_center, 25, FB_COLOR_WHITE);
    TEST_ASSERT(result == FB_SUCCESS, "Filling circle should succeed");
}

static void test_text_rendering(void) {
    TEST_START("Text Rendering");
    
    /* Switch to text mode for text testing */
    fb_set_mode(FB_MODE_TEXT, 0, 0, 0);
    
    /* Test character drawing */
    fb_color_t fg = {.value8 = 0x0F}; /* White foreground */
    fb_color_t bg = {.value8 = 0x01}; /* Blue background */
    
    int result = fb_draw_char(0, 0, 'H', fg, bg, NULL);
    TEST_ASSERT(result == FB_SUCCESS, "Drawing character should succeed");
    
    /* Test string drawing */
    result = fb_draw_string(0, 1, "Hello, IKOS!", fg, bg, NULL);
    TEST_ASSERT(result == FB_SUCCESS, "Drawing string should succeed");
    
    /* Test newline handling */
    result = fb_draw_string(0, 2, "Line 1\nLine 2", fg, bg, NULL);
    TEST_ASSERT(result == FB_SUCCESS, "Drawing string with newline should succeed");
}

static void test_color_utilities(void) {
    TEST_START("Color Utilities");
    
    /* Test RGB color creation */
    fb_color_t color = fb_rgb(255, 128, 64);
    TEST_ASSERT(color.rgba.r == 255, "Red component should be correct");
    TEST_ASSERT(color.rgba.g == 128, "Green component should be correct");
    TEST_ASSERT(color.rgba.b == 64, "Blue component should be correct");
    TEST_ASSERT(color.rgba.a == 255, "Alpha should default to opaque");
    
    /* Test RGBA color creation */
    fb_color_t rgba_color = fb_rgba(200, 100, 50, 128);
    TEST_ASSERT(rgba_color.rgba.r == 200, "RGBA red component should be correct");
    TEST_ASSERT(rgba_color.rgba.g == 100, "RGBA green component should be correct");
    TEST_ASSERT(rgba_color.rgba.b == 50, "RGBA blue component should be correct");
    TEST_ASSERT(rgba_color.rgba.a == 128, "RGBA alpha component should be correct");
    
    /* Test color packing */
    fb_info_t* info = fb_get_info();
    fb_color_t packed = fb_pack_color(255, 255, 255, 255, info);
    /* The exact packed value depends on the current mode, so just check it's not zero */
    TEST_ASSERT(packed.value32 != 0, "Packed white color should not be zero");
}

static void test_statistics(void) {
    TEST_START("Statistics and Debug");
    
    /* Reset statistics */
    fb_reset_stats();
    
    fb_stats_t stats;
    fb_get_stats(&stats);
    TEST_ASSERT(stats.pixels_drawn == 0, "Reset statistics should have zero pixels drawn");
    TEST_ASSERT(stats.lines_drawn == 0, "Reset statistics should have zero lines drawn");
    
    /* Draw some shapes to test statistics */
    fb_set_mode(FB_MODE_VGA_GRAPHICS, 0, 0, 0);
    fb_set_pixel(10, 10, FB_COLOR_RED);
    fb_set_pixel(11, 11, FB_COLOR_GREEN);
    
    fb_point_t start = {0, 0};
    fb_point_t end = {50, 50};
    fb_draw_line(start, end, FB_COLOR_BLUE);
    
    fb_get_stats(&stats);
    TEST_ASSERT(stats.pixels_drawn == 2, "Should have drawn 2 pixels");
    TEST_ASSERT(stats.lines_drawn == 1, "Should have drawn 1 line");
    TEST_ASSERT(stats.current_mode == FB_MODE_VGA_GRAPHICS, "Current mode should be VGA graphics");
}

/* ================================
 * User-Space API Tests
 * ================================ */

static void test_user_api_colors(void) {
    TEST_START("User-Space Color API");
    
    /* Test user RGB color creation */
    fb_user_color_t user_color = fb_user_rgb(255, 128, 64);
    TEST_ASSERT(user_color.rgba.r == 255, "User RGB red should be correct");
    TEST_ASSERT(user_color.rgba.g == 128, "User RGB green should be correct");
    TEST_ASSERT(user_color.rgba.b == 64, "User RGB blue should be correct");
    
    /* Test user RGBA color creation */
    fb_user_color_t user_rgba = fb_user_rgba(200, 100, 50, 128);
    TEST_ASSERT(user_rgba.rgba.r == 200, "User RGBA red should be correct");
    TEST_ASSERT(user_rgba.rgba.g == 100, "User RGBA green should be correct");
    TEST_ASSERT(user_rgba.rgba.b == 50, "User RGBA blue should be correct");
    TEST_ASSERT(user_rgba.rgba.a == 128, "User RGBA alpha should be correct");
    
    /* Test predefined colors */
    fb_user_color_t red;
    red.value32 = FB_USER_COLOR_RED;
    TEST_ASSERT(red.rgba.r == 255, "Predefined red should have correct red component");
    TEST_ASSERT(red.rgba.g == 0, "Predefined red should have zero green component");
    TEST_ASSERT(red.rgba.b == 0, "Predefined red should have zero blue component");
}

static void test_user_api_integration(void) {
    TEST_START("User-Space API Integration");
    
    /* Note: These tests simulate user-space API calls but run in kernel context
     * In a real scenario, these would be separate user programs making system calls */
    
    /* Test user info structure */
    fb_user_info_t user_info;
    /* This would normally be a system call, but we test the structure layout */
    fb_info_t* kernel_info = fb_get_info();
    if (kernel_info) {
        user_info.width = kernel_info->width;
        user_info.height = kernel_info->height;
        user_info.bpp = kernel_info->bpp;
        user_info.pitch = kernel_info->pitch;
        
        TEST_ASSERT(user_info.width > 0, "User info width should be positive");
        TEST_ASSERT(user_info.height > 0, "User info height should be positive");
        TEST_ASSERT(user_info.bpp > 0, "User info bpp should be positive");
    }
    
    /* Test user rectangle and point structures */
    fb_user_rect_t user_rect = {10, 20, 100, 80};
    TEST_ASSERT(user_rect.x == 10, "User rectangle x should be correct");
    TEST_ASSERT(user_rect.y == 20, "User rectangle y should be correct");
    TEST_ASSERT(user_rect.width == 100, "User rectangle width should be correct");
    TEST_ASSERT(user_rect.height == 80, "User rectangle height should be correct");
    
    fb_user_point_t user_point = {50, 60};
    TEST_ASSERT(user_point.x == 50, "User point x should be correct");
    TEST_ASSERT(user_point.y == 60, "User point y should be correct");
}

/* ================================
 * Demo Functions
 * ================================ */

static void demo_graphics_mode(void) {
    debug_print("\n=== Graphics Mode Demo ===\n");
    
    /* Set VGA graphics mode */
    fb_set_mode(FB_MODE_VGA_GRAPHICS, 0, 0, 0);
    
    /* Clear screen with dark blue */
    fb_clear(fb_rgb(0, 0, 64));
    
    /* Draw some colorful rectangles */
    fb_rect_t rect1 = {10, 10, 80, 60};
    fb_fill_rect(rect1, FB_COLOR_RED);
    
    fb_rect_t rect2 = {100, 10, 80, 60};
    fb_fill_rect(rect2, FB_COLOR_GREEN);
    
    fb_rect_t rect3 = {190, 10, 80, 60};
    fb_fill_rect(rect3, FB_COLOR_BLUE);
    
    /* Draw some circles */
    fb_point_t center1 = {50, 120};
    fb_fill_circle(center1, 25, FB_COLOR_YELLOW);
    
    fb_point_t center2 = {140, 120};
    fb_fill_circle(center2, 25, FB_COLOR_CYAN);
    
    fb_point_t center3 = {230, 120};
    fb_fill_circle(center3, 25, FB_COLOR_MAGENTA);
    
    /* Draw some lines */
    for (int i = 0; i < 10; i++) {
        fb_point_t start = {0, i * 5};
        fb_point_t end = {320, 200 - i * 5};
        fb_draw_line(start, end, FB_COLOR_WHITE);
    }
    
    debug_print("Graphics demo completed\n");
}

static void demo_text_mode(void) {
    debug_print("\n=== Text Mode Demo ===\n");
    
    /* Set text mode */
    fb_set_mode(FB_MODE_TEXT, 0, 0, 0);
    
    /* Clear screen with blue background */
    fb_color_t bg_color = {.value8 = 0x01}; /* Blue background */
    fb_clear(bg_color);
    
    /* Draw title */
    fb_color_t white_on_blue = {.value8 = 0x1F}; /* White on blue */
    fb_color_t blue_bg = {.value8 = 0x01};
    
    fb_draw_string(25, 2, "IKOS Framebuffer Demo", white_on_blue, blue_bg, NULL);
    fb_draw_string(25, 3, "=====================", white_on_blue, blue_bg, NULL);
    
    /* Draw some colored text */
    fb_color_t red_on_blue = {.value8 = 0x14}; /* Red on blue */
    fb_color_t green_on_blue = {.value8 = 0x12}; /* Green on blue */
    fb_color_t yellow_on_blue = {.value8 = 0x1E}; /* Yellow on blue */
    
    fb_draw_string(5, 6, "Red text example", red_on_blue, blue_bg, NULL);
    fb_draw_string(5, 7, "Green text example", green_on_blue, blue_bg, NULL);
    fb_draw_string(5, 8, "Yellow text example", yellow_on_blue, blue_bg, NULL);
    
    /* Draw some patterns */
    for (int y = 12; y < 20; y++) {
        for (int x = 10; x < 70; x++) {
            char c = 'A' + ((x + y) % 26);
            fb_color_t color = {.value8 = (uint8_t)(0x10 + (x % 7))};
            fb_draw_char(x, y, c, color, blue_bg, NULL);
        }
    }
    
    debug_print("Text demo completed\n");
}

/* ================================
 * Main Test Function
 * ================================ */

/**
 * Run comprehensive framebuffer driver tests
 */
void test_framebuffer_driver(void) {
    debug_print("\n");
    debug_print("========================================\n");
    debug_print("IKOS Framebuffer Driver Test Suite\n");
    debug_print("Issue #26 - Display Driver Testing\n");
    debug_print("========================================\n");
    
    /* Reset test counters */
    g_tests_run = 0;
    g_tests_passed = 0;
    g_tests_failed = 0;
    
    /* Core framebuffer tests */
    test_framebuffer_init();
    test_framebuffer_modes();
    test_basic_drawing();
    test_shape_drawing();
    test_text_rendering();
    test_color_utilities();
    test_statistics();
    
    /* User-space API tests */
    test_user_api_colors();
    test_user_api_integration();
    
    /* Demonstration programs */
    demo_graphics_mode();
    demo_text_mode();
    
    /* Final Results */
    debug_print("\n");
    debug_print("========================================\n");
    debug_print("Test Results Summary\n");
    debug_print("========================================\n");
    TEST_END();
    
    if (g_tests_failed == 0) {
        debug_print("\n✅ All tests passed! Framebuffer driver is working correctly.\n");
    } else {
        debug_print("\n❌ Some tests failed. Please review the implementation.\n");
    }
    
    /* Print final statistics */
    fb_stats_t stats;
    fb_get_stats(&stats);
    
    debug_print("\nFramebuffer Statistics:\n");
    debug_print("  Pixels Drawn: %lu\n", stats.pixels_drawn);
    debug_print("  Lines Drawn: %lu\n", stats.lines_drawn);
    debug_print("  Rectangles Drawn: %lu\n", stats.rects_drawn);
    debug_print("  Characters Drawn: %lu\n", stats.chars_drawn);
    debug_print("  Buffer Swaps: %lu\n", stats.buffer_swaps);
    debug_print("  Current Mode: %dx%d, %d bpp\n", 
                stats.current_width, stats.current_height, stats.current_bpp);
}
