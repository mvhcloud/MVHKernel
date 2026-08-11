# MVH Kernel Release Log

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
