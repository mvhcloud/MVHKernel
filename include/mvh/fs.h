#ifndef MVH_FS_H
#define MVH_FS_H

#include <stdint.h>

#define FS_NAME_MAX 24u
#define FS_DATA_MAX 256u
#define FS_PATH_MAX 128u
#define FS_LIST_MAX 64u

typedef struct {
    char name[FS_NAME_MAX];
    uint8_t is_directory;
    uint16_t size;
} fs_entry_t;

void fs_init(void);
int fs_chdir(const char *path);
int fs_pwd(char *output, uint32_t capacity);
int fs_list(const char *path, fs_entry_t *entries, uint32_t capacity);
int fs_mkdir(const char *path);
int fs_touch(const char *path);
int fs_write(const char *path, const char *text, uint8_t append);
int fs_read(const char *path, const char **data, uint16_t *size);
int fs_remove(const char *path);

#endif
