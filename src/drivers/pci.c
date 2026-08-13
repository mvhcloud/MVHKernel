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
    uint32_t count = 0;
    uint32_t value;
    uint16_t vendor;
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint8_t functions;
    for (bus = 0; ; bus++) {
        for (slot = 0; slot < 32u; slot++) {
            value = pci_config_read32(bus, slot, 0u, 0u);
            vendor = (uint16_t)(value & 0xFFFFu);
            if (vendor == 0xFFFFu) {
                continue;
            }
            functions = (pci_config_read32(bus, slot, 0u, 0x0Cu) & 0x00800000u) != 0u ? 8u : 1u;
            for (function = 0; function < functions; function++) {
                value = pci_config_read32(bus, slot, function, 0u);
                vendor = (uint16_t)(value & 0xFFFFu);
                if (vendor == 0xFFFFu) {
                    continue;
                }
                if (count < capacity) {
                    devices[count].bus = bus;
                    devices[count].slot = slot;
                    devices[count].function = function;
                    devices[count].vendor = vendor;
                    devices[count].device = (uint16_t)(value >> 16u);
                    value = pci_config_read32(bus, slot, function, 0x08u);
                    devices[count].class_code = (uint8_t)(value >> 24u);
                    devices[count].subclass = (uint8_t)(value >> 16u);
                }
                count++;
            }
        }
        if (bus == 255u) {
            break;
        }
    }
    return count > capacity ? capacity : count;
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
