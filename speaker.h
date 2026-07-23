#ifndef SPEAKER_H
#define SPEAKER_H

#include <stdint.h>

void speaker_beep(uint32_t freq_hz, uint32_t duration_ticks);
void speaker_stop(void);
int speaker_note_freq(char note); /* 'a'-'g', returns Hz or 0 */
void speaker_play_notes(const char *notes); /* e.g. "c d e c" */

#endif
