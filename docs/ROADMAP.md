# MVH Kernel implementation roadmap

The long-form roadmap supplied for 1.1.6 covers multiple independent kernel generations. Work is ordered by dependency and is considered complete only when implemented, documented and exercised by an appropriate self-test or host test.

## Completed foundation

- BootInfo V2 and legacy handoff compatibility
- ACPI RSDP/XSDT/RSDT discovery and table registry
- MADT/FADT/HPET/MCFG/SRAT/SLIT metadata parsing
- Existing paging hardening, heap checks, synchronization, entropy, block/GPT and RAMFS foundations

## Next P0 sequence

1. Local APIC, IOAPIC, IRQ/GSI routing and legacy fallback policy
2. HPET/TSC clocksource plus LAPIC clockevent
3. Complete BootInfo memory-map PMM and RAM above 1 GiB
4. SMP startup, per-CPU data, TSS and IPIs
5. Context switching, kernel threads and preemptive scheduling

## Subsequent dependency groups

- Workqueues, VMM/heap v2 and PCIe ECAM
- Generic device, DMA/IOMMU and block-request models
- VirtIO Block, AHCI, NVMe, persistent VFS, page cache and FAT32
- USB/xHCI/HID/mass storage
- Network-device layer, VirtIO-Net/e1000, Ethernet, ARP, IPv4/IPv6, UDP and TCP
- Crypto/RNG v2, hardening, modules, debugger, tracing and profiling
- SMBIOS/power, NUMA, hotplug/error recovery and optional multiarch ports

Every hardware-enabling phase requires rollback-safe initialization and a working legacy path. Features are not marked complete merely because metadata structures or command names exist.
