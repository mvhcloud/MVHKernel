#ifndef MVH_SYNC_H
#define MVH_SYNC_H

#include <stdint.h>

typedef struct {
    volatile uint32_t value;
} atomic_u32_t;

typedef struct {
    volatile uint64_t value;
} atomic_u64_t;

typedef struct {
    atomic_u32_t state;
} spinlock_t;

typedef struct {
    spinlock_t lock;
} mutex_t;

typedef struct {
    atomic_u32_t next;
    atomic_u32_t serving;
} ticket_lock_t;

typedef struct {
    atomic_u32_t readers;
    atomic_u32_t writer;
} rwlock_t;

typedef struct {
    atomic_u32_t count;
} semaphore_t;

typedef struct {
    atomic_u32_t done;
} completion_t;

typedef struct {
    atomic_u32_t sequence;
} condition_t;

typedef struct {
    atomic_u32_t sequence;
    spinlock_t writer;
} seqlock_t;

uint32_t atomic_u32_load(const atomic_u32_t *value);
void atomic_u32_store(atomic_u32_t *value, uint32_t next);
uint32_t atomic_u32_fetch_add(atomic_u32_t *value, uint32_t amount);
uint8_t atomic_u32_compare_exchange(atomic_u32_t *value, uint32_t expected,
                                    uint32_t desired);
uint64_t atomic_u64_load(const atomic_u64_t *value);
void atomic_u64_store(atomic_u64_t *value, uint64_t next);
uint64_t atomic_u64_fetch_add(atomic_u64_t *value, uint64_t amount);
uint8_t atomic_u64_compare_exchange(atomic_u64_t *value, uint64_t expected,
                                    uint64_t desired);
void memory_barrier(void);
void memory_read_barrier(void);
void memory_write_barrier(void);
void spinlock_init(spinlock_t *lock);
void spinlock_lock(spinlock_t *lock);
uint8_t spinlock_try_lock(spinlock_t *lock);
void spinlock_unlock(spinlock_t *lock);
void mutex_init(mutex_t *mutex);
void mutex_lock(mutex_t *mutex);
uint8_t mutex_try_lock(mutex_t *mutex);
void mutex_unlock(mutex_t *mutex);
void ticket_lock_init(ticket_lock_t *lock);
void ticket_lock_acquire(ticket_lock_t *lock);
void ticket_lock_release(ticket_lock_t *lock);
void rwlock_init(rwlock_t *lock);
void rwlock_read_lock(rwlock_t *lock);
void rwlock_read_unlock(rwlock_t *lock);
void rwlock_write_lock(rwlock_t *lock);
void rwlock_write_unlock(rwlock_t *lock);
void semaphore_init(semaphore_t *semaphore, uint32_t count);
uint8_t semaphore_try_wait(semaphore_t *semaphore);
void semaphore_wait(semaphore_t *semaphore);
void semaphore_signal(semaphore_t *semaphore);
void completion_init(completion_t *completion);
void completion_complete(completion_t *completion);
void completion_complete_all(completion_t *completion);
uint8_t completion_try_wait(completion_t *completion);
void completion_wait(completion_t *completion);
void condition_init(condition_t *condition);
uint32_t condition_snapshot(const condition_t *condition);
void condition_signal(condition_t *condition);
void condition_wait(condition_t *condition, uint32_t snapshot);
void seqlock_init(seqlock_t *lock);
void seqlock_write_lock(seqlock_t *lock);
void seqlock_write_unlock(seqlock_t *lock);
uint32_t seqlock_read_begin(const seqlock_t *lock);
uint8_t seqlock_read_retry(const seqlock_t *lock, uint32_t sequence);
int sync_self_test(void);

#endif
