#include "net.h"
#include "terminal.h"
#include <stdint.h>

/* Lightweight PCI NIC summary using config space reads (same as pci.c style). */
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

static uint16_t pci_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off)
{
    uint32_t addr = (uint32_t)((1u << 31) | ((uint32_t)bus << 16) |
                               ((uint32_t)slot << 11) | ((uint32_t)func << 8) |
                               (off & 0xFC));
    outl(0xCF8, addr);
    uint32_t data = inl(0xCFC);
    return (uint16_t)((data >> ((off & 2) * 8)) & 0xFFFF);
}

static uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off)
{
    uint32_t addr = (uint32_t)((1u << 31) | ((uint32_t)bus << 16) |
                               ((uint32_t)slot << 11) | ((uint32_t)func << 8) |
                               (off & 0xFC));
    outl(0xCF8, addr);
    return inl(0xCFC);
}

void net_info(void)
{
    int found = 0;
    for (uint8_t bus = 0; bus < 8; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint16_t vendor = pci_read16(bus, slot, 0, 0);
            if (vendor == 0xFFFF)
                continue;
            uint32_t classreg = pci_read32(bus, slot, 0, 0x08);
            uint8_t class_code = (uint8_t)((classreg >> 24) & 0xFF);
            if (class_code != 0x02) /* network */
                continue;
            uint16_t device = pci_read16(bus, slot, 0, 2);
            uint8_t subclass = (uint8_t)((classreg >> 16) & 0xFF);
            writestring("  NIC ");
            write_dec(bus);
            putchar(':');
            write_u32(slot, 16);
            writestring(".0  vend=0x");
            write_u32(vendor, 16);
            writestring(" dev=0x");
            write_u32(device, 16);
            writestring(" sub=0x");
            write_u32(subclass, 16);
            if (vendor == 0x8086 && device == 0x100E)
                writestring("  (e1000)");
            writestring("\n");
            found++;
        }
    }
    if (!found)
        writestring("No network devices found.\n");
}
