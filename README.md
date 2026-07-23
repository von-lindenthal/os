# os

A freestanding x86 operating system with an interactive shell.

## Quick run (Windows)

```powershell
Invoke-WebRequest -Uri "https://github.com/von-lindenthal/os/raw/main/kernel.elf" -OutFile "kernel.elf"
& "C:\Program Files\qemu\qemu-system-i386.exe" -kernel .\kernel.elf
```

Click the QEMU window, then type at `guest@os>`.

## Build

```bash
sudo apt install build-essential gcc-multilib nasm qemu-system-x86 make
make && make run
```

## Highlights (0.5)

- Login/auth (`login`, `whoami`, `passwd`)
- Graphics demo (`gfx`) and music (`play c d e`)
- Games: `snake`, `hangman`, `dice`, `guess`
- Files: `grep`, `diff`, `sum`, `edit`, `run`
- Aliases and variables
- PCI, dmesg, RTC, heap, RAM filesystem

Type `help` for the full command list.
