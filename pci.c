#include "pci.h"
#include "terminal.h"
#include <stdint.h>

static void outl(uint16_t port, uint32_t value)
{
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

static uint32_t inl(uint16_t port)
{
    uint32_t value;
    __asm__ volatile ("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    uint32_t address = (uint32_t)((1u << 31) |
                                  ((uint32_t)bus << 16) |
                                  ((uint32_t)slot << 11) |
                                  ((uint32_t)func << 8) |
                                  (offset & 0xFC));
    outl(0xCF8, address);
    return inl(0xCFC);
}

static uint16_t pci_config_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    uint32_t data = pci_config_read32(bus, slot, func, (uint8_t)(offset & 0xFC));
    return (uint16_t)((data >> ((offset & 2) * 8)) & 0xFFFF);
}

static void print_device(uint8_t bus, uint8_t slot, uint8_t func)
{
    uint16_t vendor = pci_config_read16(bus, slot, func, 0);
    if (vendor == 0xFFFF)
        return;

    uint16_t device = pci_config_read16(bus, slot, func, 2);
    uint32_t classreg = pci_config_read32(bus, slot, func, 0x08);
    uint8_t class_code = (uint8_t)((classreg >> 24) & 0xFF);
    uint8_t subclass = (uint8_t)((classreg >> 16) & 0xFF);

    writestring("  ");
    write_dec(bus);
    putchar(':');
    if (slot < 16)
        putchar('0');
    write_u32(slot, 16);
    putchar('.');
    write_dec(func);
    writestring("  vend=0x");
    write_u32(vendor, 16);
    writestring(" dev=0x");
    write_u32(device, 16);
    writestring(" class=");
    write_u32(class_code, 16);
    putchar('/');
    write_u32(subclass, 16);
    writestring("\n");
}

void pci_list(void)
{
    int found = 0;
    for (uint16_t bus = 0; bus < 16; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint16_t vendor = pci_config_read16((uint8_t)bus, slot, 0, 0);
            if (vendor == 0xFFFF)
                continue;

            print_device((uint8_t)bus, slot, 0);
            found++;

            uint8_t header = (uint8_t)((pci_config_read32((uint8_t)bus, slot, 0, 0x0C) >> 16) & 0xFF);
            if (header & 0x80) {
                for (uint8_t func = 1; func < 8; func++) {
                    if (pci_config_read16((uint8_t)bus, slot, func, 0) == 0xFFFF)
                        continue;
                    print_device((uint8_t)bus, slot, func);
                    found++;
                }
            }
        }
    }

    if (!found)
        writestring("No PCI devices found.\n");
    else {
        writestring("Total: ");
        write_dec((unsigned int)found);
        writestring("\n");
    }
}
