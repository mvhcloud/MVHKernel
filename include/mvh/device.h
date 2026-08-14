#ifndef MVH_DEVICE_H
#define MVH_DEVICE_H

#include <stdint.h>

#define DEVICE_MAX 32u
#define DEVICE_NAME_MAX 28u

typedef enum {
    DEVICE_CPU,
    DEVICE_INTERRUPT,
    DEVICE_TIMER,
    DEVICE_INPUT,
    DEVICE_DISPLAY,
    DEVICE_SERIAL,
    DEVICE_CLOCK,
    DEVICE_BUS,
    DEVICE_FILESYSTEM,
    DEVICE_RANDOM,
    DEVICE_BLOCK
} device_type_t;

typedef struct {
    uint32_t id;
    device_type_t type;
    uint8_t online;
    char name[DEVICE_NAME_MAX];
} device_info_t;

void device_manager_init(void);
int device_register(const char *name, device_type_t type, uint8_t online);
uint32_t device_count(void);
uint32_t device_list(device_info_t *devices, uint32_t capacity);
const char *device_type_name(device_type_t type);

#endif
