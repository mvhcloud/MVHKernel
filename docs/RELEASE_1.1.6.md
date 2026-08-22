# MVH Kernel 1.1.6

Version 1.1.6 turns the BootInfo V2 ACPI handoff into a real, defensive firmware-discovery subsystem.

## New functionality

- RSDP v1 and v2+ parsing with both checksum paths
- Preferred XSDT and validated RSDT fallback
- Central SDT ABI, length/overflow checks and checksums
- Bounded table registry, instance lookup and duplicate signatures
- Safe handling of unknown and malformed tables
- MADT interrupt-topology metadata
- FADT PM timer and reset-register metadata
- HPET address metadata
- Validated MCFG PCIe segments
- SRAT CPU/memory affinity counts and SLIT matrix validation
- Registered DMAR, IVRS, SPCR and TPM2 tables
- Boot-time ACPI initialization from BootInfo V2
- `acpiinfo`, `acpitables`, `madtinfo` and `hpetinfo`
- Kernel self-tests and native malformed/checksum host tests

## Deliberate safety boundary

This release parses firmware but does not activate Local APIC, IOAPIC, HPET, PCIe ECAM, IOMMU or NUMA. PIC and PIT remain the operational fallback. The wider submitted roadmap is retained as follow-up work; version 1.1.6 does not claim placeholder implementations for SMP, scheduling, storage, USB, networking, security or multiarch.

## Verification

The release is checked with a freestanding GCC build, ELF64 validation and native host tests. No QEMU or physical-hardware boot is required for this parser-focused release.

## Distribution transition

We apologize that version 1.1.6 is still distributed through GitHub. The upcoming MVH website is intended to become the home for future kernel downloads and updates.
