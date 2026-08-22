# MVH Kernel 1.1.5

MVH Kernel is a standalone x86_64 ELF64 kernel. The repository contains kernel code only and does not include a bootloader, installer, userspace or operating-system distribution.

## Kernel capabilities

### Architecture and boot

- Starts at `_kernel64_start` in x86_64 Long Mode
- Uses a freestanding C11 environment with no libc dependency
- Runs on the boot processor in Ring 0
- Requires the first GiB of physical memory to be identity mapped by the loader
- Requires a valid writable stack inside the mapped address range
- Aligns the loader-provided stack to the x86_64 ABI before entering C
- Accepts a validated BootInfo V2 structure from the loader
- Receives memory-map, ACPI RSDP, SMBIOS, framebuffer, random-seed and command-line metadata through capability flags
- Retains the legacy available-memory-size argument as a compatibility fallback
- Reports kernel ABI 2, boot ABI 2 and a stable build ID in logs and panic screens
- Keeps bootloader and distribution integration outside the kernel repository

### Interrupts and time

- x86_64 Interrupt Descriptor Table
- Exception handlers for CPU vectors 0 through 31
- Register-aware exception frames and decoded hardware error codes
- Remapped 8259 PIC with unused IRQ lines masked
- Intel 8254-compatible PIT running at 100 Hz
- Monotonic ticks, uptime reporting and millisecond sleeps
- Per-vector interrupt counters

### Memory management

- Physical 4 KiB page allocator for up to 1 GiB of reported memory
- Dynamic virtual page mapping, unmapping and lookup
- Null-page protection
- Read-only executable kernel text
- Non-executable kernel data and heap pages when NX is available
- Supervisor write protection and capability-gated NX, SMEP, SMAP and UMIP
- One MiB coalescing kernel heap
- Overflow-checked `kcalloc` and data-preserving `krealloc`
- Heap guard pages, allocation canaries and freed-memory poisoning
- Allocation, failure, peak-use and fragmentation statistics

### Core services

- Timestamped circular kernel log
- Stable panic codes and assertion failures with source location
- Bounded frame-pointer stack traces
- Atomic 32-bit operations
- Spinlocks, mutexes, fair ticket locks and reader/writer locks
- Kernel task registry with PID, priority and state metadata
- Typed device registry
- Combined runtime self-test for memory, paging, heap, synchronization, entropy, block devices, VFS, devices and timer progress
- Central build-time feature policy in `mvh/config.h`
- BootInfo V2 validator with a native kernel self-test
- Reusable CRC32 core with a standard-vector self-test

### Hardware support

- VGA text output, colors, scrolling and hardware cursor control
- 16550 UART serial output on COM1
- PS/2 keyboard input with English US layout, Shift, Caps Lock and Ctrl handling
- CPUID vendor, model, family, cache, SIMD and topology diagnostics
- FPU, SSE, XSAVE and available AVX state initialization
- RDRAND/RDSEED-backed entropy input when exposed by the processor
- ChaCha20-based entropy generator with an explicit 256-bit readiness threshold
- PCI configuration-space discovery with class, IRQ, header and BAR metadata
- CMOS real-time clock
- Intel Digital Thermal Sensor support
- AMD Family 10h-16h northbridge Tctl support
- AMD Family 17h-1Ah SMN Tctl support

### Filesystems and storage foundations

- Virtual filesystem boundary with a volatile RAMFS root
- Directories, relative and absolute paths, text files and file removal
- Block-device registry with bounds-checked read and write dispatch
- Read-only device enforcement
- MBR and GPT partition-table detection
- GPT header CRC32, entry-array CRC32, LBA-range and entry-layout validation
- Populated partition count, usable-LBA range and protective-MBR reporting

The block layer is an interface foundation. No AHCI, NVMe, VirtIO Block or persistent filesystem driver is included yet.

### Kernel shell and diagnostics

The built-in shell is intended for kernel diagnostics and development. Important commands include:

- System: `about`, `statics`, `date`, `uptime`, `ticks`, `sleep`, `meminfo`, `free`
- CPU and buses: `cpuinfo`, `features`, `lspci`, `irqstat`, `devices`, `drivers`
- Memory: `heapinfo`, `pagetable`, `heaptest`, `pagetest`
- Kernel: `dmesg`, `ps`, `random`, `crc32`, `blockdev`, `bootinfo`, `selftest`, `synctest`, `paniccodes`
- RAMFS: `ls`, `cd`, `pwd`, `mkdir`, `touch`, `write`, `append`, `cat`, `rm`, `mount`, `df`
- Fault injection: `faulttest`, `faulttest page`

