#include <stdint.h>
#include "mvh/cpu.h"
#include "mvh/random.h"
#include "mvh/sync.h"
#include "mvh/timer.h"

static uint32_t random_state[16];
#define RANDOM_OUTPUT_BYTES 32u
static uint8_t random_buffer[RANDOM_OUTPUT_BYTES];
static uint32_t random_available;
static uint32_t entropy_bits;
static spinlock_t random_lock;

static uint32_t rotate_left(uint32_t value, uint32_t amount)
{
    return (value << amount) | (value >> (32u - amount));
}

static void quarter(uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d)
{
    *a += *b; *d ^= *a; *d = rotate_left(*d, 16u);
    *c += *d; *b ^= *c; *b = rotate_left(*b, 12u);
    *a += *b; *d ^= *a; *d = rotate_left(*d, 8u);
    *c += *d; *b ^= *c; *b = rotate_left(*b, 7u);
}

static void generate_block(void)
{
    uint32_t working[16];
    uint32_t index;
    uint32_t round;
    for (index = 0u; index < 16u; index++) working[index] = random_state[index];
    for (round = 0u; round < 10u; round++) {
        quarter(&working[0], &working[4], &working[8], &working[12]);
        quarter(&working[1], &working[5], &working[9], &working[13]);
        quarter(&working[2], &working[6], &working[10], &working[14]);
        quarter(&working[3], &working[7], &working[11], &working[15]);
        quarter(&working[0], &working[5], &working[10], &working[15]);
        quarter(&working[1], &working[6], &working[11], &working[12]);
        quarter(&working[2], &working[7], &working[8], &working[13]);
        quarter(&working[3], &working[4], &working[9], &working[14]);
    }
    for (index = 0u; index < 16u; index++) working[index] += random_state[index];
    random_state[12]++;
    if (random_state[12] == 0u) random_state[13]++;
    for (index = 0u; index < 8u; index++) random_state[4u + index] ^= working[index];
    for (index = 0u; index < 8u; index++) {
        uint32_t value = working[8u + index];
        random_buffer[index * 4u] = (uint8_t)value;
        random_buffer[index * 4u + 1u] = (uint8_t)(value >> 8u);
        random_buffer[index * 4u + 2u] = (uint8_t)(value >> 16u);
        random_buffer[index * 4u + 3u] = (uint8_t)(value >> 24u);
    }
    random_available = RANDOM_OUTPUT_BYTES;
}

void entropy_add(const void *data, uint32_t size, uint32_t estimated_bits)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t index;
    if (bytes == 0 || size == 0u) return;
    spinlock_lock(&random_lock);
    for (index = 0u; index < size; index++) {
        uint32_t slot = index & 7u;
        random_state[4u + slot] ^= rotate_left((uint32_t)bytes[index] +
                                   0x9E3779B9u + index, (index % 31u) + 1u);
        random_state[14] ^= random_state[4u + slot] + random_state[15];
        random_state[15] = rotate_left(random_state[15] ^ bytes[index], 7u);
    }
    if (estimated_bits >= 256u - entropy_bits) entropy_bits = 256u;
    else entropy_bits += estimated_bits;
    random_available = 0u;
    spinlock_unlock(&random_lock);
}

void random_init(void)
{
    uint64_t value;
    uint64_t previous_hardware = 0u;
    uint32_t index;
    uint8_t have_hardware = 0u;
    spinlock_init(&random_lock);
    random_state[0] = 0x61707865u;
    random_state[1] = 0x3320646Eu;
    random_state[2] = 0x79622D32u;
    random_state[3] = 0x6B206574u;
    for (index = 4u; index < 16u; index++) random_state[index] = 0u;
    random_available = 0u;
    entropy_bits = 0u;
    value = cpu_read_tsc() ^ timer_ticks() ^ (uint64_t)(uintptr_t)&value;
    entropy_add(&value, sizeof(value), 8u);
    for (index = 0u; index < 4u; index++) {
        if (cpu_random64(&value) != 0u &&
            (have_hardware == 0u || value != previous_hardware)) {
            entropy_add(&value, sizeof(value), 64u);
            previous_hardware = value;
            have_hardware = 1u;
        }
    }
    value = cpu_read_tsc() ^ ((uint64_t)timer_frequency() << 32u);
    entropy_add(&value, sizeof(value), 4u);
}

uint8_t random_is_ready(void)
{
    uint8_t ready;
    spinlock_lock(&random_lock);
    ready = (uint8_t)(entropy_bits >= 256u);
    spinlock_unlock(&random_lock);
    return ready;
}

uint32_t random_entropy_bits(void)
{
    uint32_t bits;
    spinlock_lock(&random_lock);
    bits = entropy_bits;
    spinlock_unlock(&random_lock);
    return bits;
}

void random_bytes(void *output, uint32_t size)
{
    uint8_t *bytes = (uint8_t *)output;
    uint32_t index;
    if (bytes == 0 || size == 0u) return;
    spinlock_lock(&random_lock);
    for (index = 0u; index < size; index++) {
        if (random_available == 0u) generate_block();
        bytes[index] = random_buffer[RANDOM_OUTPUT_BYTES - random_available];
        random_available--;
    }
    spinlock_unlock(&random_lock);
}

uint64_t random_u64(void)
{
    uint64_t value;
    random_bytes(&value, sizeof(value));
    return value;
}

int random_self_test(void)
{
    uint32_t a = 0x11111111u;
    uint32_t b = 0x01020304u;
    uint32_t c = 0x9B8D6F43u;
    uint32_t d = 0x01234567u;
    uint64_t first = random_u64();
    uint64_t second = random_u64();
    quarter(&a, &b, &c, &d);
    if (a != 0xEA2A92F4u || b != 0xCB1CF8CEu ||
        c != 0x4581472Eu || d != 0x5881C4BBu) return -1;
    return first != second && (first != 0u || second != 0u) ? 0 : -1;
}
