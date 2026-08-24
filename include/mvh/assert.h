#ifndef MVH_ASSERT_H
#define MVH_ASSERT_H

void kernel_assert_fail(const char *expression, const char *file, unsigned int line)
    __attribute__((noreturn));

#define ASSERT(expression) \
    do { \
        if (!(expression)) kernel_assert_fail(#expression, __FILE__, __LINE__); \
    } while (0)

#define BUG_ON(expression) ASSERT(!(expression))

#endif
