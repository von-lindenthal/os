#include "shell.h"
#include "terminal.h"
#include "keyboard.h"
#include "timer.h"
#include "fs.h"
#include "string.h"
#include "io.h"
#include "multiboot.h"
#include "rtc.h"
#include "heap.h"
#include "speaker.h"
#include <stdint.h>

#define INPUT_MAX 78
#define HIST_MAX 8

static struct multiboot_info *g_mb;
static char history[8][79];
static int hist_count;
static uint32_t rng_state;

static void qemu_exit(uint8_t status)
{
    outb(0xf4, status);
}

static void reboot(void)
{
    writestring("Rebooting...\n");
    uint8_t good = 0x02;
    while (good & 0x02)
        good = inb(0x64);
    outb(0x64, 0xFE);
    qemu_exit(0x10);
    for (;;)
        hlt();
}

static void print_prompt(void)
{
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    writestring("os> ");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
}

static void ls_callback(const char *name, size_t len)
{
    writestring("  ");
    writestring(name);
    writestring("  (");
    write_dec((unsigned int)len);
    writestring(" bytes)\n");
}

static void hist_add(const char *line)
{
    if (!line[0])
        return;
    if (hist_count > 0 && strcmp(history[hist_count - 1], line) == 0)
        return;
    if (hist_count < HIST_MAX) {
        strcpy(history[hist_count++], line);
        return;
    }
    for (int i = 1; i < HIST_MAX; i++)
        strcpy(history[i - 1], history[i]);
    strcpy(history[HIST_MAX - 1], line);
}

static uint32_t rand_u32(void)
{
    /* xorshift32 */
    uint32_t x = rng_state ? rng_state : 0xA5A5u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x ? x : 0xA5A5u;
    return rng_state;
}

static unsigned int parse_uint(const char **pp)
{
    unsigned int n = 0;
    const char *p = *pp;
    while (*p >= '0' && *p <= '9') {
        n = n * 10 + (unsigned int)(*p - '0');
        p++;
    }
    *pp = p;
    return n;
}

static char *next_token(char **line)
{
    char *s = *line;
    while (*s == ' ')
        s++;
    if (*s == '\0') {
        *line = s;
        return s;
    }
    char *tok = s;
    while (*s && *s != ' ')
        s++;
    if (*s) {
        *s = '\0';
        s++;
    }
    *line = s;
    return tok;
}

static void pad2(unsigned int v)
{
    if (v < 10)
        putchar('0');
    write_dec(v);
}

static void cmd_help(void)
{
    writestring("System:  help about clear date time uptime ticks sleep\n");
    writestring("         mem free cpu bench reboot halt beep color\n");
    writestring("Files:   ls cat touch write append rm cp mv df\n");
    writestring("Misc:    echo calc peek history rand guess\n");
    writestring("Tips:    Up-arrow recalls previous command\n");
}

static void cmd_about(void)
{
    writestring("os 0.3 — freestanding x86 kernel\n");
    writestring("IRQs, PIT, RTC, heap, RAM fs, speaker, shell history\n");
}

static void cmd_date(void)
{
    struct rtc_time t;
    rtc_read(&t);
    write_dec(t.year);
    putchar('-');
    pad2(t.month);
    putchar('-');
    pad2(t.day);
    putchar(' ');
    pad2(t.hour);
    putchar(':');
    pad2(t.minute);
    putchar(':');
    pad2(t.second);
    writestring(" (CMOS RTC)\n");
}

static void cmd_uptime(void)
{
    uint32_t secs = timer_seconds();
    uint32_t mins = secs / 60;
    uint32_t hours = mins / 60;
    secs %= 60;
    mins %= 60;
    writestring("uptime: ");
    write_dec(hours);
    writestring("h ");
    write_dec(mins);
    writestring("m ");
    write_dec(secs);
    writestring("s (");
    write_dec(timer_ticks());
    writestring(" ticks)\n");
}

