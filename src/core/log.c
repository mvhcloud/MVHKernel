#include <stdint.h>
#include "mvh/log.h"
#include "mvh/serial.h"
#include "mvh/timer.h"
#include "mvh/vga.h"

#define KLOG_CAPACITY 16384u

static char log_buffer[KLOG_CAPACITY];
static uint32_t log_start;
static uint32_t log_length;
static uint8_t console_enabled;

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
    if (console_enabled != 0u) {
        serial_put(value);
        vga_put(value);
    }
}

static void log_text(const char *text)
{
    while (*text != '\0') {
        log_put(*text++);
    }
}

static void log_number(uint64_t value)
{
    char digits[21];
    uint32_t length = 0u;
    if (value == 0u) {
        log_put('0');
        return;
    }
    while (value != 0u) {
        digits[length++] = (char)('0' + value % 10u);
        value /= 10u;
    }
    while (length != 0u) log_put(digits[--length]);
}

void klog_init(void)
{
    log_start = 0u;
    log_length = 0u;
    console_enabled = 0u;
}

void klog_write(const char *level, const char *message)
{
    klog_write_category(level, "kernel", message);
}

void klog_write_category(const char *level, const char *category, const char *message)
{
    log_put('[');
    log_number(timer_ticks());
    log_put(']');
    log_put('[');
    log_text(level);
    log_put(']');
    log_put('[');
    log_text(category);
    log_put(']');
    log_put(' ');
    log_text(message);
    log_put('\n');
}

void klog_set_console(uint8_t enabled)
{
    console_enabled = enabled;
}

uint32_t klog_size(void)
{
    return log_length;
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
