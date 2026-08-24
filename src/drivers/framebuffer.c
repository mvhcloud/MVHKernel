#include <stdint.h>
#include "mvh/bootinfo.h"
#include "mvh/framebuffer.h"
#include "mvh/memory.h"

#define FRAMEBUFFER_WINDOW 0x60000000u
#define FRAMEBUFFER_MAX_BYTES (64u * 1024u * 1024u)

typedef struct { char value; uint8_t rows[7]; } glyph_t;

static const glyph_t font[] = {
    {' ',{0,0,0,0,0,0,0}}, {'!',{4,4,4,4,4,0,4}}, {'-',{0,0,0,31,0,0,0}},
    {'.',{0,0,0,0,0,6,6}}, {':',{0,6,6,0,6,6,0}}, {'/',{1,2,4,8,16,0,0}},
    {'0',{14,17,19,21,25,17,14}}, {'1',{4,12,4,4,4,4,14}},
    {'2',{14,17,1,2,4,8,31}}, {'3',{30,1,1,14,1,1,30}},
    {'4',{2,6,10,18,31,2,2}}, {'5',{31,16,16,30,1,1,30}},
    {'6',{14,16,16,30,17,17,14}}, {'7',{31,1,2,4,8,8,8}},
    {'8',{14,17,17,14,17,17,14}}, {'9',{14,17,17,15,1,1,14}},
    {'A',{14,17,17,31,17,17,17}}, {'B',{30,17,17,30,17,17,30}},
    {'C',{14,17,16,16,16,17,14}}, {'D',{28,18,17,17,17,18,28}},
    {'E',{31,16,16,30,16,16,31}}, {'F',{31,16,16,30,16,16,16}},
    {'G',{14,17,16,23,17,17,15}}, {'H',{17,17,17,31,17,17,17}},
    {'I',{14,4,4,4,4,4,14}}, {'J',{7,2,2,2,2,18,12}},
    {'K',{17,18,20,24,20,18,17}}, {'L',{16,16,16,16,16,16,31}},
    {'M',{17,27,21,21,17,17,17}}, {'N',{17,25,21,19,17,17,17}},
    {'O',{14,17,17,17,17,17,14}}, {'P',{30,17,17,30,16,16,16}},
    {'Q',{14,17,17,17,21,18,13}}, {'R',{30,17,17,30,20,18,17}},
    {'S',{15,16,16,14,1,1,30}}, {'T',{31,4,4,4,4,4,4}},
    {'U',{17,17,17,17,17,17,14}}, {'V',{17,17,17,17,17,10,4}},
    {'W',{17,17,17,21,21,21,10}}, {'X',{17,17,10,4,10,17,17}},
    {'Y',{17,17,10,4,4,4,4}}, {'Z',{31,1,2,4,8,16,31}},
    {'?',{14,17,1,2,4,0,4}}, {'_',{0,0,0,0,0,0,31}}
};

static framebuffer_status_t state;
static volatile uint8_t *pixels;

static uint32_t color_mix(uint32_t top, uint32_t bottom, uint32_t step, uint32_t total)
{
    uint32_t tr = (top >> 16u) & 0xFFu, tg = (top >> 8u) & 0xFFu, tb = top & 0xFFu;
    uint32_t br = (bottom >> 16u) & 0xFFu, bg = (bottom >> 8u) & 0xFFu, bb = bottom & 0xFFu;
    uint32_t r = (tr * (total - step) + br * step) / total;
    uint32_t g = (tg * (total - step) + bg * step) / total;
    uint32_t b = (tb * (total - step) + bb * step) / total;
    return (r << 16u) | (g << 8u) | b;
}

int framebuffer_init(void)
{
    const mvh_bootinfo_snapshot_t *boot = bootinfo_current();
    uint64_t physical;
    uint64_t offset;
    uint64_t bytes;
    uint64_t pages;
    uint64_t page;
    uint8_t *clear = (uint8_t *)&state;
    uint32_t index;
    for (index = 0u; index < sizeof(state); index++) clear[index] = 0u;
    pixels = 0;
    if ((boot->flags & MVH_BOOTINFO_FLAG_FRAMEBUFFER) == 0u ||
        boot->framebuffer.address == 0u || boot->framebuffer.width < 320u ||
        boot->framebuffer.height < 200u ||
        (boot->framebuffer.bits_per_pixel != 24u && boot->framebuffer.bits_per_pixel != 32u))
        return -1;
    bytes = (uint64_t)boot->framebuffer.pitch * boot->framebuffer.height;
    if (bytes == 0u || bytes > FRAMEBUFFER_MAX_BYTES) return -1;
    physical = boot->framebuffer.address & ~0xFFFull;
    offset = boot->framebuffer.address & 0xFFFu;
    pages = (offset + bytes + 0xFFFu) / 0x1000u;
    for (page = 0u; page < pages; page++) {
        if (vmm_map_page(FRAMEBUFFER_WINDOW + (uintptr_t)(page * 0x1000u),
                         (uintptr_t)(physical + page * 0x1000u),
                         VMM_WRITABLE | VMM_CACHE_DISABLE | VMM_NO_EXECUTE) != 0) return -1;
    }
    pixels = (volatile uint8_t *)(uintptr_t)(FRAMEBUFFER_WINDOW + offset);
    state.available = 1u;
    state.active = 1u;
    state.bits_per_pixel = (uint8_t)boot->framebuffer.bits_per_pixel;
    state.width = boot->framebuffer.width;
    state.height = boot->framebuffer.height;
    state.pitch = boot->framebuffer.pitch;
    state.physical_address = boot->framebuffer.address;
    state.mapped_bytes = bytes;
    return 0;
}

