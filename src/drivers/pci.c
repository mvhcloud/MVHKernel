#include <stdint.h>
#include "mvh/acpi.h"
#include "mvh/io.h"
#include "mvh/memory.h"
#include "mvh/pci.h"
#include "mvh/sync.h"

#define PCI_ECAM_WINDOW 0x50000000u

static pci_status_t state;
static spinlock_t config_lock;

int pci_init(void)
{
    acpi_mcfg_segment_t segment;
    uint8_t *bytes = (uint8_t *)&state;
    uint32_t index;
    for (index = 0u; index < sizeof(state); index++) bytes[index] = 0u;
    spinlock_init(&config_lock);
    if (acpi_mcfg_segment_info(0u, &segment) != 0) return -1;
    state.ecam_available = 1u;
    state.segment_group = segment.segment_group;
    state.start_bus = segment.start_bus;
    state.end_bus = segment.end_bus;
    state.base_address = segment.base_address;
    if (segment.segment_group != 0u) return -1;
    state.ecam_enabled = 1u;
    return 0;
}

const pci_status_t *pci_status(void)
{
    return &state;
}

static uint64_t ecam_address(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset)
{
    return state.base_address + ((uint64_t)(bus - state.start_bus) << 20u) +
           ((uint64_t)slot << 15u) + ((uint64_t)function << 12u) + (offset & 0xFCu);
}

uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset)
{
    uint32_t value;
    if (state.ecam_enabled != 0u && bus >= state.start_bus && bus <= state.end_bus) {
        uint64_t physical = ecam_address(bus, slot, function, offset);
        spinlock_lock(&config_lock);
        if (vmm_map_page(PCI_ECAM_WINDOW, (uintptr_t)(physical & ~0xFFFull),
                         VMM_WRITABLE | VMM_CACHE_DISABLE | VMM_NO_EXECUTE) != 0) {
            spinlock_unlock(&config_lock);
            return 0xFFFFFFFFu;
        }
        value = *(volatile uint32_t *)(uintptr_t)(PCI_ECAM_WINDOW + (physical & 0xFFFu));
        state.config_reads++;
        spinlock_unlock(&config_lock);
        return value;
    }
    uint32_t address = 0x80000000u | ((uint32_t)bus << 16u) |
                       ((uint32_t)slot << 11u) | ((uint32_t)function << 8u) |
                       (offset & 0xFCu);
    io_out32(0xCF8u, address);
    value = io_in32(0xCFCu);
    state.config_reads++;
    return value;
}

void pci_config_write32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset,
                        uint32_t value)
{
    if (state.ecam_enabled != 0u && bus >= state.start_bus && bus <= state.end_bus) {
        uint64_t physical = ecam_address(bus, slot, function, offset);
        spinlock_lock(&config_lock);
        if (vmm_map_page(PCI_ECAM_WINDOW, (uintptr_t)(physical & ~0xFFFull),
                         VMM_WRITABLE | VMM_CACHE_DISABLE | VMM_NO_EXECUTE) == 0) {
            *(volatile uint32_t *)(uintptr_t)(PCI_ECAM_WINDOW + (physical & 0xFFFu)) = value;
            state.config_writes++;
        }
        spinlock_unlock(&config_lock);
        return;
    }
    uint32_t address = 0x80000000u | ((uint32_t)bus << 16u) |
                       ((uint32_t)slot << 11u) | ((uint32_t)function << 8u) |
                       (offset & 0xFCu);
    io_out32(0xCF8u, address);
    io_out32(0xCFCu, value);
    state.config_writes++;
}

uint32_t pci_scan(pci_device_t *devices, uint32_t capacity)
{
    uint32_t count = 0u;
    uint32_t value;
    uint16_t vendor;
    uint16_t bus_index;
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint8_t functions;
    uint8_t bar;
    if (devices == 0 || capacity == 0u) return 0u;
    for (bus_index = 0u; bus_index < 256u && count < capacity; bus_index++) {
        bus = (uint8_t)bus_index;
        for (slot = 0; slot < 32u; slot++) {
            value = pci_config_read32(bus, slot, 0u, 0u);
            vendor = (uint16_t)(value & 0xFFFFu);
            if (vendor == 0xFFFFu) {
                continue;
            }
            functions = (pci_config_read32(bus, slot, 0u, 0x0Cu) & 0x00800000u) != 0u ? 8u : 1u;
            for (function = 0; function < functions && count < capacity; function++) {
                value = pci_config_read32(bus, slot, function, 0u);
                vendor = (uint16_t)(value & 0xFFFFu);
                if (vendor == 0xFFFFu) {
                    continue;
                }
                {
                    uint32_t header;
                    uint32_t interrupt;
                    devices[count].bus = bus;
                    devices[count].slot = slot;
                    devices[count].function = function;
                    devices[count].vendor = vendor;
                    devices[count].device = (uint16_t)(value >> 16u);
                    value = pci_config_read32(bus, slot, function, 0x08u);
                    devices[count].class_code = (uint8_t)(value >> 24u);
                    devices[count].subclass = (uint8_t)(value >> 16u);
                    header = pci_config_read32(bus, slot, function, 0x0Cu);
                    devices[count].header_type = (uint8_t)((header >> 16u) & 0x7Fu);
                    interrupt = pci_config_read32(bus, slot, function, 0x3Cu);
                    devices[count].irq_line = (uint8_t)interrupt;
                    devices[count].irq_pin = (uint8_t)(interrupt >> 8u);
                    devices[count].bar_count = devices[count].header_type == 0u ? 6u :
                                               (devices[count].header_type == 1u ? 2u : 0u);
                    for (bar = 0u; bar < 6u; bar++) {
                        devices[count].bar_base[bar] = 0u;
                        devices[count].bar_is_io[bar] = 0u;
                        devices[count].bar_is_64[bar] = 0u;
                    }
                    for (bar = 0u; bar < devices[count].bar_count; bar++) {
                        uint32_t low = pci_config_read32(bus, slot, function,
                                                        (uint8_t)(0x10u + bar * 4u));
                        if ((low & 1u) != 0u) {
                            devices[count].bar_is_io[bar] = 1u;
                            devices[count].bar_base[bar] = low & ~3u;
                        } else {
                            devices[count].bar_base[bar] = low & ~0xFu;
                            if (((low >> 1u) & 3u) == 2u && bar + 1u < devices[count].bar_count) {
                                uint32_t high = pci_config_read32(bus, slot, function,
                                                                 (uint8_t)(0x14u + bar * 4u));
                                devices[count].bar_is_64[bar] = 1u;
                                devices[count].bar_base[bar] |= (uint64_t)high << 32u;
                                bar++;
                            }
                        }
                    }
                }
                count++;
            }
        }
    }
    return count;
}

const char *pci_class_name(uint8_t class_code)
{
    if (class_code == 0x01u) return "Storage";
    if (class_code == 0x02u) return "Network";
    if (class_code == 0x03u) return "Display";
    if (class_code == 0x04u) return "Multimedia";
    if (class_code == 0x05u) return "Memory";
    if (class_code == 0x06u) return "Bridge";
    if (class_code == 0x07u) return "Communication";
    if (class_code == 0x08u) return "System";
    if (class_code == 0x0Cu) return "Serial bus";
    return "Other";
}
