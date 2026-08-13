#include <stdint.h>
#include "mvh/memory.h"

#define HEAP_PAGES 256u
#define HEAP_MAGIC 0x4D564848u

typedef struct heap_block {
    uint32_t magic;
    uint8_t free;
    uint8_t reserved[3];
    uint64_t size;
    struct heap_block *next;
    struct heap_block *previous;
} heap_block_t;

static heap_block_t *heap_first;
static uint64_t heap_used;

static uint64_t align16(uint64_t value)
{
    return (value + 15u) & ~15u;
}

int heap_init(void)
{
    heap_first = (heap_block_t *)pmm_alloc_pages(HEAP_PAGES);
    if (heap_first == 0) {
        return -1;
    }
    heap_first->magic = HEAP_MAGIC;
    heap_first->free = 1u;
    heap_first->size = (uint64_t)HEAP_PAGES * 4096u - sizeof(heap_block_t);
    heap_first->next = 0;
    heap_first->previous = 0;
    heap_used = 0u;
    return 0;
}

void *kmalloc(uint64_t size)
{
    heap_block_t *block = heap_first;
    heap_block_t *split;
    size = align16(size);
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
                split->next = block->next;
                split->previous = block;
                if (split->next != 0) {
                    split->next->previous = split;
                }
                block->next = split;
                block->size = size;
            }
            block->free = 0u;
            heap_used += block->size;
            return block + 1;
        }
        block = block->next;
    }
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
        return;
    }
    block->free = 1u;
    heap_used -= block->size;
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
