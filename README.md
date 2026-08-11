# MVH Kernel

An independent ELF64 kernel with VGA text graphics, serial output, CPU detection and English US keyboard input. The kernel starts at its entry point in x86_64 Long Mode.

## Bauen

```sh
make
```

The 64-bit kernel is written to `build/kernel.elf`. GNU Make, GCC with x86_64 support and GNU ld are required. Bootloaders, OS integration, emulators and local tests are not part of this kernel project.

Available commands: `help`, `about`, `ram`, `cpu`, `clear`, `reboot`.

Lizenz: MIT
