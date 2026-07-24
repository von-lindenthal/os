#include "keyboard.h"
#include "io.h"
#include "terminal.h"
#include "timer.h"
#include <stdint.h>

#define KBD_BUF_SIZE 128

static volatile int kbd_buf[KBD_BUF_SIZE];
static volatile unsigned int kbd_head;
static volatile unsigned int kbd_tail;
static volatile int shift_pressed;
static volatile int ctrl_pressed;
static volatile int extended;
static volatile uint8_t last_make;
static volatile int key_held;
static volatile uint32_t last_make_tick;
static volatile uint32_t last_push_tick;
static volatile int last_push_code;

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
    uint32_t now = timer_ticks();
    if (c == last_push_code && (now - last_push_tick) <= 3u)
        return;
    last_push_code = c;
    last_push_tick = now;

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
        uint8_t code = (uint8_t)(scancode & 0x7F);
        int is_break = scancode & 0x80;
        extended = 0;
        if (is_break) {
            if (key_held && last_make == code)
                key_held = 0;
            return;
        }
        if (key_held && last_make == code &&
            (timer_ticks() - last_make_tick) <= 3u)
            return;
        last_make = code;
        last_make_tick = timer_ticks();
        key_held = 1;
        if (code == 0x48)
            kbd_push(KEY_UP);
        else if (code == 0x50)
            kbd_push(KEY_DOWN);
        else if (code == 0x4B)
            kbd_push(KEY_LEFT);
        else if (code == 0x4D)
            kbd_push(KEY_RIGHT);
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
    if (scancode == 0x1D) {
        ctrl_pressed = 1;
        return;
    }
    if (scancode == 0x9D) {
        ctrl_pressed = 0;
        return;
    }

    if (scancode & 0x80) {
        uint8_t code = (uint8_t)(scancode & 0x7F);
        if (key_held && last_make == code)
            key_held = 0;
        return;
    }

    if (key_held && last_make == scancode &&
        (timer_ticks() - last_make_tick) <= 3u)
        return;

    last_make = scancode;
    last_make_tick = timer_ticks();
    key_held = 1;

    char c = scancode_to_ascii(scancode);
    if (!c)
        return;

    if (ctrl_pressed) {
        if (c >= 'a' && c <= 'z')
            kbd_push((int)(c - 'a' + 1));
        else if (c >= 'A' && c <= 'Z')
            kbd_push((int)(c - 'A' + 1));
        return;
    }

    kbd_push((unsigned char)c);
}

void keyboard_poll(void)
{
    int safety = 16;
    while ((inb(0x64) & 1) && safety--)
        handle_scancode(inb(0x60));
}

void keyboard_init(void)
{
    kbd_head = 0;
    kbd_tail = 0;
    shift_pressed = 0;
    ctrl_pressed = 0;
    extended = 0;
    last_make = 0;
    key_held = 0;
    last_make_tick = 0;
    last_push_tick = 0;
    last_push_code = 0;
    while (inb(0x64) & 1)
        (void)inb(0x60);
}

void keyboard_enable_irq_mode(void)
{
    /* Keep keyboard IRQ masked; timer IRQ calls keyboard_poll(). */
    outb(0x21, (uint8_t)(inb(0x21) | 0x02));
    while (inb(0x64) & 1)
        (void)inb(0x60);
    kbd_head = 0;
    kbd_tail = 0;
    key_held = 0;
    last_make = 0;
    last_push_code = 0;
    ctrl_pressed = 0;
}

void keyboard_irq_handler(void)
{
    while (inb(0x64) & 1)
        (void)inb(0x60);
}

int keyboard_read_code(void)
{
    keyboard_poll();
    return kbd_pop();
}

static int serial_esc_state;

static int poll_serial_char(void)
{
    if (!serial_received())
        return 0;
    char c = serial_read();

    if (serial_esc_state == 1) {
        if (c == '[') {
            serial_esc_state = 2;
            return 0;
        }
        serial_esc_state = 0;
        return 0;
    }
    if (serial_esc_state == 2) {
        serial_esc_state = 0;
        if (c == 'A')
            return KEY_UP;
        if (c == 'B')
            return KEY_DOWN;
        if (c == 'C')
            return KEY_RIGHT;
        if (c == 'D')
            return KEY_LEFT;
        return 0;
    }

    if (c == 27) {
        serial_esc_state = 1;
        return 0;
    }
    if (c == '\r')
        return '\n';
    return (unsigned char)c;
}

void input_drain(void)
{
    while (inb(0x64) & 1)
        (void)inb(0x60);
    while (kbd_pop())
        ;
    while (serial_received())
        (void)serial_read();
    shift_pressed = 0;
    ctrl_pressed = 0;
    extended = 0;
    key_held = 0;
    last_make = 0;
    last_push_code = 0;
    serial_esc_state = 0;
}

int input_try_code(void)
{
    int c = keyboard_read_code();
    if (c)
        return c;
    return poll_serial_char();
}

int getchar_code(void)
{
    for (;;) {
        int c = input_try_code();
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