static void cmd_mem(void)
{
    if (!g_mb || !(g_mb->flags & 1)) {
        writestring("Memory info unavailable from bootloader.\n");
    } else {
        unsigned int total_kb = g_mb->mem_lower + g_mb->mem_upper;
        writestring("mem_lower: ");
        write_dec(g_mb->mem_lower);
        writestring(" KB\nmem_upper: ");
        write_dec(g_mb->mem_upper);
        writestring(" KB\napprox total: ");
        write_dec(total_kb);
        writestring(" KB (");
        write_dec(total_kb / 1024);
        writestring(" MB)\n");
    }
    writestring("heap used/free/total: ");
    write_dec((unsigned int)heap_used());
    putchar('/');
    write_dec((unsigned int)heap_free_bytes());
    putchar('/');
    write_dec((unsigned int)heap_total());
    writestring(" bytes\n");
}

static void cmd_free(void)
{
    writestring("Heap:  ");
    write_dec((unsigned int)heap_used());
    writestring(" used, ");
    write_dec((unsigned int)heap_free_bytes());
    writestring(" free, ");
    write_dec((unsigned int)heap_total());
    writestring(" total\nRAMFS: ");
    write_dec((unsigned int)fs_used_bytes());
    writestring(" used / ");
    write_dec((unsigned int)fs_capacity_bytes());
    writestring(" capacity, ");
    write_dec((unsigned int)fs_count());
    writestring("/");
    write_dec(FS_MAX_FILES);
    writestring(" files\n");
}

static void cmd_cpu(void)
{
    uint32_t eax, ebx, ecx, edx;
    char vendor[13];

    __asm__ volatile ("cpuid"
                      : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                      : "a"(0));
    memcpy(vendor + 0, &ebx, 4);
    memcpy(vendor + 4, &edx, 4);
    memcpy(vendor + 8, &ecx, 4);
    vendor[12] = '\0';

    writestring("vendor: ");
    writestring(vendor);
    writestring("\nmax leaf: 0x");
    write_u32(eax, 16);
    writestring("\n");

    __asm__ volatile ("cpuid"
                      : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                      : "a"(1));
    writestring("family/model/stepping via eax=1: 0x");
    write_u32(eax, 16);
    writestring("\nfeatures edx=0x");
    write_u32(edx, 16);
    writestring(" ecx=0x");
    write_u32(ecx, 16);
    writestring("\n");
}

static void cmd_bench(void)
{
    uint32_t start = timer_ticks();
    volatile uint32_t x = 0;
    for (uint32_t i = 0; i < 5000000u; i++)
        x += i;
    uint32_t elapsed = timer_ticks() - start;
    writestring("bench loops done, checksum=");
    write_dec(x);
    writestring(", ticks=");
    write_dec(elapsed);
    writestring(" (~");
    write_dec(elapsed * 10);
    writestring(" ms at 100Hz)\n");
}

static void cmd_peek(const char *arg)
{
    while (*arg == ' ')
        arg++;
    if (!*arg) {
        writestring("usage: peek <hexaddr>\n");
        return;
    }

    unsigned int addr = 0;
    const char *p = arg;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
        p += 2;
    while (*p) {
        char c = *p++;
        unsigned int v;
        if (c >= '0' && c <= '9')
            v = (unsigned int)(c - '0');
        else if (c >= 'a' && c <= 'f')
            v = (unsigned int)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            v = (unsigned int)(c - 'A' + 10);
        else
            break;
        addr = (addr << 4) | v;
    }

    volatile uint8_t *ptr = (volatile uint8_t *)addr;
    writestring("0x");
    write_u32(addr, 16);
    writestring(": ");
    for (int i = 0; i < 16; i++) {
        uint8_t b = ptr[i];
        if (b < 16)
            putchar('0');
        write_u32(b, 16);
        putchar(' ');
    }
    writestring(" |");
    for (int i = 0; i < 16; i++) {
        char c = (char)ptr[i];
        putchar((c >= 32 && c <= 126) ? c : '.');
    }
    writestring("|\n");
}

