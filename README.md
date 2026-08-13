# MVH Kernel 1.1.2

An independent ELF64 kernel with VGA text graphics, serial output, CPU detection and English US keyboard input. The kernel starts at its entry point in x86_64 Long Mode.

## Bauen

```sh
make
```

The 64-bit kernel is written to `build/kernel.elf`. GNU Make, GCC with x86_64 support and GNU ld are required. Bootloaders, OS integration, emulators and local tests are not part of this kernel project.

Version 1.1.2 adds 4 KiB page mapping, kernel page protections, a null-page guard, CPU security initialization, extended CPU/cache/SIMD diagnostics, hardware RNG access, interrupt statistics, larger structured logs, heap canaries, free poisoning and allocator statistics.

CPU temperature supports Intel Digital Thermal Sensor MSRs, AMD Family 10h-16h northbridge Tctl and AMD Family 17h-1Ah SMN Tctl. Readings are range-validated. QEMU normally exposes no usable thermal sensor, so the shell reports the value as unavailable instead of inventing a temperature.

Exception crashes display a stable panic code, decoded hardware error-code flags, control registers, general registers and a bounded frame-pointer stack trace on VGA and serial output.

Available commands include `help`, `about`, `statics`, `ls`, `dir`, `cd`, `pwd`, `mkdir`, `touch`, `write`, `append`, `cat`, `type`, `open`, `rm`, `rmdir`, `mount`, `df`, `echo`, `date`, `uptime`, `ticks`, `sleep`, `meminfo`, `free`, `heaptest`, `pagetest`, `synctest`, `selftest`, `dmesg`, `faulttest`, `ps`, `devices`, `lspci`, `drivers`, `features`, `uname`, `version`, `hostname`, `whoami`, `language`, `clear`, `cls` and `reboot`.

`faulttest` validates an exception without a CPU error code. `faulttest page` validates page-fault error handling and CR2 reporting. Both commands intentionally halt the kernel.

The built-in RAM filesystem is volatile and is reset on every boot.

The x86_64 interrupt layer uses an IDT, remapped 8259 PIC and an Intel 8254-compatible PIT timer at 100 Hz. Unused hardware interrupt lines remain masked.

The current release runs on the boot processor in kernel mode with the legacy PIC. APIC, additional CPU startup, Ring 3 processes, persistent disk filesystems, networking, USB, NVMe, ACPI and a user ELF loader remain separate future milestones. Compiler stack protection is not claimed because the current freestanding Zig target rejects that instrumentation without libc support.

Lizenz: MIT
