# MVH Kernel Release Log

## 1.1.10 Daily

- Added 100 freestanding kernel utility APIs, increasing the validated utility surface from 50 to 150 functions
- Added checked memory copying, memory comparison, word filling, byte counting and binary FNV-1a hashing
- Added case-insensitive strings, token scanning, signed parsing, integer formatting and slash normalization
- Added checked and saturating arithmetic, GCD/LCM, powers, factorials, Fibonacci values and integer square roots
- Added bit-range extraction/insertion, bit scans, sign extension, bit reversal and byte-lane counting
- Added unaligned little-endian and big-endian load/store helpers for driver and protocol code
- Extended native coverage so every public utility API is exercised

## 1.1.9 Daily

- Added a low-memory x86 startup trampoline and INIT-SIPI-SIPI application-processor boot sequence
- Added per-CPU kernel stacks, GS-based CPU-local records, online state and AP idle loops
- Added per-CPU GDT/TSS instances, Ring-3-ready descriptors and separate IST stacks for Double Fault, NMI and Machine Check
- Added Local APIC IPI delivery, per-CPU LAPIC enablement and reusable IDT loading
- Added acknowledged reschedule and TLB-shootdown IPIs plus a controlled CPU-stop IPI
- Added a real cooperative assembly context switch with kernel threads, dedicated stacks, priorities, affinity, lifecycle queues and scheduler statistics
- Added SMP serialization to the physical page allocator and hardened kernel heap
- Added atomic 64-bit operations, semaphores, completions, conditions, seqlocks and explicit memory barriers
- Added coherent DMA32/DMA64 allocation, identity DMA mapping and scatter/gather construction
- Extended ACPI table validation across the loader-mapped 4 GiB window for high-RAM guests
- Extended `smpinfo`, CPU diagnostics and QEMU validation to require every configured vCPU online
- Added a root Mintlify `docs.json` configuration without a documentation subdirectory
- Added a transactional release updater for the local MVH Betriebsystem project
- Replaced repetitive kernel build rules with automatically discovered architecture and subsystem sources
- Promoted the release branch name from `agent/...` to `release/1.1.9`
- Retained the existing desktop without further UI expansion

## 1.1.8 Daily

- Added a Multiboot bootstrap that enters x86_64 Long Mode and a bootable GRUB ISO image
- Added reproducible QEMU boot validation for `pc` and `q35`, 128 MiB to 1 GiB RAM and one to four vCPUs
- Added BIOS-memory ACPI RSDP and SMBIOS discovery when the loader does not supply firmware pointers
- Activated Local APIC and IOAPIC interrupt routing with MADT overrides, spurious-vector handling and safe PIC fallback
- Activated the HPET main counter as a monotonic nanosecond time source while retaining PIT ticks
- Activated PCIe ECAM configuration access through a reusable MMIO mapping window with legacy PCI fallback
- Added an ATA PIO LBA28 driver that registers writable disks with the block layer
- Added bidirectional UART shell input for headless testing and recovery
- Added an automated MVHFS format/write/cold-reboot/remount/read persistence test on a real QEMU disk
- Added Ethernet II, ARP, IPv4, ICMP echo and UDP parsing/building with validated checksums
- Added an expiring ARP cache and longest-prefix/metric route selection
- Added native positive, corruption and boundary tests for the network protocol core
- Updated runtime diagnostics for active IOAPIC, HPET, PCIe ECAM, storage and networking state
- Kept unsupported SMP, Ring 3, high-memory, modern storage, NIC and USB hardware paths explicit

## 1.1.7 Daily

- Added MVHFS, a small persistent filesystem with dual CRC32 superblocks, duplicated directories and copy-on-write file slots
- Added format, mount, create, overwrite, read, list and remove operations for writable 512-byte block devices
- Added `pmkfs`, `pmount`, `pls`, `pwrite`, `pcat` and `prm` persistent shell commands
- Added native reboot-remount, interrupted-commit, superblock-fallback and data-corruption recovery tests
- Added 50 exported, bounded kernel utility APIs for memory, strings, ASCII, integer parsing, alignment, overflow checks, rotations, population counts and byte swapping
- Added native positive, boundary, overlap, truncation and overflow coverage for all 50 utility APIs
- Enabled GCC `-fstack-protector-strong` with a freestanding global guard and panic handler
- Added compiler stack-protector status to `securityinfo`
- Updated the canonical documentation address to `https://kernel.mvhcloud.com/`
- Shortened repository and release-facing documentation and removed transitional GitHub wording
- Added scheduled validation: daily through 27 August 2026 and weekly afterward
- Retained hardware-dependent APIC/SMP, Ring 3, storage, USB and network work as explicit engineering boundaries rather than placeholder claims

## 1.1.6-2

- Added strict SMBIOS 2.x and SMBIOS 3.x entry-point parsing with all required checksums
- Added bounded SMBIOS structure walking with length and double-NUL string validation
- Added BIOS vendor/version, system, baseboard, processor-socket and memory-device inventory
- Added installed-memory and maximum configured memory-speed summaries
- Added native malformed SMBIOS 2.x/3.x entry and structure tests
- Added persistent MADT CPU records for xAPIC and x2APIC processors with enabled/online-capable state
- Added persistent IOAPIC records and IRQ-to-GSI interrupt-source overrides with firmware flags
- Added persistent MCFG segment records with ECAM base, segment group and bus ranges
- Added `firmwareinfo`, `smbiosinfo`, `ioapicinfo`, `mcfginfo` and `smpinfo` commands
- Added `pmmstat`, `timerstat`, `randomstat` and `securityinfo` diagnostic commands
- Expanded host tests to cover SMBIOS alongside ACPI, BootInfo, storage and CRC32
- Preserved the inactive APIC/ECAM boundary: firmware topology is indexed but hardware is not enabled yet

## 1.1.6

- Added complete ACPI 1.0 and 2+ RSDP parsing with signature, legacy checksum and extended checksum validation
- Added preferred XSDT traversal with validated RSDT fallback
- Added centralized SDT headers, bounded length/checksum validation and a 64-entry table registry
- Added signature lookup with duplicate-table support, safe unknown-table retention and malformed-table rejection
- Added MADT parsing for Local APIC, x2APIC, IOAPIC, interrupt overrides, NMIs and LAPIC address overrides
- Added FADT reset-register and PM-timer metadata parsing
- Added HPET MMIO metadata, validated MCFG segments, SRAT affinities and SLIT distance-matrix parsing
- Registered valid DMAR, IVRS, SPCR and TPM2 tables for later consumers
- Changed the BootInfo ACPI pointer from display-only metadata into an initialized kernel subsystem
- Added `acpiinfo`, `acpitables`, `madtinfo` and `hpetinfo` diagnostic commands
- Added checksum, malformed RSDP and malformed SDT self-tests plus native host coverage
- Kept PIC/PIT active and APIC/HPET/ECAM activation disabled until their dedicated implementation is complete

### Scope note

The supplied roadmap spans many major kernel releases. Version 1.1.6 completes the ACPI discovery and metadata layer; APIC, SMP, scheduler, storage drivers, USB, networking, security and multiarch remain tracked follow-up work rather than being represented by unsafe placeholders.

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
