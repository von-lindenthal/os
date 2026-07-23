#include "gfx.h"
#include "io.h"
#include "timer.h"
#include "keyboard.h"
#include "terminal.h"
#include <stdint.h>

/* Switch VGA to mode 13h (320x200x256) without BIOS. */
static void set_mode_13h(void)
{
    static const uint8_t misc = 0x63;
    static const uint8_t seq[] = {0x03, 0x01, 0x0F, 0x00, 0x0E};
    static const uint8_t crtc[] = {
        0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0xBF, 0x1F, 0x00, 0x41, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x9C, 0x0E, 0x8F, 0x28, 0x40, 0x96, 0xB9, 0xA3,
        0xFF
    };
    static const uint8_t gc[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x05, 0x0F, 0xFF};
    static const uint8_t ac[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00, 0x0F, 0x00, 0x00
    };

    outb(0x3C2, misc);
    for (uint8_t i = 0; i < sizeof(seq); i++) {
        outb(0x3C4, i);
        outb(0x3C5, seq[i]);
    }
    outb(0x3D4, 0x03);
    outb(0x3D5, (uint8_t)(inb(0x3D5) | 0x80));
    outb(0x3D4, 0x11);
    outb(0x3D5, (uint8_t)(inb(0x3D5) & ~0x80));
    uint8_t crtc_mut[sizeof(crtc)];
    for (uint8_t i = 0; i < sizeof(crtc); i++)
        crtc_mut[i] = crtc[i];
    crtc_mut[0x03] |= 0x80;
    crtc_mut[0x11] &= (uint8_t)~0x80;
    for (uint8_t i = 0; i < sizeof(crtc_mut); i++) {
        outb(0x3D4, i);
        outb(0x3D5, crtc_mut[i]);
    }
    for (uint8_t i = 0; i < sizeof(gc); i++) {
        outb(0x3CE, i);
        outb(0x3CF, gc[i]);
    }
    for (uint8_t i = 0; i < sizeof(ac); i++) {
        (void)inb(0x3DA);
        outb(0x3C0, i);
        outb(0x3C0, ac[i]);
    }
    (void)inb(0x3DA);
    outb(0x3C0, 0x20);
}

/* Restore text mode 03h-ish registers (80x25). */
static void set_mode_03h(void)
{
    static const uint8_t misc = 0x67;
    static const uint8_t seq[] = {0x03, 0x00, 0x03, 0x00, 0x02};
    static const uint8_t crtc[] = {
        0x5F, 0x4F, 0x50, 0x82, 0x55, 0x81, 0xBF, 0x1F, 0x00, 0x4F, 0x0D, 0x0E,
        0x00, 0x00, 0x00, 0x50, 0x9C, 0x0E, 0x8F, 0x28, 0x1F, 0x96, 0xB9, 0xA3,
        0xFF
    };
    static const uint8_t gc[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0E, 0x00, 0xFF};
    static const uint8_t ac[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07, 0x38, 0x39, 0x3A, 0x3B,
        0x3C, 0x3D, 0x3E, 0x3F, 0x0C, 0x00, 0x0F, 0x08, 0x00
    };

    outb(0x3C2, misc);
    for (uint8_t i = 0; i < sizeof(seq); i++) {
        outb(0x3C4, i);
        outb(0x3C5, seq[i]);
    }
    outb(0x3D4, 0x03);
    outb(0x3D5, (uint8_t)(inb(0x3D5) | 0x80));
    outb(0x3D4, 0x11);
    outb(0x3D5, (uint8_t)(inb(0x3D5) & ~0x80));
    for (uint8_t i = 0; i < sizeof(crtc); i++) {
        outb(0x3D4, i);
        outb(0x3D5, crtc[i]);
    }
    for (uint8_t i = 0; i < sizeof(gc); i++) {
        outb(0x3CE, i);
        outb(0x3CF, gc[i]);
    }
    for (uint8_t i = 0; i < sizeof(ac); i++) {
        (void)inb(0x3DA);
        outb(0x3C0, i);
        outb(0x3C0, ac[i]);
    }
    (void)inb(0x3DA);
    outb(0x3C0, 0x20);
}

static void put_pixel(int x, int y, uint8_t color)
{
    if (x < 0 || x >= 320 || y < 0 || y >= 200)
        return;
    volatile uint8_t *fb = (volatile uint8_t *)0xA0000;
    fb[y * 320 + x] = color;
}

void gfx_demo(void)
{
    set_mode_13h();

    volatile uint8_t *fb = (volatile uint8_t *)0xA0000;
    for (int i = 0; i < 320 * 200; i++)
        fb[i] = 0;

    /* Color bars */
    for (int y = 0; y < 40; y++)
        for (int x = 0; x < 320; x++)
            put_pixel(x, y, (uint8_t)(x / 20));

    /* Animated plasma-ish pattern */
    for (int frame = 0; frame < 80; frame++) {
        for (int y = 40; y < 200; y++) {
            for (int x = 0; x < 320; x++) {
                uint8_t c = (uint8_t)(((x * x) / 64 + (y * y) / 64 + frame * 3) & 0xFF);
                put_pixel(x, y, c);
            }
        }
        if (keyboard_read_code() == 'q')
            break;
        timer_sleep(2);
    }

    set_mode_03h();
    terminal_initialize();
    writestring("Graphics demo finished (press keys in gfx with q to skip).\n");
    input_drain();
}