static void cmd_calc(const char *expr)
{
    while (*expr == ' ')
        expr++;
    if (!*expr) {
        writestring("usage: calc <a> <+|-|*|/> <b>\n");
        return;
    }

    const char *p = expr;
    int neg = 0;
    if (*p == '-') {
        neg = 1;
        p++;
    }
    unsigned int a = parse_uint(&p);
    int ia = neg ? -(int)a : (int)a;
    while (*p == ' ')
        p++;
    char op = *p++;
    while (*p == ' ')
        p++;
    neg = 0;
    if (*p == '-') {
        neg = 1;
        p++;
    }
    unsigned int b = parse_uint(&p);
    int ib = neg ? -(int)b : (int)b;

    int result = 0;
    if (op == '+')
        result = ia + ib;
    else if (op == '-')
        result = ia - ib;
    else if (op == '*')
        result = ia * ib;
    else if (op == '/') {
        if (ib == 0) {
            writestring("divide by zero\n");
            return;
        }
        result = ia / ib;
    } else {
        writestring("usage: calc <a> <+|-|*|/> <b>\n");
        return;
    }

    char buf[16];
    itoa(result, buf, 10);
    writestring(buf);
    writestring("\n");
}

static void cmd_color(const char *arg)
{
    while (*arg == ' ')
        arg++;
    if (strcmp(arg, "green") == 0)
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    else if (strcmp(arg, "cyan") == 0)
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    else if (strcmp(arg, "yellow") == 0)
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK));
    else if (strcmp(arg, "white") == 0)
        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    else if (strcmp(arg, "red") == 0)
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
    else {
        writestring("usage: color <green|cyan|yellow|white|red>\n");
        return;
    }
    writestring("color set\n");
}

static void cmd_guess(void)
{
    unsigned int secret = (rand_u32() % 100u) + 1u;
    writestring("Guess a number 1-100. Type the number and Enter.\n");
    writestring("Type q to quit.\n");

    for (int tries = 1; tries <= 10; tries++) {
        writestring("guess> ");
        char buf[16];
        size_t n = 0;
        for (;;) {
            int c = getchar_code();
            if (c == '\n') {
                putchar('\n');
                break;
            }
            if (c == '\b') {
                if (n > 0) {
                    n--;
                    putchar('\b');
                }
                continue;
            }
            if (c >= 32 && c < 256 && n < sizeof(buf) - 1) {
                buf[n++] = (char)c;
                putchar((char)c);
            }
        }
        buf[n] = '\0';
        if (buf[0] == 'q') {
            writestring("quit. answer was ");
            write_dec(secret);
            writestring("\n");
            return;
        }
        const char *p = buf;
        unsigned int g = parse_uint(&p);
        if (g == secret) {
            writestring("Correct in ");
            write_dec((unsigned int)tries);
            writestring(" tries!\n");
            speaker_beep(880, 10);
            return;
        }
        if (g < secret)
            writestring("higher\n");
        else
            writestring("lower\n");
    }
    writestring("out of tries. answer was ");
    write_dec(secret);
    writestring("\n");
}

