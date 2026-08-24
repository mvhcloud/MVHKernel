#ifndef MVH_VGA_H
#define MVH_VGA_H

#include <stdint.h>

void vga_init(void);
void vga_clear(void);
void vga_put(char value);
void vga_set_color(uint8_t color);
uint8_t vga_get_color(void);
void vga_cursor_enable(void);
void vga_cursor_disable(void);

#endif
