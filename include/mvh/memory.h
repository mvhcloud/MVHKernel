#ifndef MVH_MEMORY_H
#define MVH_MEMORY_H

#include <stdint.h>

typedef struct {
    uint64_t total_pages;
    uint64_t used_pages;
    uint64_t free_pages;
    uint64_t reserved_pages;
    uint64_t allocation_requests;
    uint64_t free_requests;
    uint64_t failed_allocations;
    uint64_t peak_used_pages;
} pmm_stats_t;

typedef struct {
    uint64_t total_bytes;
    uint64_t used_bytes;
    uint64_t free_bytes;
    uint64_t largest_free_block;
    uint64_t allocations;
    uint64_t allocation_failures;
    uint64_t invalid_frees;
    uint32_t blocks;
    uint32_t free_blocks;
} heap_stats_t;

#define VMM_PRESENT (1ull << 0u)
#define VMM_WRITABLE (1ull << 1u)
#define VMM_USER (1ull << 2u)
#define VMM_WRITE_THROUGH (1ull << 3u)
#define VMM_CACHE_DISABLE (1ull << 4u)
#define VMM_GLOBAL (1ull << 8u)
#define VMM_NO_EXECUTE (1ull << 63u)

void pmm_init(uint64_t memory_kib, uintptr_t kernel_end);
void *pmm_alloc_pages(uint32_t count);
void pmm_free_pages(void *address, uint32_t count);
void pmm_get_stats(pmm_stats_t *stats);
int pmm_self_test(void);
int vmm_init(void);
int vmm_map_page(uintptr_t virtual_address, uintptr_t physical_address, uint64_t flags);
int vmm_unmap_page(uintptr_t virtual_address);
int vmm_query_page(uintptr_t virtual_address, uintptr_t *physical_address, uint64_t *flags);
uint64_t vmm_mapped_pages(void);
int vmm_self_test(void);
int heap_init(void);
void *kmalloc(uint64_t size);
void kfree(void *address);
uint64_t heap_total_bytes(void);
uint64_t heap_used_bytes(void);
uint64_t heap_allocation_count(void);
void heap_get_stats(heap_stats_t *stats);
int heap_validate(void);
int heap_self_test(void);

#endif
