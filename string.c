#include "string.h"

size_t strlen(const char *str)
{
    size_t len = 0;
    if (!str)
        return 0;
    while (str[len])
        len++;
    return len;
}

size_t strnlen(const char *str, size_t max)
{
    size_t len = 0;
    if (!str)
        return 0;
    while (len < max && str[len])
        len++;
    return len;
}

int strcmp(const char *a, const char *b)
{
    while (*a && (*a == *b)) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i] || a[i] == '\0' || b[i] == '\0')
            return (unsigned char)a[i] - (unsigned char)b[i];
    }
    return 0;
}

void *memset(void *dst, int value, size_t n)
{
    unsigned char *p = dst;
    for (size_t i = 0; i < n; i++)
        p[i] = (unsigned char)value;
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;
    for (size_t i = 0; i < n; i++)
        d[i] = s[i];
    return dst;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *x = a;
    const unsigned char *y = b;
    for (size_t i = 0; i < n; i++) {
        if (x[i] != y[i])
            return (int)x[i] - (int)y[i];
    }
    return 0;
}

char *strcpy(char *dst, const char *src)
{
    char *out = dst;
    if (!dst || !src)
        return dst;
    while ((*dst++ = *src++))
        ;
    return out;
}

size_t strlcpy(char *dst, const char *src, size_t dstsize)
{
    if (!src)
        src = "";
    size_t srclen = strlen(src);
    if (!dst || dstsize == 0)
        return srclen;
    size_t copy = srclen;
    if (copy >= dstsize)
        copy = dstsize - 1;
    memcpy(dst, src, copy);
    dst[copy] = '\0';
    return srclen;
}

size_t strlcat(char *dst, const char *src, size_t dstsize)
{
    size_t dstlen = strnlen(dst, dstsize);
    size_t srclen = strlen(src);
    if (dstlen == dstsize)
        return dstsize + srclen;
    size_t copy = srclen;
    if (dstlen + copy >= dstsize)
        copy = dstsize - dstlen - 1;
    memcpy(dst + dstlen, src, copy);
    dst[dstlen + copy] = '\0';
    return dstlen + srclen;
}

int is_valid_name(const char *name, size_t max_len)
{
    if (!name || !name[0])
        return 0;
    size_t n = strnlen(name, max_len + 1);
    if (n == 0 || n >= max_len)
        return 0;
    for (size_t i = 0; i < n; i++) {
        char c = name[i];
        int ok = (c >= 'a' && c <= 'z') ||
                 (c >= 'A' && c <= 'Z') ||
                 (c >= '0' && c <= '9') ||
                 c == '.' || c == '_' || c == '-';
        if (!ok)
            return 0;
    }
    return 1;
}

void u32toa(unsigned int value, char *buf, int base)
{
    char tmp[33];
    int i = 0;

    if (base < 2 || base > 16) {
        buf[0] = '\0';
        return;
    }

    if (value == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }

    while (value > 0) {
        unsigned int digit = value % (unsigned int)base;
        tmp[i++] = (char)(digit < 10 ? '0' + digit : 'a' + digit - 10);
        value /= (unsigned int)base;
    }

    int j = 0;
    while (i > 0)
        buf[j++] = tmp[--i];
    buf[j] = '\0';
}

void itoa(int value, char *buf, int base)
{
    if (!buf)
        return;
    if (value < 0 && base == 10) {
        *buf++ = '-';
        /* Avoid UB on INT_MIN: (-INT_MIN) overflows signed int. */
        u32toa((unsigned int)(-(value + 1)) + 1u, buf, base);
    } else {
        u32toa((unsigned int)value, buf, base);
    }
}
