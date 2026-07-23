# os

A minimal x86 operating system from scratch:

- Multiboot-compliant boot stub (`boot.s`)
- Freestanding C kernel (`kernel.c`)
- VGA text-mode console plus COM1 serial output

## Requirements

- `nasm`
- `gcc` with 32-bit support (`gcc-multilib` on Debian/Ubuntu)
- `binutils` (`ld`)
- `qemu-system-i386` (from `qemu-system-x86`)

## Build

```bash
make
```

Produces `kernel.elf`.

## Run

Headless (serial output to your terminal; exits via QEMU debug port):

```bash
make run-serial
```

Graphical VGA window:

```bash
make run
```

## Layout

| File        | Role                                      |
|-------------|-------------------------------------------|
| `boot.s`    | Multiboot header, stack, jump to C        |
| `kernel.c`  | Kernel entry, VGA + serial writers        |
| `linker.ld` | Places kernel at 1 MiB                    |
| `Makefile`  | Build and QEMU targets                    |
