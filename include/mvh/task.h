#ifndef MVH_TASK_H
#define MVH_TASK_H

#include <stdint.h>

#define TASK_MAX 16u
#define TASK_NAME_MAX 24u

typedef enum {
    TASK_UNUSED = 0,
    TASK_RUNNING = 1,
    TASK_READY = 2,
    TASK_SLEEPING = 3,
    TASK_STOPPED = 4
} task_state_t;

typedef struct {
    uint32_t pid;
    uint8_t priority;
    task_state_t state;
    uint64_t created_tick;
    char name[TASK_NAME_MAX];
} task_info_t;

void task_init(uint64_t tick);
uint32_t task_count(void);
uint32_t task_list(task_info_t *tasks, uint32_t capacity);
const char *task_state_name(task_state_t state);

#endif
