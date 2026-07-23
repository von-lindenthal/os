#ifndef KEYBOARD_H
#define KEYBOARD_H

#define KEY_UP   0x100
#define KEY_DOWN 0x101

void keyboard_init(void);
void keyboard_enable_irq_mode(void);
void keyboard_poll(void); /* drain PS/2 into software buffer */
void keyboard_irq_handler(void);
int keyboard_read_code(void);
int getchar_code(void);
void input_drain(void);
char getchar(void);

#endif
