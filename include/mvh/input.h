#ifndef MVH_INPUT_H
#define MVH_INPUT_H

#include <stdint.h>

typedef enum {
    INPUT_EVENT_NONE = 0,
    INPUT_EVENT_KEY = 1,
    INPUT_EVENT_ACCEPT = 2,
    INPUT_EVENT_BACK = 3,
    INPUT_EVENT_NEXT = 4
} input_event_type_t;

typedef struct {
    input_event_type_t type;
    uint32_t code;
    char text;
    uint64_t tick;
} input_event_t;

void input_init(void);
int input_poll(input_event_t *event);
uint64_t input_event_count(void);

#endif
