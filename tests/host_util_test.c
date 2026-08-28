#include <stdint.h>
#include <stdio.h>
#include "mvh/util.h"

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

int main(void)
{
    uint8_t bytes[12];
    uint8_t copy[12];
    char text[12];
    char left[12];
    char right[12];
    char joined[24];
    const char level[] = "level";
    uint64_t value;
    int64_t signed_value;
    uint16_t words16[2];
    uint32_t words32[2];
    uint8_t endian_buffer[8];
    const char *cursor;
    const uint8_t little_endian[] = { 0x78u, 0x56u, 0x34u, 0x12u, 0xEFu, 0xCDu, 0xABu, 0x90u };

    CHECK(mvh_mem_zero(bytes, sizeof(bytes)) == bytes);
    CHECK(mvh_mem_set(bytes, 0x5Au, sizeof(bytes)) == bytes && bytes[11] == 0x5Au);
    CHECK(mvh_mem_copy(copy, bytes, sizeof(bytes)) == copy);
    CHECK(mvh_mem_move(copy + 2, copy, 8u) == copy + 2 && copy[9] == 0x5Au);
    CHECK(mvh_mem_compare(bytes, copy, 2u) == 0);
    bytes[0] = 1u; bytes[1] = 2u; copy[0] = 3u; copy[1] = 4u;
    mvh_mem_swap(bytes, copy, 2u);
    CHECK(bytes[0] == 3u && bytes[1] == 4u && copy[0] == 1u && copy[1] == 2u);
    CHECK(mvh_mem_find(bytes, 4u, sizeof(bytes)) == bytes + 1u);
    CHECK(mvh_mem_reverse(bytes, sizeof(bytes)) == bytes && bytes[11] == 3u);
    mvh_mem_zero(copy, sizeof(copy));
    CHECK(mvh_mem_is_zero(copy, sizeof(copy)));
    CHECK(mvh_mem_xor(copy, bytes, sizeof(copy)) == copy && copy[0] == bytes[0]);

    CHECK(mvh_str_length("kernel") == 6u);
    CHECK(mvh_str_nlength("kernel", 3u) == 3u);
    CHECK(mvh_str_equal("mvh", "mvh"));
    CHECK(mvh_str_nequal("kernel", "kern", 4u));
    CHECK(mvh_str_compare("a", "b") < 0);
    CHECK(mvh_str_ncompare("abc", "abd", 2u) == 0);
    CHECK(mvh_str_copy(text, sizeof(text), "MVH") == 3u && mvh_str_equal(text, "MVH"));
    CHECK(mvh_str_ncopy(text, sizeof(text), "Kernel", 3u) == 3u && mvh_str_equal(text, "Ker"));
    CHECK(mvh_str_concat(text, sizeof(text), "nel") == 6u && mvh_str_equal(text, "Kernel"));
    CHECK(mvh_str_find_char(text, 'r') == text + 2);
    CHECK(mvh_str_rfind_char(level, 'e') == level + 3);
    CHECK(mvh_str_starts_with("MVH Kernel", "MVH"));
    CHECK(mvh_str_ends_with("kernel.elf", ".elf"));
    CHECK(mvh_str_contains("MVH Kernel", "Kernel"));
    CHECK(mvh_str_count_char("banana", 'a') == 3u);
    CHECK(mvh_str_equal(mvh_str_trim_left("  kernel"), "kernel"));
    mvh_str_copy(text, sizeof(text), "kernel  ");
    CHECK(mvh_str_trim_right(text) == 6u && mvh_str_equal(text, "kernel"));
    mvh_str_to_upper(text);
    CHECK(mvh_str_equal(text, "KERNEL"));
    mvh_str_to_lower(text);
    CHECK(mvh_str_equal(text, "kernel"));
    CHECK(mvh_str_replace_char(text, 'e', '3') == 2u && mvh_str_equal(text, "k3rn3l"));
    CHECK(mvh_str_split_once("name=value", '=', left, sizeof(left), right, sizeof(right)) == 0 &&
          mvh_str_equal(left, "name") && mvh_str_equal(right, "value"));
    CHECK(mvh_str_join(joined, sizeof(joined), "boot", "/", "kernel.elf") == 15u &&
          mvh_str_equal(joined, "boot/kernel.elf"));
    CHECK(mvh_str_is_empty("") && mvh_str_is_empty(0));
    CHECK(mvh_str_hash_fnv1a32("hello") == 0x4F9F2CABu);
    CHECK(mvh_str_hash_fnv1a64("hello") == 0xA430D84680AABD0Bull);
    CHECK(mvh_str_common_prefix("kernel", "keeper") == 2u);
    CHECK(mvh_str_equal(mvh_str_find("mvh-kernel", "kernel"), "kernel"));
    mvh_str_copy(text, sizeof(text), "kernel");
    mvh_str_reverse(text);
    CHECK(mvh_str_equal(text, "lenrek"));

    CHECK(mvh_ascii_is_upper('A'));
    CHECK(mvh_ascii_is_lower('z'));
    CHECK(mvh_ascii_is_alpha('K'));
    CHECK(mvh_ascii_is_digit('7'));
    CHECK(mvh_ascii_is_alnum('1'));
    CHECK(mvh_ascii_is_space('\n'));
    CHECK(mvh_ascii_is_hex('f'));
    CHECK(mvh_ascii_is_printable('~'));
    CHECK(mvh_ascii_is_control('\n'));
    CHECK(mvh_ascii_is_punctuation('!'));
    CHECK(mvh_ascii_is_graph('@'));
    CHECK(mvh_ascii_is_binary('1') && !mvh_ascii_is_binary('2'));
    CHECK(mvh_ascii_to_upper('m') == 'M');
    CHECK(mvh_ascii_to_lower('V') == 'v');
    CHECK(mvh_ascii_digit_value('9') == 9);
    CHECK(mvh_ascii_hex_value('B') == 11);

    CHECK(mvh_min_u64(4u, 7u) == 4u);
    CHECK(mvh_max_u64(4u, 7u) == 7u);
    CHECK(mvh_clamp_u64(9u, 2u, 6u) == 6u);
    CHECK(mvh_align_up_u64(17u, 16u) == 32u);
    CHECK(mvh_align_down_u64(31u, 16u) == 16u);
    CHECK(mvh_is_power_of_two_u64(4096u));
    CHECK(mvh_next_power_of_two_u64(4097u) == 8192u);
    CHECK(mvh_checked_add_u64(20u, 22u, &value) == 0 && value == 42u);
    CHECK(mvh_checked_mul_u64(6u, 7u, &value) == 0 && value == 42u);
    CHECK(mvh_rotate_left32(1u, 8u) == 0x100u);
    CHECK(mvh_rotate_right32(0x100u, 8u) == 1u);
    CHECK(mvh_rotate_left64(1u, 40u) == ((uint64_t)1u << 40u));
    CHECK(mvh_rotate_right64((uint64_t)1u << 40u, 40u) == 1u);
    CHECK(mvh_popcount32(0xF0F0u) == 8u);
    CHECK(mvh_popcount64(0xF00000000000000Full) == 8u);
    CHECK(mvh_bswap16(0x1234u) == 0x3412u);
    CHECK(mvh_bswap32(0x12345678u) == 0x78563412u);
    CHECK(mvh_bswap64(0x0123456789ABCDEFull) == 0xEFCDAB8967452301ull);
    CHECK(mvh_parse_u64("18446744073709551615", &value) == 0 && value == ~(uint64_t)0);
    CHECK(mvh_parse_u64("18446744073709551616", &value) != 0);
    CHECK(mvh_parse_hex_u64("0xC0FFEE", &value) == 0 && value == 0xC0FFEEu);
    CHECK(mvh_parse_hex_u64("xyz", &value) != 0);

    CHECK(mvh_min_u32(9u, 4u) == 4u);
    CHECK(mvh_max_u32(9u, 4u) == 9u);
    CHECK(mvh_clamp_u32(20u, 3u, 12u) == 12u);
    CHECK(mvh_abs_i64(INT64_MIN) == ((uint64_t)1u << 63u));
    CHECK(mvh_gcd_u64(84u, 30u) == 6u);
    CHECK(mvh_checked_lcm_u64(21u, 6u, &value) == 0 && value == 42u);
    CHECK(mvh_ceil_div_u64(17u, 4u) == 5u);
    CHECK(mvh_round_up_multiple_u64(17u, 10u) == 20u);
    CHECK(mvh_round_down_multiple_u64(17u, 10u) == 10u);
    CHECK(mvh_checked_sub_u64(50u, 8u, &value) == 0 && value == 42u);
    CHECK(mvh_checked_add_i64(20, 22, &signed_value) == 0 && signed_value == 42);
    CHECK(mvh_checked_mul_i64(-6, 7, &signed_value) == 0 && signed_value == -42);
    CHECK(mvh_log2_floor_u64(65u) == 6);
    CHECK(mvh_log2_ceil_u64(65u) == 7);
    CHECK(mvh_count_leading_zero32(1u) == 31u);
    CHECK(mvh_count_trailing_zero32(0x100u) == 8u);
    CHECK(mvh_bit_set64(0u, 63u) == ((uint64_t)1u << 63u));
    CHECK(mvh_bit_clear64(0xFFu, 3u) == 0xF7u);
    CHECK(mvh_bit_test64(0x80u, 7u));
    CHECK(mvh_bit_toggle64(0u, 5u) == 0x20u);
    CHECK(mvh_mask_range64(4u, 4u) == 0xF0u);
    CHECK(mvh_parity32(7u) == 1u);
    CHECK(mvh_parity64(3u) == 0u);
    CHECK(mvh_load_le16(little_endian) == 0x5678u);
    CHECK(mvh_load_le32(little_endian) == 0x12345678u);
    CHECK(mvh_load_le64(little_endian) == 0x90ABCDEF12345678ull);

    CHECK(mvh_mem_copy_checked(copy, sizeof(copy), bytes, sizeof(bytes)) == 0);
    CHECK(mvh_mem_equal(copy, bytes, sizeof(bytes)));
    mvh_mem_fill16(words16, 0x55AAu, 2u);
    CHECK(words16[0] == 0x55AAu && words16[1] == 0x55AAu);
    mvh_mem_fill32(words32, 0x12345678u, 2u);
    CHECK(words32[0] == 0x12345678u && words32[1] == 0x12345678u);
    CHECK(mvh_mem_count_byte("banana", 'a', 6u) == 3u);
    CHECK(mvh_mem_hash_fnv1a64("hello", 5u) == 0xA430D84680AABD0Bull);

    CHECK(mvh_str_index_char("kernel", 'r') == 2);
    CHECK(mvh_str_last_index_char("level", 'e') == 3);
    CHECK(mvh_str_case_equal("Kernel", "kErNeL"));
    CHECK(mvh_str_ncase_equal("Kernel", "KERMIT", 3u));
    CHECK(mvh_str_equal(mvh_str_skip_space("\t kernel"), "kernel"));
    cursor = "  load module  ";
    CHECK(mvh_str_next_token(&cursor, text, sizeof(text)) == 1 && mvh_str_equal(text, "load"));
    CHECK(mvh_parse_i64("-9223372036854775808", &signed_value) == 0 && signed_value == INT64_MIN);
    CHECK(mvh_format_u64(text, sizeof(text), 123456789u) == 9u && mvh_str_equal(text, "123456789"));
    CHECK(mvh_format_hex_u64(text, sizeof(text), 0xC0FFEEu) == 6u && mvh_str_equal(text, "c0ffee"));
    mvh_str_copy(joined, sizeof(joined), "//boot\\kernel///elf");
    CHECK(mvh_str_normalize_slashes(joined) == 16u && mvh_str_equal(joined, "/boot/kernel/elf"));

    mvh_store_le16(endian_buffer, 0x1234u);
    CHECK(endian_buffer[0] == 0x34u && endian_buffer[1] == 0x12u);
    mvh_store_le32(endian_buffer, 0x12345678u);
    CHECK(mvh_load_le32(endian_buffer) == 0x12345678u);
    mvh_store_le64(endian_buffer, 0x0123456789ABCDEFull);
    CHECK(mvh_load_le64(endian_buffer) == 0x0123456789ABCDEFull);
    CHECK(mvh_load_be16(little_endian) == 0x7856u);
    CHECK(mvh_load_be32(little_endian) == 0x78563412u);
    CHECK(mvh_load_be64(little_endian) == 0x78563412EFCDAB90ull);
    mvh_store_be16(endian_buffer, 0x1234u);
    CHECK(endian_buffer[0] == 0x12u && endian_buffer[1] == 0x34u);
    mvh_store_be32(endian_buffer, 0x12345678u);
    CHECK(mvh_load_be32(endian_buffer) == 0x12345678u);
    mvh_store_be64(endian_buffer, 0x0123456789ABCDEFull);
    CHECK(mvh_load_be64(endian_buffer) == 0x0123456789ABCDEFull);

    CHECK(mvh_min_i64(-8, 4) == -8);
    CHECK(mvh_max_i64(-8, 4) == 4);
    CHECK(mvh_clamp_i64(20, -3, 12) == 12);
    CHECK(mvh_saturating_add_u64(UINT64_MAX, 1u) == UINT64_MAX);
    CHECK(mvh_saturating_sub_u64(2u, 5u) == 0u);
    CHECK(mvh_saturating_mul_u64(UINT64_MAX, 2u) == UINT64_MAX);
    CHECK(mvh_div_round_nearest_u64(8u, 3u) == 3u);
    CHECK(mvh_is_even_u64(42u));
    CHECK(mvh_is_odd_u64(43u));
    CHECK(mvh_fibonacci_u64(10u, &value) == 0 && value == 55u);
    CHECK(mvh_factorial_u64(10u, &value) == 0 && value == 3628800u);
    CHECK(mvh_pow_u64(3u, 5u, &value) == 0 && value == 243u);
    CHECK(mvh_integer_sqrt_u64(1000u) == 31u);
    CHECK(mvh_average_u64(UINT64_MAX, UINT64_MAX - 2u) == UINT64_MAX - 1u);
    CHECK(mvh_sign_i64(-42) == -1);

    CHECK(mvh_count_leading_zero64(1u) == 63u);
    CHECK(mvh_count_trailing_zero64((uint64_t)1u << 40u) == 40u);
    CHECK(mvh_reverse_bits32(1u) == 0x80000000u);
    CHECK(mvh_reverse_bits64(1u) == ((uint64_t)1u << 63u));
    CHECK(mvh_extract_bits64(0xABCDu, 4u, 8u) == 0xBCu);
    CHECK(mvh_insert_bits64(0u, 0xABu, 8u, 8u) == 0xAB00u);
    CHECK(mvh_next_set_bit64(0x100u, 0u) == 8);
    CHECK(mvh_previous_set_bit64(0x101u, 7u) == 0);
    CHECK(mvh_sign_extend64(0xFFu, 8u) == -1);
    CHECK(mvh_count_byte64(0xAAAAAAAA0000AAAAull, 0xAAu) == 6u);

    puts("150 kernel utility API tests passed (100 added in 1.1.10)");
    return 0;
}
