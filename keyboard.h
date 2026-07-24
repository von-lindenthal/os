#ifndef KEYBOARD_H
#define KEYBOARD_H

#define KEY_UP    0x100
#define KEY_DOWN  0x101
#define KEY_LEFT  0x102
#define KEY_RIGHT 0x103
#define KEY_CTRL_C 3
#define KEY_CTRL_L 12
#define KEY_CTRL_U 21

void keyboard_init(void);
void keyboard_enable_irq_mode(void);
void keyboard_poll(void); /* drain PS/2 into software buffer */
void keyboard_irq_handler(void);
int keyboard_read_code(void);
int getchar_code(void);
int input_try_code(void); /* non-blocking: 0 if nothing ready */
void input_drain(void);
char getchar(void);

#endif
