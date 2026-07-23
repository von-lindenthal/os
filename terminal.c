#include "terminal.h"
#include "io.h"
#include "string.h"

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static const uint16_t COM1 = 0x3F8;

static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;
static volatile uint16_t *terminal_buffer;

uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg)
{
    return (uint8_t)(fg | (bg << 4));
}

static uint16_t vga_entry(unsigned char c, uint8_t color)
{
    return (uint16_t)c | ((uint16_t)color << 8);
}

static void serial_init(void)
{
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

static int serial_is_transmit_empty(void)
{
    return inb(COM1 + 5) & 0x20;
}

static void serial_putchar(char c)
{
    if (c == '\n')
        serial_putchar('\r');
    while (!serial_is_transmit_empty())
        ;
    outb(COM1, (uint8_t)c);
}

int serial_received(void)
{
    return inb(COM1 + 5) & 0x01;
}

char serial_read(void)
{
    return (char)inb(COM1);
}

void terminal_clear(void)
{
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++)
            terminal_buffer[y * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
    }
    terminal_row = 0;
    terminal_column = 0;
}

void terminal_initialize(void)
{
    serial_init();
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_buffer = (volatile uint16_t *)0xB8000;
    terminal_clear();
}

void terminal_setcolor(uint8_t color)
{
    terminal_color = color;
}

static void terminal_scroll(void)
{
    for (size_t y = 1; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++)
            terminal_buffer[(y - 1) * VGA_WIDTH + x] =
                terminal_buffer[y * VGA_WIDTH + x];
    }
    for (size_t x = 0; x < VGA_WIDTH; x++)
        terminal_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] =
            vga_entry(' ', terminal_color);
    terminal_row = VGA_HEIGHT - 1;
}

static void terminal_putchar(char c)
{
    if (c == '\n') {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT)
            terminal_scroll();
        return;
    }

    if (c == '\b') {
        if (terminal_column > 0) {
            terminal_column--;
            terminal_buffer[terminal_row * VGA_WIDTH + terminal_column] =
                vga_entry(' ', terminal_color);
        }
        return;
    }

    terminal_buffer[terminal_row * VGA_WIDTH + terminal_column] =
        vga_entry((unsigned char)c, terminal_color);
    if (++terminal_column == VGA_WIDTH) {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT)
            terminal_scroll();
    }
}

void putchar(char c)
{
    terminal_putchar(c);
    if (c == '\b') {
        serial_putchar('\b');
        serial_putchar(' ');
        serial_putchar('\b');
    } else {
        serial_putchar(c);
    }
}

void writestring(const char *data)
{
    for (size_t i = 0; data[i]; i++)
        putchar(data[i]);
}

void write_u32(unsigned int value, int base)
{
    char buf[33];
    u32toa(value, buf, base);
    writestring(buf);
}

void write_dec(unsigned int value)
{
    write_u32(value, 10);
}
