# MVH Kernel Release Log

## 1.1.5

- Added a strictly validated, versioned BootInfo V2 handoff for memory maps, ACPI RSDP, SMBIOS, framebuffer metadata, random seeds and kernel command lines
- Kept the legacy memory-size calling convention as an explicit compatibility fallback
- Added the `bootinfo` diagnostic command and BootInfo validator self-test
- Extended native host tests with BootInfo validation and legacy-handoff coverage
- Added a central kernel feature-configuration header for compiled subsystem policy
- Versioned the kernel and boot ABI at revision 2 and added a stable release build ID
- Added kernel version, ABI and build identity to boot logs and every panic path
- Hardened the 64-bit entry path by aligning the loader stack before entering C
- Added complete loader-facing BootInfo V2 and 1.1.5 release documentation
- Removed the downloaded GitHub Actions workflow and GitHub-specific build wording from the source package

### Transition notice

We apologize that this transitional release is still distributed through GitHub. A new website is being prepared as the future home for MVH Kernel downloads and updates. Version 1.1.5 remains available here until that release channel is ready.

## 1.1.4

- Added a reusable CRC32 core with standard-vector and incremental-update self-tests
- Added strict GPT header CRC32 and partition-entry-array CRC32 validation
- Added GPT revision, header-size, reserved-field, LBA-range and entry-layout checks
- Added populated GPT partition counting and usable-LBA reporting
- Added overflow-checked `kcalloc` and data-preserving `krealloc` heap APIs
- Fixed zero-length `kmalloc` behavior and preserved exact requested allocation sizes
- Extended kernel self-tests for CRC32, zeroed allocation, reallocation and overflow rejection
- Added the `crc32 <text>` diagnostic command and verified GPT details in `blockdev`
- Added native host-side storage and CRC32 tests that run without QEMU

## 1.1.3

- Completed kernel assertion handling with stable panic code and source location
- Added a locked ChaCha20-based entropy pool with hardware RNG seeding and explicit readiness reporting
- Added fair ticket locks and reader/writer locks with self-test coverage
- Added a validated block-device registry plus bounded MBR and GPT partition probing
- Added `random` and `blockdev` diagnostic commands and extended the combined kernel self-test
- Added bounded PCI enumeration with interrupt, header and BAR resource metadata
- Fixed capacity handling in PCI discovery and cleanup after block-layer self-test failures
- Hardened device and block registries against invalid inputs and inconsistent reads
- Centralized compiled release metadata in `mvh/version.h`
- Removed OS product branding from the standalone kernel interface
- Added push, pull-request, manual and daily ELF64 build artifacts through GitHub Actions

## 1.1.2

- Added dynamic 4 KiB page mapping, unmapping and lookup APIs
- Added read-only executable kernel text, non-executable data and a null-page guard
- Enabled supervisor write protection and available NX, SMEP, SMAP and UMIP protections
- Added FPU, SSE and XSAVE initialization with AVX state support when available
- Added extended CPU family, model, APIC, cache, SIMD, TSC and RNG diagnostics
- Added capability-guarded MSR primitives and explicit unavailable reporting for unsupported temperature and microcode sources
- Added PMM request, failure and peak-use statistics
- Added heap canaries, corruption panic, free poisoning, invalid-free tracking and fragmentation statistics
- Added exception register/control-register dumps and per-vector interrupt counters
- Increased the structured timestamped kernel log ring to 16 KiB
- Added `cpuinfo`, `heapinfo`, `irqstat` and `pagetable` shell commands
- Added Intel DTS and AMD northbridge/SMN temperature backends with range validation
- Added stable panic codes, decoded page-fault and selector-error flags and frame-pointer stack traces
- Added the `paniccodes` shell command
- Kept ACPI, HPET, IOAPIC and Local APIC activation disabled until verified firmware-table handoff is available

## 1.1.1

- CPU exception gates for vectors 0 through 31
- Register-aware kernel panic output with exception name, error code, RIP and RFLAGS
- Circular kernel log with the `dmesg` command
- Atomic 32-bit operations, spinlocks and mutex foundations
- Registry-based device manager with typed online state
- Heap structure validation and allocation counting
- Combined kernel `selftest` for memory, heap, locks, VFS, devices and timer
- Deliberate `faulttest` command for exception-path validation
- ABI and release compatibility metadata in the manifest
- Version correction after the earlier 1.1 foundation upload

## 1.1

- Hardware abstraction layer for platform initialization, input, timer, RTC, PCI and reboot
- x86_64 IDT and remapped 8259 PIC interrupt foundation
- Intel 8254-compatible PIT system timer at 100 Hz
- Physical 4 KiB page allocator limited to mapped and reported memory
- One MiB coalescing kernel heap with `kmalloc` and `kfree`
- Virtual filesystem boundary with RAMFS mounted as the root filesystem
- Kernel task registry with PID, priority and execution state metadata
- Accurate CPU execution-state display without simulated usage bars
- Detailed `features` command for CPUID capabilities and enabled kernel support
- Commands for memory, heap validation, tasks and mounted filesystems
- Shell aliases: `dir`, `type`, `rmdir` and `cls`

## 1.0

- x86_64 ELF64 kernel entry
- VGA text graphics driver with colors and scrolling
- 16550 UART serial driver
- PS/2 keyboard driver with English US layout, Shift and Caps Lock
- x86 CPUID driver with vendor, model and feature detection
- Memory usage display
- Interactive shell and reboot sequence
- Runtime language switching for English, German, Spanish and French
- English default language at startup
- Full-screen `statics` system monitor with hidden cursor
- Hardware text cursor synchronized with the shell prompt
- `statics` exits with Q or Ctrl+C
- Compact `mvh>` shell prompt
- Per-core CPU status bars in `statics`
- Language selection removed from the startup screen
- Staged hardware reboot display with progress dots
- Volatile RAM filesystem with directories and text files
- Relative and absolute path navigation with `cd` and `pwd`
- File commands: `ls`, `mkdir`, `touch`, `write`, `append`, `cat`, `open`, `rm`
- CMOS real-time clock driver and `date` command
- PCI configuration driver and `lspci` command
- Driver, version, hostname, user and echo commands
- RAM and CPU panels removed from the startup screen
- Standalone kernel manifest and MIT license
