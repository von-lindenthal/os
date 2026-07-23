#include "timer.h"
#include "io.h"
#include "keyboard.h"

static volatile uint32_t g_ticks;

void timer_irq_handler(void)
{
    g_ticks++;
    /* Poll PS/2 every tick so the 1-byte HW buffer never overflows,
       without using flaky keyboard IRQs on QEMU. */
    keyboard_poll();
}

void timer_init(uint32_t hz)
{
    g_ticks = 0;
    if (hz == 0)
        hz = 100;

    uint32_t divisor = 1193182 / hz;
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

uint32_t timer_ticks(void)
{
    return g_ticks;
}

uint32_t timer_seconds(void)
{
    return g_ticks / 100;
}

void timer_sleep(uint32_t ticks)
{
    uint32_t start = g_ticks;
    while ((g_ticks - start) < ticks)
        pause();
}
