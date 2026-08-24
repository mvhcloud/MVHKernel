#include <stdint.h>
#include "mvh/fs.h"
#include "mvh/vfs.h"

void vfs_init(void)
{
    fs_init();
}

const char *vfs_root_type(void)
{
    return "ramfs";
}

int vfs_chdir(const char *path)
{
    return fs_chdir(path);
}

int vfs_pwd(char *output, uint32_t capacity)
{
    return fs_pwd(output, capacity);
}

int vfs_list(const char *path, fs_entry_t *entries, uint32_t capacity)
{
    return fs_list(path, entries, capacity);
}

int vfs_mkdir(const char *path)
{
    return fs_mkdir(path);
}

int vfs_touch(const char *path)
{
    return fs_touch(path);
}

int vfs_write(const char *path, const char *text, uint8_t append)
{
    return fs_write(path, text, append);
}

int vfs_read(const char *path, const char **data, uint16_t *size)
{
    return fs_read(path, data, size);
}

int vfs_remove(const char *path)
{
    return fs_remove(path);
}
