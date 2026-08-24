#include <stdint.h>
#include "mvh/memory.h"
#include "mvh/smp.h"
#include "mvh/sync.h"
#include "mvh/task.h"

#define TASK_STACK_PAGES 4u
#define TASK_NO_CURRENT UINT32_MAX

typedef struct {
    uintptr_t rsp;
    uint64_t rbx, rbp, r12, r13, r14, r15;
} task_context_t;

typedef struct {
    task_info_t info;
    task_context_t context;
    kernel_thread_entry_t entry;
    void *argument;
    void *stack_base;
    uint32_t stack_pages;
} task_control_t;

extern void task_context_switch(task_context_t *previous, const task_context_t *next);

static task_control_t tasks[TASK_MAX];
static uint32_t current_task[SMP_MAX_CPUS];
static uint32_t next_pid;
static uint32_t round_robin_cursor[SMP_MAX_CPUS];
static spinlock_t scheduler_lock;
static scheduler_stats_t scheduler_stats;
static volatile uint32_t self_test_ran;

static void clear_bytes(void *address, uint32_t size)
{
    uint8_t *bytes = (uint8_t *)address;
    uint32_t index;
    for (index = 0u; index < size; index++) bytes[index] = 0u;
}

static void copy_name(char *target, const char *source)
{
    uint32_t index = 0u;
    while (source[index] != '\0' && index + 1u < TASK_NAME_MAX) {
        target[index] = source[index];
        index++;
    }
    target[index] = '\0';
}

static uint32_t local_cpu_id(void)
{
    const smp_cpu_t *cpu = smp_current_cpu();
    return cpu != 0 && cpu->logical_id < SMP_MAX_CPUS ? cpu->logical_id : 0u;
}

static int index_by_pid(uint32_t pid)
{
    uint32_t index;
    for (index = 0u; index < TASK_MAX; index++)
        if (tasks[index].info.state != TASK_UNUSED && tasks[index].info.pid == pid)
            return (int)index;
    return -1;
}

static uint32_t choose_next(uint32_t cpu_id, uint32_t previous)
{
    uint32_t offset;
    uint32_t selected = previous;
    uint8_t best_priority = 0u;
    for (offset = 1u; offset <= TASK_MAX; offset++) {
        uint32_t index = (round_robin_cursor[cpu_id] + offset) % TASK_MAX;
        task_info_t *info = &tasks[index].info;
        if (info->state == TASK_READY && (info->affinity & (1ull << cpu_id)) != 0u &&
            (selected == previous || info->priority > best_priority)) {
            selected = index;
            best_priority = info->priority;
        }
    }
    return selected;
}

static void task_bootstrap(void)
{
    uint32_t cpu_id = local_cpu_id();
    uint32_t index = current_task[cpu_id];
    tasks[index].entry(tasks[index].argument);
    spinlock_lock(&scheduler_lock);
    tasks[index].info.state = TASK_ZOMBIE;
    scheduler_stats.zombies++;
    scheduler_stats.running--;
    spinlock_unlock(&scheduler_lock);
    for (;;) task_yield();
}

void task_init(uint64_t tick)
{
    uint32_t index;
    spinlock_init(&scheduler_lock);
    clear_bytes(tasks, sizeof(tasks));
    clear_bytes(&scheduler_stats, sizeof(scheduler_stats));
    for (index = 0u; index < SMP_MAX_CPUS; index++) {
        current_task[index] = TASK_NO_CURRENT;
        round_robin_cursor[index] = 0u;
    }
    tasks[0].info.pid = 1u;
    tasks[0].info.tid = 1u;
    tasks[0].info.cpu_id = 0u;
    tasks[0].info.priority = 10u;
    tasks[0].info.state = TASK_RUNNING;
    tasks[0].info.affinity = 1u;
    tasks[0].info.created_tick = tick;
    copy_name(tasks[0].info.name, "kernel-shell");
    current_task[0] = 0u;
    next_pid = 2u;
    scheduler_stats.created = 1u;
    scheduler_stats.running = 1u;
}