const framebuffer_status_t *framebuffer_status(void) { return &state; }

void framebuffer_put_pixel(int32_t x, int32_t y, uint32_t color)
{
    volatile uint8_t *target;
    if (state.active == 0u || x < 0 || y < 0 || (uint32_t)x >= state.width ||
        (uint32_t)y >= state.height) return;
    target = pixels + (uint32_t)y * state.pitch + (uint32_t)x * (state.bits_per_pixel / 8u);
    target[0] = (uint8_t)color;
    target[1] = (uint8_t)(color >> 8u);
    target[2] = (uint8_t)(color >> 16u);
    if (state.bits_per_pixel == 32u) target[3] = 0u;
    state.pixels_written++;
}

void framebuffer_fill_rect(int32_t x, int32_t y, int32_t width, int32_t height,
                           uint32_t color)
{
    int32_t py, px;
    if (width <= 0 || height <= 0) return;
    for (py = y; py < y + height; py++)
        for (px = x; px < x + width; px++) framebuffer_put_pixel(px, py, color);
}

void framebuffer_clear(uint32_t color)
{
    framebuffer_fill_rect(0, 0, (int32_t)state.width, (int32_t)state.height, color);
}

void framebuffer_outline_rect(int32_t x, int32_t y, int32_t width, int32_t height,
                              int32_t thickness, uint32_t color)
{
    if (thickness <= 0) return;
    framebuffer_fill_rect(x, y, width, thickness, color);
    framebuffer_fill_rect(x, y + height - thickness, width, thickness, color);
    framebuffer_fill_rect(x, y, thickness, height, color);
    framebuffer_fill_rect(x + width - thickness, y, thickness, height, color);
}

void framebuffer_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color)
{
    int32_t dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int32_t sx = x0 < x1 ? 1 : -1;
    int32_t dy = -(y1 > y0 ? y1 - y0 : y0 - y1);
    int32_t sy = y0 < y1 ? 1 : -1;
    int32_t error = dx + dy;
    for (;;) {
        int32_t twice;
        framebuffer_put_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        twice = error * 2;
        if (twice >= dy) { error += dy; x0 += sx; }
        if (twice <= dx) { error += dx; y0 += sy; }
    }
}

void framebuffer_gradient(uint32_t top, uint32_t bottom)
{
    uint32_t y;
    if (state.height < 2u) return;
    for (y = 0u; y < state.height; y++)
        framebuffer_fill_rect(0, (int32_t)y, (int32_t)state.width, 1,
                              color_mix(top, bottom, y, state.height - 1u));
}

static const uint8_t *glyph_rows(char value)
{
    uint32_t index;
    if (value >= 'a' && value <= 'z') value = (char)(value - 'a' + 'A');
    for (index = 0u; index < sizeof(font) / sizeof(font[0]); index++)
        if (font[index].value == value) return font[index].rows;
    return font[sizeof(font) / sizeof(font[0]) - 1u].rows;
}

void framebuffer_draw_char(int32_t x, int32_t y, char value, uint32_t scale,
                           uint32_t foreground)
{
    const uint8_t *rows = glyph_rows(value);
    uint32_t row, column;
    if (scale == 0u || scale > 8u) return;
    for (row = 0u; row < 7u; row++) {
        for (column = 0u; column < 5u; column++) {
            if ((rows[row] & (1u << (4u - column))) != 0u)
                framebuffer_fill_rect(x + (int32_t)(column * scale),
                                      y + (int32_t)(row * scale),
                                      (int32_t)scale, (int32_t)scale, foreground);
        }
    }
}

void framebuffer_draw_text(int32_t x, int32_t y, const char *text, uint32_t scale,
                           uint32_t foreground)
{
    if (text == 0) return;
    while (*text != '\0') {
        framebuffer_draw_char(x, y, *text++, scale, foreground);
        x += (int32_t)(6u * scale);
    }
}
