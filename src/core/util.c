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

void mvh_mem_swap(void *left, void *right, uint64_t size)
{
    uint8_t *a = (uint8_t *)left;
    uint8_t *b = (uint8_t *)right;
    uint64_t index;
    if (a == 0 || b == 0 || a == b) return;
    for (index = 0u; index < size; index++) {
        uint8_t value = a[index];
        a[index] = b[index];
        b[index] = value;
    }
}

const void *mvh_mem_find(const void *memory, uint8_t value, uint64_t size)
{
    const uint8_t *bytes = (const uint8_t *)memory;
    uint64_t index;
    if (bytes == 0) return 0;
    for (index = 0u; index < size; index++) if (bytes[index] == value) return bytes + index;
    return 0;
}

void *mvh_mem_reverse(void *memory, uint64_t size)
{
    uint8_t *bytes = (uint8_t *)memory;
    uint64_t index;
    if (bytes == 0) return 0;
    for (index = 0u; index < size / 2u; index++) {
        uint8_t value = bytes[index];
        bytes[index] = bytes[size - index - 1u];
        bytes[size - index - 1u] = value;
    }
    return memory;
}

int mvh_mem_is_zero(const void *memory, uint64_t size)
{
    const uint8_t *bytes = (const uint8_t *)memory;
    uint64_t index;
    if (bytes == 0) return size == 0u;
    for (index = 0u; index < size; index++) if (bytes[index] != 0u) return 0;
    return 1;
}

