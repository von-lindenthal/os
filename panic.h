#ifndef PANIC_H
#define PANIC_H

void panic(const char *msg);
void fault_handler(void *frame);

#endif
