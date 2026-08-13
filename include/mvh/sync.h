#ifndef MVH_SYNC_H
#define MVH_SYNC_H

#include <stdint.h>

typedef struct {
    volatile uint32_t value;
} atomic_u32_t;

typedef struct {
    atomic_u32_t state;
} spinlock_t;

typedef struct {
    spinlock_t lock;
} mutex_t;

uint32_t atomic_u32_load(const atomic_u32_t *value);
void atomic_u32_store(atomic_u32_t *value, uint32_t next);
uint32_t atomic_u32_fetch_add(atomic_u32_t *value, uint32_t amount);
uint8_t atomic_u32_compare_exchange(atomic_u32_t *value, uint32_t expected,
                                    uint32_t desired);
void spinlock_init(spinlock_t *lock);
void spinlock_lock(spinlock_t *lock);
uint8_t spinlock_try_lock(spinlock_t *lock);
void spinlock_unlock(spinlock_t *lock);
void mutex_init(mutex_t *mutex);
void mutex_lock(mutex_t *mutex);
uint8_t mutex_try_lock(mutex_t *mutex);
void mutex_unlock(mutex_t *mutex);
int sync_self_test(void);

#endif
