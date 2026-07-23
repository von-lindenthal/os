#include "speaker.h"
#include "io.h"
#include "timer.h"

void speaker_beep(uint32_t freq_hz, uint32_t duration_ticks)
{
    if (freq_hz < 20)
        freq_hz = 20;
    if (freq_hz > 20000)
        freq_hz = 20000;

    uint32_t divisor = 1193180 / freq_hz;
    outb(0x43, 0xB6);
    outb(0x42, (uint8_t)(divisor & 0xFF));
    outb(0x42, (uint8_t)((divisor >> 8) & 0xFF));

    uint8_t tmp = inb(0x61);
    if ((tmp & 3) != 3)
        outb(0x61, (uint8_t)(tmp | 3));

    timer_sleep(duration_ticks);

    tmp = inb(0x61) & 0xFC;
    outb(0x61, tmp);
}
