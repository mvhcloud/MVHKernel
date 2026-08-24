#include <stdint.h>
#include "mvh/dma.h"
#include "mvh/memory.h"
#include "mvh/sync.h"

#define DMA_PAGE_SIZE 4096u

static uint64_t align_up(uint64_t value, uint64_t alignment)
{
    return (value + alignment - 1u) & ~(alignment - 1u);
}

int dma_alloc(uint64_t size, uint64_t alignment, uint8_t address_bits,
              dma_buffer_t *buffer)
{
    uint64_t pages;
    uint64_t alignment_pages;
    uint64_t total_pages;
    uintptr_t physical;
    void *allocation;
    if (buffer == 0 || size == 0u || address_bits < 32u || address_bits > 64u) return -1;
    if (alignment < DMA_PAGE_SIZE) alignment = DMA_PAGE_SIZE;
    if ((alignment & (alignment - 1u)) != 0u || size > UINT64_MAX - DMA_PAGE_SIZE) return -1;
    pages = (size + DMA_PAGE_SIZE - 1u) / DMA_PAGE_SIZE;
    alignment_pages = alignment / DMA_PAGE_SIZE;
    total_pages = pages + alignment_pages - 1u;
    if (total_pages > UINT32_MAX) return -1;
    allocation = pmm_alloc_pages((uint32_t)total_pages);
    if (allocation == 0) return -1;
    physical = (uintptr_t)align_up((uintptr_t)allocation, alignment);
    if (address_bits < 64u &&
        ((uint64_t)physical + size > (1ull << address_bits))) {
        pmm_free_pages(allocation, (uint32_t)total_pages);
        return -1;
    }
    buffer->virtual_address = (void *)physical;
    buffer->physical_address = physical;
    buffer->size = size;
    buffer->allocation_base = allocation;
    buffer->allocation_pages = (uint32_t)total_pages;
    buffer->address_bits = address_bits;
    buffer->coherent = 1u;
    return 0;
}

void dma_free(dma_buffer_t *buffer)
{
    if (buffer == 0 || buffer->allocation_base == 0 || buffer->allocation_pages == 0u) return;
    pmm_free_pages(buffer->allocation_base, buffer->allocation_pages);
    buffer->virtual_address = 0;
    buffer->physical_address = 0u;
    buffer->size = 0u;
    buffer->allocation_base = 0;
    buffer->allocation_pages = 0u;
}

int dma_map(const void *address, uint64_t size, uint8_t address_bits,
            dma_direction_t direction, uintptr_t *physical_address)
{
    uintptr_t physical = (uintptr_t)address;
    (void)direction;
    if (address == 0 || physical_address == 0 || size == 0u ||
        address_bits < 32u || address_bits > 64u || size > UINTPTR_MAX - physical) return -1;
    if (address_bits < 64u && (uint64_t)physical + size > (1ull << address_bits)) return -1;
    memory_barrier();
    *physical_address = physical;
    return 0;
}

void dma_unmap(uintptr_t physical_address, uint64_t size, dma_direction_t direction)
{
    (void)physical_address;
    (void)size;
    (void)direction;
    memory_barrier();
}

void dma_sync_for_cpu(const dma_buffer_t *buffer, dma_direction_t direction)
{
    (void)buffer;
    (void)direction;
    memory_read_barrier();
}

void dma_sync_for_device(const dma_buffer_t *buffer, dma_direction_t direction)
{
    (void)buffer;
    (void)direction;
    memory_write_barrier();
}

int dma_build_scatter(const void *address, uint64_t size, uint32_t max_segment_size,
                      dma_segment_t *segments, uint32_t capacity, uint32_t *count)
{
    uintptr_t cursor = (uintptr_t)address;
    uint32_t used = 0u;
    if (address == 0 || segments == 0 || count == 0 || size == 0u ||
        max_segment_size == 0u || size > UINTPTR_MAX - cursor) return -1;
    while (size != 0u) {
        uint64_t page_left = DMA_PAGE_SIZE - (cursor & (DMA_PAGE_SIZE - 1u));
        uint64_t length = size < page_left ? size : page_left;
        if (length > max_segment_size) length = max_segment_size;
        if (used >= capacity) return -1;
        segments[used].physical_address = cursor;
        segments[used].length = (uint32_t)length;
        used++;
        cursor += (uintptr_t)length;
        size -= length;
    }
    *count = used;
    return 0;
}

int dma_self_test(void)
{
    dma_buffer_t buffer = {0};
    dma_segment_t segments[4];
    uintptr_t mapped;
    uint32_t count;
    uint8_t *bytes;
    if (dma_alloc(8192u, 4096u, 32u, &buffer) != 0) return -1;
    bytes = (uint8_t *)buffer.virtual_address;
    bytes[0] = 0x4Du;
    bytes[8191] = 0x56u;
    if (dma_map(bytes, 8192u, 32u, DMA_BIDIRECTIONAL, &mapped) != 0 ||
        mapped != buffer.physical_address ||
        dma_build_scatter(bytes, 8192u, 4096u, segments, 4u, &count) != 0 || count != 2u) {
        dma_free(&buffer);
        return -1;
    }
    dma_sync_for_device(&buffer, DMA_TO_DEVICE);
    dma_sync_for_cpu(&buffer, DMA_FROM_DEVICE);
    dma_unmap(mapped, 8192u, DMA_BIDIRECTIONAL);
    dma_free(&buffer);
    return 0;
}
