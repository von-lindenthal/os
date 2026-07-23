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

## Features (0.8)

- GDT/IDT faults, panic, bounded strings, safer FS/heap
- Shell: files, aliases, vars, scripts, clipboard, stopwatch
- `ps` / `debug`, `sort`/`uniq`/`rev`, `hex`/`base`, `repeat`
- Ctrl+L clear, Ctrl+U kill line, Ctrl+C cancel
- Games, PCI/ATA/net scan, themes, RTC, speaker

## Tests

```bash
make test-shell   # general shell smoke
make test-bugs    # edge-case / bugfix regressions
```

Type `help` for commands.
