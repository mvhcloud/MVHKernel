#include <stdint.h>
#include "mvh/io.h"
#include "mvh/vga.h"

#define VGA_WIDTH 80u
#define VGA_HEIGHT 25u
#define VGA_MEMORY ((volatile uint16_t *)0xB8000)

static unsigned int row;
static unsigned int column;
static uint8_t color = 0x0Fu;

static void cursor_update(void)
{
    uint16_t position = (uint16_t)(row * VGA_WIDTH + column);
    io_out8(0x3D4u, 0x0Fu);
    io_out8(0x3D5u, (uint8_t)(position & 0xFFu));
    io_out8(0x3D4u, 0x0Eu);
    io_out8(0x3D5u, (uint8_t)((position >> 8u) & 0xFFu));
}

static void scroll(void)
{
    unsigned int y;
    unsigned int x;
    for (y = 1; y < VGA_HEIGHT; y++) {
        for (x = 0; x < VGA_WIDTH; x++) {
            VGA_MEMORY[(y - 1u) * VGA_WIDTH + x] = VGA_MEMORY[y * VGA_WIDTH + x];
        }
    }
    for (x = 0; x < VGA_WIDTH; x++) {
        VGA_MEMORY[(VGA_HEIGHT - 1u) * VGA_WIDTH + x] =
            (uint16_t)' ' | ((uint16_t)color << 8u);
    }
    row = VGA_HEIGHT - 1u;
}

void vga_init(void)
{
    color = 0x0Fu;
    vga_clear();
    vga_cursor_enable();
}

void vga_clear(void)
{
    unsigned int index;
    for (index = 0; index < VGA_WIDTH * VGA_HEIGHT; index++) {
        VGA_MEMORY[index] = (uint16_t)' ' | ((uint16_t)color << 8u);
    }
    row = 0;
    column = 0;
    cursor_update();
}

void vga_put(char value)
{
    if (value == '\n') {
        column = 0;
        row++;
    } else if (value == '\b') {
        if (column > 0u) {
            column--;
            VGA_MEMORY[row * VGA_WIDTH + column] =
                (uint16_t)' ' | ((uint16_t)color << 8u);
        }
        cursor_update();
        return;
    } else {
        VGA_MEMORY[row * VGA_WIDTH + column] =
            (uint16_t)(uint8_t)value | ((uint16_t)color << 8u);
        column++;
        if (column == VGA_WIDTH) {
            column = 0;
            row++;
        }
    }
    if (row == VGA_HEIGHT) {
        scroll();
    }
    cursor_update();
}

void vga_set_color(uint8_t value)
{
    color = value;
}

uint8_t vga_get_color(void)
{
    return color;
}

void vga_cursor_enable(void)
{
    io_out8(0x3D4u, 0x0Au);
    io_out8(0x3D5u, 0x0Eu);
    io_out8(0x3D4u, 0x0Bu);
    io_out8(0x3D5u, 0x0Fu);
    cursor_update();
}

void vga_cursor_disable(void)
{
    io_out8(0x3D4u, 0x0Au);
    io_out8(0x3D5u, 0x20u);
}
