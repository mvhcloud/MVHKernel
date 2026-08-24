#include <stdint.h>
#include "mvh/memory.h"
#include "mvh/module.h"
#include "mvh/sync.h"
#include "mvh/util.h"
#include "mvh/version.h"

#define MODULE_LIMIT 32u
#define EXPORT_LIMIT 128u
#define ELF_SECTION_LIMIT 128u
#define ELF_MAGIC 0x464C457Fu
#define ELFCLASS64 2u
#define ELFDATA2LSB 1u
#define ET_REL 1u
#define EM_X86_64 62u
#define SHT_SYMTAB 2u
#define SHT_RELA 4u
#define SHT_NOBITS 8u
#define SHF_ALLOC 2ull
#define SHN_UNDEF 0u
#define SHN_ABS 0xFFF1u
#define R_X86_64_NONE 0u
#define R_X86_64_64 1u
#define R_X86_64_PC32 2u
#define R_X86_64_PLT32 4u
#define R_X86_64_32 10u
#define R_X86_64_32S 11u

typedef struct {
    const mvh_module_descriptor_t *descriptor;
    mvh_module_state_t state;
    uint32_t references;
    void *image;
    uint64_t image_size;
    uint8_t owned;
} module_slot_t;

typedef struct {
    const char *name;
    uintptr_t address;
    const char *owner;
} export_slot_t;

typedef struct {
    uint32_t magic;
    uint8_t class_type;
    uint8_t data;
    uint8_t ident_version;
    uint8_t osabi;
    uint8_t abi_version;
    uint8_t padding[7];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t program_offset;
    uint64_t section_offset;
    uint32_t flags;
    uint16_t header_size;
    uint16_t program_entry_size;
    uint16_t program_count;
    uint16_t section_entry_size;
    uint16_t section_count;
    uint16_t section_names;
} elf64_header_t;

typedef struct {
    uint32_t name;
    uint32_t type;
    uint64_t flags;
    uint64_t address;
    uint64_t offset;
    uint64_t size;
    uint32_t link;
    uint32_t info;
    uint64_t alignment;
    uint64_t entry_size;
} elf64_section_t;

typedef struct {
    uint32_t name;
    uint8_t info;
    uint8_t other;
    uint16_t section;
    uint64_t value;
    uint64_t size;
} elf64_symbol_t;

typedef struct {
    uint64_t offset;
    uint64_t info;
    int64_t addend;
} elf64_rela_t;

static module_slot_t modules[MODULE_LIMIT];
static export_slot_t exports[EXPORT_LIMIT];
static spinlock_t module_lock;
static uint32_t registered_modules;
static uint32_t registered_exports;

static int range_valid(uint64_t offset, uint64_t length, uint64_t total)
{
    return offset <= total && length <= total - offset;
}

static int allocation_range_valid(const void *pointer, uint64_t length,
                                  const void *allocation, uint64_t allocation_size)
{
    uintptr_t value = (uintptr_t)pointer;
    uintptr_t base = (uintptr_t)allocation;
    return value >= base && length <= allocation_size && value - base <= allocation_size - length;
}

static module_slot_t *find_slot(const char *name)
{
    uint32_t index;
    for (index = 0u; index < registered_modules; index++)
        if (mvh_str_equal(modules[index].descriptor->name, name)) return &modules[index];
    return 0;
}

static int descriptor_valid(const mvh_module_descriptor_t *descriptor)
{
    uint32_t index;
    if (descriptor == 0 || descriptor->structure_size != sizeof(*descriptor) ||
        descriptor->kernel_abi != MVH_KERNEL_ABI_VERSION || descriptor->name == 0 ||
        descriptor->version == 0 || descriptor->name[0] == '\0' ||
        mvh_str_nlength(descriptor->name, MVH_MODULE_NAME_MAX) >= MVH_MODULE_NAME_MAX ||
        mvh_str_nlength(descriptor->version, MVH_MODULE_VERSION_MAX) >= MVH_MODULE_VERSION_MAX ||
        descriptor->dependency_count > MVH_MODULE_MAX_DEPENDENCIES ||
        (descriptor->dependency_count != 0u && descriptor->dependencies == 0)) return 0;
    for (index = 0u; index < descriptor->dependency_count; index++)
        if (descriptor->dependencies[index] == 0 || descriptor->dependencies[index][0] == '\0' ||
            mvh_str_equal(descriptor->dependencies[index], descriptor->name)) return 0;
    return 1;
}

