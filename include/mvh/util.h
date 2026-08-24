#ifndef MVH_UTIL_H
#define MVH_UTIL_H

#include <stdint.h>

void *mvh_mem_zero(void *destination, uint64_t size);
void *mvh_mem_set(void *destination, uint8_t value, uint64_t size);
void *mvh_mem_copy(void *destination, const void *source, uint64_t size);
void *mvh_mem_move(void *destination, const void *source, uint64_t size);
int mvh_mem_compare(const void *left, const void *right, uint64_t size);

uint64_t mvh_str_length(const char *text);
uint64_t mvh_str_nlength(const char *text, uint64_t limit);
int mvh_str_equal(const char *left, const char *right);
int mvh_str_nequal(const char *left, const char *right, uint64_t limit);
int mvh_str_compare(const char *left, const char *right);
int mvh_str_ncompare(const char *left, const char *right, uint64_t limit);
uint64_t mvh_str_copy(char *destination, uint64_t capacity, const char *source);
uint64_t mvh_str_ncopy(char *destination, uint64_t capacity, const char *source,
                       uint64_t source_limit);
uint64_t mvh_str_concat(char *destination, uint64_t capacity, const char *source);
const char *mvh_str_find_char(const char *text, char character);
const char *mvh_str_rfind_char(const char *text, char character);
int mvh_str_starts_with(const char *text, const char *prefix);
int mvh_str_ends_with(const char *text, const char *suffix);

int mvh_ascii_is_upper(char character);
int mvh_ascii_is_lower(char character);
int mvh_ascii_is_alpha(char character);
int mvh_ascii_is_digit(char character);
int mvh_ascii_is_alnum(char character);
int mvh_ascii_is_space(char character);
int mvh_ascii_is_hex(char character);
int mvh_ascii_is_printable(char character);
char mvh_ascii_to_upper(char character);
char mvh_ascii_to_lower(char character);
int mvh_ascii_digit_value(char character);
int mvh_ascii_hex_value(char character);

uint64_t mvh_min_u64(uint64_t left, uint64_t right);
uint64_t mvh_max_u64(uint64_t left, uint64_t right);
uint64_t mvh_clamp_u64(uint64_t value, uint64_t minimum, uint64_t maximum);
uint64_t mvh_align_up_u64(uint64_t value, uint64_t alignment);
uint64_t mvh_align_down_u64(uint64_t value, uint64_t alignment);
int mvh_is_power_of_two_u64(uint64_t value);
uint64_t mvh_next_power_of_two_u64(uint64_t value);
int mvh_checked_add_u64(uint64_t left, uint64_t right, uint64_t *result);
int mvh_checked_mul_u64(uint64_t left, uint64_t right, uint64_t *result);
uint32_t mvh_rotate_left32(uint32_t value, uint32_t shift);
uint32_t mvh_rotate_right32(uint32_t value, uint32_t shift);
uint64_t mvh_rotate_left64(uint64_t value, uint32_t shift);
uint64_t mvh_rotate_right64(uint64_t value, uint32_t shift);
uint32_t mvh_popcount32(uint32_t value);
uint32_t mvh_popcount64(uint64_t value);
uint16_t mvh_bswap16(uint16_t value);
uint32_t mvh_bswap32(uint32_t value);
uint64_t mvh_bswap64(uint64_t value);
int mvh_parse_u64(const char *text, uint64_t *result);
int mvh_parse_hex_u64(const char *text, uint64_t *result);

#endif
