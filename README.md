# os

A freestanding x86 operating system with an interactive shell.

## Quick run (Windows)

```powershell
Invoke-WebRequest -Uri "https://github.com/von-lindenthal/os/raw/main/kernel.elf" -OutFile "kernel.elf"
& "C:\Program Files\qemu\qemu-system-i386.exe" -kernel .\kernel.elf
```

Click the QEMU window, then type at `os>`.

## Build

```bash
sudo apt install build-essential gcc-multilib nasm qemu-system-x86 make
make && make run
```

## Commands

**System:** `help` `about` `clear` `date` `time` `uptime` `ticks` `sleep` `mem` `free` `cpu` `bench` `beep` `color` `reboot` `halt`

**Files:** `ls` `df` `cat` `touch` `write` `append` `rm` `cp` `mv`

**Misc:** `echo` `calc` `peek` `history` `!!` `rand` `guess`

Up-arrow recalls previous commands.

## Features

- Multiboot + VGA/serial console
- IRQ timer (PIT) and keyboard
- CMOS RTC clock
- Kernel heap (`kmalloc`)
- RAM filesystem
- PC speaker beep
- Number-guessing game
