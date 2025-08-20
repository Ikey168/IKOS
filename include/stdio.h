/* IKOS Standard Headers
 * Minimal standard library definitions for kernel compilation
 */

#ifndef STDIO_H
#define STDIO_H

#include <stddef.h>
#include <stdarg.h>

/* Printf function */
int printf(const char* format, ...);

/* sprintf and snprintf functions */
int sprintf(char* buffer, const char* format, ...);
int snprintf(char* buffer, size_t size, const char* format, ...);
int vsprintf(char* buffer, const char* format, va_list args);
int vsnprintf(char* buffer, size_t size, const char* format, va_list args);

#endif /* STDIO_H */
