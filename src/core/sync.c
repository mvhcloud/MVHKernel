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

uint64_t atomic_u64_load(const atomic_u64_t *value)
{
    return __atomic_load_n(&value->value, __ATOMIC_ACQUIRE);
}

void atomic_u64_store(atomic_u64_t *value, uint64_t next)
{
    __atomic_store_n(&value->value, next, __ATOMIC_RELEASE);
}

uint64_t atomic_u64_fetch_add(atomic_u64_t *value, uint64_t amount)
{
    return __atomic_fetch_add(&value->value, amount, __ATOMIC_ACQ_REL);
}

uint8_t atomic_u64_compare_exchange(atomic_u64_t *value, uint64_t expected,
                                    uint64_t desired)
{
    return (uint8_t)__atomic_compare_exchange_n(&value->value, &expected, desired,
                                                 0, __ATOMIC_ACQ_REL,
                                                 __ATOMIC_ACQUIRE);
}

void memory_barrier(void) { __atomic_thread_fence(__ATOMIC_SEQ_CST); }
void memory_read_barrier(void) { __atomic_thread_fence(__ATOMIC_ACQUIRE); }
void memory_write_barrier(void) { __atomic_thread_fence(__ATOMIC_RELEASE); }

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

void ticket_lock_init(ticket_lock_t *lock)
{
    atomic_u32_store(&lock->next, 0u);
    atomic_u32_store(&lock->serving, 0u);
}

void ticket_lock_acquire(ticket_lock_t *lock)
{
    uint32_t ticket = atomic_u32_fetch_add(&lock->next, 1u);
    while (atomic_u32_load(&lock->serving) != ticket) __asm__ volatile ("pause");
}

void ticket_lock_release(ticket_lock_t *lock)
{
    atomic_u32_fetch_add(&lock->serving, 1u);
}

void rwlock_init(rwlock_t *lock)
{
    atomic_u32_store(&lock->readers, 0u);
    atomic_u32_store(&lock->writer, 0u);
}

void rwlock_read_lock(rwlock_t *lock)
{
    for (;;) {
        while (atomic_u32_load(&lock->writer) != 0u) __asm__ volatile ("pause");
        atomic_u32_fetch_add(&lock->readers, 1u);
        if (atomic_u32_load(&lock->writer) == 0u) return;
        atomic_u32_fetch_add(&lock->readers, (uint32_t)-1);
    }
}

void rwlock_read_unlock(rwlock_t *lock)
{
    atomic_u32_fetch_add(&lock->readers, (uint32_t)-1);
}

void rwlock_write_lock(rwlock_t *lock)
{
    while (atomic_u32_compare_exchange(&lock->writer, 0u, 1u) == 0u) {
        __asm__ volatile ("pause");
    }
    while (atomic_u32_load(&lock->readers) != 0u) __asm__ volatile ("pause");
}

void rwlock_write_unlock(rwlock_t *lock)
{
    atomic_u32_store(&lock->writer, 0u);
}

void semaphore_init(semaphore_t *semaphore, uint32_t count)
{
    atomic_u32_store(&semaphore->count, count);
}

uint8_t semaphore_try_wait(semaphore_t *semaphore)
{
    uint32_t count = atomic_u32_load(&semaphore->count);
    while (count != 0u) {
        if (atomic_u32_compare_exchange(&semaphore->count, count, count - 1u) != 0u)
            return 1u;
        count = atomic_u32_load(&semaphore->count);
    }
    return 0u;
}

void semaphore_wait(semaphore_t *semaphore)
{
    while (semaphore_try_wait(semaphore) == 0u) __asm__ volatile ("pause");
}

void semaphore_signal(semaphore_t *semaphore)
{
    atomic_u32_fetch_add(&semaphore->count, 1u);
}

void completion_init(completion_t *completion) { atomic_u32_store(&completion->done, 0u); }
void completion_complete(completion_t *completion) { atomic_u32_fetch_add(&completion->done, 1u); }
void completion_complete_all(completion_t *completion) { atomic_u32_store(&completion->done, UINT32_MAX); }

