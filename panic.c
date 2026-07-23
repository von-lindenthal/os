#include "panic.h"
#include "terminal.h"
#include "io.h"
#include "klog.h"
#include <stdint.h>

struct fault_frame {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp;
    uint32_t ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
};

static const char *exception_name(uint32_t n)
{
    static const char *names[] = {
        "Divide by zero", "Debug", "NMI", "Breakpoint",
        "Overflow", "Bound range", "Invalid opcode", "Device not available",
        "Double fault", "Coprocessor overrun", "Invalid TSS", "Segment not present",
        "Stack fault", "General protection", "Page fault", "Reserved",
        "x87 FP", "Alignment check", "Machine check", "SIMD FP"
    };
    if (n < 20)
        return names[n];
    return "Unknown";
}

void panic(const char *msg)
{
    cli();
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_RED));
    writestring("\n*** KERNEL PANIC ***\n");
    if (msg)
        writestring(msg);
    writestring("\nSystem halted.\n");
    klog("panic");
    for (;;)
        hlt();
}

void fault_handler(void *frame_ptr)
{
    struct fault_frame *f = frame_ptr;
    cli();

    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
    writestring("\nCPU exception #");
    write_dec(f->int_no);
    writestring(" (");
    writestring(exception_name(f->int_no));
    writestring(") err=0x");
    write_u32(f->err_code, 16);
    writestring("\nEIP=0x");
    write_u32(f->eip, 16);
    writestring(" CS=0x");
    write_u32(f->cs, 16);
    writestring(" EFLAGS=0x");
    write_u32(f->eflags, 16);
    writestring("\n");

    klog("cpu exception");

    /* Breakpoint: return to continue execution */
    if (f->int_no == 3)
        return;

    panic("unrecoverable CPU exception");
}