static void handle_command(char *line)
{
    while (*line == ' ')
        line++;
    if (*line == '\0')
        return;

    if (strcmp(line, "!!") == 0) {
        if (hist_count == 0) {
            writestring("no history\n");
            return;
        }
        writestring(history[hist_count - 1]);
        writestring("\n");
        char again[INPUT_MAX + 1];
        strcpy(again, history[hist_count - 1]);
        handle_command(again);
        return;
    }

    hist_add(line);

    char *rest = line;
    char *cmd = next_token(&rest);

    if (strcmp(cmd, "help") == 0) {
        cmd_help();
        return;
    }
    if (strcmp(cmd, "about") == 0 || strcmp(cmd, "uname") == 0) {
        cmd_about();
        return;
    }
    if (strcmp(cmd, "clear") == 0) {
        terminal_clear();
        return;
    }
    if (strcmp(cmd, "date") == 0 || strcmp(cmd, "time") == 0) {
        cmd_date();
        return;
    }
    if (strcmp(cmd, "uptime") == 0) {
        cmd_uptime();
        return;
    }
    if (strcmp(cmd, "ticks") == 0) {
        writestring("ticks: ");
        write_dec(timer_ticks());
        writestring("\n");
        return;
    }
    if (strcmp(cmd, "mem") == 0) {
        cmd_mem();
        return;
    }
    if (strcmp(cmd, "free") == 0) {
        cmd_free();
        return;
    }
    if (strcmp(cmd, "cpu") == 0) {
        cmd_cpu();
        return;
    }
    if (strcmp(cmd, "bench") == 0) {
        cmd_bench();
        return;
    }
    if (strcmp(cmd, "sleep") == 0) {
        while (*rest == ' ')
            rest++;
        if (!*rest) {
            writestring("usage: sleep <seconds>\n");
            return;
        }
        const char *p = rest;
        unsigned int n = parse_uint(&p);
        writestring("sleeping...\n");
        timer_sleep(n * 100);
        writestring("done\n");
        return;
    }
    if (strcmp(cmd, "beep") == 0) {
        speaker_beep(440, 15);
        writestring("beep\n");
        return;
    }
    if (strcmp(cmd, "color") == 0) {
        cmd_color(rest);
        return;
    }
    if (strcmp(cmd, "ls") == 0) {
        if (fs_count() == 0) {
            writestring("(empty)\n");
            return;
        }
        fs_list(ls_callback);
        return;
    }
    if (strcmp(cmd, "df") == 0) {
        writestring("RAMFS ");
        write_dec((unsigned int)fs_used_bytes());
        writestring("/");
        write_dec((unsigned int)fs_capacity_bytes());
        writestring(" bytes, ");
        write_dec((unsigned int)fs_count());
        writestring("/");
        write_dec(FS_MAX_FILES);
        writestring(" inodes\n");
        return;
    }
    if (strcmp(cmd, "cat") == 0) {
        char *name = next_token(&rest);
        if (!*name) {
            writestring("usage: cat <file>\n");
            return;
        }
        char buf[FS_DATA_MAX];
        size_t len = 0;
        if (fs_read(name, buf, sizeof(buf), &len) != 0) {
            writestring("file not found\n");
            return;
        }
        writestring(buf);
        if (len == 0 || buf[len - 1] != '\n')
            writestring("\n");
        return;
    }
    if (strcmp(cmd, "touch") == 0) {
        char *name = next_token(&rest);
        if (!*name) {
            writestring("usage: touch <file>\n");
            return;
        }
        int rc = fs_create(name);
        if (rc == -1)
            writestring("invalid name\n");
        else if (rc == -2)
            writestring("filesystem full\n");
        else
            writestring("ok\n");
        return;
    }
    if (strcmp(cmd, "write") == 0) {
        char *name = next_token(&rest);
        if (!*name) {
            writestring("usage: write <file> <text>\n");
            return;
        }
        while (*rest == ' ')
            rest++;
        if (fs_create(name) == -2) {
            writestring("filesystem full\n");
            return;
        }
        if (fs_write(name, rest) != 0)
            writestring("write failed\n");
        else
            writestring("ok\n");
        return;
    }
    if (strcmp(cmd, "append") == 0) {
        char *name = next_token(&rest);
        if (!*name) {
            writestring("usage: append <file> <text>\n");
            return;
        }
        while (*rest == ' ')
            rest++;
        if (!fs_exists(name) && fs_create(name) != 0) {
            writestring("cannot create\n");
            return;
        }
        if (fs_append(name, rest) != 0)
            writestring("append failed\n");
        else
            writestring("ok\n");
        return;
    }
    if (strcmp(cmd, "rm") == 0) {
        char *name = next_token(&rest);
        if (!*name) {
            writestring("usage: rm <file>\n");
            return;
        }
        if (fs_remove(name) != 0)
            writestring("file not found\n");
        else
            writestring("ok\n");
        return;
    }
    if (strcmp(cmd, "cp") == 0) {
        char *src = next_token(&rest);
        char *dst = next_token(&rest);
        if (!*src || !*dst) {
            writestring("usage: cp <src> <dst>\n");
            return;
        }
        int rc = fs_copy(src, dst);
        if (rc == -1)
            writestring("source not found\n");
        else if (rc != 0)
            writestring("copy failed\n");
        else
            writestring("ok\n");
        return;
    }
    if (strcmp(cmd, "mv") == 0) {
        char *src = next_token(&rest);
        char *dst = next_token(&rest);
        if (!*src || !*dst) {
            writestring("usage: mv <old> <new>\n");
            return;
        }
        int rc = fs_rename(src, dst);
        if (rc == -1)
            writestring("not found\n");
        else if (rc == -3)
            writestring("destination exists\n");
        else if (rc != 0)
            writestring("rename failed\n");
        else
            writestring("ok\n");
        return;
    }
    if (strcmp(cmd, "echo") == 0) {
        while (*rest == ' ')
            rest++;
        writestring(rest);
        writestring("\n");
        return;
    }
    if (strcmp(cmd, "calc") == 0) {
        cmd_calc(rest);
        return;
    }
    if (strcmp(cmd, "peek") == 0) {
        cmd_peek(rest);
        return;
    }
    if (strcmp(cmd, "history") == 0) {
        if (hist_count == 0) {
            writestring("(empty)\n");
            return;
        }
        for (int i = 0; i < hist_count; i++) {
            write_dec((unsigned int)(i + 1));
            writestring("  ");
            writestring(history[i]);
            writestring("\n");
        }
        return;
    }
    if (strcmp(cmd, "rand") == 0) {
        write_dec(rand_u32());
        writestring("\n");
        return;
    }
    if (strcmp(cmd, "guess") == 0) {
        cmd_guess();
        return;
    }
    if (strcmp(cmd, "halt") == 0) {
        writestring("Halting.\n");
        qemu_exit(0x10);
        cli();
        for (;;)
            hlt();
    }
    if (strcmp(cmd, "reboot") == 0) {
        reboot();
        return;
    }

    writestring("Unknown command: ");
    writestring(cmd);
    writestring("\nType 'help' for commands.\n");
}

