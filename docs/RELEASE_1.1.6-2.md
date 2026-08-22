# MVH Kernel 1.1.6-2

The second 1.1.6 release expands the defensive firmware layer into a useful platform inventory and adds a much richer diagnostic surface.

## Firmware additions

- SMBIOS 2.x and 3.x entry validation
- Legacy, intermediate and 64-bit entry checksums
- Bounded structure and string-set walking
- BIOS, system and baseboard identity
- Processor-socket counts
- Memory-device population, installed capacity and speed summaries
- Indexed MADT xAPIC/x2APIC CPU topology
- Indexed IOAPIC address and GSI-base resources
- Indexed IRQ-to-GSI overrides and polarity/trigger flags
- Indexed PCIe MCFG segment groups, ECAM bases and bus ranges

## New diagnostics

- `firmwareinfo`
- `smbiosinfo`
- `ioapicinfo`
- `mcfginfo`
- `smpinfo`
- `pmmstat`
- `timerstat`
- `randomstat`
- `securityinfo`

Each command reports real initialized state and labels inactive future hardware paths. No simulated AP startup, HPET clock, PCIe ECAM access or security mitigation is shown as enabled.

## Verification

The release receives a strict freestanding GCC build, native ACPI/SMBIOS/BootInfo/storage tests and ELF64 validation. QEMU and physical-hardware boot testing are intentionally outside this maintenance release.

## Distribution transition

We apologize that MVH Kernel 1.1.6-2 remains distributed through GitHub. The upcoming MVH website is intended to become the primary download and update channel.
