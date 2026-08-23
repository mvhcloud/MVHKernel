# MVH Kernel 1.1.7 Daily

Version 1.1.7 adds a reusable utility layer and enables compiler stack protection while keeping the kernel-only architecture unchanged.

## Persistent MVHFS layer

MVHFS provides a compact persistent store for writable 512-byte block devices. It uses alternating superblocks, duplicated directory tables and two copy-on-write data slots per file. CRC32 protects superblocks, directory snapshots and file contents. An interrupted metadata commit falls back to the last valid generation on the next mount.

The initial format supports 32 root-level files of up to 4096 bytes each and requires 522 sectors. Kernel shell commands are `pmkfs`, `pmount`, `pls`, `pwrite`, `pcat` and `prm`. Real-machine persistence still depends on a registered AHCI, NVMe, VirtIO or other writable block driver.

## 50 utility APIs

### Memory (5)

`mvh_mem_zero`, `mvh_mem_set`, `mvh_mem_copy`, `mvh_mem_move`, `mvh_mem_compare`

### Strings (13)

`mvh_str_length`, `mvh_str_nlength`, `mvh_str_equal`, `mvh_str_nequal`, `mvh_str_compare`, `mvh_str_ncompare`, `mvh_str_copy`, `mvh_str_ncopy`, `mvh_str_concat`, `mvh_str_find_char`, `mvh_str_rfind_char`, `mvh_str_starts_with`, `mvh_str_ends_with`

### ASCII (12)

`mvh_ascii_is_upper`, `mvh_ascii_is_lower`, `mvh_ascii_is_alpha`, `mvh_ascii_is_digit`, `mvh_ascii_is_alnum`, `mvh_ascii_is_space`, `mvh_ascii_is_hex`, `mvh_ascii_is_printable`, `mvh_ascii_to_upper`, `mvh_ascii_to_lower`, `mvh_ascii_digit_value`, `mvh_ascii_hex_value`

### Integer and bit operations (20)

`mvh_min_u64`, `mvh_max_u64`, `mvh_clamp_u64`, `mvh_align_up_u64`, `mvh_align_down_u64`, `mvh_is_power_of_two_u64`, `mvh_next_power_of_two_u64`, `mvh_checked_add_u64`, `mvh_checked_mul_u64`, `mvh_rotate_left32`, `mvh_rotate_right32`, `mvh_rotate_left64`, `mvh_rotate_right64`, `mvh_popcount32`, `mvh_popcount64`, `mvh_bswap16`, `mvh_bswap32`, `mvh_bswap64`, `mvh_parse_u64`, `mvh_parse_hex_u64`

## Stack protection

Kernel C modules now compile with GCC strong stack protection and a freestanding global guard. Guard corruption enters the normal kernel panic path.

## Validation

- Strict freestanding x86_64 build with warnings as errors
- Native utility, firmware, BootInfo, CRC32, storage and MVHFS recovery tests
- ELF64/x86-64 validation
- CodeQL analysis

No QEMU result is claimed for this release.
