#ifndef MVH_VFS_H
#define MVH_VFS_H

#include <stdint.h>
#include "mvh/fs.h"

void vfs_init(void);
const char *vfs_root_type(void);
int vfs_chdir(const char *path);
int vfs_pwd(char *output, uint32_t capacity);
int vfs_list(const char *path, fs_entry_t *entries, uint32_t capacity);
int vfs_mkdir(const char *path);
int vfs_touch(const char *path);
int vfs_write(const char *path, const char *text, uint8_t append);
int vfs_read(const char *path, const char **data, uint16_t *size);
int vfs_remove(const char *path);

#endif
