# MVH Kernel 1.1

An independent ELF64 kernel with VGA text graphics, serial output, CPU detection and English US keyboard input. The kernel starts at its entry point in x86_64 Long Mode.

## Bauen

```sh
make
```

The 64-bit kernel is written to `build/kernel.elf`. GNU Make, GCC with x86_64 support and GNU ld are required. Bootloaders, OS integration, emulators and local tests are not part of this kernel project.

Version 1.1 adds a HAL boundary, a physical page allocator, a coalescing kernel heap, a VFS boundary and a kernel task registry.

Available commands include `help`, `about`, `statics`, `ls`, `dir`, `cd`, `pwd`, `mkdir`, `touch`, `write`, `append`, `cat`, `type`, `open`, `rm`, `rmdir`, `mount`, `df`, `echo`, `date`, `uptime`, `ticks`, `sleep`, `meminfo`, `free`, `heaptest`, `pagetest`, `ps`, `devices`, `lspci`, `drivers`, `features`, `uname`, `version`, `hostname`, `whoami`, `language`, `clear`, `cls` and `reboot`.

The built-in RAM filesystem is volatile and is reset on every boot.

The x86_64 interrupt layer uses an IDT, remapped 8259 PIC and an Intel 8254-compatible PIT timer at 100 Hz. Unused hardware interrupt lines remain masked.

The current release runs on the boot processor in kernel mode. Additional CPU startup, Ring 3 processes, persistent disk filesystems, networking, USB, NVMe and a user ELF loader remain separate future milestones.

Lizenz: MIT
