/* IKOS String Functions
 * String manipulation functions
 */

#ifndef STRING_H
#define STRING_H

#include <stddef.h>

/* String functions */
size_t strlen(const char* str);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t count);
int strcmp(const char* str1, const char* str2);
int strncmp(const char* str1, const char* str2, size_t count);

/* Memory functions */
void* memset(void* dest, int value, size_t count);
void* memcpy(void* dest, const void* src, size_t count);
int memcmp(const void* ptr1, const void* ptr2, size_t count);

#endif /* STRING_H */
