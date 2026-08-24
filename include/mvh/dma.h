#ifndef MVH_DMA_H
#define MVH_DMA_H

#include <stdint.h>

#define DMA_MAX_SCATTER 64u

typedef enum {
    DMA_TO_DEVICE = 0,
    DMA_FROM_DEVICE = 1,
    DMA_BIDIRECTIONAL = 2
} dma_direction_t;

typedef struct {
    void *virtual_address;
    uintptr_t physical_address;
    uint64_t size;
    void *allocation_base;
    uint32_t allocation_pages;
    uint8_t address_bits;
    uint8_t coherent;
} dma_buffer_t;

typedef struct {
    uintptr_t physical_address;
    uint32_t length;
} dma_segment_t;

int dma_alloc(uint64_t size, uint64_t alignment, uint8_t address_bits,
              dma_buffer_t *buffer);
void dma_free(dma_buffer_t *buffer);
int dma_map(const void *address, uint64_t size, uint8_t address_bits,
            dma_direction_t direction, uintptr_t *physical_address);
void dma_unmap(uintptr_t physical_address, uint64_t size, dma_direction_t direction);
void dma_sync_for_cpu(const dma_buffer_t *buffer, dma_direction_t direction);
void dma_sync_for_device(const dma_buffer_t *buffer, dma_direction_t direction);
int dma_build_scatter(const void *address, uint64_t size, uint32_t max_segment_size,
                      dma_segment_t *segments, uint32_t capacity, uint32_t *count);
int dma_self_test(void);

#endif
