#ifndef MVH_GDT_H
#define MVH_GDT_H

#include <stdint.h>

#define GDT_KERNEL_CODE 0x08u
#define GDT_KERNEL_DATA 0x10u
#define GDT_TSS 0x18u
#define GDT_USER_DATA 0x2Bu
#define GDT_USER_CODE 0x33u
#define GDT_IST_DOUBLE_FAULT 1u
#define GDT_IST_NMI 2u
#define GDT_IST_MACHINE_CHECK 3u

int gdt_init_cpu(uint32_t cpu_id, uintptr_t rsp0);
void gdt_set_rsp0(uint32_t cpu_id, uintptr_t rsp0);
uintptr_t gdt_ist_top(uint32_t cpu_id, uint8_t ist_index);

#endif
