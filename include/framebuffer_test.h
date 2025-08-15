/* IKOS Framebuffer Driver Test Header
 * Issue #26 - Display (Framebuffer) Driver Test Suite
 */

#ifndef FRAMEBUFFER_TEST_H
#define FRAMEBUFFER_TEST_H

/**
 * Run comprehensive framebuffer driver tests
 * 
 * This function tests all components of the framebuffer driver:
 * - Basic initialization and mode setting
 * - Pixel drawing and retrieval operations
 * - Shape drawing (lines, rectangles, circles)
 * - Text rendering in both text and graphics modes
 * - Color utilities and conversion functions
 * - User-space API compatibility
 * - Statistics and debugging features
 * 
 * Also includes demonstration programs showing:
 * - Graphics mode with colorful shapes and patterns
 * - Text mode with colored text and character patterns
 */
void test_framebuffer_driver(void);

#endif /* FRAMEBUFFER_TEST_H */
