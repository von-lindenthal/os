#include "speaker.h"
#include "io.h"
#include "timer.h"

void speaker_stop(void)
{
    uint8_t tmp = inb(0x61) & 0xFC;
    outb(0x61, tmp);
}

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
    speaker_stop();
}

int speaker_note_freq(char note)
{
    switch (note) {
    case 'c': case 'C': return 262;
    case 'd': case 'D': return 294;
    case 'e': case 'E': return 330;
    case 'f': case 'F': return 349;
    case 'g': case 'G': return 392;
    case 'a': case 'A': return 440;
    case 'b': case 'B': return 494;
    default: return 0;
    }
}

void speaker_play_notes(const char *notes)
{
    while (*notes) {
        while (*notes == ' ')
            notes++;
        if (!*notes)
            break;
        int freq = speaker_note_freq(*notes++);
        if (freq)
            speaker_beep((uint32_t)freq, 12);
        else
            timer_sleep(12);
    }
}