void module_init(void)
{
    mvh_mem_zero(modules, sizeof(modules));
    mvh_mem_zero(exports, sizeof(exports));
    spinlock_init(&module_lock);
    registered_modules = 0u;
    registered_exports = 0u;
}

static int register_descriptor(const mvh_module_descriptor_t *descriptor, void *image,
                               uint64_t image_size, uint8_t owned, uint8_t live)
{
    module_slot_t *slot;
    if (!descriptor_valid(descriptor)) return -1;
    spinlock_lock(&module_lock);
    if (find_slot(descriptor->name) != 0 || registered_modules == MODULE_LIMIT) {
        spinlock_unlock(&module_lock);
        return -1;
    }
    slot = &modules[registered_modules++];
    slot->descriptor = descriptor;
    slot->state = live != 0u ? MVH_MODULE_LIVE : MVH_MODULE_REGISTERED;
    slot->references = 0u;
    slot->image = image;
    slot->image_size = image_size;
    slot->owned = owned;
    spinlock_unlock(&module_lock);
    return 0;
}

int module_register(const mvh_module_descriptor_t *descriptor)
{
    return register_descriptor(descriptor, 0, 0u, 0u, 0u);
}

int module_register_builtin(const mvh_module_descriptor_t *descriptor)
{
    if (descriptor == 0 || (descriptor->flags & MVH_MODULE_BUILTIN) == 0u) return -1;
    return register_descriptor(descriptor, 0, 0u, 0u, 1u);
}

int module_load(const char *name)
{
    module_slot_t *slot;
    const mvh_module_descriptor_t *descriptor;
    uint32_t index;
    int result = 0;
    spinlock_lock(&module_lock);
    slot = find_slot(name);
    if (slot == 0 || slot->state == MVH_MODULE_LOADING || slot->state == MVH_MODULE_UNLOADING) {
        spinlock_unlock(&module_lock);
        return -1;
    }
    if (slot->state == MVH_MODULE_LIVE) {
        spinlock_unlock(&module_lock);
        return 0;
    }
    slot->state = MVH_MODULE_LOADING;
    descriptor = slot->descriptor;
    spinlock_unlock(&module_lock);
    for (index = 0u; index < descriptor->dependency_count; index++) {
        if (module_load(descriptor->dependencies[index]) != 0 ||
            module_acquire(descriptor->dependencies[index]) != 0) {
            result = -1;
            break;
        }
    }
    if (result == 0 && descriptor->init != 0) result = descriptor->init();
    if (result != 0) {
        while (index != 0u) module_release(descriptor->dependencies[--index]);
    }
    spinlock_lock(&module_lock);
    slot->state = result == 0 ? MVH_MODULE_LIVE : MVH_MODULE_FAILED;
    spinlock_unlock(&module_lock);
    return result;
}

int module_unload(const char *name)
{
    module_slot_t *slot;
    const mvh_module_descriptor_t *descriptor;
    void *image;
    uint32_t index;
    uint32_t position;
    uint8_t owned;
    spinlock_lock(&module_lock);
    slot = find_slot(name);
    if (slot == 0 || slot->state != MVH_MODULE_LIVE || slot->references != 0u ||
        (slot->descriptor->flags & MVH_MODULE_BUILTIN) != 0u) {
        spinlock_unlock(&module_lock);
        return -1;
    }
    slot->state = MVH_MODULE_UNLOADING;
    descriptor = slot->descriptor;
    image = slot->image;
    owned = slot->owned;
    position = (uint32_t)(slot - modules);
    spinlock_unlock(&module_lock);
    if (descriptor->exit != 0) descriptor->exit();
    for (index = 0u; index < descriptor->dependency_count; index++)
        module_release(descriptor->dependencies[index]);
    spinlock_lock(&module_lock);
    for (index = position + 1u; index < registered_modules; index++) modules[index - 1u] = modules[index];
    registered_modules--;
    mvh_mem_zero(&modules[registered_modules], sizeof(modules[0]));
    spinlock_unlock(&module_lock);
    if (owned != 0u) kfree(image);
    return 0;
}

int module_acquire(const char *name)
{
    module_slot_t *slot;
    spinlock_lock(&module_lock);
    slot = find_slot(name);
    if (slot == 0 || slot->state != MVH_MODULE_LIVE || slot->references == UINT32_MAX) {
        spinlock_unlock(&module_lock);
        return -1;
    }
    slot->references++;
    spinlock_unlock(&module_lock);
    return 0;
}

