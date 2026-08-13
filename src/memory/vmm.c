#include <stdint.h>
#include "mvh/memory.h"

#define PAGE_SIZE 4096ull
#define PAGE_MASK 0x000FFFFFFFFFF000ull
#define PAGE_HUGE (1ull << 7u)

static uint64_t mapped_page_count;

extern char __kernel_start;
extern char __text_end;
extern char __rodata_start;
extern char __rodata_end;
extern char __kernel_end;

static uint64_t *active_pml4(void)
{
    uint64_t address;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(address));
    return (uint64_t *)(uintptr_t)(address & PAGE_MASK);
}

static void invalidate(uintptr_t address)
{
    __asm__ volatile ("invlpg (%0)" : : "r"(address) : "memory");
}

static uint64_t *new_table(void)
{
    uint64_t *table = (uint64_t *)pmm_alloc_pages(1u);
    uint32_t index;
    if (table == 0) return 0;
    for (index = 0u; index < 512u; index++) table[index] = 0u;
    return table;
}

static uint64_t *split_huge_page(uint64_t *entry)
{
    uint64_t original = *entry;
    uint64_t base = original & 0x000FFFFFFFE00000ull;
    uint64_t common = original & (VMM_WRITABLE | VMM_USER | VMM_WRITE_THROUGH |
                                  VMM_CACHE_DISABLE | VMM_GLOBAL | VMM_NO_EXECUTE);
    uint64_t *table = new_table();
    uint32_t index;
    if (table == 0) return 0;
    for (index = 0u; index < 512u; index++) {
        table[index] = base + (uint64_t)index * PAGE_SIZE | VMM_PRESENT | common;
    }
    *entry = ((uint64_t)(uintptr_t)table & PAGE_MASK) | VMM_PRESENT | VMM_WRITABLE;
    return table;
}

static uint64_t *walk(uintptr_t address, uint8_t create)
{
    uint64_t *pml4 = active_pml4();
    uint64_t *pdpt;
    uint64_t *pd;
    uint64_t *pt;
    uint32_t pml4_index = (uint32_t)((address >> 39u) & 0x1FFu);
    uint32_t pdpt_index = (uint32_t)((address >> 30u) & 0x1FFu);
    uint32_t pd_index = (uint32_t)((address >> 21u) & 0x1FFu);
    if ((pml4[pml4_index] & VMM_PRESENT) == 0u) {
        if (create == 0u || (pdpt = new_table()) == 0) return 0;
        pml4[pml4_index] = (uint64_t)(uintptr_t)pdpt | VMM_PRESENT | VMM_WRITABLE;
    }
    pdpt = (uint64_t *)(uintptr_t)(pml4[pml4_index] & PAGE_MASK);
    if ((pdpt[pdpt_index] & VMM_PRESENT) == 0u) {
        if (create == 0u || (pd = new_table()) == 0) return 0;
        pdpt[pdpt_index] = (uint64_t)(uintptr_t)pd | VMM_PRESENT | VMM_WRITABLE;
    }
    pd = (uint64_t *)(uintptr_t)(pdpt[pdpt_index] & PAGE_MASK);
    if ((pd[pd_index] & VMM_PRESENT) != 0u && (pd[pd_index] & PAGE_HUGE) != 0u) {
        pt = split_huge_page(&pd[pd_index]);
        if (pt == 0) return 0;
    } else if ((pd[pd_index] & VMM_PRESENT) == 0u) {
        if (create == 0u || (pt = new_table()) == 0) return 0;
        pd[pd_index] = (uint64_t)(uintptr_t)pt | VMM_PRESENT | VMM_WRITABLE;
    } else {
        pt = (uint64_t *)(uintptr_t)(pd[pd_index] & PAGE_MASK);
    }
    return &pt[(address >> 12u) & 0x1FFu];
}

