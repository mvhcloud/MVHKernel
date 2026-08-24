#include <stdint.h>
#include "mvh/util.h"

#define MVH_U64_MAX (~(uint64_t)0)

void *mvh_mem_zero(void *destination, uint64_t size)
{
    return mvh_mem_set(destination, 0u, size);
}

void *mvh_mem_set(void *destination, uint8_t value, uint64_t size)
{
    uint8_t *bytes = (uint8_t *)destination;
    uint64_t index;
    if (bytes == 0 && size != 0u) return 0;
    for (index = 0u; index < size; index++) bytes[index] = value;
    return destination;
}

void *mvh_mem_copy(void *destination, const void *source, uint64_t size)
{
    uint8_t *target = (uint8_t *)destination;
    const uint8_t *input = (const uint8_t *)source;
    uint64_t index;
    if ((target == 0 || input == 0) && size != 0u) return 0;
    for (index = 0u; index < size; index++) target[index] = input[index];
    return destination;
}

void *mvh_mem_move(void *destination, const void *source, uint64_t size)
{
    uint8_t *target = (uint8_t *)destination;
    const uint8_t *input = (const uint8_t *)source;
    uint64_t index;
    if ((target == 0 || input == 0) && size != 0u) return 0;
    if (size == 0u) return destination;
    if (target > input && target < input + size) {
        for (index = size; index != 0u; index--) target[index - 1u] = input[index - 1u];
    } else {
        for (index = 0u; index < size; index++) target[index] = input[index];
    }
    return destination;
}

int mvh_mem_compare(const void *left, const void *right, uint64_t size)
{
    const uint8_t *a = (const uint8_t *)left;
    const uint8_t *b = (const uint8_t *)right;
    uint64_t index;
    if (a == b || size == 0u) return 0;
    if (a == 0) return -1;
    if (b == 0) return 1;
    for (index = 0u; index < size; index++) {
        if (a[index] != b[index]) return a[index] < b[index] ? -1 : 1;
    }
    return 0;
}

uint64_t mvh_str_length(const char *text)
{
    uint64_t length = 0u;
    if (text == 0) return 0u;
    while (text[length] != '\0') length++;
    return length;
}

uint64_t mvh_str_nlength(const char *text, uint64_t limit)
{
    uint64_t length = 0u;
    if (text == 0) return 0u;
    while (length < limit && text[length] != '\0') length++;
    return length;
}

int mvh_str_equal(const char *left, const char *right)
{
    return mvh_str_compare(left, right) == 0;
}

int mvh_str_nequal(const char *left, const char *right, uint64_t limit)
{
    return mvh_str_ncompare(left, right, limit) == 0;
}

int mvh_str_compare(const char *left, const char *right)
{
    uint64_t index = 0u;
    if (left == right) return 0;
    if (left == 0) return -1;
    if (right == 0) return 1;
    while (left[index] != '\0' && left[index] == right[index]) index++;
    if ((uint8_t)left[index] == (uint8_t)right[index]) return 0;
    return (uint8_t)left[index] < (uint8_t)right[index] ? -1 : 1;
}

int mvh_str_ncompare(const char *left, const char *right, uint64_t limit)
{
    uint64_t index;
    if (left == right || limit == 0u) return 0;
    if (left == 0) return -1;
    if (right == 0) return 1;
    for (index = 0u; index < limit; index++) {
        if ((uint8_t)left[index] != (uint8_t)right[index])
            return (uint8_t)left[index] < (uint8_t)right[index] ? -1 : 1;
        if (left[index] == '\0') return 0;
    }
    return 0;
}

uint64_t mvh_str_copy(char *destination, uint64_t capacity, const char *source)
{
    return mvh_str_ncopy(destination, capacity, source, MVH_U64_MAX);
}

uint64_t mvh_str_ncopy(char *destination, uint64_t capacity, const char *source,
                       uint64_t source_limit)
{
    uint64_t length = mvh_str_nlength(source, source_limit);
    uint64_t copy = capacity == 0u ? 0u : mvh_min_u64(length, capacity - 1u);
    uint64_t index;
    if (destination == 0 || capacity == 0u) return length;
    for (index = 0u; index < copy; index++) destination[index] = source[index];
    destination[copy] = '\0';
    return length;
}

