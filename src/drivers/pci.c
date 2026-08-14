#include <stdint.h>
#include "mvh/io.h"
#include "mvh/pci.h"

uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset)
{
    uint32_t address = 0x80000000u | ((uint32_t)bus << 16u) |
                       ((uint32_t)slot << 11u) | ((uint32_t)function << 8u) |
                       (offset & 0xFCu);
    io_out32(0xCF8u, address);
    return io_in32(0xCFCu);
}

void pci_config_write32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset,
                        uint32_t value)
{
    uint32_t address = 0x80000000u | ((uint32_t)bus << 16u) |
                       ((uint32_t)slot << 11u) | ((uint32_t)function << 8u) |
                       (offset & 0xFCu);
    io_out32(0xCF8u, address);
    io_out32(0xCFCu, value);
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
