#include <stdint.h>
#include "mvh/hal.h"
#include "mvh/input.h"

static uint64_t events;

void input_init(void) { events = 0u; }

int input_poll(input_event_t *event)
{
    char value;
    if (event == 0) return -1;
    value = hal_keyboard_read();
    event->type = INPUT_EVENT_KEY;
    event->code = (uint8_t)value;
    event->text = value;
    event->tick = hal_ticks();
    if (value == '\n') event->type = INPUT_EVENT_ACCEPT;
    else if (value == 27 || value == '\b') event->type = INPUT_EVENT_BACK;
    else if (value == '\t' || value == ' ') event->type = INPUT_EVENT_NEXT;
    events++;
    return 0;
}

uint64_t input_event_count(void) { return events; }
