AS = nasm
CC = gcc
LD = ld

CFLAGS = -m32 -ffreestanding -fno-stack-protector -fno-pic \
         -Wall -Wextra -O2 -std=gnu99
ASFLAGS = -f elf32
LDFLAGS = -m elf_i386 -T linker.ld -nostdlib -z noexecstack

OBJS = boot.o irq.o string.o terminal.o keyboard.o idt.o timer.o \
       fs.o heap.o rtc.o speaker.o klog.o pci.o game.o gfx.o auth.o \
       shell.o kernel.o
KERNEL = kernel.elf

.PHONY: all clean run run-serial test-shell

all: $(KERNEL)

$(KERNEL): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

boot.o: boot.s
	$(AS) $(ASFLAGS) $< -o $@

irq.o: irq.s
	$(AS) $(ASFLAGS) $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL) -serial null -nic none

run-serial: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL) -nographic

test-shell: $(KERNEL)
	(sleep 0.5; printf 'about\nwhoami\nlogin dean os\nwhoami\nset greet=hello\necho $$greet\nalias hi=echo hello-alias\nhi\nwrite demo.txt hello world\ngrep hello demo.txt\ncp demo.txt demo2.txt\ndiff demo.txt demo2.txt\nsum demo.txt\nfortune\nplay c e g\ndice\nhalt\n') | \
	qemu-system-i386 -kernel $(KERNEL) -display none \
		-serial stdio \
		-device isa-debug-exit,iobase=0xf4,iosize=0x04 \
		-no-reboot || [ $$? -eq 33 ]

clean:
	rm -f $(OBJS) $(KERNEL)
