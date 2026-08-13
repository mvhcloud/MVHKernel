#include <stdint.h>
#include "mvh/memory.h"

#define PAGE_SIZE 4096u
#define PMM_MAX_PAGES 262144u
#define PMM_BITMAP_BYTES (PMM_MAX_PAGES / 8u)

static uint8_t page_bitmap[PMM_BITMAP_BYTES];
static uint32_t managed_pages;
static uint32_t reserved_pages;
static uint32_t used_pages;
static uint64_t allocation_requests;
static uint64_t free_requests;
static uint64_t failed_allocations;
static uint32_t peak_used_pages;

static void page_set(uint32_t page)
{
    page_bitmap[page >> 3u] |= (uint8_t)(1u << (page & 7u));
}

static void page_clear(uint32_t page)
{
    page_bitmap[page >> 3u] &= (uint8_t)~(1u << (page & 7u));
}

static uint8_t page_used(uint32_t page)
{
    return page_bitmap[page >> 3u] & (uint8_t)(1u << (page & 7u));
}

void pmm_init(uint64_t memory_kib, uintptr_t kernel_end)
{
    uint64_t bytes = memory_kib * 1024u;
    uint32_t page;
    if (bytes > 0x40000000u) {
        bytes = 0x40000000u;
    }
    managed_pages = (uint32_t)(bytes / PAGE_SIZE);
    if (managed_pages > PMM_MAX_PAGES) {
        managed_pages = PMM_MAX_PAGES;
    }
    reserved_pages = (uint32_t)((kernel_end + PAGE_SIZE - 1u) / PAGE_SIZE);
    if (reserved_pages > managed_pages) {
        reserved_pages = managed_pages;
    }
    for (page = 0u; page < PMM_BITMAP_BYTES; page++) {
        page_bitmap[page] = 0xFFu;
    }
    for (page = reserved_pages; page < managed_pages; page++) {
        page_clear(page);
    }
    used_pages = reserved_pages;
    allocation_requests = 0u;
    free_requests = 0u;
    failed_allocations = 0u;
    peak_used_pages = used_pages;
}

void *pmm_alloc_pages(uint32_t count)
{
    uint32_t start;
    uint32_t page;
    uint32_t found;
    allocation_requests++;
    if (count == 0u || count > managed_pages) {
        failed_allocations++;
        return 0;
    }
    for (start = reserved_pages; start + count <= managed_pages; start++) {
        found = 0u;
        for (page = 0u; page < count; page++) {
            if (page_used(start + page) != 0u) {
                start += page;
                break;
            }
            found++;
        }
        if (found == count) {
            for (page = 0u; page < count; page++) {
                page_set(start + page);
            }
            used_pages += count;
            if (used_pages > peak_used_pages) peak_used_pages = used_pages;
            return (void *)(uintptr_t)((uint64_t)start * PAGE_SIZE);
        }
    }
    failed_allocations++;
    return 0;
}

void pmm_free_pages(void *address, uint32_t count)
{
    uint32_t start = (uint32_t)((uintptr_t)address / PAGE_SIZE);
    uint32_t page;
    free_requests++;
    if (count == 0u || ((uintptr_t)address & (PAGE_SIZE - 1u)) != 0u || start < reserved_pages ||
        start + count > managed_pages) {
        return;
    }
    for (page = 0u; page < count; page++) {
        if (page_used(start + page) != 0u) {
            page_clear(start + page);
            used_pages--;
        }
    }
}

void pmm_get_stats(pmm_stats_t *stats)
{
    stats->total_pages = managed_pages;
    stats->used_pages = used_pages;
    stats->free_pages = managed_pages - used_pages;
    stats->reserved_pages = reserved_pages;
    stats->allocation_requests = allocation_requests;
    stats->free_requests = free_requests;
    stats->failed_allocations = failed_allocations;
    stats->peak_used_pages = peak_used_pages;
}

int pmm_self_test(void)
{
    pmm_stats_t before;
    pmm_stats_t during;
    pmm_stats_t after;
    uint8_t *pages;
    pmm_get_stats(&before);
    pages = (uint8_t *)pmm_alloc_pages(3u);
    if (pages == 0 || ((uintptr_t)pages & (PAGE_SIZE - 1u)) != 0u) {
        return -1;
    }
    pmm_get_stats(&during);
    pages[0] = 0x4Du;
    pages[PAGE_SIZE - 1u] = 0x56u;
    pages[PAGE_SIZE] = 0x48u;
    pages[PAGE_SIZE * 3u - 1u] = 0x11u;
    if (pages[0] != 0x4Du || pages[PAGE_SIZE - 1u] != 0x56u ||
        pages[PAGE_SIZE] != 0x48u || pages[PAGE_SIZE * 3u - 1u] != 0x11u ||
        during.used_pages != before.used_pages + 3u) {
        pmm_free_pages(pages, 3u);
        return -1;
    }
    pmm_free_pages(pages, 3u);
    pmm_get_stats(&after);
    return after.used_pages == before.used_pages &&
           after.free_pages == before.free_pages ? 0 : -1;
}
