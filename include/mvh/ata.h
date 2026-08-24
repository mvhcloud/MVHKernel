#ifndef MVH_ATA_H
#define MVH_ATA_H

#include <stdint.h>

typedef struct {
    uint8_t present;
    uint8_t channel;
    uint8_t drive;
    uint8_t lba48;
    uint8_t writable;
    uint16_t io_base;
    uint16_t control_base;
    uint64_t sectors;
    char model[41];
} ata_device_info_t;

uint32_t ata_init(void);
uint32_t ata_device_count(void);
int ata_device_info(uint32_t index, ata_device_info_t *info);

#endif
