#include "klog.h"
#include "string.h"
#include "terminal.h"

#define KLOG_LINES 32
#define KLOG_WIDTH 72

static char lines[KLOG_LINES][KLOG_WIDTH];
static int count;
static int start;

void klog_init(void)
{
    count = 0;
    start = 0;
    for (int i = 0; i < KLOG_LINES; i++)
        lines[i][0] = '\0';
}

void klog(const char *msg)
{
    int idx;
    if (count < KLOG_LINES) {
        idx = count++;
    } else {
        idx = start;
        start = (start + 1) % KLOG_LINES;
    }

    size_t n = strlen(msg);
    if (n >= KLOG_WIDTH)
        n = KLOG_WIDTH - 1;
    memcpy(lines[idx], msg, n);
    lines[idx][n] = '\0';
}

void klog_dump(void)
{
    if (count == 0) {
        writestring("(log empty)\n");
        return;
    }
    int n = count;
    int i = (count < KLOG_LINES) ? 0 : start;
    for (int c = 0; c < n; c++) {
        writestring(lines[i]);
        writestring("\n");
        i = (i + 1) % KLOG_LINES;
    }
}

void klog_clear(void)
{
    count = 0;
    start = 0;
}
