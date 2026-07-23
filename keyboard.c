#include "keyboard.h"
#include "io.h"
#include "terminal.h"
#include <stdint.h>

#define KBD_BUF_SIZE 128

static volatile int kbd_buf[KBD_BUF_SIZE];
static volatile unsigned int kbd_head;
static volatile unsigned int kbd_tail;
static int shift_pressed;
static int extended;

static const char scancode_set1[] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*',
    0, ' '
};

static const char scancode_set1_shift[] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*',
    0, ' '
};

static void kbd_push(int c)
{
    unsigned int next = (kbd_head + 1) % KBD_BUF_SIZE;
    if (next == kbd_tail)
        return;
    kbd_buf[kbd_head] = c;
    kbd_head = next;
}

static int kbd_pop(void)
{
    if (kbd_head == kbd_tail)
        return 0;
    int c = kbd_buf[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUF_SIZE;
    return c;
}

static char scancode_to_ascii(uint8_t scancode)
{
    if (scancode & 0x80)
        return 0;
    if (scancode >= sizeof(scancode_set1))
        return 0;
    if (shift_pressed)
        return scancode_set1_shift[scancode];
    return scancode_set1[scancode];
}

static void handle_scancode(uint8_t scancode)
{
    if (scancode == 0xE0) {
        extended = 1;
        return;
    }

    if (extended) {
        extended = 0;
        if (scancode == 0x48)
            kbd_push(KEY_UP);
        else if (scancode == 0x50)
            kbd_push(KEY_DOWN);
        return;
    }

    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
        return;
    }
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = 0;
        return;
    }

    char c = scancode_to_ascii(scancode);
    if (c)
        kbd_push((unsigned char)c);
}

void keyboard_init(void)
{
    kbd_head = 0;
    kbd_tail = 0;
    shift_pressed = 0;
    extended = 0;
    while (inb(0x64) & 1)
        (void)inb(0x60);
}

void keyboard_irq_handler(void)
{
    if (!(inb(0x64) & 1))
        return;
    handle_scancode(inb(0x60));
}

static int poll_keyboard_legacy(void)
{
    if (!(inb(0x64) & 1))
        return 0;
    uint8_t sc = inb(0x60);
    if (sc == 0xE0) {
        extended = 1;
        return 0;
    }
    if (extended) {
        extended = 0;
        if (sc == 0x48)
            return KEY_UP;
        if (sc == 0x50)
            return KEY_DOWN;
        return 0;
    }
    if (sc == 0x2A || sc == 0x36) {
        shift_pressed = 1;
        return 0;
    }
    if (sc == 0xAA || sc == 0xB6) {
        shift_pressed = 0;
        return 0;
    }
    return (unsigned char)scancode_to_ascii(sc);
}

int keyboard_read_code(void)
{
    int c = kbd_pop();
    if (c)
        return c;
    return poll_keyboard_legacy();
}

static int poll_serial_char(void)
{
    if (!serial_received())
        return 0;
    char c = serial_read();
    if (c == '\r')
        return '\n';
    return (unsigned char)c;
}

void input_drain(void)
{
    while (keyboard_read_code())
        ;
    while (serial_received())
        (void)serial_read();
    shift_pressed = 0;
    extended = 0;
}

int getchar_code(void)
{
    for (;;) {
        int c = keyboard_read_code();
        if (c)
            return c;
        c = poll_serial_char();
        if (c)
            return c;
        pause();
    }
}

char getchar(void)
{
    for (;;) {
        int c = getchar_code();
        if (c > 0 && c < 256)
            return (char)c;
    }
}