static void redraw_line(const char *line, size_t len)
{
    /* naive: print prompt again after clearing visual line via \r for serial;
       for VGA just putchar the recalled text after erasing current. */
    (void)len;
    writestring(line);
}

void shell_run(struct multiboot_info *mb)
{
    char line[INPUT_MAX + 1];
    size_t len = 0;
    int hist_index = -1;

    g_mb = mb;
    hist_count = 0;
    rng_state = timer_ticks() ^ 0xC0FFEEu;

    print_prompt();

    for (;;) {
        int c = getchar_code();

        if (c == KEY_UP) {
            if (hist_count == 0)
                continue;
            while (len > 0) {
                putchar('\b');
                len--;
            }
            if (hist_index < 0)
                hist_index = hist_count - 1;
            else if (hist_index > 0)
                hist_index--;
            strcpy(line, history[hist_index]);
            len = strlen(line);
            redraw_line(line, len);
            continue;
        }

        if (c == KEY_DOWN) {
            while (len > 0) {
                putchar('\b');
                len--;
            }
            if (hist_count == 0 || hist_index < 0)
                continue;
            if (hist_index + 1 >= hist_count) {
                hist_index = -1;
                line[0] = '\0';
                len = 0;
                continue;
            }
            hist_index++;
            strcpy(line, history[hist_index]);
            len = strlen(line);
            redraw_line(line, len);
            continue;
        }

        if (c == '\n') {
            putchar('\n');
            line[len] = '\0';
            handle_command(line);
            len = 0;
            hist_index = -1;
            print_prompt();
            continue;
        }

        if (c == '\b') {
            if (len > 0) {
                len--;
                putchar('\b');
            }
            continue;
        }

        if (c == 27)
            continue;

        if (c >= 32 && c < 256 && len < INPUT_MAX) {
            line[len++] = (char)c;
            putchar((char)c);
            hist_index = -1;
        }
    }
}
