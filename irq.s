; Interrupt stubs for IRQ0 (timer) and IRQ1 (keyboard)

section .text
global irq0
global irq1
global idt_load
extern timer_irq_handler
extern keyboard_irq_handler

%macro IRQ_STUB 2
global irq%1
irq%1:
    pusha
    call %2
    ; Send EOI to PIC
    mov al, 0x20
    out 0x20, al
    popa
    iret
%endmacro

IRQ_STUB 0, timer_irq_handler
IRQ_STUB 1, keyboard_irq_handler

idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