void *mvh_mem_xor(void *destination, const void *source, uint64_t size)
{
    uint8_t *target = (uint8_t *)destination;
    const uint8_t *input = (const uint8_t *)source;
    uint64_t index;
    if (target == 0 || input == 0) return 0;
    for (index = 0u; index < size; index++) target[index] ^= input[index];
    return destination;
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

int mvh_str_contains(const char *text, const char *needle)
{
    return mvh_str_find(text, needle) != 0;
}

uint64_t mvh_str_count_char(const char *text, char character)
{
    uint64_t count = 0u;
    if (text == 0) return 0u;
    while (*text != '\0') if (*text++ == character) count++;
    return count;
}

const char *mvh_str_trim_left(const char *text)
{
    if (text == 0) return 0;
    while (mvh_ascii_is_space(*text)) text++;
    return text;
}

uint64_t mvh_str_trim_right(char *text)
{
    uint64_t length = mvh_str_length(text);
    if (text == 0) return 0u;
    while (length != 0u && mvh_ascii_is_space(text[length - 1u])) length--;
    text[length] = '\0';
    return length;
}

void mvh_str_to_upper(char *text)
{
    if (text == 0) return;
    while (*text != '\0') { *text = mvh_ascii_to_upper(*text); text++; }
}

void mvh_str_to_lower(char *text)
{
    if (text == 0) return;
    while (*text != '\0') { *text = mvh_ascii_to_lower(*text); text++; }
}

uint64_t mvh_str_replace_char(char *text, char old_character, char new_character)
{
    uint64_t count = 0u;
    if (text == 0) return 0u;
    while (*text != '\0') {
        if (*text == old_character) { *text = new_character; count++; }
        text++;
    }
    return count;
}

int mvh_str_split_once(const char *text, char separator, char *left, uint64_t left_capacity,
                       char *right, uint64_t right_capacity)
{
    const char *split = mvh_str_find_char(text, separator);
    uint64_t left_length;
    if (text == 0 || split == 0 || left == 0 || right == 0 || left_capacity == 0u ||
        right_capacity == 0u) return -1;
    left_length = (uint64_t)(split - text);
    if (left_length >= left_capacity || mvh_str_length(split + 1) >= right_capacity) return -1;
    mvh_str_ncopy(left, left_capacity, text, left_length);
    mvh_str_copy(right, right_capacity, split + 1);
    return 0;
}

uint64_t mvh_str_join(char *destination, uint64_t capacity, const char *left,
                      const char *separator, const char *right)
{
    uint64_t required = mvh_str_length(left) + mvh_str_length(separator) + mvh_str_length(right);
    if (destination == 0 || capacity == 0u) return required;
    destination[0] = '\0';
    mvh_str_concat(destination, capacity, left);
    mvh_str_concat(destination, capacity, separator);
    mvh_str_concat(destination, capacity, right);
    return required;
}

int mvh_str_is_empty(const char *text) { return text == 0 || text[0] == '\0'; }

uint32_t mvh_str_hash_fnv1a32(const char *text)
{
    uint32_t hash = 2166136261u;
    if (text == 0) return hash;
    while (*text != '\0') { hash ^= (uint8_t)*text++; hash *= 16777619u; }
    return hash;
}

uint64_t mvh_str_hash_fnv1a64(const char *text)
{
    uint64_t hash = 14695981039346656037ull;
    if (text == 0) return hash;
    while (*text != '\0') { hash ^= (uint8_t)*text++; hash *= 1099511628211ull; }
    return hash;
}

uint64_t mvh_str_common_prefix(const char *left, const char *right)
{
    uint64_t length = 0u;
    if (left == 0 || right == 0) return 0u;
    while (left[length] != '\0' && left[length] == right[length]) length++;
    return length;
}

const char *mvh_str_find(const char *text, const char *needle)
{
    uint64_t needle_length = mvh_str_length(needle);
    if (text == 0 || needle == 0) return 0;
    if (needle_length == 0u) return text;
    while (*text != '\0') {
        if (mvh_str_ncompare(text, needle, needle_length) == 0) return text;
        text++;
    }
    return 0;
}

void mvh_str_reverse(char *text)
{
    if (text != 0) mvh_mem_reverse(text, mvh_str_length(text));
}

int mvh_ascii_is_upper(char character) { return character >= 'A' && character <= 'Z'; }
int mvh_ascii_is_lower(char character) { return character >= 'a' && character <= 'z'; }
int mvh_ascii_is_alpha(char character) { return mvh_ascii_is_upper(character) || mvh_ascii_is_lower(character); }
int mvh_ascii_is_digit(char character) { return character >= '0' && character <= '9'; }
int mvh_ascii_is_alnum(char character) { return mvh_ascii_is_alpha(character) || mvh_ascii_is_digit(character); }
int mvh_ascii_is_space(char character) { return character == ' ' || character == '\t' || character == '\n' || character == '\r' || character == '\f' || character == '\v'; }
int mvh_ascii_is_hex(char character) { return mvh_ascii_is_digit(character) || (mvh_ascii_to_lower(character) >= 'a' && mvh_ascii_to_lower(character) <= 'f'); }
int mvh_ascii_is_printable(char character) { return (uint8_t)character >= 32u && (uint8_t)character <= 126u; }
int mvh_ascii_is_control(char character) { return (uint8_t)character < 32u || (uint8_t)character == 127u; }
int mvh_ascii_is_punctuation(char character) { return mvh_ascii_is_graph(character) && !mvh_ascii_is_alnum(character); }
int mvh_ascii_is_graph(char character) { return (uint8_t)character >= 33u && (uint8_t)character <= 126u; }
int mvh_ascii_is_binary(char character) { return character == '0' || character == '1'; }
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

uint32_t mvh_min_u32(uint32_t left, uint32_t right) { return left < right ? left : right; }
uint32_t mvh_max_u32(uint32_t left, uint32_t right) { return left > right ? left : right; }
uint32_t mvh_clamp_u32(uint32_t value, uint32_t minimum, uint32_t maximum)
{
    if (value < minimum) return minimum;
    return value > maximum ? maximum : value;
}

uint64_t mvh_abs_i64(int64_t value)
{
    return value < 0 ? (uint64_t)(-(value + 1)) + 1u : (uint64_t)value;
}

uint64_t mvh_gcd_u64(uint64_t left, uint64_t right)
{
    while (right != 0u) { uint64_t remainder = left % right; left = right; right = remainder; }
    return left;
}

int mvh_checked_lcm_u64(uint64_t left, uint64_t right, uint64_t *result)
{
    uint64_t gcd;
    if (result == 0) return -1;
    if (left == 0u || right == 0u) { *result = 0u; return 0; }
    gcd = mvh_gcd_u64(left, right);
    return mvh_checked_mul_u64(left / gcd, right, result);
}

uint64_t mvh_ceil_div_u64(uint64_t numerator, uint64_t denominator)
{
    return denominator == 0u ? 0u : numerator / denominator + (numerator % denominator != 0u);
}

uint64_t mvh_round_up_multiple_u64(uint64_t value, uint64_t multiple)
{
    uint64_t remainder;
    if (multiple == 0u) return 0u;
    remainder = value % multiple;
    if (remainder == 0u) return value;
    return value > MVH_U64_MAX - (multiple - remainder) ? 0u : value + multiple - remainder;
}

uint64_t mvh_round_down_multiple_u64(uint64_t value, uint64_t multiple)
{
    return multiple == 0u ? 0u : value - value % multiple;
}

int mvh_checked_sub_u64(uint64_t left, uint64_t right, uint64_t *result)
{
    if (result == 0 || left < right) return -1;
    *result = left - right;
    return 0;
}

int mvh_checked_add_i64(int64_t left, int64_t right, int64_t *result)
{
    if (result == 0 || (right > 0 && left > INT64_MAX - right) ||
        (right < 0 && left < INT64_MIN - right)) return -1;
    *result = left + right;
    return 0;
}

int mvh_checked_mul_i64(int64_t left, int64_t right, int64_t *result)
{
    uint64_t a;
    uint64_t b;
    uint64_t product;
    uint64_t limit;
    int negative;
    if (result == 0) return -1;
    a = mvh_abs_i64(left);
    b = mvh_abs_i64(right);
    negative = (left < 0) != (right < 0);
    limit = negative ? ((uint64_t)1u << 63u) : (uint64_t)INT64_MAX;
    if (b != 0u && a > limit / b) return -1;
    product = a * b;
    if (negative && product == ((uint64_t)1u << 63u)) *result = INT64_MIN;
    else *result = negative ? -(int64_t)product : (int64_t)product;
    return 0;
}

int mvh_log2_floor_u64(uint64_t value)
{
    int result = -1;
    while (value != 0u) { value >>= 1u; result++; }
    return result;
}

int mvh_log2_ceil_u64(uint64_t value)
{
    int floor;
    if (value == 0u) return -1;
    floor = mvh_log2_floor_u64(value);
    return (value & (value - 1u)) == 0u ? floor : floor + 1;
}

uint32_t mvh_count_leading_zero32(uint32_t value)
{
    uint32_t count = 0u;
    if (value == 0u) return 32u;
    while ((value & 0x80000000u) == 0u) { count++; value <<= 1u; }
    return count;
}

uint32_t mvh_count_trailing_zero32(uint32_t value)
{
    uint32_t count = 0u;
    if (value == 0u) return 32u;
    while ((value & 1u) == 0u) { count++; value >>= 1u; }
    return count;
}

uint64_t mvh_bit_set64(uint64_t value, uint32_t bit) { return bit < 64u ? value | ((uint64_t)1u << bit) : value; }
uint64_t mvh_bit_clear64(uint64_t value, uint32_t bit) { return bit < 64u ? value & ~((uint64_t)1u << bit) : value; }
int mvh_bit_test64(uint64_t value, uint32_t bit) { return bit < 64u && (value & ((uint64_t)1u << bit)) != 0u; }
uint64_t mvh_bit_toggle64(uint64_t value, uint32_t bit) { return bit < 64u ? value ^ ((uint64_t)1u << bit) : value; }

uint64_t mvh_mask_range64(uint32_t offset, uint32_t width)
{
    if (offset >= 64u || width == 0u) return 0u;
    if (width > 64u - offset) width = 64u - offset;
    return (width == 64u ? MVH_U64_MAX : (((uint64_t)1u << width) - 1u)) << offset;
}

uint32_t mvh_parity32(uint32_t value) { return mvh_popcount32(value) & 1u; }
uint32_t mvh_parity64(uint64_t value) { return mvh_popcount64(value) & 1u; }

uint16_t mvh_load_le16(const void *memory)
{
    const uint8_t *bytes = (const uint8_t *)memory;
    return bytes == 0 ? 0u : (uint16_t)bytes[0] | (uint16_t)((uint16_t)bytes[1] << 8u);
}

uint32_t mvh_load_le32(const void *memory)
{
    const uint8_t *bytes = (const uint8_t *)memory;
    return bytes == 0 ? 0u : (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8u) |
           ((uint32_t)bytes[2] << 16u) | ((uint32_t)bytes[3] << 24u);
}

uint64_t mvh_load_le64(const void *memory)
{
    const uint8_t *bytes = (const uint8_t *)memory;
    return bytes == 0 ? 0u : (uint64_t)mvh_load_le32(bytes) |
           ((uint64_t)mvh_load_le32(bytes + 4u) << 32u);
}

int mvh_mem_copy_checked(void *destination, uint64_t destination_size, const void *source,
                         uint64_t size)
{
    if (size > destination_size || (size != 0u && (destination == 0 || source == 0))) return -1;
    mvh_mem_copy(destination, source, size);
    return 0;
}

int mvh_mem_equal(const void *left, const void *right, uint64_t size)
{
    return mvh_mem_compare(left, right, size) == 0;
}

void mvh_mem_fill16(uint16_t *destination, uint16_t value, uint64_t count)
{
    uint64_t index;
    if (destination == 0) return;
    for (index = 0u; index < count; index++) destination[index] = value;
}

void mvh_mem_fill32(uint32_t *destination, uint32_t value, uint64_t count)
{
    uint64_t index;
    if (destination == 0) return;
    for (index = 0u; index < count; index++) destination[index] = value;
}

uint64_t mvh_mem_count_byte(const void *memory, uint8_t value, uint64_t size)
{
    const uint8_t *bytes = (const uint8_t *)memory;
    uint64_t count = 0u;
    uint64_t index;
    if (bytes == 0) return 0u;
    for (index = 0u; index < size; index++) if (bytes[index] == value) count++;
    return count;
}

uint64_t mvh_mem_hash_fnv1a64(const void *memory, uint64_t size)
{
    const uint8_t *bytes = (const uint8_t *)memory;
    uint64_t hash = 14695981039346656037ull;
    uint64_t index;
    if (bytes == 0) return hash;
    for (index = 0u; index < size; index++) { hash ^= bytes[index]; hash *= 1099511628211ull; }
    return hash;
}

int64_t mvh_str_index_char(const char *text, char character)
{
    const char *found = mvh_str_find_char(text, character);
    return found == 0 || text == 0 ? -1 : (int64_t)(found - text);
}

int64_t mvh_str_last_index_char(const char *text, char character)
{
    const char *found = mvh_str_rfind_char(text, character);
    return found == 0 || text == 0 ? -1 : (int64_t)(found - text);
}

int mvh_str_case_equal(const char *left, const char *right)
{
    if (left == 0 || right == 0) return left == right;
    while (*left != '\0' && mvh_ascii_to_lower(*left) == mvh_ascii_to_lower(*right)) {
        left++; right++;
    }
    return mvh_ascii_to_lower(*left) == mvh_ascii_to_lower(*right);
}

int mvh_str_ncase_equal(const char *left, const char *right, uint64_t limit)
{
    uint64_t index;
    if (left == 0 || right == 0) return left == right;
    for (index = 0u; index < limit; index++) {
        if (mvh_ascii_to_lower(left[index]) != mvh_ascii_to_lower(right[index])) return 0;
        if (left[index] == '\0') return 1;
    }
    return 1;
}

const char *mvh_str_skip_space(const char *text) { return mvh_str_trim_left(text); }

int mvh_str_next_token(const char **cursor, char *token, uint64_t capacity)
{
    const char *start;
    uint64_t length = 0u;
    if (cursor == 0 || *cursor == 0 || token == 0 || capacity == 0u) return -1;
    start = mvh_str_skip_space(*cursor);
    if (*start == '\0') { token[0] = '\0'; *cursor = start; return 0; }
    while (start[length] != '\0' && !mvh_ascii_is_space(start[length])) length++;
    if (length >= capacity) return -1;
    mvh_str_ncopy(token, capacity, start, length);
    *cursor = start + length;
    return 1;
}

int mvh_parse_i64(const char *text, int64_t *result)
{
    uint64_t magnitude = 0u;
    uint64_t limit;
    int negative = 0;
    if (text == 0 || result == 0 || *text == '\0') return -1;
    if (*text == '-' || *text == '+') { negative = *text == '-'; text++; }
    if (*text == '\0') return -1;
    limit = negative ? ((uint64_t)1u << 63u) : (uint64_t)INT64_MAX;
    while (*text != '\0') {
        int digit = mvh_ascii_digit_value(*text++);
        if (digit < 0 || magnitude > (limit - (uint64_t)digit) / 10u) return -1;
        magnitude = magnitude * 10u + (uint64_t)digit;
    }
    if (negative && magnitude == ((uint64_t)1u << 63u)) *result = INT64_MIN;
    else *result = negative ? -(int64_t)magnitude : (int64_t)magnitude;
    return 0;
}

uint64_t mvh_format_u64(char *destination, uint64_t capacity, uint64_t value)
{
    char reverse[20];
    uint64_t length = 0u;
    uint64_t index;
    do { reverse[length++] = (char)('0' + value % 10u); value /= 10u; } while (value != 0u);
    if (destination != 0 && capacity != 0u) {
        uint64_t copy = length < capacity - 1u ? length : capacity - 1u;
        for (index = 0u; index < copy; index++) destination[index] = reverse[length - index - 1u];
        destination[copy] = '\0';
    }
    return length;
}

uint64_t mvh_format_hex_u64(char *destination, uint64_t capacity, uint64_t value)
{
    static const char digits[] = "0123456789abcdef";
    char reverse[16];
    uint64_t length = 0u;
    uint64_t index;
    do { reverse[length++] = digits[value & 0xFu]; value >>= 4u; } while (value != 0u);
    if (destination != 0 && capacity != 0u) {
        uint64_t copy = length < capacity - 1u ? length : capacity - 1u;
        for (index = 0u; index < copy; index++) destination[index] = reverse[length - index - 1u];
        destination[copy] = '\0';
    }
    return length;
}

uint64_t mvh_str_normalize_slashes(char *text)
{
    uint64_t read = 0u;
    uint64_t write = 0u;
    int previous_slash = 0;
    if (text == 0) return 0u;
    while (text[read] != '\0') {
        char value = text[read++];
        int slash = value == '/' || value == '\\';
        if (slash && previous_slash) continue;
        text[write++] = slash ? '/' : value;
        previous_slash = slash;
    }
    text[write] = '\0';
    return write;
}

void mvh_store_le16(void *memory, uint16_t value)
{
    uint8_t *bytes = (uint8_t *)memory;
    if (bytes == 0) return;
    bytes[0] = (uint8_t)value; bytes[1] = (uint8_t)(value >> 8u);
}

void mvh_store_le32(void *memory, uint32_t value)
{
    uint8_t *bytes = (uint8_t *)memory;
    if (bytes == 0) return;
    mvh_store_le16(bytes, (uint16_t)value); mvh_store_le16(bytes + 2u, (uint16_t)(value >> 16u));
}

void mvh_store_le64(void *memory, uint64_t value)
{
    uint8_t *bytes = (uint8_t *)memory;
    if (bytes == 0) return;
    mvh_store_le32(bytes, (uint32_t)value); mvh_store_le32(bytes + 4u, (uint32_t)(value >> 32u));
}

uint16_t mvh_load_be16(const void *memory)
{
    const uint8_t *bytes = (const uint8_t *)memory;
    return bytes == 0 ? 0u : (uint16_t)((uint16_t)bytes[0] << 8u) | bytes[1];
}

uint32_t mvh_load_be32(const void *memory)
{
    const uint8_t *bytes = (const uint8_t *)memory;
    return bytes == 0 ? 0u : ((uint32_t)mvh_load_be16(bytes) << 16u) | mvh_load_be16(bytes + 2u);
}

uint64_t mvh_load_be64(const void *memory)
{
    const uint8_t *bytes = (const uint8_t *)memory;
    return bytes == 0 ? 0u : ((uint64_t)mvh_load_be32(bytes) << 32u) | mvh_load_be32(bytes + 4u);
}

void mvh_store_be16(void *memory, uint16_t value)
{
    uint8_t *bytes = (uint8_t *)memory;
    if (bytes == 0) return;
    bytes[0] = (uint8_t)(value >> 8u); bytes[1] = (uint8_t)value;
}

void mvh_store_be32(void *memory, uint32_t value)
{
    uint8_t *bytes = (uint8_t *)memory;
    if (bytes == 0) return;
    mvh_store_be16(bytes, (uint16_t)(value >> 16u)); mvh_store_be16(bytes + 2u, (uint16_t)value);
}

void mvh_store_be64(void *memory, uint64_t value)
{
    uint8_t *bytes = (uint8_t *)memory;
    if (bytes == 0) return;
    mvh_store_be32(bytes, (uint32_t)(value >> 32u)); mvh_store_be32(bytes + 4u, (uint32_t)value);
}

int64_t mvh_min_i64(int64_t left, int64_t right) { return left < right ? left : right; }
int64_t mvh_max_i64(int64_t left, int64_t right) { return left > right ? left : right; }
int64_t mvh_clamp_i64(int64_t value, int64_t minimum, int64_t maximum)
{
    if (value < minimum) return minimum;
    return value > maximum ? maximum : value;
}

uint64_t mvh_saturating_add_u64(uint64_t left, uint64_t right)
{
    return left > MVH_U64_MAX - right ? MVH_U64_MAX : left + right;
}

uint64_t mvh_saturating_sub_u64(uint64_t left, uint64_t right) { return left < right ? 0u : left - right; }
uint64_t mvh_saturating_mul_u64(uint64_t left, uint64_t right)
{
    return left != 0u && right > MVH_U64_MAX / left ? MVH_U64_MAX : left * right;
}

uint64_t mvh_div_round_nearest_u64(uint64_t numerator, uint64_t denominator)
{
    uint64_t quotient;
    uint64_t remainder;
    if (denominator == 0u) return 0u;
    quotient = numerator / denominator;
    remainder = numerator % denominator;
    return remainder >= denominator - remainder ? quotient + 1u : quotient;
}

int mvh_is_even_u64(uint64_t value) { return (value & 1u) == 0u; }
int mvh_is_odd_u64(uint64_t value) { return (value & 1u) != 0u; }

int mvh_fibonacci_u64(uint32_t index, uint64_t *result)
{
    uint64_t previous = 0u;
    uint64_t current = 1u;
    uint32_t step;
    if (result == 0) return -1;
    for (step = 0u; step < index; step++) {
        uint64_t next;
        if (mvh_checked_add_u64(previous, current, &next) != 0) return -1;
        previous = current; current = next;
    }
    *result = previous;
    return 0;
}

int mvh_factorial_u64(uint32_t value, uint64_t *result)
{
    uint64_t product = 1u;
    uint32_t factor;
    if (result == 0) return -1;
    for (factor = 2u; factor <= value; factor++)
        if (mvh_checked_mul_u64(product, factor, &product) != 0) return -1;
    *result = product;
    return 0;
}

int mvh_pow_u64(uint64_t base, uint32_t exponent, uint64_t *result)
{
    uint64_t product = 1u;
    if (result == 0) return -1;
    while (exponent != 0u) {
        if ((exponent & 1u) != 0u && mvh_checked_mul_u64(product, base, &product) != 0) return -1;
        exponent >>= 1u;
        if (exponent != 0u && mvh_checked_mul_u64(base, base, &base) != 0) return -1;
    }
    *result = product;
    return 0;
}

uint64_t mvh_integer_sqrt_u64(uint64_t value)
{
    uint64_t result = 0u;
    uint64_t bit = (uint64_t)1u << 62u;
    while (bit > value) bit >>= 2u;
    while (bit != 0u) {
        if (value >= result + bit) { value -= result + bit; result = (result >> 1u) + bit; }
        else result >>= 1u;
        bit >>= 2u;
    }
    return result;
}

uint64_t mvh_average_u64(uint64_t left, uint64_t right) { return (left & right) + ((left ^ right) >> 1u); }
int mvh_sign_i64(int64_t value) { return (value > 0) - (value < 0); }

uint32_t mvh_count_leading_zero64(uint64_t value)
{
    uint32_t count = 0u;
    if (value == 0u) return 64u;
    while ((value & ((uint64_t)1u << 63u)) == 0u) { count++; value <<= 1u; }
    return count;
}

uint32_t mvh_count_trailing_zero64(uint64_t value)
{
    uint32_t count = 0u;
    if (value == 0u) return 64u;
    while ((value & 1u) == 0u) { count++; value >>= 1u; }
    return count;
}

uint32_t mvh_reverse_bits32(uint32_t value)
{
    uint32_t result = 0u;
    uint32_t index;
    for (index = 0u; index < 32u; index++) { result = (result << 1u) | (value & 1u); value >>= 1u; }
    return result;
}

uint64_t mvh_reverse_bits64(uint64_t value)
{
    uint64_t result = 0u;
    uint32_t index;
    for (index = 0u; index < 64u; index++) { result = (result << 1u) | (value & 1u); value >>= 1u; }
    return result;
}

uint64_t mvh_extract_bits64(uint64_t value, uint32_t offset, uint32_t width)
{
    uint64_t mask = mvh_mask_range64(offset, width);
    return offset >= 64u ? 0u : (value & mask) >> offset;
}

uint64_t mvh_insert_bits64(uint64_t original, uint64_t field, uint32_t offset, uint32_t width)
{
    uint64_t mask = mvh_mask_range64(offset, width);
    return offset >= 64u ? original : (original & ~mask) | ((field << offset) & mask);
}

int mvh_next_set_bit64(uint64_t value, uint32_t start)
{
    uint32_t bit;
    for (bit = start; bit < 64u; bit++) if ((value & ((uint64_t)1u << bit)) != 0u) return (int)bit;
    return -1;
}

int mvh_previous_set_bit64(uint64_t value, uint32_t start)
{
    int bit = start < 64u ? (int)start : 63;
    for (; bit >= 0; bit--) if ((value & ((uint64_t)1u << (uint32_t)bit)) != 0u) return bit;
    return -1;
}

int64_t mvh_sign_extend64(uint64_t value, uint32_t width)
{
    uint64_t mask;
    uint64_t sign;
    if (width == 0u || width > 64u) return 0;
    if (width == 64u) return (int64_t)value;
    mask = ((uint64_t)1u << width) - 1u;
    sign = (uint64_t)1u << (width - 1u);
    value &= mask;
    return (int64_t)((value ^ sign) - sign);
}

uint32_t mvh_count_byte64(uint64_t value, uint8_t byte)
{
    uint32_t count = 0u;
    uint32_t index;
    for (index = 0u; index < 8u; index++) if ((uint8_t)(value >> (index * 8u)) == byte) count++;
    return count;
}
