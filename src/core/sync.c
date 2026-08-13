#include <stdint.h>
#include "mvh/sync.h"

uint32_t atomic_u32_load(const atomic_u32_t *value)
{
    return __atomic_load_n(&value->value, __ATOMIC_ACQUIRE);
}

void atomic_u32_store(atomic_u32_t *value, uint32_t next)
{
    __atomic_store_n(&value->value, next, __ATOMIC_RELEASE);
}

uint32_t atomic_u32_fetch_add(atomic_u32_t *value, uint32_t amount)
{
    return __atomic_fetch_add(&value->value, amount, __ATOMIC_ACQ_REL);
}

uint8_t atomic_u32_compare_exchange(atomic_u32_t *value, uint32_t expected,
                                    uint32_t desired)
{
    return (uint8_t)__atomic_compare_exchange_n(&value->value, &expected, desired,
                                                 0, __ATOMIC_ACQ_REL,
                                                 __ATOMIC_ACQUIRE);
}

void spinlock_init(spinlock_t *lock)
{
    atomic_u32_store(&lock->state, 0u);
}

void spinlock_lock(spinlock_t *lock)
{
    while (atomic_u32_compare_exchange(&lock->state, 0u, 1u) == 0u) {
        while (atomic_u32_load(&lock->state) != 0u) {
            __asm__ volatile ("pause");
        }
    }
}

uint8_t spinlock_try_lock(spinlock_t *lock)
{
    return atomic_u32_compare_exchange(&lock->state, 0u, 1u);
}

void spinlock_unlock(spinlock_t *lock)
{
    atomic_u32_store(&lock->state, 0u);
}

void mutex_init(mutex_t *mutex)
{
    spinlock_init(&mutex->lock);
}

void mutex_lock(mutex_t *mutex)
{
    spinlock_lock(&mutex->lock);
}

uint8_t mutex_try_lock(mutex_t *mutex)
{
    return spinlock_try_lock(&mutex->lock);
}

void mutex_unlock(mutex_t *mutex)
{
    spinlock_unlock(&mutex->lock);
}

int sync_self_test(void)
{
    atomic_u32_t value;
    spinlock_t spinlock;
    mutex_t mutex;
    atomic_u32_store(&value, 4u);
    if (atomic_u32_fetch_add(&value, 3u) != 4u || atomic_u32_load(&value) != 7u) {
        return -1;
    }
    if (atomic_u32_compare_exchange(&value, 7u, 9u) == 0u ||
        atomic_u32_load(&value) != 9u) {
        return -1;
    }
    spinlock_init(&spinlock);
    if (spinlock_try_lock(&spinlock) == 0u || spinlock_try_lock(&spinlock) != 0u) {
        return -1;
    }
    spinlock_unlock(&spinlock);
    mutex_init(&mutex);
    if (mutex_try_lock(&mutex) == 0u || mutex_try_lock(&mutex) != 0u) {
        return -1;
    }
    mutex_unlock(&mutex);
    return 0;
}
