# MVH Kernel BootInfo V2

BootInfo V2 is the versioned loader-to-kernel contract introduced in MVH Kernel 1.1.5. Its declarations live in `include/mvh/bootinfo.h`.

## Entry convention

The loader enters `_kernel64_start` in x86_64 Long Mode with interrupts disabled and the first GiB identity mapped.

The loader must provide a writable stack and place BootInfo plus all referenced input ranges outside the kernel image. The entry stub aligns the stack down to a 16-byte boundary before calling C.

- `RDI` contains the physical, identity-mapped address of an 8-byte-aligned `mvh_bootinfo_v2_t`.
- `RSI` contains `MVH_BOOTINFO_HANDOFF_MAGIC` (`0x32564F464E494856`).
- The structure's `magic` field contains `MVH_BOOTINFO_MAGIC` (`0x324F464E4948564D`).
- `version` is `2`, `size` is at least `sizeof(mvh_bootinfo_v2_t)`, and all reserved fields are zero.

Both arguments are preserved as full 64-bit values by the entry stub.

For compatibility with loaders for versions 1.1 through 1.1.4, any other `RSI` value selects the legacy convention: `RDI` is interpreted as available memory in KiB. Values below 4096 KiB are rejected.

## Safety and lifetime

The kernel copies scalar metadata before initializing its physical allocator. The BootInfo structure and every flagged input range must be wholly inside the identity-mapped first GiB and must remain valid throughout kernel initialization. Unknown flags, inconsistent flag/data pairs, oversized arrays, overflowed ranges and non-zero reserved fields cause an early panic.

The kernel currently manages at most the first GiB even if `memory_kib` describes more memory. Supplying a firmware memory map does not yet remove that allocator limit.

## Flags

| Flag | Required fields | Rules |
| --- | --- | --- |
| `MEMORY_MAP` | address, entry count, entry size | At least one entry; stride at least `sizeof(mvh_memory_map_entry_t)` |
| `ACPI_RSDP` | ACPI RSDP address | At least the 20-byte ACPI 1.0 prefix must be mapped |
| `SMBIOS` | SMBIOS entry-point address | At least 16 bytes must be mapped |
| `FRAMEBUFFER` | framebuffer structure | Non-zero dimensions/pitch; 24 or 32 bits per pixel |
| `RANDOM_SEED` | seed address and size | 1 to 256 mapped bytes |
| `COMMAND_LINE` | address and size | 1 to 4096 mapped bytes; reserved for kernel flags |

When a flag is clear, its address must be zero. Memory-map counts and sizes, random-seed sizes and command-line sizes must also be zero. The kernel never guesses whether an unflagged pointer is valid.

## Memory-map entries

Each entry begins with `base`, `length`, `type` and `attributes`. Version 2 defines usable, reserved, ACPI reclaimable, ACPI NVS, bad-memory and MMIO types. Loaders may use a larger stride for future fields, but the first fields must retain this layout.

## Diagnostics

The kernel command `bootinfo` reports the selected contract and every supplied metadata class. `selftest` runs malformed-header checks without requiring an emulator.
