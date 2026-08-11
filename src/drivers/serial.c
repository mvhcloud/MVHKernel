#include <stdint.h>
#include "mvh/io.h"
#include "mvh/serial.h"

#define COM1 0x3F8u

void serial_init(void)
{
    io_out8(COM1 + 1u, 0x00u);
    io_out8(COM1 + 3u, 0x80u);
    io_out8(COM1, 0x03u);
    io_out8(COM1 + 1u, 0x00u);
    io_out8(COM1 + 3u, 0x03u);
    io_out8(COM1 + 2u, 0xC7u);
    io_out8(COM1 + 4u, 0x0Bu);
}

void serial_put(char value)
{
    while ((io_in8(COM1 + 5u) & 0x20u) == 0u) {
    }
    io_out8(COM1, (uint8_t)value);
}
