#include <stdint.h>
#include "mvh/block.h"
#include "mvh/io.h"
#include "mvh/memory.h"
#include "mvh/pci.h"
#include "mvh/sync.h"
#include "mvh/virtio_blk.h"

#define VIRTIO_VENDOR 0x1AF4u
#define VIRTIO_BLOCK_TRANSITIONAL 0x1001u
#define VIRTIO_PCI_HOST_FEATURES 0u
#define VIRTIO_PCI_GUEST_FEATURES 4u
#define VIRTIO_PCI_QUEUE_PFN 8u
#define VIRTIO_PCI_QUEUE_SIZE 12u
#define VIRTIO_PCI_QUEUE_SELECT 14u
#define VIRTIO_PCI_QUEUE_NOTIFY 16u
#define VIRTIO_PCI_STATUS 18u
#define VIRTIO_PCI_CONFIG 20u
#define VIRTIO_STATUS_ACK 1u
#define VIRTIO_STATUS_DRIVER 2u
#define VIRTIO_STATUS_DRIVER_OK 4u
#define VIRTQ_DESC_NEXT 1u
#define VIRTQ_DESC_WRITE 2u
#define VIRTIO_BLK_READ 0u
#define VIRTIO_BLK_WRITE 1u
#define VIRTIO_QUEUE_MAX 256u
#define VIRTIO_TIMEOUT 100000000u

typedef struct __attribute__((packed)) {
    uint64_t address;
    uint32_t length;
    uint16_t flags;
    uint16_t next;
} virtq_desc_t;

typedef struct __attribute__((packed)) {
    uint16_t flags;
    uint16_t index;
    uint16_t ring[VIRTIO_QUEUE_MAX];
} virtq_avail_t;

typedef struct __attribute__((packed)) { uint32_t id; uint32_t length; } virtq_used_element_t;
typedef struct __attribute__((packed)) {
    uint16_t flags;
    uint16_t index;
    virtq_used_element_t ring[VIRTIO_QUEUE_MAX];
} virtq_used_t;

typedef struct __attribute__((packed)) {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
} virtio_request_t;

typedef struct {
    uint16_t io_base;
    uint16_t queue_size;
    virtq_desc_t *descriptors;
    virtq_avail_t *available;
    volatile virtq_used_t *used;
    uint16_t last_used;
    virtio_request_t request;
    uint8_t request_status;
    spinlock_t lock;
} virtio_device_t;

static virtio_device_t device;
static virtio_blk_status_t state;

static uint64_t io_in64(uint16_t port)
{
    uint32_t low = io_in32(port);
    uint32_t high = io_in32((uint16_t)(port + 4u));
    return (uint64_t)low | ((uint64_t)high << 32u);
}

static int find_device(uint8_t *slot_result, uint8_t *function_result, uint16_t *io_base)
{
    uint8_t slot;
    for (slot = 0u; slot < 32u; slot++) {
        uint8_t function;
        uint32_t header = pci_config_read32(0u, slot, 0u, 0x0Cu);
        uint8_t functions = (header & 0x00800000u) != 0u ? 8u : 1u;
        for (function = 0u; function < functions; function++) {
            uint32_t identity = pci_config_read32(0u, slot, function, 0u);
            uint16_t vendor = (uint16_t)identity;
            uint16_t product = (uint16_t)(identity >> 16u);
            uint8_t bar;
            if (vendor != VIRTIO_VENDOR || product != VIRTIO_BLOCK_TRANSITIONAL) continue;
            for (bar = 0u; bar < 6u; bar++) {
                uint32_t value = pci_config_read32(0u, slot, function,
                                                   (uint8_t)(0x10u + bar * 4u));
                if ((value & 1u) != 0u && (value & ~3u) <= 0xFFFFu) {
                    *slot_result = slot;
                    *function_result = function;
                    *io_base = (uint16_t)(value & ~3u);
                    return 0;
                }
            }
        }
    }
    return -1;
}

