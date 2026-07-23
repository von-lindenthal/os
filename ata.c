#include "ata.h"
#include "io.h"
#include "terminal.h"
#include "timer.h"
#include <stdint.h>

static void ata_wait_bsy(void)
{
    int spins = 100000;
    while ((inb(0x1F7) & 0x80) && spins--)
        ;
}

void ata_identify(void)
{
    uint16_t id[256];

    outb(0x1F6, 0xA0); /* drive 0 */
    outb(0x1F2, 0);
    outb(0x1F3, 0);
    outb(0x1F4, 0);
    outb(0x1F5, 0);
    outb(0x1F7, 0xEC); /* IDENTIFY */

    uint8_t status = inb(0x1F7);
    if (status == 0) {
        writestring("No ATA drive on primary master.\n");
        return;
    }

    ata_wait_bsy();
    /* Not ATAPI */
    if (inb(0x1F4) || inb(0x1F5)) {
        writestring("Device is not ATA (maybe ATAPI).\n");
        return;
    }

    /* Wait for ERR or DRQ */
    for (;;) {
        status = inb(0x1F7);
        if (status & 1) {
            writestring("ATA IDENTIFY error.\n");
            return;
        }
        if (status & 0x08)
            break;
    }

    for (int i = 0; i < 256; i++) {
        uint16_t v;
        __asm__ volatile ("inw %1, %0" : "=a"(v) : "Nd"((uint16_t)0x1F0));
        id[i] = v;
    }

    char model[41];
    for (int i = 0; i < 20; i++) {
        model[i * 2] = (char)(id[27 + i] >> 8);
        model[i * 2 + 1] = (char)(id[27 + i] & 0xFF);
    }
    model[40] = '\0';
    /* Trim trailing spaces */
    for (int i = 39; i >= 0; i--) {
        if (model[i] == ' ' || model[i] == '\0')
            model[i] = '\0';
        else
            break;
    }

    uint32_t sectors = ((uint32_t)id[61] << 16) | id[60];
    writestring("ATA primary master\n  model: ");
    writestring(model[0] ? model : "(unknown)");
    writestring("\n  LBA28 sectors: ");
    write_dec(sectors);
    writestring(" (~");
    write_dec(sectors / 2048);
    writestring(" MB)\n");
}
