#include <stdint.h>
#include "mvh/log.h"

#define KLOG_CAPACITY 4096u

static char log_buffer[KLOG_CAPACITY];
static uint32_t log_start;
static uint32_t log_length;

static void log_put(char value)
{
    uint32_t position;
    if (log_length < KLOG_CAPACITY) {
        position = (log_start + log_length) % KLOG_CAPACITY;
        log_length++;
    } else {
        position = log_start;
        log_start = (log_start + 1u) % KLOG_CAPACITY;
    }
    log_buffer[position] = value;
}

static void log_text(const char *text)
{
    while (*text != '\0') {
        log_put(*text++);
    }
}

void klog_init(void)
{
    log_start = 0u;
    log_length = 0u;
}

void klog_write(const char *level, const char *message)
{
    log_put('[');
    log_text(level);
    log_put(']');
    log_put(' ');
    log_text(message);
    log_put('\n');
}

uint32_t klog_copy(char *output, uint32_t capacity)
{
    uint32_t count = log_length;
    uint32_t index;
    if (capacity == 0u) {
        return 0u;
    }
    if (count >= capacity) {
        count = capacity - 1u;
    }
    for (index = 0u; index < count; index++) {
        output[index] = log_buffer[(log_start + index) % KLOG_CAPACITY];
    }
    output[count] = '\0';
    return count;
}
