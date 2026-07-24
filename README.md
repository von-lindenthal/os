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

## Features (0.9)

- Persistent themes/colors across prompts
- Safer number parsing (`sleep foo` rejected)
- Snake: arrow keys, serial input, food never on body
- Serial ANSI arrows for command history
- Backspace wraps lines; Tab ignored in the shell
- Clipboard, stopwatch, ps/debug, games, PCI/ATA/net

## Tests

```bash
make test-shell   # general shell smoke
make test-bugs    # edge-case / bugfix regressions
make test-more    # deeper stress (calc overflow, nest, FS full)
```

Type `help` for commands.
