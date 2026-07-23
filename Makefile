AS = nasm
CC = gcc
LD = ld

CFLAGS = -m32 -ffreestanding -fno-stack-protector -fno-pic \
         -Wall -Wextra -O2 -std=gnu99
ASFLAGS = -f elf32
LDFLAGS = -m elf_i386 -T linker.ld -nostdlib -z noexecstack

OBJS = boot.o irq.o gdt.o string.o terminal.o keyboard.o idt.o timer.o \
       fs.o heap.o rtc.o speaker.o klog.o pci.o game.o gfx.o auth.o \
       ata.o net.o panic.o shell.o kernel.o
KERNEL = kernel.elf

.PHONY: all clean run run-serial test-shell test-bugs

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
	dd if=/dev/zero of=/tmp/os-disk.img bs=1M count=8 status=none
	@printf '%s\n' \
		'about' 'motd' 'cal' 'bin 42' 'hex 255' 'base 255 16' 'prime 17' 'fact 5' \
		'alias loop=loop' 'loop' 'unalias loop' \
		'set x=hi' 'echo $$x' 'unset x' \
		'yank hello' 'clip' 'paste clip.txt' 'cat clip.txt' \
		'ps' 'debug' \
		'stopwatch start' 'sleep 1' 'stopwatch stop' 'stopwatch status' \
		'repeat 2 echo hi' 'tail readme.txt 1' \
		'write z.txt c\nb\na\na' 'cat z.txt' 'sort z.txt' 'uniq z.txt' 'rev os' \
		'disk' 'net' 'countdown 1' 'halt' > /tmp/os-shell-test.txt
	@(sleep 0.5; cat /tmp/os-shell-test.txt) | \
	qemu-system-i386 -kernel $(KERNEL) -display none \
		-drive file=/tmp/os-disk.img,format=raw,if=ide \
		-serial stdio \
		-device isa-debug-exit,iobase=0xf4,iosize=0x04 \
		-nic none \
		-no-reboot || [ $$? -eq 33 ]

test-bugs: $(KERNEL)
	@bash scripts/bugfix-test.sh

clean:
	rm -f $(OBJS) $(KERNEL)
