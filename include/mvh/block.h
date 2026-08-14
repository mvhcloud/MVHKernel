#ifndef MVH_BLOCK_H
#define MVH_BLOCK_H

#include <stdint.h>

#define BLOCK_DEVICE_MAX 16u
#define BLOCK_NAME_MAX 24u

typedef int (*block_read_fn)(void *context, uint64_t lba, uint32_t count, void *buffer);
typedef int (*block_write_fn)(void *context, uint64_t lba, uint32_t count,
                              const void *buffer);

typedef struct {
    uint32_t id;
    char name[BLOCK_NAME_MAX];
    uint32_t sector_size;
    uint64_t sector_count;
    uint8_t writable;
    void *context;
    block_read_fn read;
    block_write_fn write;
} block_device_t;

typedef enum {
    PARTITION_TABLE_NONE,
    PARTITION_TABLE_MBR,
    PARTITION_TABLE_GPT
} partition_table_t;

typedef struct {
    partition_table_t type;
    uint32_t partition_count;
    uint8_t protective_mbr;
} partition_info_t;

void block_init(void);
int block_register(const char *name, uint32_t sector_size, uint64_t sector_count,
                   uint8_t writable, void *context, block_read_fn read,
                   block_write_fn write);
uint32_t block_count(void);
uint32_t block_list(block_device_t *devices, uint32_t capacity);
int block_read(uint32_t id, uint64_t lba, uint32_t count, void *buffer);
int block_write(uint32_t id, uint64_t lba, uint32_t count, const void *buffer);
int partition_probe(uint32_t device_id, partition_info_t *info);
const char *partition_table_name(partition_table_t type);
int block_self_test(void);

#endif
