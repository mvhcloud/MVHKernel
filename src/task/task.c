#include <stdint.h>
#include "mvh/task.h"

static task_info_t task_table[TASK_MAX];

static void task_name_copy(char *target, const char *source)
{
    uint32_t index = 0u;
    while (source[index] != '\0' && index + 1u < TASK_NAME_MAX) {
        target[index] = source[index];
        index++;
    }
    target[index] = '\0';
}

void task_init(uint64_t tick)
{
    uint32_t index;
    for (index = 0u; index < TASK_MAX; index++) {
        task_table[index].state = TASK_UNUSED;
    }
    task_table[0].pid = 1u;
    task_table[0].priority = 10u;
    task_table[0].state = TASK_RUNNING;
    task_table[0].created_tick = tick;
    task_name_copy(task_table[0].name, "kernel-shell");
}

uint32_t task_count(void)
{
    uint32_t count = 0u;
    uint32_t index;
    for (index = 0u; index < TASK_MAX; index++) {
        if (task_table[index].state != TASK_UNUSED) {
            count++;
        }
    }
    return count;
}

uint32_t task_list(task_info_t *tasks, uint32_t capacity)
{
    uint32_t count = 0u;
    uint32_t index;
    for (index = 0u; index < TASK_MAX && count < capacity; index++) {
        if (task_table[index].state != TASK_UNUSED) {
            tasks[count++] = task_table[index];
        }
    }
    return count;
}

const char *task_state_name(task_state_t state)
{
    if (state == TASK_RUNNING) {
        return "RUNNING";
    }
    if (state == TASK_READY) {
        return "READY";
    }
    if (state == TASK_SLEEPING) {
        return "SLEEPING";
    }
    if (state == TASK_STOPPED) {
        return "STOPPED";
    }
    return "UNUSED";
}
