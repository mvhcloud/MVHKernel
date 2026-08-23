# MVHFS persistent filesystem

MVHFS is a compact kernel-owned filesystem for writable block devices with 512-byte sectors. It is intended as the persistence layer beneath the current RAMFS root until hardware block drivers and a general mount table are available.

## On-disk layout

| LBA range | Purpose |
| --- | --- |
| 0-1 | Alternating CRC32-protected superblocks |
| 2-5 | Directory snapshot A |
| 6-9 | Directory snapshot B |
| 10-521 | Two eight-sector data copies for each of 32 files |

Every directory entry stores a filename, size, content CRC32 and active data-copy index. Files are root-level, have names up to 31 bytes and contain up to 4096 bytes.

## Commit and recovery

A write stores the complete new file in its inactive data copy, writes the new directory to its inactive snapshot, then commits by writing the inactive superblock with a higher generation. Mount validates both superblocks and their referenced directory CRCs, chooses the newest complete generation and falls back to the previous generation if the latest copy is damaged.

This protects confirmed content against reset or power loss before the final superblock sector is written. File CRC32 detects later media corruption; it is integrity detection, not cryptographic authentication.

## API and shell

The public API is declared in `include/mvh/mvhfs.h`. The shell exposes:

- `pmkfs <device-id>` formats and mounts a device; this destroys its existing contents
- `pmount <device-id>` mounts an existing MVHFS volume
- `pls` lists persistent files
- `pwrite <name> <text>` creates or replaces a file
- `pcat <name>` verifies and prints a file
- `prm <name>` removes a file entry

Use `blockdev` to find registered device IDs. Persistence across a real reboot requires a writable hardware block driver to register the same storage device during the next boot.