uint64_t mvh_str_concat(char *destination, uint64_t capacity, const char *source)
{
    uint64_t prefix = mvh_str_nlength(destination, capacity);
    uint64_t suffix = mvh_str_length(source);
    uint64_t copy;
    uint64_t index;
    if (destination == 0 || capacity == 0u || prefix == capacity) return prefix + suffix;
    copy = mvh_min_u64(suffix, capacity - prefix - 1u);
    for (index = 0u; index < copy; index++) destination[prefix + index] = source[index];
    destination[prefix + copy] = '\0';
    return prefix + suffix;
}

const char *mvh_str_find_char(const char *text, char character)
{
    if (text == 0) return 0;
    do {
        if (*text == character) return text;
    } while (*text++ != '\0');
    return 0;
}

const char *mvh_str_rfind_char(const char *text, char character)
{
    const char *found = 0;
    if (text == 0) return 0;
    do {
        if (*text == character) found = text;
    } while (*text++ != '\0');
    return found;
}

int mvh_str_starts_with(const char *text, const char *prefix)
{
    uint64_t length = mvh_str_length(prefix);
    return text != 0 && prefix != 0 && mvh_str_ncompare(text, prefix, length) == 0;
}

int mvh_str_ends_with(const char *text, const char *suffix)
{
    uint64_t text_length = mvh_str_length(text);
    uint64_t suffix_length = mvh_str_length(suffix);
    return text != 0 && suffix != 0 && suffix_length <= text_length &&
           mvh_str_compare(text + text_length - suffix_length, suffix) == 0;
}

int mvh_ascii_is_upper(char character) { return character >= 'A' && character <= 'Z'; }
int mvh_ascii_is_lower(char character) { return character >= 'a' && character <= 'z'; }
int mvh_ascii_is_alpha(char character) { return mvh_ascii_is_upper(character) || mvh_ascii_is_lower(character); }
int mvh_ascii_is_digit(char character) { return character >= '0' && character <= '9'; }
int mvh_ascii_is_alnum(char character) { return mvh_ascii_is_alpha(character) || mvh_ascii_is_digit(character); }
int mvh_ascii_is_space(char character) { return character == ' ' || character == '\t' || character == '\n' || character == '\r' || character == '\f' || character == '\v'; }
int mvh_ascii_is_hex(char character) { return mvh_ascii_is_digit(character) || (mvh_ascii_to_lower(character) >= 'a' && mvh_ascii_to_lower(character) <= 'f'); }
int mvh_ascii_is_printable(char character) { return (uint8_t)character >= 32u && (uint8_t)character <= 126u; }
char mvh_ascii_to_upper(char character) { return mvh_ascii_is_lower(character) ? (char)(character - ('a' - 'A')) : character; }
char mvh_ascii_to_lower(char character) { return mvh_ascii_is_upper(character) ? (char)(character + ('a' - 'A')) : character; }
int mvh_ascii_digit_value(char character) { return mvh_ascii_is_digit(character) ? character - '0' : -1; }
int mvh_ascii_hex_value(char character)
{
    character = mvh_ascii_to_lower(character);
    if (mvh_ascii_is_digit(character)) return character - '0';
    return character >= 'a' && character <= 'f' ? character - 'a' + 10 : -1;
}

uint64_t mvh_min_u64(uint64_t left, uint64_t right) { return left < right ? left : right; }
uint64_t mvh_max_u64(uint64_t left, uint64_t right) { return left > right ? left : right; }
uint64_t mvh_clamp_u64(uint64_t value, uint64_t minimum, uint64_t maximum)
{
    if (value < minimum) return minimum;
    return value > maximum ? maximum : value;
}

