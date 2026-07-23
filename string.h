#ifndef STRING_H
#define STRING_H

#include <stddef.h>

size_t strlen(const char *str);
size_t strnlen(const char *str, size_t max);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
void *memset(void *dst, int value, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
int memcmp(const void *a, const void *b, size_t n);
char *strcpy(char *dst, const char *src);
size_t strlcpy(char *dst, const char *src, size_t dstsize);
size_t strlcat(char *dst, const char *src, size_t dstsize);
int is_valid_name(const char *name, size_t max_len);
void itoa(int value, char *buf, int base);
void u32toa(unsigned int value, char *buf, int base);

#endif
