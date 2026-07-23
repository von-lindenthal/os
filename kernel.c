#include "multiboot.h"
#include "terminal.h"
#include "keyboard.h"
#include "idt.h"
#include "timer.h"
#include "fs.h"
#include "shell.h"
#include "heap.h"
#include "io.h"
#include <stdint.h>

void kernel_main(uint32_t magic, struct multiboot_info *mb)
{
    terminal_initialize();
    keyboard_init();
    heap_init();
    fs_init();

    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    writestring("os 0.3 booting...\n");

    if (magic != MULTIBOOT_MAGIC) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        writestring("Warning: bad Multiboot magic.\n");
    } else {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
        writestring("Multiboot OK. ");
        if (mb && (mb->flags & 1)) {
            writestring("Memory: ");
            write_dec(mb->mem_lower + mb->mem_upper);
            writestring(" KB\n");
        } else {
            writestring("\n");
        }
    }

    idt_init();
    timer_init(100);
    irq_install();

    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    writestring("Timer, keyboard IRQ, heap, RTC, speaker ready.\n");
    writestring("Type 'help'. Click the QEMU window to type.\n\n");

    input_drain();
    shell_run(mb);
}