uint64_t mvh_align_up_u64(uint64_t value, uint64_t alignment)
{
    uint64_t mask;
    if (!mvh_is_power_of_two_u64(alignment)) return 0u;
    mask = alignment - 1u;
    if (value > MVH_U64_MAX - mask) return 0u;
    return (value + mask) & ~mask;
}

uint64_t mvh_align_down_u64(uint64_t value, uint64_t alignment)
{
    return mvh_is_power_of_two_u64(alignment) ? value & ~(alignment - 1u) : 0u;
}

int mvh_is_power_of_two_u64(uint64_t value) { return value != 0u && (value & (value - 1u)) == 0u; }

uint64_t mvh_next_power_of_two_u64(uint64_t value)
{
    uint32_t shift;
    if (value <= 1u) return 1u;
    if (value > ((uint64_t)1u << 63u)) return 0u;
    value--;
    for (shift = 1u; shift < 64u; shift <<= 1u) value |= value >> shift;
    return value + 1u;
}

int mvh_checked_add_u64(uint64_t left, uint64_t right, uint64_t *result)
{
    if (result == 0 || left > MVH_U64_MAX - right) return -1;
    *result = left + right;
    return 0;
}

int mvh_checked_mul_u64(uint64_t left, uint64_t right, uint64_t *result)
{
    if (result == 0 || (left != 0u && right > MVH_U64_MAX / left)) return -1;
    *result = left * right;
    return 0;
}

uint32_t mvh_rotate_left32(uint32_t value, uint32_t shift)
{
    shift &= 31u;
    return shift == 0u ? value : (value << shift) | (value >> (32u - shift));
}

uint32_t mvh_rotate_right32(uint32_t value, uint32_t shift)
{
    shift &= 31u;
    return shift == 0u ? value : (value >> shift) | (value << (32u - shift));
}

uint64_t mvh_rotate_left64(uint64_t value, uint32_t shift)
{
    shift &= 63u;
    return shift == 0u ? value : (value << shift) | (value >> (64u - shift));
}

uint64_t mvh_rotate_right64(uint64_t value, uint32_t shift)
{
    shift &= 63u;
    return shift == 0u ? value : (value >> shift) | (value << (64u - shift));
}

uint32_t mvh_popcount32(uint32_t value)
{
    uint32_t count = 0u;
    while (value != 0u) { value &= value - 1u; count++; }
    return count;
}

uint32_t mvh_popcount64(uint64_t value)
{
    uint32_t count = 0u;
    while (value != 0u) { value &= value - 1u; count++; }
    return count;
}

uint16_t mvh_bswap16(uint16_t value) { return (uint16_t)((value << 8u) | (value >> 8u)); }
uint32_t mvh_bswap32(uint32_t value)
{
    return ((value & 0x000000FFu) << 24u) | ((value & 0x0000FF00u) << 8u) |
           ((value & 0x00FF0000u) >> 8u) | ((value & 0xFF000000u) >> 24u);
}

uint64_t mvh_bswap64(uint64_t value)
{
    return ((uint64_t)mvh_bswap32((uint32_t)value) << 32u) |
           mvh_bswap32((uint32_t)(value >> 32u));
}

int mvh_parse_u64(const char *text, uint64_t *result)
{
    uint64_t value = 0u;
    int digit;
    if (text == 0 || result == 0 || *text == '\0') return -1;
    while (*text != '\0') {
        digit = mvh_ascii_digit_value(*text++);
        if (digit < 0 || value > (MVH_U64_MAX - (uint64_t)digit) / 10u) return -1;
        value = value * 10u + (uint64_t)digit;
    }
    *result = value;
    return 0;
}

int mvh_parse_hex_u64(const char *text, uint64_t *result)
{
    uint64_t value = 0u;
    int digit;
    if (text == 0 || result == 0) return -1;
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) text += 2;
    if (*text == '\0') return -1;
    while (*text != '\0') {
        digit = mvh_ascii_hex_value(*text++);
        if (digit < 0 || value > (MVH_U64_MAX - (uint64_t)digit) / 16u) return -1;
        value = value * 16u + (uint64_t)digit;
    }
    *result = value;
    return 0;
}