void module_release(const char *name)
{
    module_slot_t *slot;
    spinlock_lock(&module_lock);
    slot = find_slot(name);
    if (slot != 0 && slot->references != 0u) slot->references--;
    spinlock_unlock(&module_lock);
}

uint32_t module_count(void) { return registered_modules; }

uint32_t module_snapshot(mvh_module_info_t *output, uint32_t capacity)
{
    uint32_t count;
    uint32_t index;
    if (output == 0 && capacity != 0u) return 0u;
    spinlock_lock(&module_lock);
    count = registered_modules < capacity ? registered_modules : capacity;
    for (index = 0u; index < count; index++) {
        mvh_str_copy(output[index].name, sizeof(output[index].name), modules[index].descriptor->name);
        mvh_str_copy(output[index].version, sizeof(output[index].version), modules[index].descriptor->version);
        output[index].state = modules[index].state;
        output[index].flags = modules[index].descriptor->flags;
        output[index].references = modules[index].references;
        output[index].dependency_count = modules[index].descriptor->dependency_count;
        output[index].image_size = modules[index].image_size;
    }
    spinlock_unlock(&module_lock);
    return count;
}

const char *module_state_name(mvh_module_state_t state)
{
    static const char *const names[] = { "unloaded", "registered", "loading", "live", "failed", "unloading" };
    return (uint32_t)state < sizeof(names) / sizeof(names[0]) ? names[state] : "invalid";
}

int module_export_symbol(const char *name, uintptr_t address, const char *owner)
{
    uint32_t index;
    if (name == 0 || name[0] == '\0' || address == 0u) return -1;
    spinlock_lock(&module_lock);
    for (index = 0u; index < registered_exports; index++) {
        if (mvh_str_equal(exports[index].name, name)) {
            spinlock_unlock(&module_lock);
            return -1;
        }
    }
    if (registered_exports == EXPORT_LIMIT) {
        spinlock_unlock(&module_lock);
        return -1;
    }
    exports[registered_exports].name = name;
    exports[registered_exports].address = address;
    exports[registered_exports].owner = owner;
    registered_exports++;
    spinlock_unlock(&module_lock);
    return 0;
}

uintptr_t module_resolve_symbol(const char *name)
{
    uint32_t index;
    uintptr_t result = 0u;
    spinlock_lock(&module_lock);
    for (index = 0u; index < registered_exports; index++)
        if (mvh_str_equal(exports[index].name, name)) { result = exports[index].address; break; }
    spinlock_unlock(&module_lock);
    return result;
}

static uintptr_t resolve_elf_symbol(const elf64_symbol_t *symbol, const char *strings,
                                    uint64_t strings_size, const uintptr_t *sections,
                                    uint16_t section_count)
{
    if (symbol->section == SHN_ABS) return (uintptr_t)symbol->value;
    if (symbol->section == SHN_UNDEF) {
        if (symbol->name >= strings_size ||
            mvh_str_nlength(strings + symbol->name, strings_size - symbol->name) == strings_size - symbol->name)
            return 0u;
        return module_resolve_symbol(strings + symbol->name);
    }
    if (symbol->section >= section_count || sections[symbol->section] == 0u) return 0u;
    return sections[symbol->section] + (uintptr_t)symbol->value;
}

