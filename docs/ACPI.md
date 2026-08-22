# ACPI subsystem

MVH Kernel 1.1.6 consumes the ACPI RSDP address supplied through BootInfo V2. The parser is allocation-free and operates during early kernel initialization while the loader's first-GiB identity mapping is still the only guaranteed physical mapping.

## Validation model

- ACPI 1.0 RSDP signature and 20-byte checksum are mandatory.
- ACPI 2+ additionally requires a valid length and extended checksum.
- XSDT is preferred. If its address, signature, length or checksum is invalid, a valid RSDT is tried.
- Every SDT must fit in the identity-mapped range, have a length from 36 bytes through 1 MiB and pass its checksum.
- Root-entry arithmetic is checked, the registry is bounded to 64 tables, duplicate signatures are retained and lookup is instance-based.
- Unknown valid tables remain discoverable. Malformed known or unknown tables are rejected without stopping the legacy PIC/PIT boot path.

## Parsed metadata

MADT parsing records the LAPIC base and flags plus Local APIC, x2APIC, IOAPIC, interrupt-source override and NMI entries. CPU UID/APIC IDs and enabled state, IOAPIC address/GSI bases and complete IRQ override flags are retained through bounded query APIs. FADT parsing records the PM timer and, when present, the reset GAS/value. HPET parsing records its GAS address. MCFG retains aligned ECAM bases, segment groups and ordered bus ranges. SRAT counts CPU and memory affinities. SLIT validates the complete square distance matrix. DMAR, IVRS, SPCR and TPM2 are checksum-validated and registered for future consumers.

The parser does not activate hardware. In 1.1.6, Local APIC, IOAPIC, HPET, PCIe ECAM, IOMMU, NUMA and ACPI shutdown/reboot remain disabled. This separation keeps firmware discovery testable before interrupt routing and MMIO policy change.

## Diagnostics and tests

- `acpiinfo` summarizes the root path, registry and parsed firmware metadata.
- `acpitables` lists every accepted table with address, length and revision.
- `madtinfo` reports interrupt-controller topology.
- `ioapicinfo` reports IOAPIC resources and IRQ-to-GSI overrides.
- `mcfginfo` reports every validated PCIe ECAM segment.
- `smpinfo` reports firmware CPUs while clearly separating the running BSP from inactive APs.
- `hpetinfo` reports the firmware MMIO address and clearly labels HPET as inactive.
- `selftest` validates correct and malformed RSDP/SDT samples.
- `make host-test` runs the same pure parser checks without a VM.

## Current mapping boundary

Tables above the first GiB are rejected because VMM `ioremap` support is not available yet. A later ACPI revision will map firmware tables explicitly and then relax this temporary boundary.
