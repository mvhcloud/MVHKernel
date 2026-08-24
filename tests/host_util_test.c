#include <stdint.h>
#include <stdio.h>
#include "mvh/util.h"

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

int main(void)
{
    uint8_t bytes[12];
    uint8_t copy[12];
    char text[12];
    const char level[] = "level";
    uint64_t value;

    CHECK(mvh_mem_zero(bytes, sizeof(bytes)) == bytes);
    CHECK(mvh_mem_set(bytes, 0x5Au, sizeof(bytes)) == bytes && bytes[11] == 0x5Au);
    CHECK(mvh_mem_copy(copy, bytes, sizeof(bytes)) == copy);
    CHECK(mvh_mem_move(copy + 2, copy, 8u) == copy + 2 && copy[9] == 0x5Au);
    CHECK(mvh_mem_compare(bytes, copy, 2u) == 0);

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

    CHECK(mvh_ascii_is_upper('A'));
    CHECK(mvh_ascii_is_lower('z'));
    CHECK(mvh_ascii_is_alpha('K'));
    CHECK(mvh_ascii_is_digit('7'));
    CHECK(mvh_ascii_is_alnum('1'));
    CHECK(mvh_ascii_is_space('\n'));
    CHECK(mvh_ascii_is_hex('f'));
    CHECK(mvh_ascii_is_printable('~'));
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

    puts("50 kernel utility API tests passed");
    return 0;
}
