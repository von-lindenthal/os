#include "multiboot.h"
#include "terminal.h"
#include "keyboard.h"
#include "idt.h"
#include "timer.h"
#include "fs.h"
#include "shell.h"
#include "heap.h"
#include "klog.h"
#include "auth.h"
#include "io.h"
#include <stdint.h>

void kernel_main(uint32_t magic, struct multiboot_info *mb)
{
    terminal_initialize();
    keyboard_init();
    heap_init();
    klog_init();
    auth_init();
    fs_init();

    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    writestring("os 0.5 booting...\n");
    klog("boot: kernel_main entered");

    if (magic != MULTIBOOT_MAGIC) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        writestring("Warning: bad Multiboot magic.\n");
        klog("boot: bad multiboot magic");
    } else {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
        writestring("Multiboot OK. ");
        klog("boot: multiboot ok");
        if (mb && (mb->flags & 1)) {
            writestring("Memory: ");
            write_dec(mb->mem_lower + mb->mem_upper);
            writestring(" KB\n");
            klog("boot: memory map present");
        } else {
            writestring("\n");
        }
    }

    idt_init();
    timer_init(100);
    irq_install();
    klog("boot: idt/timer/irqs online");

    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    writestring("Timer, keyboard, heap, RTC, PCI, gfx, auth ready.\n");
    writestring("Type 'help'. Click the QEMU window to type.\n\n");
    klog("boot: shell starting");

    input_drain();
    shell_run(mb);
}
