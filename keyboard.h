#ifndef KEYBOARD_H
#define KEYBOARD_H

#define KEY_UP   0x100
#define KEY_DOWN 0x101

void keyboard_init(void);
void keyboard_irq_handler(void);
int keyboard_read_code(void); /* 0 if none; ASCII or KEY_* */
int getchar_code(void);       /* blocking keyboard/serial */
void input_drain(void);

/* Back-compat ASCII helper */
char getchar(void);

#endif