int task_create_kernel(const char *name, kernel_thread_entry_t entry, void *argument,
                       uint8_t priority, uint64_t affinity)
{
    uint32_t index;
    void *stack;
    uintptr_t top;
    if (name == 0 || name[0] == '\0' || entry == 0 || affinity == 0u) return -1;
    stack = pmm_alloc_pages(TASK_STACK_PAGES);
    if (stack == 0) return -1;
    spinlock_lock(&scheduler_lock);
    for (index = 1u; index < TASK_MAX; index++)
        if (tasks[index].info.state == TASK_UNUSED) break;
    if (index == TASK_MAX) {
        spinlock_unlock(&scheduler_lock);
        pmm_free_pages(stack, TASK_STACK_PAGES);
        return -1;
    }
    clear_bytes(&tasks[index], sizeof(tasks[index]));
    tasks[index].info.pid = next_pid++;
    tasks[index].info.tid = tasks[index].info.pid;
    tasks[index].info.cpu_id = UINT32_MAX;
    tasks[index].info.priority = priority;
    tasks[index].info.state = TASK_READY;
    tasks[index].info.affinity = affinity;
    copy_name(tasks[index].info.name, name);
    tasks[index].entry = entry;
    tasks[index].argument = argument;
    tasks[index].stack_base = stack;
    tasks[index].stack_pages = TASK_STACK_PAGES;
    top = (uintptr_t)stack + TASK_STACK_PAGES * 4096u - 16u;
    *(uintptr_t *)top = (uintptr_t)task_bootstrap;
    tasks[index].context.rsp = top;
    scheduler_stats.created++;
    scheduler_stats.ready++;
    spinlock_unlock(&scheduler_lock);
    return (int)tasks[index].info.pid;
}

void task_yield(void)
{
    uint32_t cpu_id = local_cpu_id();
    uint32_t previous = current_task[cpu_id];
    uint32_t next;
    if (previous == TASK_NO_CURRENT) return;
    spinlock_lock(&scheduler_lock);
    if (tasks[previous].info.state == TASK_RUNNING) {
        tasks[previous].info.state = TASK_READY;
        scheduler_stats.running--;
        scheduler_stats.ready++;
    }
    next = choose_next(cpu_id, previous);
    if (next == previous) {
        tasks[previous].info.state = TASK_RUNNING;
        scheduler_stats.ready--;
        scheduler_stats.running++;
        spinlock_unlock(&scheduler_lock);
        return;
    }
    tasks[next].info.state = TASK_RUNNING;
    tasks[next].info.cpu_id = cpu_id;
    tasks[next].info.context_switches++;
    scheduler_stats.ready--;
    scheduler_stats.running++;
    scheduler_stats.context_switches++;
    scheduler_stats.yields++;
    current_task[cpu_id] = next;
    round_robin_cursor[cpu_id] = next;
    spinlock_unlock(&scheduler_lock);
    task_context_switch(&tasks[previous].context, &tasks[next].context);
}

void task_sleep_until(uint64_t wake_tick)
{
    uint32_t index = current_task[local_cpu_id()];
    if (index == TASK_NO_CURRENT) return;
    spinlock_lock(&scheduler_lock);
    tasks[index].info.state = TASK_SLEEPING;
    tasks[index].info.wake_tick = wake_tick;
    scheduler_stats.running--;
    scheduler_stats.sleeping++;
    spinlock_unlock(&scheduler_lock);
    task_yield();
}

int task_wake(uint32_t pid)
{
    int index;
    spinlock_lock(&scheduler_lock);
    index = index_by_pid(pid);
    if (index < 0 || (tasks[index].info.state != TASK_SLEEPING && tasks[index].info.state != TASK_BLOCKED)) {
        spinlock_unlock(&scheduler_lock);
        return -1;
    }
    if (tasks[index].info.state == TASK_SLEEPING) scheduler_stats.sleeping--;
    else scheduler_stats.blocked--;
    tasks[index].info.state = TASK_READY;
    scheduler_stats.ready++;
    spinlock_unlock(&scheduler_lock);
    return 0;
}

