# SMBIOS subsystem

MVH Kernel 1.1.6-2 consumes the SMBIOS entry-point address supplied by BootInfo V2 and builds a bounded, read-only hardware inventory.

## Entry-point validation

- SMBIOS 2.x requires `_SM_`, its full entry checksum, `_DMI_` and the intermediate checksum.
- SMBIOS 3.x requires `_SM3_`, a valid declared entry length and checksum.
- Versions, table address, table size and announced structure count are copied only after validation.
- Entry points and tables must lie wholly within the loader's identity-mapped first GiB.
- Tables larger than 1 MiB are rejected.

## Structure walking

Every structure must have a four-byte minimum formatted header, fit within the table and terminate its string set with a double NUL. SMBIOS 2.x announced counts are checked exactly. SMBIOS 3.x walking requires an end-of-table structure. Broken lengths or unterminated strings reject the complete handoff.

The inventory currently records BIOS vendor/version, system manufacturer/product/version/serial, baseboard manufacturer/product, processor-socket count, memory-device population, installed MiB and maximum reported memory speed. Strings are bounded to 63 printable characters.

## Diagnostics

- `firmwareinfo` summarizes BootInfo, ACPI, SMBIOS and framebuffer availability.
- `smbiosinfo` shows the validated platform inventory.
- `selftest` and `make host-test` cover valid and malformed SMBIOS 2.x/3.x data without accessing hardware.

SMBIOS data is descriptive and is never trusted for memory allocation, address mapping or security decisions.
