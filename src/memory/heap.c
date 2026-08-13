#include <stdint.h>
#include "mvh/memory.h"
#include "mvh/panic.h"

#define HEAP_PAGES 256u
#define HEAP_GUARD_PAGES 2u
#define HEAP_MAGIC 0x4D564848u
#define HEAP_CANARY 0xC0DEFACEBADC0FFEu
#define HEAP_POISON 0xDDu

typedef struct heap_block {
    uint32_t magic;
    uint8_t free;
    uint8_t reserved[3];
    uint64_t size;
    uint64_t requested;
    struct heap_block *next;
    struct heap_block *previous;
} heap_block_t;

static heap_block_t *heap_first;
static void *heap_allocation_base;
static uint64_t heap_used;
static uint64_t heap_allocations;
static uint64_t heap_failures;
static uint64_t heap_invalid_frees;

static uint64_t *block_canary(heap_block_t *block)
{
    return (uint64_t *)((uint8_t *)(block + 1) + block->size - sizeof(uint64_t));
}

static uint64_t align16(uint64_t value)
{
    return (value + 15u) & ~15u;
}

int heap_init(void)
{
    uint32_t page;
    heap_allocation_base = pmm_alloc_pages(HEAP_PAGES + HEAP_GUARD_PAGES);
    if (heap_allocation_base == 0) {
        return -1;
    }
    heap_first = (heap_block_t *)((uint8_t *)heap_allocation_base + 4096u);
    if (vmm_unmap_page((uintptr_t)heap_allocation_base) != 0 ||
        vmm_unmap_page((uintptr_t)heap_first + (uint64_t)HEAP_PAGES * 4096u) != 0) {
        pmm_free_pages(heap_allocation_base, HEAP_PAGES + HEAP_GUARD_PAGES);
        heap_first = 0;
        return -1;
    }
    for (page = 0u; page < HEAP_PAGES; page++) {
        uintptr_t address = (uintptr_t)heap_first + (uint64_t)page * 4096u;
        if (vmm_map_page(address, address, VMM_WRITABLE | VMM_GLOBAL | VMM_NO_EXECUTE) != 0) {
            pmm_free_pages(heap_allocation_base, HEAP_PAGES + HEAP_GUARD_PAGES);
            heap_first = 0;
            return -1;
        }
    }
    heap_first->magic = HEAP_MAGIC;
    heap_first->free = 1u;
    heap_first->size = (uint64_t)HEAP_PAGES * 4096u - sizeof(heap_block_t);
    heap_first->requested = 0u;
    heap_first->next = 0;
    heap_first->previous = 0;
    heap_used = 0u;
    heap_allocations = 0u;
    heap_failures = 0u;
    heap_invalid_frees = 0u;
    return 0;
}

void *kmalloc(uint64_t size)
{
    heap_block_t *block = heap_first;
    heap_block_t *split;
    if (size > UINT64_MAX - sizeof(uint64_t) - 15u) {
        heap_failures++;
        return 0;
    }
    size = align16(size + sizeof(uint64_t));
    if (size == 0u || block == 0) {
        return 0;
    }
    while (block != 0) {
        if (block->magic != HEAP_MAGIC) {
            return 0;
        }
        if (block->free != 0u && block->size >= size) {
            if (block->size >= size + sizeof(heap_block_t) + 16u) {
                split = (heap_block_t *)((uint8_t *)(block + 1) + size);
                split->magic = HEAP_MAGIC;
                split->free = 1u;
                split->size = block->size - size - sizeof(heap_block_t);
                split->requested = 0u;
                split->next = block->next;
                split->previous = block;
                if (split->next != 0) {
                    split->next->previous = split;
                }
                block->next = split;
                block->size = size;
            }
            block->free = 0u;
            block->requested = size - sizeof(uint64_t);
            *block_canary(block) = HEAP_CANARY ^ (uint64_t)(uintptr_t)block;
            heap_used += block->size;
            heap_allocations++;
            return block + 1;
        }
        block = block->next;
    }
    heap_failures++;
    return 0;
}

