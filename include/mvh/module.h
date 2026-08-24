#ifndef MVH_MODULE_H
#define MVH_MODULE_H

#include <stdint.h>

#define MVH_MODULE_NAME_MAX 48u
#define MVH_MODULE_VERSION_MAX 24u
#define MVH_MODULE_MAX_DEPENDENCIES 8u
#define MVH_MODULE_BUILTIN (1u << 0u)
#define MVH_MODULE_AUTOSTART (1u << 1u)

typedef enum {
    MVH_MODULE_UNLOADED = 0,
    MVH_MODULE_REGISTERED,
    MVH_MODULE_LOADING,
    MVH_MODULE_LIVE,
    MVH_MODULE_FAILED,
    MVH_MODULE_UNLOADING
} mvh_module_state_t;

typedef int (*mvh_module_init_fn)(void);
typedef void (*mvh_module_exit_fn)(void);

typedef struct {
    uint32_t structure_size;
    uint32_t kernel_abi;
    const char *name;
    const char *version;
    uint32_t flags;
    uint32_t dependency_count;
    const char *const *dependencies;
    mvh_module_init_fn init;
    mvh_module_exit_fn exit;
} mvh_module_descriptor_t;

typedef struct {
    char name[MVH_MODULE_NAME_MAX];
    char version[MVH_MODULE_VERSION_MAX];
    mvh_module_state_t state;
    uint32_t flags;
    uint32_t references;
    uint32_t dependency_count;
    uint64_t image_size;
} mvh_module_info_t;

void module_init(void);
int module_register(const mvh_module_descriptor_t *descriptor);
int module_register_builtin(const mvh_module_descriptor_t *descriptor);
int module_load(const char *name);
int module_unload(const char *name);
int module_acquire(const char *name);
void module_release(const char *name);
uint32_t module_count(void);
uint32_t module_snapshot(mvh_module_info_t *output, uint32_t capacity);
const char *module_state_name(mvh_module_state_t state);
int module_export_symbol(const char *name, uintptr_t address, const char *owner);
uintptr_t module_resolve_symbol(const char *name);
int module_elf_load(const void *image, uint64_t size, const char **loaded_name);
int module_self_test(void);

#endif
