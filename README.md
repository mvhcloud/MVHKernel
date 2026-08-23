# MVH Kernel 1.1.7 Daily

MVH Kernel is a standalone x86_64 ELF64 kernel, not an operating-system distribution. Full integration and API documentation is available at [kernel.mvhcloud.com](https://kernel.mvhcloud.com/).

## Capabilities

- BootInfo V2 and legacy loader handoff
- ACPI, SMBIOS, MADT, FADT, HPET, MCFG, SRAT and SLIT validation
- Physical pages, virtual mappings and a guarded one-MiB kernel heap
- GCC strong stack protection, NX, SMEP, SMAP, UMIP and supervisor write protection where supported
- IDT exceptions, legacy PIC/PIT timing and per-vector counters
- VGA, UART, PS/2 keyboard, RTC, CPUID, PCI and CPU-temperature diagnostics
- ChaCha20 entropy pool with RDRAND/RDSEED input
- VFS with volatile RAMFS
- MVHFS persistent file layer with dual superblocks, copy-on-write metadata and CRC32 data verification
- Block registry plus validated MBR/GPT probing
- CRC32 integrity core
- 50 bounded utility APIs for memory, strings, ASCII and integer/bit operations
- Interactive diagnostic shell, `utilinfo` inventory and combined runtime self-tests

The 50 utility APIs are declared in [`include/mvh/util.h`](include/mvh/util.h) and covered by native tests. Full documentation is maintained at [kernel.mvhcloud.com](https://kernel.mvhcloud.com/).

## Build and test

```sh
make clean all
make host-test
```

Output: `build/kernel.elf`. The native suite does not require QEMU.

MVHFS becomes durable when a writable 512-byte block driver is registered. Use `pmkfs`, `pmount`, `pls`, `pwrite`, `pcat` and `prm` from the kernel shell. Formatting requires at least 522 sectors and supports 32 files of up to 4096 bytes each.

## Common failures

| Area | Cause |
| --- | --- |
| Early boot | Long Mode, identity mapping, stack or BootInfo contract is invalid |
| ACPI/SMBIOS | Signature, checksum, length or mapped address validation failed |
| Memory | Reported memory does not cover required kernel mappings |
| GPT | Header, entry CRC or LBA layout is invalid |
| Entropy | Hardware RNG is unavailable and insufficient timing input was collected |
| Files | RAMFS is volatile and is cleared on reboot |

## Hardware work still pending

- Application-processor startup and SMP scheduling
- Ring 3 execution, syscall ABI and userspace ELF loading
- Local APIC, IOAPIC, HPET and PCIe ECAM activation
- Memory management beyond the loader's identity-mapped first GiB
- AHCI, NVMe, VirtIO and USB block drivers; MVHFS is ready but requires one of these hardware backends for real-machine persistence
- Network-device drivers and a networking stack

These items remain documented until complete initialization, rollback and hardware validation exist.

Documentation updates run daily through 27 August 2026 and weekly afterward.

## License

MIT