void kfree(void *address)
{
    heap_block_t *block;
    if (address == 0) {
        return;
    }
    block = ((heap_block_t *)address) - 1;
    if (block->magic != HEAP_MAGIC || block->free != 0u) {
        heap_invalid_frees++;
        return;
    }
    if (*block_canary(block) != (HEAP_CANARY ^ (uint64_t)(uintptr_t)block)) {
        kernel_panic("kernel heap canary corrupted");
    }
    block->free = 1u;
    {
        uint64_t index;
        uint8_t *payload = (uint8_t *)(block + 1);
        for (index = 0u; index < block->size; index++) payload[index] = HEAP_POISON;
    }
    block->requested = 0u;
    heap_used -= block->size;
    heap_allocations--;
    if (block->next != 0 && block->next->free != 0u && block->next->magic == HEAP_MAGIC) {
        block->size += sizeof(heap_block_t) + block->next->size;
        block->next = block->next->next;
        if (block->next != 0) {
            block->next->previous = block;
        }
    }
    if (block->previous != 0 && block->previous->free != 0u &&
        block->previous->magic == HEAP_MAGIC) {
        block->previous->size += sizeof(heap_block_t) + block->size;
        block->previous->next = block->next;
        if (block->next != 0) {
            block->next->previous = block->previous;
        }
    }
}

uint64_t heap_total_bytes(void)
{
    return (uint64_t)HEAP_PAGES * 4096u;
}

uint64_t heap_used_bytes(void)
{
    return heap_used;
}

uint64_t heap_allocation_count(void)
{
    return heap_allocations;
}

void heap_get_stats(heap_stats_t *stats)
{
    heap_block_t *block = heap_first;
    stats->total_bytes = heap_total_bytes();
    stats->used_bytes = heap_used;
    stats->free_bytes = 0u;
    stats->largest_free_block = 0u;
    stats->allocations = heap_allocations;
    stats->allocation_failures = heap_failures;
    stats->invalid_frees = heap_invalid_frees;
    stats->blocks = 0u;
    stats->free_blocks = 0u;
    while (block != 0 && block->magic == HEAP_MAGIC && stats->blocks < 65536u) {
        stats->blocks++;
        if (block->free != 0u) {
            stats->free_blocks++;
            stats->free_bytes += block->size;
            if (block->size > stats->largest_free_block) {
                stats->largest_free_block = block->size;
            }
        }
        block = block->next;
    }
}

int heap_validate(void)
{
    heap_block_t *block = heap_first;
    heap_block_t *previous = 0;
    uintptr_t heap_start = (uintptr_t)heap_first;
    uintptr_t heap_end = heap_start + (uint64_t)HEAP_PAGES * 4096u;
    uint32_t blocks = 0u;
    while (block != 0) {
        uintptr_t address = (uintptr_t)block;
        uintptr_t block_end;
        if (address < heap_start || address + sizeof(heap_block_t) > heap_end ||
            block->magic != HEAP_MAGIC || block->previous != previous) {
            return -1;
        }
        block_end = address + sizeof(heap_block_t) + block->size;
        if (block_end > heap_end || (++blocks > 65536u)) {
            return -1;
        }
        if (block->next != 0 && (uintptr_t)block->next != block_end) {
            return -1;
        }
        if (block->free == 0u &&
            *block_canary(block) != (HEAP_CANARY ^ (uint64_t)(uintptr_t)block)) {
            return -1;
        }
        previous = block;
        block = block->next;
    }
    return 0;
}

int heap_self_test(void)
{
    uint64_t used_before = heap_used_bytes();
    uint64_t allocations_before = heap_allocation_count();
    uint8_t *first = (uint8_t *)kmalloc(64u);
    uint8_t *second = (uint8_t *)kmalloc(4096u);
    uint32_t index;
    if (first == 0 || second == 0) {
        kfree(first);
        kfree(second);
        return -1;
    }
    for (index = 0u; index < 64u; index++) {
        first[index] = (uint8_t)index;
    }
    for (index = 0u; index < 64u; index++) {
        if (first[index] != (uint8_t)index) {
            kfree(second);
            kfree(first);
            return -1;
        }
    }
    kfree(second);
    kfree(first);
    return heap_validate() == 0 && heap_used_bytes() == used_before &&
           heap_allocation_count() == allocations_before ? 0 : -1;
}
