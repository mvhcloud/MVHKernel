#include <stdint.h>
#include "mvh/device.h"
#include "mvh/sync.h"

static device_info_t device_table[DEVICE_MAX];
static uint32_t registered_devices;
static spinlock_t device_lock;

static void name_copy(char *target, const char *source)
{
    uint32_t index = 0u;
    while (source[index] != '\0' && index + 1u < DEVICE_NAME_MAX) {
        target[index] = source[index];
        index++;
    }
    target[index] = '\0';
}

void device_manager_init(void)
{
    registered_devices = 0u;
    spinlock_init(&device_lock);
}

int device_register(const char *name, device_type_t type, uint8_t online)
{
    uint32_t index;
    if (name == 0 || name[0] == '\0' || type > DEVICE_BLOCK) return -1;
    spinlock_lock(&device_lock);
    if (registered_devices >= DEVICE_MAX) {
        spinlock_unlock(&device_lock);
        return -1;
    }
    index = registered_devices++;
    device_table[index].id = index;
    device_table[index].type = type;
    device_table[index].online = online;
    name_copy(device_table[index].name, name);
    spinlock_unlock(&device_lock);
    return (int)index;
}

uint32_t device_count(void)
{
    uint32_t count;
    spinlock_lock(&device_lock);
    count = registered_devices;
    spinlock_unlock(&device_lock);
    return count;
}

uint32_t device_list(device_info_t *devices, uint32_t capacity)
{
    uint32_t count;
    uint32_t index;
    if (devices == 0 || capacity == 0u) return 0u;
    spinlock_lock(&device_lock);
    count = registered_devices < capacity ? registered_devices : capacity;
    for (index = 0u; index < count; index++) {
        devices[index] = device_table[index];
    }
    spinlock_unlock(&device_lock);
    return count;
}

const char *device_type_name(device_type_t type)
{
    static const char *const names[] = {
        "CPU", "IRQ", "TIMER", "INPUT", "DISPLAY",
        "SERIAL", "CLOCK", "BUS", "FILESYSTEM", "RANDOM", "BLOCK"
    };
    return type <= DEVICE_BLOCK ? names[type] : "UNKNOWN";
}