int module_elf_load(const void *image, uint64_t size, const char **loaded_name)
{
    const uint8_t *bytes = (const uint8_t *)image;
    const elf64_header_t *header;
    const elf64_section_t *section;
    const elf64_section_t *symbol_section = 0;
    const elf64_section_t *string_section;
    const elf64_symbol_t *symbols;
    const char *strings;
    uintptr_t section_addresses[ELF_SECTION_LIMIT];
    uint64_t offsets[ELF_SECTION_LIMIT];
    uint64_t allocation_size = 0u;
    uint8_t *allocation;
    const mvh_module_descriptor_t *descriptor = 0;
    uint32_t index;
    uint32_t item;
    if (loaded_name != 0) *loaded_name = 0;
    if (image == 0 || size < sizeof(elf64_header_t)) return -1;
    header = (const elf64_header_t *)image;
    if (header->magic != ELF_MAGIC || header->class_type != ELFCLASS64 ||
        header->data != ELFDATA2LSB || header->type != ET_REL || header->machine != EM_X86_64 ||
        header->header_size != sizeof(*header) || header->section_entry_size != sizeof(elf64_section_t) ||
        header->section_count == 0u || header->section_count > ELF_SECTION_LIMIT ||
        !range_valid(header->section_offset, (uint64_t)header->section_count * sizeof(*section), size)) return -1;
    section = (const elf64_section_t *)(bytes + header->section_offset);
    mvh_mem_zero(section_addresses, sizeof(section_addresses));
    mvh_mem_zero(offsets, sizeof(offsets));
    for (index = 0u; index < header->section_count; index++) {
        uint64_t alignment = section[index].alignment == 0u ? 1u : section[index].alignment;
        if ((alignment & (alignment - 1u)) != 0u || alignment > 4096u) return -1;
        if (section[index].type != SHT_NOBITS && !range_valid(section[index].offset, section[index].size, size)) return -1;
        if ((section[index].flags & SHF_ALLOC) != 0u) {
            allocation_size = mvh_align_up_u64(allocation_size, alignment);
            if (allocation_size == 0u && offsets[index] != 0u) return -1;
            offsets[index] = allocation_size;
            if (mvh_checked_add_u64(allocation_size, section[index].size, &allocation_size) != 0) return -1;
        }
        if (section[index].type == SHT_SYMTAB) {
            if (symbol_section != 0) return -1;
            symbol_section = &section[index];
        }
    }
    if (allocation_size == 0u || symbol_section == 0 || symbol_section->entry_size != sizeof(elf64_symbol_t) ||
        symbol_section->link >= header->section_count || symbol_section->size % sizeof(elf64_symbol_t) != 0u) return -1;
    string_section = &section[symbol_section->link];
    if (!range_valid(string_section->offset, string_section->size, size)) return -1;
    allocation = (uint8_t *)kcalloc(1u, allocation_size);
    if (allocation == 0) return -1;
    for (index = 0u; index < header->section_count; index++) {
        if ((section[index].flags & SHF_ALLOC) == 0u) continue;
        section_addresses[index] = (uintptr_t)(allocation + offsets[index]);
        if (section[index].type != SHT_NOBITS)
            mvh_mem_copy((void *)section_addresses[index], bytes + section[index].offset, section[index].size);
    }
    symbols = (const elf64_symbol_t *)(bytes + symbol_section->offset);
    strings = (const char *)(bytes + string_section->offset);
    for (index = 0u; index < header->section_count; index++) {
        const elf64_rela_t *relocations;
        uint64_t relocation_count;
        if (section[index].type != SHT_RELA) continue;
        if (section[index].info >= header->section_count || section_addresses[section[index].info] == 0u ||
            section[index].entry_size != sizeof(elf64_rela_t) || section[index].size % sizeof(elf64_rela_t) != 0u) goto fail;
        relocations = (const elf64_rela_t *)(bytes + section[index].offset);
        relocation_count = section[index].size / sizeof(elf64_rela_t);
        for (item = 0u; item < relocation_count; item++) {
            uint32_t symbol_index = (uint32_t)(relocations[item].info >> 32u);
            uint32_t type = (uint32_t)relocations[item].info;
            uint64_t relocation_width = type == R_X86_64_64 ? 8u : 4u;
            uintptr_t symbol_value;
            uintptr_t place;
            int64_t value;
            if (symbol_index >= symbol_section->size / sizeof(elf64_symbol_t) ||
                relocations[item].offset > section[section[index].info].size ||
                section[section[index].info].size - relocations[item].offset < relocation_width) goto fail;
            symbol_value = resolve_elf_symbol(&symbols[symbol_index], strings, string_section->size,
                                              section_addresses, header->section_count);
            if (symbol_value == 0u && symbols[symbol_index].section == SHN_UNDEF) goto fail;
            place = section_addresses[section[index].info] + (uintptr_t)relocations[item].offset;
            value = (int64_t)symbol_value + relocations[item].addend;
            if (type == R_X86_64_NONE) continue;
            if (type == R_X86_64_64) *(uint64_t *)place = (uint64_t)value;
            else if (type == R_X86_64_PC32 || type == R_X86_64_PLT32) {
                int64_t relative = value - (int64_t)place;
                if (relative < INT32_MIN || relative > INT32_MAX) goto fail;
                *(uint32_t *)place = (uint32_t)(int32_t)relative;
            } else if (type == R_X86_64_32) {
                if (value < 0 || (uint64_t)value > UINT32_MAX) goto fail;
                *(uint32_t *)place = (uint32_t)value;
            } else if (type == R_X86_64_32S) {
                if (value < INT32_MIN || value > INT32_MAX) goto fail;
                *(uint32_t *)place = (uint32_t)(int32_t)value;
            }
            else goto fail;
        }
    }
    for (index = 0u; index < symbol_section->size / sizeof(elf64_symbol_t); index++) {
        const char *name;
        uintptr_t value;
        if (symbols[index].name >= string_section->size) goto fail;
        name = strings + symbols[index].name;
        if (!mvh_str_equal(name, "mvh_module_descriptor")) continue;
        value = resolve_elf_symbol(&symbols[index], strings, string_section->size,
                                   section_addresses, header->section_count);
        descriptor = (const mvh_module_descriptor_t *)value;
        break;
    }
    if (descriptor == 0 || !allocation_range_valid(descriptor, sizeof(*descriptor), allocation, allocation_size) ||
        !allocation_range_valid(descriptor->name, 1u, allocation, allocation_size) ||
        !allocation_range_valid(descriptor->version, 1u, allocation, allocation_size) ||
        mvh_str_nlength(descriptor->name, allocation_size - ((uintptr_t)descriptor->name - (uintptr_t)allocation)) >=
            allocation_size - ((uintptr_t)descriptor->name - (uintptr_t)allocation) ||
        mvh_str_nlength(descriptor->version, allocation_size - ((uintptr_t)descriptor->version - (uintptr_t)allocation)) >=
            allocation_size - ((uintptr_t)descriptor->version - (uintptr_t)allocation) ||
        (descriptor->dependency_count != 0u &&
         !allocation_range_valid(descriptor->dependencies,
                                 (uint64_t)descriptor->dependency_count * sizeof(char *),
                                 allocation, allocation_size))) goto fail;
    for (index = 0u; index < descriptor->dependency_count; index++) {
        uint64_t remaining;
        if (!allocation_range_valid(descriptor->dependencies[index], 1u, allocation, allocation_size)) goto fail;
        remaining = allocation_size - ((uintptr_t)descriptor->dependencies[index] - (uintptr_t)allocation);
        if (mvh_str_nlength(descriptor->dependencies[index], remaining) >= remaining) goto fail;
    }
    if ((descriptor->init != 0 && !allocation_range_valid((const void *)(uintptr_t)descriptor->init, 1u, allocation, allocation_size)) ||
        (descriptor->exit != 0 && !allocation_range_valid((const void *)(uintptr_t)descriptor->exit, 1u, allocation, allocation_size)) ||
        register_descriptor(descriptor, allocation, allocation_size, 1u, 0u) != 0) goto fail;
    if (module_load(descriptor->name) != 0) {
        module_slot_t *failed;
        spinlock_lock(&module_lock);
        failed = find_slot(descriptor->name);
        if (failed != 0) {
            uint32_t position = (uint32_t)(failed - modules);
            for (index = position + 1u; index < registered_modules; index++) modules[index - 1u] = modules[index];
            registered_modules--;
            mvh_mem_zero(&modules[registered_modules], sizeof(modules[0]));
        }
        spinlock_unlock(&module_lock);
        kfree(allocation);
        return -1;
    }
    if (loaded_name != 0) *loaded_name = descriptor->name;
    return 0;
fail:
    kfree(allocation);
    return -1;
}

static int self_test_init_count;
static int self_test_init(void) { self_test_init_count++; return 0; }
static void self_test_exit(void) { self_test_init_count--; }

int module_self_test(void)
{
    static const mvh_module_descriptor_t test = {
        sizeof(mvh_module_descriptor_t), MVH_KERNEL_ABI_VERSION, "module-selftest", "1", 0u,
        0u, 0, self_test_init, self_test_exit
    };
    static const uint8_t invalid_elf[64] = { 0u };
    uint32_t before = module_count();
    self_test_init_count = 0;
    if (module_register(&test) != 0 || module_load(test.name) != 0 || self_test_init_count != 1 ||
        module_acquire(test.name) != 0 || module_unload(test.name) == 0) return -1;
    module_release(test.name);
    if (module_unload(test.name) != 0 || self_test_init_count != 0 || module_count() != before ||
        module_elf_load(invalid_elf, sizeof(invalid_elf), 0) == 0) return -1;
    return 0;
}
