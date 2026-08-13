#ifndef MVH_PCI_H
#define MVH_PCI_H

#include <stdint.h>

typedef struct {
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint8_t class_code;
    uint8_t subclass;
    uint16_t vendor;
    uint16_t device;
} pci_device_t;

uint32_t pci_scan(pci_device_t *devices, uint32_t capacity);
uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset);
void pci_config_write32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset,
                        uint32_t value);
const char *pci_class_name(uint8_t class_code);

#endif