int vmm_map_page(uintptr_t virtual_address, uintptr_t physical_address, uint64_t flags)
{
    uint64_t *entry;
    if ((virtual_address & (PAGE_SIZE - 1u)) != 0u ||
        (physical_address & (PAGE_SIZE - 1u)) != 0u) return -1;
    entry = walk(virtual_address, 1u);
    if (entry == 0) return -1;
    if ((*entry & VMM_PRESENT) == 0u) mapped_page_count++;
    *entry = ((uint64_t)physical_address & PAGE_MASK) | VMM_PRESENT | flags;
    invalidate(virtual_address);
    return 0;
}

int vmm_unmap_page(uintptr_t virtual_address)
{
    uint64_t *entry;
    if ((virtual_address & (PAGE_SIZE - 1u)) != 0u) return -1;
    entry = walk(virtual_address, 0u);
    if (entry == 0 || (*entry & VMM_PRESENT) == 0u) return -1;
    *entry = 0u;
    if (mapped_page_count != 0u) mapped_page_count--;
    invalidate(virtual_address);
    return 0;
}

int vmm_query_page(uintptr_t virtual_address, uintptr_t *physical_address, uint64_t *flags)
{
    uint64_t *entry = walk(virtual_address, 0u);
    if (entry == 0 || (*entry & VMM_PRESENT) == 0u) return -1;
    if (physical_address != 0) *physical_address = (uintptr_t)(*entry & PAGE_MASK) |
                                                (virtual_address & (PAGE_SIZE - 1u));
    if (flags != 0) *flags = *entry & ~PAGE_MASK;
    return 0;
}

uint64_t vmm_mapped_pages(void)
{
    return mapped_page_count;
}

int vmm_init(void)
{
    uintptr_t address;
    uint64_t flags;
    mapped_page_count = 512u * 512u;
    if (vmm_unmap_page(0u) != 0) return -1;
    for (address = (uintptr_t)&__kernel_start; address < (uintptr_t)&__text_end;
         address += PAGE_SIZE) {
        if (vmm_map_page(address, address, VMM_GLOBAL) != 0) return -1;
    }
    for (address = (uintptr_t)&__rodata_start; address < (uintptr_t)&__rodata_end;
         address += PAGE_SIZE) {
        flags = VMM_GLOBAL | VMM_NO_EXECUTE;
        if (vmm_map_page(address, address, flags) != 0) return -1;
    }
    for (address = ((uintptr_t)&__rodata_end + PAGE_SIZE - 1u) & ~(PAGE_SIZE - 1u);
         address < (((uintptr_t)&__kernel_end + PAGE_SIZE - 1u) & ~(PAGE_SIZE - 1u));
         address += PAGE_SIZE) {
        if (vmm_map_page(address, address, VMM_WRITABLE | VMM_GLOBAL | VMM_NO_EXECUTE) != 0) return -1;
    }
    return 0;
}

int vmm_self_test(void)
{
    uintptr_t physical;
    uintptr_t queried;
    uint64_t flags;
    volatile uint64_t *test = (volatile uint64_t *)(uintptr_t)0x40000000u;
    physical = (uintptr_t)pmm_alloc_pages(1u);
    if (physical == 0u) return -1;
    if (vmm_map_page((uintptr_t)test, physical, VMM_WRITABLE | VMM_NO_EXECUTE) != 0) {
        pmm_free_pages((void *)physical, 1u);
        return -1;
    }
    *test = 0x4D5648313132ull;
    if (*test != 0x4D5648313132ull ||
        vmm_query_page((uintptr_t)test, &queried, &flags) != 0 || queried != physical ||
        (flags & VMM_NO_EXECUTE) == 0u) {
        vmm_unmap_page((uintptr_t)test);
        pmm_free_pages((void *)physical, 1u);
        return -1;
    }
    if (vmm_unmap_page((uintptr_t)test) != 0 ||
        vmm_query_page((uintptr_t)test, 0, 0) == 0) {
        pmm_free_pages((void *)physical, 1u);
        return -1;
    }
    pmm_free_pages((void *)physical, 1u);
    return 0;
}
