/* The few <string.h> functions the core uses, for the freestanding WebAssembly build (see libc_min.c). */
#ifndef BRICK_MINLIBC_STRING_H
#define BRICK_MINLIBC_STRING_H

#include <stddef.h>

void *memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
void *memset(void *dst, int c, size_t n);
int memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, size_t n);

#endif
