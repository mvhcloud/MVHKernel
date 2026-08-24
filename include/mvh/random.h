#ifndef MVH_RANDOM_H
#define MVH_RANDOM_H

#include <stdint.h>

void random_init(void);
void entropy_add(const void *data, uint32_t size, uint32_t estimated_bits);
uint8_t random_is_ready(void);
uint32_t random_entropy_bits(void);
uint64_t random_u64(void);
void random_bytes(void *output, uint32_t size);
int random_self_test(void);

#endif
