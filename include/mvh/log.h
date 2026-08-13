#ifndef MVH_LOG_H
#define MVH_LOG_H

#include <stdint.h>

void klog_init(void);
void klog_write(const char *level, const char *message);
uint32_t klog_copy(char *output, uint32_t capacity);

#endif