int task_block(uint32_t pid)
{
    int index;
    spinlock_lock(&scheduler_lock);
    index = index_by_pid(pid);
    if (index < 0 || tasks[index].info.state != TASK_READY) {
        spinlock_unlock(&scheduler_lock);
        return -1;
    }
    tasks[index].info.state = TASK_BLOCKED;
    scheduler_stats.ready--;
    scheduler_stats.blocked++;
    spinlock_unlock(&scheduler_lock);
    return 0;
}

int task_set_affinity(uint32_t pid, uint64_t affinity)
{
    int index;
    if (affinity == 0u) return -1;
    spinlock_lock(&scheduler_lock);
    index = index_by_pid(pid);
    if (index >= 0) tasks[index].info.affinity = affinity;
    spinlock_unlock(&scheduler_lock);
    return index >= 0 ? 0 : -1;
}

void task_scheduler_tick(uint64_t tick)
{
    uint32_t index;
    spinlock_lock(&scheduler_lock);
    for (index = 0u; index < TASK_MAX; index++) {
        if (tasks[index].info.state == TASK_RUNNING) tasks[index].info.runtime_ticks++;
        if (tasks[index].info.state == TASK_SLEEPING && tasks[index].info.wake_tick <= tick) {
            tasks[index].info.state = TASK_READY;
            scheduler_stats.sleeping--;
            scheduler_stats.ready++;
        }
    }
    spinlock_unlock(&scheduler_lock);
}

uint32_t task_current_pid(void)
{
    uint32_t index = current_task[local_cpu_id()];
    return index != TASK_NO_CURRENT ? tasks[index].info.pid : 0u;
}

int task_reap(uint32_t pid)
{
    int index;
    void *stack;
    uint32_t pages;
    spinlock_lock(&scheduler_lock);
    index = index_by_pid(pid);
    if (index < 0 || tasks[index].info.state != TASK_ZOMBIE) {
        spinlock_unlock(&scheduler_lock);
        return -1;
    }
    stack = tasks[index].stack_base;
    pages = tasks[index].stack_pages;
    clear_bytes(&tasks[index], sizeof(tasks[index]));
    scheduler_stats.zombies--;
    scheduler_stats.reaped++;
    spinlock_unlock(&scheduler_lock);
    pmm_free_pages(stack, pages);
    return 0;
}

uint32_t task_count(void)
{
    uint32_t count = 0u, index;
    spinlock_lock(&scheduler_lock);
    for (index = 0u; index < TASK_MAX; index++)
        if (tasks[index].info.state != TASK_UNUSED) count++;
    spinlock_unlock(&scheduler_lock);
    return count;
}

uint32_t task_list(task_info_t *result, uint32_t capacity)
{
    uint32_t count = 0u, index;
    if (result == 0 || capacity == 0u) return 0u;
    spinlock_lock(&scheduler_lock);
    for (index = 0u; index < TASK_MAX && count < capacity; index++)
        if (tasks[index].info.state != TASK_UNUSED) result[count++] = tasks[index].info;
    spinlock_unlock(&scheduler_lock);
    return count;
}

void task_scheduler_stats(scheduler_stats_t *stats)
{
    if (stats == 0) return;
    spinlock_lock(&scheduler_lock);
    *stats = scheduler_stats;
    spinlock_unlock(&scheduler_lock);
}

const char *task_state_name(task_state_t state)
{
    static const char *const names[] = {
        "UNUSED", "RUNNING", "READY", "SLEEPING", "BLOCKED", "STOPPED", "ZOMBIE"
    };
    return state <= TASK_ZOMBIE ? names[state] : "INVALID";
}

static void self_test_entry(void *argument)
{
    *(volatile uint32_t *)argument = 0x4D564853u;
}

int task_self_test(void)
{
    int pid;
    self_test_ran = 0u;
    pid = task_create_kernel("scheduler-test", self_test_entry, (void *)&self_test_ran, 10u, 1u);
    if (pid < 0) return -1;
    task_yield();
    if (self_test_ran != 0x4D564853u || task_reap((uint32_t)pid) != 0) return -1;
    return scheduler_stats.context_switches >= 2u ? 0 : -1;
}
