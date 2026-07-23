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

**System:** `help` `about` `sysinfo` `clear` `date` `time` `uptime` `ticks` `sleep` `mem` `free` `cpu` `bench` `dmesg` `lspci` `beep` `color` `reboot` `halt` `shutdown`

**Files:** `ls` `df` `cat` `head` `wc` `hexdump` `touch` `write` `append` `rm` `cp` `mv` `find` `run`

**Vars:** `set name=value` `get name` `vars` `echo $name`

**Misc:** `echo` `calc` `peek` `history` `!!` `rand` `guess` `snake`

## Features

- Multiboot + VGA/serial console
- IRQ timer (PIT) and keyboard
- CMOS RTC, kernel log, PCI scan
- Kernel heap and RAM filesystem
- Scripts via `run`, variables, snake game