`faulttest` and `faulttest page` deliberately stop the kernel after validating the exception path.

## Build output

```sh
make clean all
```

Requirements:

- GNU Make
- GCC with x86_64 support
- GNU ld

The result is written to `build/kernel.elf`.

Pure CRC32 and storage-layer checks run natively without an emulator:

```sh
make host-test
```

## Known failure conditions

| Area | Symptom | Likely cause |
| --- | --- | --- |
| Boot | No output or an immediate reset | The loader did not enter x86_64 Long Mode, identity-map the first GiB or call `_kernel64_start` with BootInfo V2 or the legacy memory argument |
| BootInfo | Panic reports `invalid boot handoff data` | The BootInfo V2 marker, magic, version, size, flags, pointer ranges or reserved fields violate the documented contract |
| Memory initialization | Panic during PMM, VMM or heap initialization | Reported memory is smaller than the kernel image and reserved regions, or the loader mapping does not cover allocated pages |
| Page fault during startup | Panic code with CR2 and page-fault flags | A kernel section or heap page is outside the identity-mapped range, or page permissions conflict with the accessed address |
| Temperature | `unavailable` is reported | The CPU model is unsupported, required MSRs are unavailable, or the emulator exposes no thermal sensor |
| Random generator | Entropy pool remains below 256 bits | RDRAND/RDSEED is unavailable or unsuccessful and too few keyboard timing events have been collected |
| PCI scan | Device is listed without usable BARs or IRQ | Firmware or the virtual machine did not configure resources, or the device uses a capability not handled by the legacy configuration-space layer |
| Block devices | `blockdev` reports no devices | The registry exists, but no AHCI, NVMe, VirtIO or other block driver has registered hardware |
| GPT | Partition metadata is reported as unavailable | The protective MBR, GPT signature, header CRC32, entry-array CRC32 or LBA bounds are invalid |
| Files | Files disappear after reboot | RAMFS is intentionally volatile and has no persistent storage backend |
| CPU compatibility | Reserved-bit page fault after paging setup | NX is unavailable; the current protected mappings use the NX page-table bit for non-code memory |
| Stack trace | Trace stops early or contains only a few frames | The frame-pointer chain is invalid, corrupted or outside the accepted identity-mapped address range |
| Interrupts | Timer or keyboard stops progressing | The legacy PIC/PIT path is masked, misconfigured or incompatible with the current platform |
| Multicore systems | Additional CPUs stay detected but inactive | Application-processor startup and SMP scheduling are not implemented |

Panics are written to both VGA and serial output when those devices are available. Serial output should be captured first when debugging an early boot failure.

## Current limitations

- Single running boot processor
- Kernel mode only; no Ring 3 execution
- Legacy PIC and PIT remain active
- Maximum managed physical memory is 1 GiB
- Volatile RAMFS only
- No userspace ELF loader or syscall ABI
- ACPI RSDP handoff is available, but no ACPI table parser is implemented yet
- No IOAPIC, Local APIC activation or SMP startup
- No AHCI, NVMe, VirtIO, USB or persistent filesystem driver
- No networking stack
- No compiler-provided stack protector in the current freestanding build

## Planned kernel work

1. Parse and checksum-validate ACPI tables received through BootInfo V2.
2. Introduce Local APIC, IOAPIC and HPET support while retaining a safe legacy fallback.
3. Consume the complete firmware memory map and remove the current one-GiB allocator limit.
4. Start additional processors and make allocators, logs and registries SMP-safe.
5. Add context switching, a preemptive scheduler and per-task kernel stacks.
6. Implement VirtIO Block first, followed by AHCI and NVMe drivers.
7. Add a persistent filesystem layer with cache and writeback rules.
8. Add USB host-controller and input support.
9. Add network-device drivers and an initial IPv4 stack.

## Release-channel transition

We apologize that this transitional release is still provided through GitHub. A new website is being prepared as the future home for MVH Kernel downloads and updates. Version 1.1.5 remains here until that release channel is ready.

Loader authors should read [`docs/BOOTINFO_V2.md`](docs/BOOTINFO_V2.md). All changes and compatibility notes for this release are collected in [`docs/RELEASE_1.1.5.md`](docs/RELEASE_1.1.5.md).

## License

MIT
