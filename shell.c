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
#include "klog.h"
#include "pci.h"
#include "game.h"
#include "gfx.h"
#include "auth.h"
#include "ata.h"
#include "net.h"
#include <stdint.h>

#define INPUT_MAX 78
#define HIST_MAX 8
#define VAR_MAX 8
#define VAR_NAME 24
#define VAR_VAL 48
#define ALIAS_MAX 8

static struct multiboot_info *g_mb;
static char history[8][79];
static int hist_count;
static uint32_t rng_state;
static char var_names[VAR_MAX][VAR_NAME];
static char var_vals[VAR_MAX][VAR_VAL];
static int var_count;
static int script_depth;
static int alias_depth;
static char alias_names[ALIAS_MAX][VAR_NAME];
static char alias_vals[ALIAS_MAX][INPUT_MAX + 1];
static int alias_count;

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
        strlcpy(history[hist_count++], line, sizeof(history[0]));
        return;
    }
    for (int i = 1; i < HIST_MAX; i++)
        strlcpy(history[i - 1], history[i], sizeof(history[0]));
    strlcpy(history[HIST_MAX - 1], line, sizeof(history[0]));
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
    writestring("System:  help about sysinfo clear date time uptime ticks\n");
    writestring("         sleep mem free cpu bench dmesg lspci disk net\n");
    writestring("         beep color theme reboot halt shutdown\n");
    writestring("         login logout whoami passwd countdown\n");
    writestring("Files:   ls df cat head tail wc hexdump grep diff sum\n");
    writestring("         touch write append rm cp mv find run edit\n");
    writestring("Vars:    set get vars alias env\n");
    writestring("Fun:     echo calc peek history rand fortune play gfx\n");
    writestring("         ascii bin prime fact motd\n");
    writestring("Games:   guess snake hangman dice rps\n");
}

static void cmd_about(void)
{
    writestring("os 0.7 — freestanding x86 kernel\n");
    writestring("GDT/IDT faults, panic, bounded strings, safer FS/shell\n");
}

