# MVH Kernel 1.1.9 Daily

MVH Kernel is a standalone x86_64 kernel, not an operating-system distribution. Commands and integration details are maintained at [kernel.mvhcloud.com](https://kernel.mvhcloud.com/).

## Capabilities

- Direct Multiboot/GRUB boot with a 32-bit to 64-bit bootstrap and a bootable ISO image
- BootInfo V2 and legacy loader handoff compatibility
- BIOS discovery and strict validation of ACPI, SMBIOS, MADT, FADT, HPET, MCFG, SRAT and SLIT data
- Local APIC and IOAPIC activation, firmware IRQ overrides and automatic legacy PIC fallback
- INIT-SIPI-SIPI application-processor startup with per-CPU stacks, GS-local state and online diagnostics
- Active HPET monotonic counter with PIT system-tick fallback
- PCIe ECAM configuration access through a remapped MMIO window with CF8/CFC fallback
- Physical-page allocation, dynamic virtual mappings, null-page protection and a guarded kernel heap
- Strong stack protection, NX, SMEP, SMAP, UMIP and supervisor write protection where supported
- ATA PIO and transitional VirtIO writable block storage plus validated MBR and GPT probing
- VFS with RAMFS root and MVHFS persistent storage with dual superblocks, copy-on-write metadata and CRC32 recovery
- Ethernet II, ARP, IPv4, ICMP and UDP packet processing
- Expiring ARP cache and longest-prefix route selection
- VGA text output, bidirectional UART console, PS/2 keyboard, RTC, CPUID and CPU diagnostics
- ChaCha20 entropy pool using supported CPU random sources and runtime timing input
- Interactive shell, per-vector interrupt counters, kernel log, panic diagnostics and combined self-tests

## Build and test

```sh
make clean all
make host-test
make iso
```

The build produces `build/kernel.elf` and `build/mvh-kernel.iso`. The ISO boots directly in QEMU or another Multiboot-compatible x86_64 environment.

```sh
scripts/qemu-smoke.sh build/mvh-kernel.iso pc 128 1
scripts/qemu-persistence.sh build/mvh-kernel.iso
```

Validation covers `pc` and `q35`, 128 MiB through 2 GiB of guest RAM, one through four online virtual CPUs, native protocol/storage tests and ATA/VirtIO cold-reboot MVHFS persistence tests.

For persistent QEMU storage, attach a writable IDE disk. Use `pmkfs`, `pmount`, `pls`, `pwrite`, `pcat` and `prm` in the kernel shell. MVHFS supports 32 files of up to 4096 bytes each and requires at least 522 sectors.

## Common failures

| Area | Cause |
| --- | --- |
| GRUB boot | The Multiboot header is missing, damaged or outside the loader scan range |
| Early 64-bit entry | Long Mode, paging, stack alignment or loader memory reporting is invalid |
| ACPI or SMBIOS | Firmware signature, checksum, length or address validation failed; safe fallbacks remain active |
| APIC or HPET | Required firmware tables or MMIO registers are unavailable; PIC/PIT fallback is used |
| PCIe | MCFG is absent or unsupported; legacy PCI configuration access is used |
| ATA | The device is ATAPI, not LBA-capable, larger than the LBA28 driver range or reports an I/O error |
| GPT | Header CRC, entry-array CRC or LBA layout is invalid |
| MVHFS | Device is read-only, too small, unformatted or both recovery superblocks are damaged |
| SMP | x2APIC IDs above 255 are not started yet; failed APs remain offline and are reported by `smpinfo` |
| Network | Protocol handling is available but no supported NIC driver is attached |
| Entropy | Hardware sources are unavailable and insufficient runtime input has accumulated |

## Planned work

- SMP-aware scheduler queues, preemptive kernel threads and cross-CPU rescheduling
- Ring 3 execution, syscall ABI and userspace ELF loading
- Managed physical memory beyond the Multiboot-mapped first 4 GiB
- AHCI and NVMe block drivers
- e1000, VirtIO-net and other network-device drivers
- xHCI, USB HID and USB mass storage
- FAT32/ext2 mounts and a general persistent VFS root
- Multi-console TTY and complete input-device routing

Repository documentation updates run daily through 27 August 2026 and weekly afterward.

## License

MIT
