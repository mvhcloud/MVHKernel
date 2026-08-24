#ifndef MVH_FRAMEBUFFER_H
#define MVH_FRAMEBUFFER_H

#include <stdint.h>

typedef struct {
    uint8_t available;
    uint8_t active;
    uint8_t bits_per_pixel;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint64_t physical_address;
    uint64_t mapped_bytes;
    uint64_t pixels_written;
} framebuffer_status_t;

int framebuffer_init(void);
const framebuffer_status_t *framebuffer_status(void);
void framebuffer_clear(uint32_t color);
void framebuffer_put_pixel(int32_t x, int32_t y, uint32_t color);
void framebuffer_fill_rect(int32_t x, int32_t y, int32_t width, int32_t height,
                           uint32_t color);
void framebuffer_outline_rect(int32_t x, int32_t y, int32_t width, int32_t height,
                              int32_t thickness, uint32_t color);
void framebuffer_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color);
void framebuffer_gradient(uint32_t top, uint32_t bottom);
void framebuffer_draw_char(int32_t x, int32_t y, char value, uint32_t scale,
                           uint32_t foreground);
void framebuffer_draw_text(int32_t x, int32_t y, const char *text, uint32_t scale,
                           uint32_t foreground);

#endif
