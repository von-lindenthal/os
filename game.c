#include "game.h"
#include "terminal.h"
#include "keyboard.h"
#include "timer.h"
#include "speaker.h"
#include "string.h"
#include <stdint.h>

#define GW 40
#define GH 20

static void draw_cell(int x, int y, char c, uint8_t color)
{
    /* Map into VGA around center of screen */
    int vx = x + 20;
    int vy = y + 2;
    if (vx < 0 || vx >= 80 || vy < 0 || vy >= 25)
        return;
    volatile uint16_t *vga = (volatile uint16_t *)0xB8000;
    vga[vy * 80 + vx] = (uint16_t)((uint16_t)c | ((uint16_t)color << 8));
}

void game_snake(void)
{
    int sx[GW * GH];
    int sy[GW * GH];
    int len = 3;
    int dx = 1, dy = 0;
    int food_x = 10, food_y = 8;
    int score = 0;
    int alive = 1;

    sx[0] = 5;
    sy[0] = 5;
    sx[1] = 4;
    sy[1] = 5;
    sx[2] = 3;
    sy[2] = 5;

    terminal_clear();
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    writestring("SNAKE  WASD/arrows move  q quits\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));

    input_drain();

    while (alive) {
        /* Draw border + field */
        for (int y = 0; y < GH; y++) {
            for (int x = 0; x < GW; x++)
                draw_cell(x, y, ' ', vga_entry_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK));
        }
        for (int x = 0; x < GW; x++) {
            draw_cell(x, 0, '#', vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
            draw_cell(x, GH - 1, '#', vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        }
        for (int y = 0; y < GH; y++) {
            draw_cell(0, y, '#', vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
            draw_cell(GW - 1, y, '#', vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        }

        draw_cell(food_x, food_y, '*', vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        for (int i = 0; i < len; i++)
            draw_cell(sx[i], sy[i], i == 0 ? 'O' : 'o',
                      vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));

        /* Score line */
        volatile uint16_t *vga = (volatile uint16_t *)0xB8000;
        const char *label = "Score: ";
        int col = 2;
        for (int i = 0; label[i]; i++)
            vga[22 * 80 + col++] =
                (uint16_t)((uint16_t)label[i] | ((uint16_t)vga_entry_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK) << 8));
        char nbuf[12];
        u32toa((unsigned int)score, nbuf, 10);
        for (int i = 0; nbuf[i]; i++)
            vga[22 * 80 + col++] =
                (uint16_t)((uint16_t)nbuf[i] | ((uint16_t)vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK) << 8));

        /* Input (non-blocking for a short window) */
        uint32_t start = timer_ticks();
        while (timer_ticks() - start < 8) {
            int c = keyboard_read_code();
            if (!c)
                continue;
            if (c == 'q' || c == 'Q') {
                alive = 0;
                break;
            }
            if ((c == 'w' || c == 'W' || c == KEY_UP) && dy == 0) {
                dx = 0;
                dy = -1;
            } else if ((c == 's' || c == 'S' || c == KEY_DOWN) && dy == 0) {
                dx = 0;
                dy = 1;
            } else if ((c == 'a' || c == 'A') && dx == 0) {
                dx = -1;
                dy = 0;
            } else if ((c == 'd' || c == 'D') && dx == 0) {
                dx = 1;
                dy = 0;
            }
        }
        if (!alive)
            break;

        int nx = sx[0] + dx;
        int ny = sy[0] + dy;
        if (nx <= 0 || nx >= GW - 1 || ny <= 0 || ny >= GH - 1) {
            speaker_beep(220, 8);
            break;
        }
        for (int i = 0; i < len; i++) {
            if (sx[i] == nx && sy[i] == ny) {
                speaker_beep(220, 8);
                alive = 0;
                break;
            }
        }
        if (!alive)
            break;

        for (int i = len; i > 0; i--) {
            sx[i] = sx[i - 1];
            sy[i] = sy[i - 1];
        }
        sx[0] = nx;
        sy[0] = ny;

        if (nx == food_x && ny == food_y) {
            if (len < GW * GH - 1)
                len++;
            score += 10;
            speaker_beep(880, 2);
            food_x = 1 + (int)(timer_ticks() % (uint32_t)(GW - 2));
            food_y = 1 + (int)((timer_ticks() / 3) % (uint32_t)(GH - 2));
        }
    }

    terminal_clear();
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
    writestring("Game over. Score: ");
    write_dec((unsigned int)score);
    writestring("\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    input_drain();
}

void game_dice(void)
{
    unsigned int n = (timer_ticks() * 2654435761u) % 6u + 1u;
    writestring("You rolled: ");
    write_dec(n);
    writestring("\n");
    speaker_beep(600 + n * 40, 6);
}

void game_hangman(void)
{
    static const char *words[] = {
        "kernel", "qemu", "memory", "shell", "interrupt",
        "pixel", "driver", "thread", "binary", "socket"
    };
    unsigned int wi = timer_ticks() % 10u;
    const char *word = words[wi];
    char shown[32];
    size_t wlen = strlen(word);
    for (size_t i = 0; i < wlen; i++)
        shown[i] = '_';
    shown[wlen] = '\0';

    int lives = 6;
    char guessed[32];
    size_t gcount = 0;
    guessed[0] = '\0';

    writestring("Hangman! Type letters. q quits.\n");

    while (lives > 0) {
        writestring("Word: ");
        for (size_t i = 0; i < wlen; i++) {
            putchar(shown[i]);
            putchar(' ');
        }
        writestring("  lives=");
        write_dec((unsigned int)lives);
        writestring("\nletter> ");

        char buf[8];
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
        if (buf[0] == 'q' && buf[1] == '\0') {
            writestring("quit\n");
            return;
        }
        if (n != 1 || buf[0] < 'a' || buf[0] > 'z') {
            if (n == 1 && buf[0] >= 'A' && buf[0] <= 'Z')
                buf[0] = (char)(buf[0] - 'A' + 'a');
            else {
                writestring("type one letter\n");
                continue;
            }
        }

        char ch = buf[0];
        int already = 0;
        for (size_t i = 0; i < gcount; i++) {
            if (guessed[i] == ch)
                already = 1;
        }
        if (already) {
            writestring("already guessed\n");
            continue;
        }
        if (gcount < sizeof(guessed) - 1)
            guessed[gcount++] = ch;

        int hit = 0;
        for (size_t i = 0; i < wlen; i++) {
            if (word[i] == ch) {
                shown[i] = ch;
                hit = 1;
            }
        }
        if (!hit) {
            lives--;
            writestring("miss\n");
            speaker_beep(200, 6);
        } else {
            writestring("hit\n");
            speaker_beep(800, 4);
        }

        int done = 1;
        for (size_t i = 0; i < wlen; i++) {
            if (shown[i] == '_')
                done = 0;
        }
        if (done) {
            writestring("You win! Word was ");
            writestring(word);
            writestring("\n");
            return;
        }
    }
    writestring("You lose. Word was ");
    writestring(word);
    writestring("\n");
}
