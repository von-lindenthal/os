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

## Robustness (0.7)

- Own GDT + full CPU exception handlers
- Kernel panic path with logged faults
- Bounded string helpers (`strlcpy` / name validation)
- Safer RAM filesystem and heap metadata checks
- Alias/script recursion limits
- Keyboard polled from timer (avoids QEMU double-key IRQ bug)

Type `help` for commands.
