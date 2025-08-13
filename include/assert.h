/* IKOS Assert Macros
 * Assertion support for debugging
 */

#ifndef ASSERT_H
#define ASSERT_H

#ifdef NDEBUG
#define assert(condition) ((void)0)
#else
#define assert(condition) \
    do { \
        if (!(condition)) { \
            printf("Assertion failed: %s, file %s, line %d\n", \
                   #condition, __FILE__, __LINE__); \
            __builtin_trap(); \
        } \
    } while(0)
#endif

#endif /* ASSERT_H */
