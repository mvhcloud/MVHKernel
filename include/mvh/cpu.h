#ifndef MVH_CPU_H
#define MVH_CPU_H

#include <stdint.h>

void cpu_vendor(char *vendor);
void cpu_brand(char *brand);
uint32_t cpu_logical_count(void);
uint32_t cpu_feature_ecx(void);
uint32_t cpu_feature_edx(void);
uint32_t cpu_extended_feature_edx(void);

#endif
