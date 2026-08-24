#ifndef MVH_TASK_H
#define MVH_TASK_H

#include <stdint.h>

#define TASK_MAX 64u
#define TASK_NAME_MAX 24u
#define TASK_AFFINITY_ALL UINT64_MAX

typedef enum {
    TASK_UNUSED = 0, TASK_RUNNING, TASK_READY, TASK_SLEEPING,
    TASK_BLOCKED, TASK_STOPPED, TASK_ZOMBIE
} task_state_t;

typedef struct {
    uint32_t pid;
    uint32_t tid;
    uint32_t cpu_id;
    uint8_t priority;
    task_state_t state;
    uint64_t affinity;
    uint64_t created_tick;
    uint64_t runtime_ticks;
    uint64_t wake_tick;
    uint64_t context_switches;
    char name[TASK_NAME_MAX];
} task_info_t;

typedef struct {
    uint64_t context_switches;
    uint64_t yields;
    uint64_t created;
    uint64_t reaped;
    uint32_t running;
    uint32_t ready;
    uint32_t sleeping;
    uint32_t blocked;
    uint32_t zombies;
} scheduler_stats_t;

typedef void (*kernel_thread_entry_t)(void *argument);

void task_init(uint64_t tick);
int task_create_kernel(const char *name, kernel_thread_entry_t entry, void *argument,
                       uint8_t priority, uint64_t affinity);
int task_reap(uint32_t pid);
void task_yield(void);
void task_sleep_until(uint64_t wake_tick);
int task_wake(uint32_t pid);
int task_block(uint32_t pid);
int task_set_affinity(uint32_t pid, uint64_t affinity);
void task_scheduler_tick(uint64_t tick);
uint32_t task_current_pid(void);
uint32_t task_count(void);
uint32_t task_list(task_info_t *tasks, uint32_t capacity);
void task_scheduler_stats(scheduler_stats_t *stats);
const char *task_state_name(task_state_t state);
int task_self_test(void);

#endif
