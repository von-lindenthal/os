; Multiboot header + entry for a freestanding 32-bit kernel.

MBALIGN  equ 1 << 0
MEMINFO  equ 1 << 1
FLAGS    equ MBALIGN | MEMINFO
MAGIC    equ 0x1BADB002
CHECKSUM equ -(MAGIC + FLAGS)

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

section .bss
align 16
stack_bottom:
    resb 65536
stack_top:

section .text
global _start
extern kernel_main

_start:
    mov esp, stack_top
    ; Multiboot: eax = magic, ebx = info pointer
    push ebx
    push eax
    call kernel_main
.hang:
    cli
    hlt
    jmp .hang

section .note.GNU-stack noalloc noexec nowrite progbits
