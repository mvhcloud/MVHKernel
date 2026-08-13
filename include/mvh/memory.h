#ifndef MVH_MEMORY_H
#define MVH_MEMORY_H

#include <stdint.h>

typedef struct {
    uint64_t total_pages;
    uint64_t used_pages;
    uint64_t free_pages;
    uint64_t reserved_pages;
} pmm_stats_t;

void pmm_init(uint64_t memory_kib, uintptr_t kernel_end);
void *pmm_alloc_pages(uint32_t count);
void pmm_free_pages(void *address, uint32_t count);
void pmm_get_stats(pmm_stats_t *stats);
int heap_init(void);
void *kmalloc(uint64_t size);
void kfree(void *address);
uint64_t heap_total_bytes(void);
uint64_t heap_used_bytes(void);

#endif
