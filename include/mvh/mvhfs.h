#ifndef MVH_MVHFS_H
#define MVH_MVHFS_H

#include <stdint.h>

#define MVHFS_MAX_FILES 32u
#define MVHFS_NAME_MAX 32u
#define MVHFS_FILE_MAX 4096u
#define MVHFS_DIRECTORY_BYTES 2048u
#define MVHFS_MIN_SECTORS 522u

typedef struct {
    char name[MVHFS_NAME_MAX];
    uint32_t size;
} mvhfs_entry_t;

typedef struct {
    uint32_t device_id;
    uint32_t superblock_slot;
    uint32_t directory_slot;
    uint64_t generation;
    uint8_t mounted;
    uint8_t directory[MVHFS_DIRECTORY_BYTES];
} mvhfs_t;

int mvhfs_format(uint32_t device_id);
int mvhfs_mount(mvhfs_t *filesystem, uint32_t device_id);
int mvhfs_create(mvhfs_t *filesystem, const char *name);
int mvhfs_write(mvhfs_t *filesystem, const char *name,
                const void *data, uint32_t size);
int mvhfs_read(mvhfs_t *filesystem, const char *name, void *buffer,
               uint32_t capacity, uint32_t *size);
int mvhfs_remove(mvhfs_t *filesystem, const char *name);
uint32_t mvhfs_list(const mvhfs_t *filesystem, mvhfs_entry_t *entries,
                    uint32_t capacity);

#endif
