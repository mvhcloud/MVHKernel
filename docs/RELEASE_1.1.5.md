# MVH Kernel 1.1.5

Version 1.1.5 establishes a safe firmware-handoff boundary without pretending that the later ACPI, APIC or SMP phases are already complete.

## New functionality

- Versioned BootInfo V2 with separate handoff and structure magic values
- Strict validation of structure version, size, flags, reserved fields, counts, strides and identity-mapped pointer ranges
- Typed firmware memory-map entries
- Optional ACPI RSDP, SMBIOS, linear-framebuffer, random-seed and kernel-command-line metadata
- Explicit compatibility fallback for the 1.1 through 1.1.4 memory-size entry convention
- `bootinfo` shell diagnostics
- BootInfo malformed-input self-test integrated into `selftest`
- Native host coverage for BootInfo validation and the legacy handoff
- Central compile-time feature policy in `include/mvh/config.h`
- Kernel ABI 2, boot ABI 2 and stable build ID `mvh-1.1.5`
- Version, ABI and build identity in boot logs and panic output

## Compatibility

Existing loaders can continue passing memory KiB in `RDI` as long as `RSI` is not the BootInfo V2 handoff marker. New loaders should implement the contract in `BOOTINFO_V2.md`. BootInfo metadata is accepted and exposed diagnostically; ACPI parsing, framebuffer rendering and whole-map physical allocation remain future work.

## Verification policy

This release was prepared without QEMU or hardware boot testing, as requested. Source/manifest consistency, JSON parsing and available non-emulator build or host checks are used instead. Consumers should treat the new loader path as not yet boot-verified.

## Distribution transition

We apologize that version 1.1.5 is still distributed through GitHub. A new website is coming and is intended to become the home of kernel downloads and updates. This GitHub release is the temporary source until that channel is available.