static void print_prompt(void)
{
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    writestring(auth_user());
    writestring("@os> ");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
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

static const char *var_get(const char *name)
{
    for (int i = 0; i < var_count; i++) {
        if (strcmp(var_names[i], name) == 0)
            return var_vals[i];
    }
    return 0;
}

static int var_set(const char *name, const char *val)
{
    if (!name[0] || strlen(name) >= VAR_NAME || strlen(val) >= VAR_VAL)
        return -1;
    for (int i = 0; i < var_count; i++) {
        if (strcmp(var_names[i], name) == 0) {
            strcpy(var_vals[i], val);
            return 0;
        }
    }
    if (var_count >= VAR_MAX)
        return -2;
    strcpy(var_names[var_count], name);
    strcpy(var_vals[var_count], val);
    var_count++;
    return 0;
}

static void cmd_hexdump(const char *name)
{
    char buf[FS_DATA_MAX];
    size_t len = 0;
    if (fs_read(name, buf, sizeof(buf), &len) != 0) {
        writestring("file not found\n");
        return;
    }
    for (size_t i = 0; i < len; i += 16) {
        write_u32((unsigned int)i, 16);
        writestring(": ");
        for (size_t j = 0; j < 16; j++) {
            if (i + j < len) {
                uint8_t b = (uint8_t)buf[i + j];
                if (b < 16)
                    putchar('0');
                write_u32(b, 16);
                putchar(' ');
            } else {
                writestring("   ");
            }
        }
        writestring(" |");
        for (size_t j = 0; j < 16 && i + j < len; j++) {
            char c = buf[i + j];
            putchar((c >= 32 && c <= 126) ? c : '.');
        }
        writestring("|\n");
    }
}

static void cmd_wc(const char *name)
{
    char buf[FS_DATA_MAX];
    size_t len = 0;
    if (fs_read(name, buf, sizeof(buf), &len) != 0) {
        writestring("file not found\n");
        return;
    }
    unsigned int lines = 0, words = 0, in_word = 0;
    for (size_t i = 0; i < len; i++) {
        char c = buf[i];
        if (c == '\n')
            lines++;
        if (c == ' ' || c == '\n' || c == '\t') {
            in_word = 0;
        } else if (!in_word) {
            in_word = 1;
            words++;
        }
    }
    if (len > 0 && buf[len - 1] != '\n')
        lines++;
    write_dec(lines);
    putchar(' ');
    write_dec(words);
    putchar(' ');
    write_dec((unsigned int)len);
    putchar(' ');
    writestring(name);
    writestring("\n");
}

static void cmd_head(const char *name, unsigned int nlines)
{
    char buf[FS_DATA_MAX];
    size_t len = 0;
    if (fs_read(name, buf, sizeof(buf), &len) != 0) {
        writestring("file not found\n");
        return;
    }
    if (nlines == 0)
        nlines = 10;
    unsigned int shown = 0;
    for (size_t i = 0; i < len && shown < nlines; i++) {
        putchar(buf[i]);
        if (buf[i] == '\n')
            shown++;
    }
    if (len > 0 && buf[len - 1] != '\n' && shown < nlines)
        writestring("\n");
}

static char g_find_pat[FS_NAME_MAX];
static int g_find_hits;

static void find_cb(const char *name, size_t len)
{
    (void)len;
    for (size_t i = 0; name[i]; i++) {
        size_t j = 0;
        while (g_find_pat[j] && name[i + j] == g_find_pat[j])
            j++;
        if (!g_find_pat[j]) {
            writestring("  ");
            writestring(name);
            writestring("\n");
            g_find_hits++;
            return;
        }
    }
}

static void cmd_sysinfo(void)
{
    cmd_about();
    cmd_date();
    cmd_uptime();
    cmd_free();
}

static void shutdown_qemu(void)
{
    writestring("Shutting down...\n");
    __asm__ volatile ("outw %0, %1" : : "a"((uint16_t)0x2000), "Nd"((uint16_t)0x604));
    __asm__ volatile ("outw %0, %1" : : "a"((uint16_t)0x2000), "Nd"((uint16_t)0xB004));
    qemu_exit(0x10);
    cli();
    for (;;)
        hlt();
}

static void handle_command(char *line);

static void cmd_run(const char *name)
{
    if (script_depth > 3) {
        writestring("script nesting too deep\n");
        return;
    }
    char buf[FS_DATA_MAX];
    size_t len = 0;
    if (fs_read(name, buf, sizeof(buf), &len) != 0) {
        writestring("file not found\n");
        return;
    }
    script_depth++;
    size_t i = 0;
    while (i < len) {
        char line[INPUT_MAX + 1];
        size_t n = 0;
        while (i < len && buf[i] != '\n' && n < INPUT_MAX)
            line[n++] = buf[i++];
        if (i < len && buf[i] == '\n')
            i++;
        line[n] = '\0';
        if (line[0] == '#' || line[0] == '\0')
            continue;
        writestring("+ ");
        writestring(line);
        writestring("\n");
        handle_command(line);
    }
    script_depth--;
}

static void handle_command(char *line)
{
    if (!line)
        return;
    while (*line == ' ')
        line++;
    if (*line == '\0')
        return;
    if (strnlen(line, INPUT_MAX + 2) > INPUT_MAX) {
        writestring("line too long\n");
        return;
    }

    if (strcmp(line, "!!") == 0) {
        if (hist_count == 0) {
            writestring("no history\n");
            return;
        }
        writestring(history[hist_count - 1]);
        writestring("\n");
        char again[INPUT_MAX + 1];
        strlcpy(again, history[hist_count - 1], sizeof(again));
        handle_command(again);
        return;
    }

    hist_add(line);

    /* Alias expansion for the first token only */
    {
        char tmp[INPUT_MAX + 1];
        strlcpy(tmp, line, sizeof(tmp));
        char *tr = tmp;
        char *tcmd = next_token(&tr);
        for (int i = 0; i < alias_count; i++) {
            if (strcmp(tcmd, alias_names[i]) == 0) {
                if (alias_depth >= 4) {
                    writestring("alias recursion limit\n");
                    return;
                }
                char expanded[INPUT_MAX + 1];
                size_t el = strlen(alias_vals[i]);
                if (el >= INPUT_MAX)
                    break;
                strlcpy(expanded, alias_vals[i], sizeof(expanded));
                if (*tr) {
                    if (el + 1 + strlen(tr) >= INPUT_MAX) {
                        writestring("expanded alias too long\n");
                        return;
                    }
                    expanded[el] = ' ';
                    strlcpy(expanded + el + 1, tr, sizeof(expanded) - el - 1);
                }
                alias_depth++;
                handle_command(expanded);
                alias_depth--;
                return;
            }
        }
    }

    char *rest = line;
    char *cmd = next_token(&rest);
    if (!cmd || !cmd[0])
        return;

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
        if (rest[0] == '$' && rest[1]) {
            const char *v = var_get(rest + 1);
            writestring(v ? v : "");
            writestring("\n");
        } else {
            writestring(rest);
            writestring("\n");
        }
        return;
    }
    if (strcmp(cmd, "set") == 0) {
        while (*rest == ' ')
            rest++;
        char *eq = rest;
        while (*eq && *eq != '=')
            eq++;
        if (!*eq || eq == rest) {
            writestring("usage: set name=value\n");
            return;
        }
        *eq = '\0';
        int rc = var_set(rest, eq + 1);
        if (rc == -1)
            writestring("invalid name/value\n");
        else if (rc == -2)
            writestring("too many vars\n");
        else
            writestring("ok\n");
        return;
    }
    if (strcmp(cmd, "get") == 0) {
        char *name = next_token(&rest);
        if (!*name) {
            writestring("usage: get name\n");
            return;
        }
        const char *v = var_get(name);
        if (!v)
            writestring("(unset)\n");
        else {
            writestring(v);
            writestring("\n");
        }
        return;
    }
    if (strcmp(cmd, "vars") == 0) {
        if (var_count == 0) {
            writestring("(none)\n");
            return;
        }
        for (int i = 0; i < var_count; i++) {
            writestring(var_names[i]);
            putchar('=');
            writestring(var_vals[i]);
            writestring("\n");
        }
        return;
    }
    if (strcmp(cmd, "hexdump") == 0 || strcmp(cmd, "hd") == 0) {
        char *name = next_token(&rest);
        if (!*name) {
            writestring("usage: hexdump <file>\n");
            return;
        }
        cmd_hexdump(name);
        return;
    }
    if (strcmp(cmd, "wc") == 0) {
        char *name = next_token(&rest);
        if (!*name) {
            writestring("usage: wc <file>\n");
            return;
        }
        cmd_wc(name);
        return;
    }
    if (strcmp(cmd, "head") == 0) {
        char *a = next_token(&rest);
        char *b = next_token(&rest);
        if (!*a) {
            writestring("usage: head <file> [lines]\n");
            return;
        }
        unsigned int n = 10;
        if (*b) {
            const char *p = b;
            n = parse_uint(&p);
        }
        cmd_head(a, n);
        return;
    }
    if (strcmp(cmd, "find") == 0) {
        char *pat = next_token(&rest);
        if (!*pat) {
            writestring("usage: find <substring>\n");
            return;
        }
        if (strlen(pat) >= FS_NAME_MAX) {
            writestring("pattern too long\n");
            return;
        }
        strcpy(g_find_pat, pat);
        g_find_hits = 0;
        fs_list(find_cb);
        if (g_find_hits == 0)
            writestring("(no matches)\n");
        return;
    }
    if (strcmp(cmd, "run") == 0) {
        char *name = next_token(&rest);
        if (!*name) {
            writestring("usage: run <file>\n");
            return;
        }
        cmd_run(name);
        return;
    }
    if (strcmp(cmd, "dmesg") == 0) {
        klog_dump();
        return;
    }
    if (strcmp(cmd, "lspci") == 0) {
        pci_list();
        return;
    }
    if (strcmp(cmd, "sysinfo") == 0) {
        cmd_sysinfo();
        return;
    }
    if (strcmp(cmd, "snake") == 0) {
        game_snake();
        return;
    }
    if (strcmp(cmd, "hangman") == 0) {
        game_hangman();
        return;
    }
    if (strcmp(cmd, "dice") == 0) {
        game_dice();
        return;
    }
    if (strcmp(cmd, "rps") == 0) {
        game_rps();
        return;
    }
    if (strcmp(cmd, "disk") == 0) {
        ata_identify();
        return;
    }
    if (strcmp(cmd, "net") == 0) {
        net_info();
        return;
    }
    if (strcmp(cmd, "env") == 0) {
        if (var_count == 0) {
            writestring("(none)\n");
            return;
        }
        for (int i = 0; i < var_count; i++) {
            writestring(var_names[i]);
            putchar('=');
            writestring(var_vals[i]);
            writestring("\n");
        }
        return;
    }
    if (strcmp(cmd, "motd") == 0) {
        char buf[FS_DATA_MAX];
        size_t len = 0;
        if (fs_read("motd", buf, sizeof(buf), &len) != 0)
            writestring("(no motd)\n");
        else {
            writestring(buf);
            if (len == 0 || buf[len - 1] != '\n')
                writestring("\n");
        }
        return;
    }
    if (strcmp(cmd, "ascii") == 0) {
        for (int i = 32; i < 127; i++) {
            putchar((char)i);
            if ((i - 31) % 32 == 0)
                writestring("\n");
        }
        writestring("\n");
        return;
    }
    if (strcmp(cmd, "bin") == 0) {
        while (*rest == ' ')
            rest++;
        if (!*rest) {
            writestring("usage: bin <number>\n");
            return;
        }
        const char *p = rest;
        unsigned int n = parse_uint(&p);
        writestring("0b");
        if (n == 0) {
            writestring("0\n");
            return;
        }
        char bits[33];
        int i = 0;
        unsigned int x = n;
        while (x) {
            bits[i++] = (char)('0' + (x & 1));
            x >>= 1;
        }
        while (i--)
            putchar(bits[i]);
        writestring("\n");
        return;
    }
    if (strcmp(cmd, "prime") == 0) {
        while (*rest == ' ')
            rest++;
        if (!*rest) {
            writestring("usage: prime <n>\n");
            return;
        }
        const char *p = rest;
        unsigned int n = parse_uint(&p);
        if (n < 2) {
            writestring("not prime\n");
            return;
        }
        int is_p = 1;
        for (unsigned int d = 2; d * d <= n; d++) {
            if (n % d == 0) {
                is_p = 0;
                break;
            }
        }
        writestring(is_p ? "prime\n" : "not prime\n");
        return;
    }
    if (strcmp(cmd, "fact") == 0) {
        while (*rest == ' ')
            rest++;
        if (!*rest) {
            writestring("usage: fact <n>  (n<=12)\n");
            return;
        }
        const char *p = rest;
        unsigned int n = parse_uint(&p);
        if (n > 12) {
            writestring("too large\n");
            return;
        }
        unsigned int f = 1;
        for (unsigned int i = 2; i <= n; i++)
            f *= i;
        write_dec(f);
        writestring("\n");
        return;
    }
    if (strcmp(cmd, "countdown") == 0) {
        while (*rest == ' ')
            rest++;
        if (!*rest) {
            writestring("usage: countdown <seconds>\n");
            return;
        }
        const char *p = rest;
        unsigned int n = parse_uint(&p);
        if (n > 60)
            n = 60;
        while (n > 0) {
            write_dec(n);
            writestring("...\n");
            timer_sleep(100);
            n--;
        }
        writestring("done!\n");
        speaker_beep(880, 10);
        return;
    }
    if (strcmp(cmd, "theme") == 0) {
        while (*rest == ' ')
            rest++;
        if (strcmp(rest, "matrix") == 0)
            terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
        else if (strcmp(rest, "ocean") == 0)
            terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLUE));
        else if (strcmp(rest, "amber") == 0)
            terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK));
        else if (strcmp(rest, "danger") == 0)
            terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        else if (strcmp(rest, "default") == 0)
            terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        else {
            writestring("usage: theme <matrix|ocean|amber|danger|default>\n");
            return;
        }
        writestring("theme set\n");
        return;
    }
    if (strcmp(cmd, "tail") == 0) {
        char *name = next_token(&rest);
        char *narg = next_token(&rest);
        if (!*name) {
            writestring("usage: tail <file> [lines]\n");
            return;
        }
        unsigned int want = 10;
        if (*narg) {
            const char *p = narg;
            want = parse_uint(&p);
        }
        char buf[FS_DATA_MAX];
        size_t len = 0;
        if (fs_read(name, buf, sizeof(buf), &len) != 0) {
            writestring("file not found\n");
            return;
        }
        /* Count lines, find start */
        unsigned int lines = 0;
        for (size_t i = 0; i < len; i++)
            if (buf[i] == '\n')
                lines++;
        if (len > 0 && buf[len - 1] != '\n')
            lines++;
        unsigned int skip = (lines > want) ? (lines - want) : 0;
        unsigned int seen = 0;
        size_t i = 0;
        while (i < len && seen < skip) {
            if (buf[i++] == '\n')
                seen++;
        }
        while (i < len)
            putchar(buf[i++]);
        if (len == 0 || buf[len - 1] != '\n')
            writestring("\n");
        return;
    }
    if (strcmp(cmd, "cal") == 0) {
        struct rtc_time t;
        rtc_read(&t);
        writestring("    ");
        write_dec(t.month);
        putchar('/');
        write_dec(t.year);
        writestring("\nSu Mo Tu We Th Fr Sa\n");
        /* Sakamoto day-of-week for day 1 */
        int y = (int)t.year;
        int m = (int)t.month;
        static const int t_vals[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
        int yy = y - (m < 3);
        int dow = (yy + yy / 4 - yy / 100 + yy / 400 + t_vals[m - 1] + 1) % 7;
        int dim = 31;
        if (m == 4 || m == 6 || m == 9 || m == 11)
            dim = 30;
        else if (m == 2) {
            int leap = ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0));
            dim = leap ? 29 : 28;
        }
        for (int i = 0; i < dow; i++)
            writestring("   ");
        for (int d = 1; d <= dim; d++) {
            if (d < 10)
                putchar(' ');
            write_dec((unsigned int)d);
            putchar(' ');
            if ((dow + d) % 7 == 0)
                writestring("\n");
        }
        writestring("\n");
        return;
    }
    if (strcmp(cmd, "gfx") == 0) {
        gfx_demo();
        return;
    }
    if (strcmp(cmd, "play") == 0) {
        while (*rest == ' ')
            rest++;
        if (!*rest) {
            writestring("usage: play <notes>  e.g. play c d e c\n");
            return;
        }
        speaker_play_notes(rest);
        writestring("done\n");
        return;
    }
    if (strcmp(cmd, "fortune") == 0) {
        static const char *quotes[] = {
            "It boots, therefore it is.",
            "There is no cloud, just someone else's mainframe.",
            "Segmentation fault: core dumped... just kidding.",
            "Have you tried turning it off and on again?",
            "In URAM, nobody can hear you segfault."
        };
        writestring(quotes[rand_u32() % 5u]);
        writestring("\n");
        return;
    }
    if (strcmp(cmd, "whoami") == 0) {
        writestring(auth_user());
        writestring("\n");
        return;
    }
    if (strcmp(cmd, "login") == 0) {
        char *user = next_token(&rest);
        char *pass = next_token(&rest);
        if (!*user) {
            writestring("usage: login <user> [pass]  (default pass: os)\n");
            return;
        }
        if (!*pass)
            pass = "os";
        int rc = auth_login(user, pass);
        if (rc == -2)
            writestring("bad password\n");
        else if (rc != 0)
            writestring("login failed\n");
        else
            writestring("welcome\n");
        return;
    }
    if (strcmp(cmd, "logout") == 0) {
        auth_logout();
        writestring("logged out\n");
        return;
    }
    if (strcmp(cmd, "passwd") == 0) {
        char *pass = next_token(&rest);
        if (!*pass) {
            writestring("usage: passwd <newpass>\n");
            return;
        }
        if (auth_set_pass(pass) != 0)
            writestring("failed\n");
        else
            writestring("password updated\n");
        return;
    }
    if (strcmp(cmd, "grep") == 0) {
        char *pat = next_token(&rest);
        char *name = next_token(&rest);
        if (!*pat || !*name) {
            writestring("usage: grep <pattern> <file>\n");
            return;
        }
        char buf[FS_DATA_MAX];
        size_t len = 0;
        if (fs_read(name, buf, sizeof(buf), &len) != 0) {
            writestring("file not found\n");
            return;
        }
        size_t i = 0;
        int hits = 0;
        while (i < len) {
            char line[INPUT_MAX + 1];
            size_t n = 0;
            while (i < len && buf[i] != '\n' && n < INPUT_MAX)
                line[n++] = buf[i++];
            if (i < len && buf[i] == '\n')
                i++;
            line[n] = '\0';
            for (size_t a = 0; line[a]; a++) {
                size_t b = 0;
                while (pat[b] && line[a + b] == pat[b])
                    b++;
                if (!pat[b]) {
                    writestring(line);
                    writestring("\n");
                    hits++;
                    break;
                }
            }
        }
        if (!hits)
            writestring("(no matches)\n");
        return;
    }
    if (strcmp(cmd, "diff") == 0) {
        char *a = next_token(&rest);
        char *b = next_token(&rest);
        if (!*a || !*b) {
            writestring("usage: diff <file1> <file2>\n");
            return;
        }
        char ba[FS_DATA_MAX], bb[FS_DATA_MAX];
        size_t la = 0, lb = 0;
        if (fs_read(a, ba, sizeof(ba), &la) != 0 || fs_read(b, bb, sizeof(bb), &lb) != 0) {
            writestring("file not found\n");
            return;
        }
        if (la == lb && memcmp(ba, bb, la) == 0) {
            writestring("files identical\n");
            return;
        }
        writestring("files differ (");
        write_dec((unsigned int)la);
        writestring(" vs ");
        write_dec((unsigned int)lb);
        writestring(" bytes)\n");
        return;
    }
    if (strcmp(cmd, "sum") == 0) {
        char *name = next_token(&rest);
        if (!*name) {
            writestring("usage: sum <file>\n");
            return;
        }
        char buf[FS_DATA_MAX];
        size_t len = 0;
        if (fs_read(name, buf, sizeof(buf), &len) != 0) {
            writestring("file not found\n");
            return;
        }
        uint32_t s = 0;
        for (size_t i = 0; i < len; i++)
            s = (s * 33u) + (uint8_t)buf[i];
        writestring("0x");
        write_u32(s, 16);
        writestring("  ");
        write_dec((unsigned int)len);
        putchar(' ');
        writestring(name);
        writestring("\n");
        return;
    }
    if (strcmp(cmd, "edit") == 0) {
        char *name = next_token(&rest);
        if (!*name) {
            writestring("usage: edit <file>  (end with a single '.' line)\n");
            return;
        }
        writestring("Editing ");
        writestring(name);
        writestring(". Enter lines, '.' alone to save.\n");
        char content[FS_DATA_MAX];
        size_t clen = 0;
        content[0] = '\0';
        for (;;) {
            writestring("* ");
            char line[INPUT_MAX + 1];
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
                if (c >= 32 && c < 256 && n < INPUT_MAX)
                    line[n++] = (char)c, putchar((char)c);
            }
            line[n] = '\0';
            if (strcmp(line, ".") == 0)
                break;
            size_t need = n + 1;
            if (clen + need >= FS_DATA_MAX) {
                writestring("file too large\n");
                break;
            }
            memcpy(content + clen, line, n);
            clen += n;
            content[clen++] = '\n';
            content[clen] = '\0';
        }
        if (fs_create(name) == -2) {
            writestring("filesystem full\n");
            return;
        }
        if (fs_write(name, content) != 0)
            writestring("write failed\n");
        else
            writestring("saved\n");
        return;
    }
    if (strcmp(cmd, "alias") == 0) {
        while (*rest == ' ')
            rest++;
        if (!*rest) {
            if (alias_count == 0) {
                writestring("(none)\n");
                return;
            }
            for (int i = 0; i < alias_count; i++) {
                writestring(alias_names[i]);
                writestring("='");
                writestring(alias_vals[i]);
                writestring("'\n");
            }
            return;
        }
        char *eq = rest;
        while (*eq && *eq != '=')
            eq++;
        if (!*eq) {
            writestring("usage: alias name=command\n");
            return;
        }
        *eq = '\0';
        if (strlen(rest) >= VAR_NAME || strlen(eq + 1) >= INPUT_MAX) {
            writestring("too long\n");
            return;
        }
        for (int i = 0; i < alias_count; i++) {
            if (strcmp(alias_names[i], rest) == 0) {
                strcpy(alias_vals[i], eq + 1);
                writestring("ok\n");
                return;
            }
        }
        if (alias_count >= ALIAS_MAX) {
            writestring("too many aliases\n");
            return;
        }
        strcpy(alias_names[alias_count], rest);
        strcpy(alias_vals[alias_count], eq + 1);
        alias_count++;
        writestring("ok\n");
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
    if (strcmp(cmd, "shutdown") == 0) {
        shutdown_qemu();
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
    var_count = 0;
    alias_count = 0;
    script_depth = 0;
    alias_depth = 0;
    rng_state = timer_ticks() ^ 0xC0FFEEu;
    auth_init();

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
