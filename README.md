# os

A freestanding x86 operating system with an interactive shell.

## Quick run (Windows)

```powershell
Invoke-WebRequest -Uri "https://github.com/von-lindenthal/os/raw/main/kernel.elf" -OutFile "kernel.elf"
& "C:\Program Files\qemu\qemu-system-i386.exe" -kernel .\kernel.elf -serial null -nic none
```

Click the QEMU window, then type at `guest@os>`.

## Build

```bash
sudo apt install build-essential gcc-multilib nasm qemu-system-x86 make
make && make run
```

## Highlights (0.6)

- ATA disk identify (`disk`) and NIC scan (`net`)
- Calendar (`cal`), themes, countdown
- Math helpers: `bin`, `prime`, `fact`
- `tail`, `ascii`, `motd`, `env`, `rps`
- Previous: auth, gfx, snake/hangman, grep/diff, aliases, PCI, klog

Type `help` for the full command list.
