AS = nasm
CC = gcc
LD = ld

CFLAGS = -m32 -ffreestanding -fno-stack-protector -fno-pic \
         -Wall -Wextra -O2 -std=gnu99
ASFLAGS = -f elf32
LDFLAGS = -m elf_i386 -T linker.ld -nostdlib -z noexecstack

OBJS = boot.o kernel.o
KERNEL = kernel.elf

.PHONY: all clean run run-serial

all: $(KERNEL)

$(KERNEL): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

boot.o: boot.s
	$(AS) $(ASFLAGS) $< -o $@

kernel.o: kernel.c
	$(CC) $(CFLAGS) -c $< -o $@

# Headless: Multiboot load, print to COM1, exit via isa-debug-exit.
# Guest writes 0x10 to 0xf4 => QEMU process status ((0x10 << 1) | 1) = 33.
run-serial: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL) -display none \
		-serial stdio \
		-device isa-debug-exit,iobase=0xf4,iosize=0x04 \
		-no-reboot || [ $$? -eq 33 ]

run: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL)

clean:
	rm -f $(OBJS) $(KERNEL)