uint8_t completion_try_wait(completion_t *completion)
{
    uint32_t done = atomic_u32_load(&completion->done);
    while (done != 0u) {
        if (done == UINT32_MAX) return 1u;
        if (atomic_u32_compare_exchange(&completion->done, done, done - 1u) != 0u) return 1u;
        done = atomic_u32_load(&completion->done);
    }
    return 0u;
}

void completion_wait(completion_t *completion)
{
    while (completion_try_wait(completion) == 0u) __asm__ volatile ("pause");
}

void condition_init(condition_t *condition) { atomic_u32_store(&condition->sequence, 0u); }
uint32_t condition_snapshot(const condition_t *condition) { return atomic_u32_load(&condition->sequence); }
void condition_signal(condition_t *condition) { atomic_u32_fetch_add(&condition->sequence, 1u); }

void condition_wait(condition_t *condition, uint32_t snapshot)
{
    while (atomic_u32_load(&condition->sequence) == snapshot) __asm__ volatile ("pause");
}

void seqlock_init(seqlock_t *lock)
{
    atomic_u32_store(&lock->sequence, 0u);
    spinlock_init(&lock->writer);
}

void seqlock_write_lock(seqlock_t *lock)
{
    spinlock_lock(&lock->writer);
    atomic_u32_fetch_add(&lock->sequence, 1u);
    memory_write_barrier();
}

void seqlock_write_unlock(seqlock_t *lock)
{
    memory_write_barrier();
    atomic_u32_fetch_add(&lock->sequence, 1u);
    spinlock_unlock(&lock->writer);
}

uint32_t seqlock_read_begin(const seqlock_t *lock)
{
    uint32_t sequence;
    do {
        sequence = atomic_u32_load(&lock->sequence);
        if ((sequence & 1u) != 0u) __asm__ volatile ("pause");
    } while ((sequence & 1u) != 0u);
    memory_read_barrier();
    return sequence;
}

uint8_t seqlock_read_retry(const seqlock_t *lock, uint32_t sequence)
{
    memory_read_barrier();
    return atomic_u32_load(&lock->sequence) != sequence;
}

int sync_self_test(void)
{
    atomic_u32_t value;
    spinlock_t spinlock;
    mutex_t mutex;
    ticket_lock_t ticket;
    rwlock_t rwlock;
    atomic_u64_t value64;
    semaphore_t semaphore;
    completion_t completion;
    condition_t condition;
    seqlock_t sequence;
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
    ticket_lock_init(&ticket);
    ticket_lock_acquire(&ticket);
    if (atomic_u32_load(&ticket.serving) != 0u) return -1;
    ticket_lock_release(&ticket);
    if (atomic_u32_load(&ticket.serving) != 1u) return -1;
    rwlock_init(&rwlock);
    rwlock_read_lock(&rwlock);
    if (atomic_u32_load(&rwlock.readers) != 1u) return -1;
    rwlock_read_unlock(&rwlock);
    rwlock_write_lock(&rwlock);
    if (atomic_u32_load(&rwlock.writer) != 1u) return -1;
    rwlock_write_unlock(&rwlock);
    atomic_u64_store(&value64, 8u);
    if (atomic_u64_fetch_add(&value64, 2u) != 8u || atomic_u64_load(&value64) != 10u ||
        atomic_u64_compare_exchange(&value64, 10u, 12u) == 0u) return -1;
    semaphore_init(&semaphore, 1u);
    if (semaphore_try_wait(&semaphore) == 0u || semaphore_try_wait(&semaphore) != 0u) return -1;
    semaphore_signal(&semaphore);
    completion_init(&completion);
    completion_complete(&completion);
    if (completion_try_wait(&completion) == 0u) return -1;
    condition_init(&condition);
    condition_signal(&condition);
    if (condition_snapshot(&condition) != 1u) return -1;
    seqlock_init(&sequence);
    seqlock_write_lock(&sequence);
    seqlock_write_unlock(&sequence);
    if (seqlock_read_begin(&sequence) != 2u) return -1;
    return 0;
}
