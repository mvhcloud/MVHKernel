#ifndef MVH_VIRTIO_BLK_H
#define MVH_VIRTIO_BLK_H

#include <stdint.h>

typedef struct {
    uint8_t available;
    uint8_t initialized;
    uint8_t pci_slot;
    uint8_t pci_function;
    uint16_t io_base;
    uint16_t queue_size;
    uint64_t sectors;
    uint64_t requests;
    uint64_t failures;
} virtio_blk_status_t;

int virtio_blk_init(void);
const virtio_blk_status_t *virtio_blk_status(void);

#endif
