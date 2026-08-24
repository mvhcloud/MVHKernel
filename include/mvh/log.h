#ifndef MVH_LOG_H
#define MVH_LOG_H

#include <stdint.h>

void klog_init(void);
void klog_write(const char *level, const char *message);
void klog_write_category(const char *level, const char *category, const char *message);
void klog_set_console(uint8_t enabled);
uint32_t klog_copy(char *output, uint32_t capacity);
uint32_t klog_size(void);

#endif
