#include <stdint.h>
#include "mvh/io.h"
#include "mvh/keyboard.h"

static const char normal_map[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z',
    'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

static const char shift_map[128] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|', 'Z',
    'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' '
};

char keyboard_read_char(void)
{
    static uint8_t shift;
    static uint8_t caps_lock;
    static uint8_t control;
    uint8_t scan_code;
    uint8_t released;
    char value;
    for (;;) {
        if ((io_in8(0x64u) & 1u) == 0u) {
            continue;
        }
        scan_code = io_in8(0x60u);
        released = scan_code & 0x80u;
        scan_code &= 0x7Fu;
        if (scan_code == 42u || scan_code == 54u) {
            shift = released == 0u;
            continue;
        }
        if (scan_code == 29u) {
            control = released == 0u;
            continue;
        }
        if (released != 0u) {
            continue;
        }
        if (scan_code == 58u) {
            caps_lock ^= 1u;
            continue;
        }
        if (scan_code >= 128u) {
            continue;
        }
        value = shift ? shift_map[scan_code] : normal_map[scan_code];
        if (control != 0u && value >= 'a' && value <= 'z') {
            return (char)(value - 'a' + 1);
        }
        if (control != 0u && value >= 'A' && value <= 'Z') {
            return (char)(value - 'A' + 1);
        }
        if (value >= 'a' && value <= 'z' && caps_lock != 0u) {
            value = (char)(value - 'a' + 'A');
        } else if (value >= 'A' && value <= 'Z' && caps_lock != 0u) {
            value = (char)(value - 'A' + 'a');
        }
        if (value != 0) {
            return value;
        }
    }
}