static int submit(uint32_t type, uint64_t sector, uint32_t count, void *buffer)
{
    uint16_t available_index;
    uint16_t used_before;
    uint32_t timeout;
    if (count == 0u || count > 128u || buffer == 0) return -1;
    spinlock_lock(&device.lock);
    device.request.type = type;
    device.request.reserved = 0u;
    device.request.sector = sector;
    device.request_status = 0xFFu;
    device.descriptors[0].address = (uint64_t)(uintptr_t)&device.request;
    device.descriptors[0].length = sizeof(device.request);
    device.descriptors[0].flags = VIRTQ_DESC_NEXT;
    device.descriptors[0].next = 1u;
    device.descriptors[1].address = (uint64_t)(uintptr_t)buffer;
    device.descriptors[1].length = count * 512u;
    device.descriptors[1].flags = VIRTQ_DESC_NEXT |
                                  (type == VIRTIO_BLK_READ ? VIRTQ_DESC_WRITE : 0u);
    device.descriptors[1].next = 2u;
    device.descriptors[2].address = (uint64_t)(uintptr_t)&device.request_status;
    device.descriptors[2].length = 1u;
    device.descriptors[2].flags = VIRTQ_DESC_WRITE;
    device.descriptors[2].next = 0u;
    available_index = device.available->index;
    device.available->ring[available_index % device.queue_size] = 0u;
    __asm__ volatile ("mfence" : : : "memory");
    device.available->index = (uint16_t)(available_index + 1u);
    __asm__ volatile ("mfence" : : : "memory");
    used_before = device.last_used;
    io_out16((uint16_t)(device.io_base + VIRTIO_PCI_QUEUE_NOTIFY), 0u);
    for (timeout = 0u; timeout < VIRTIO_TIMEOUT; timeout++) {
        if (device.used->index != used_before) break;
        __asm__ volatile ("pause");
    }
    if (timeout == VIRTIO_TIMEOUT || device.request_status != 0u) {
        state.failures++;
        spinlock_unlock(&device.lock);
        return -1;
    }
    device.last_used = device.used->index;
    state.requests++;
    spinlock_unlock(&device.lock);
    return 0;
}

static int read_blocks(void *context, uint64_t lba, uint32_t count, void *buffer)
{
    (void)context;
    return submit(VIRTIO_BLK_READ, lba, count, buffer);
}

static int write_blocks(void *context, uint64_t lba, uint32_t count, const void *buffer)
{
    (void)context;
    return submit(VIRTIO_BLK_WRITE, lba, count, (void *)buffer);
}

int virtio_blk_init(void)
{
    uint8_t slot;
    uint8_t function;
    uint16_t io_base;
    uint32_t command;
    uint8_t status;
    uint8_t *queue;
    uintptr_t used_offset;
    uint8_t *bytes = (uint8_t *)&state;
    uint32_t index;
    for (index = 0u; index < sizeof(state); index++) bytes[index] = 0u;
    if (find_device(&slot, &function, &io_base) != 0) return -1;
    state.available = 1u;
    state.pci_slot = slot;
    state.pci_function = function;
    state.io_base = io_base;
    command = pci_config_read32(0u, slot, function, 0x04u);
    pci_config_write32(0u, slot, function, 0x04u, command | 0x00000005u);
    io_out8((uint16_t)(io_base + VIRTIO_PCI_STATUS), 0u);
    status = VIRTIO_STATUS_ACK;
    io_out8((uint16_t)(io_base + VIRTIO_PCI_STATUS), status);
    status |= VIRTIO_STATUS_DRIVER;
    io_out8((uint16_t)(io_base + VIRTIO_PCI_STATUS), status);
    (void)io_in32((uint16_t)(io_base + VIRTIO_PCI_HOST_FEATURES));
    io_out32((uint16_t)(io_base + VIRTIO_PCI_GUEST_FEATURES), 0u);
    io_out16((uint16_t)(io_base + VIRTIO_PCI_QUEUE_SELECT), 0u);
    device.queue_size = io_in16((uint16_t)(io_base + VIRTIO_PCI_QUEUE_SIZE));
    if (device.queue_size < 3u || device.queue_size > VIRTIO_QUEUE_MAX) return -1;
    queue = (uint8_t *)pmm_alloc_pages(3u);
    if (queue == 0 || (uintptr_t)queue > 0xFFFFF000u) return -1;
    for (index = 0u; index < 3u * 4096u; index++) queue[index] = 0u;
    device.descriptors = (virtq_desc_t *)(void *)queue;
    device.available = (virtq_avail_t *)(void *)(queue + 16u * device.queue_size);
    used_offset = ((uintptr_t)((uint8_t *)device.available + 6u + 2u * device.queue_size) +
                   4095u) & ~4095ull;
    device.used = (volatile virtq_used_t *)used_offset;
    device.io_base = io_base;
    device.last_used = 0u;
    spinlock_init(&device.lock);
    io_out32((uint16_t)(io_base + VIRTIO_PCI_QUEUE_PFN), (uint32_t)((uintptr_t)queue >> 12u));
    status |= VIRTIO_STATUS_DRIVER_OK;
    io_out8((uint16_t)(io_base + VIRTIO_PCI_STATUS), status);
    state.queue_size = device.queue_size;
    state.sectors = io_in64((uint16_t)(io_base + VIRTIO_PCI_CONFIG));
    if (state.sectors == 0u ||
        block_register("virtio0", 512u, state.sectors, 1u, &device,
                       read_blocks, write_blocks) < 0) return -1;
    state.initialized = 1u;
    return 0;
}

const virtio_blk_status_t *virtio_blk_status(void) { return &state; }
