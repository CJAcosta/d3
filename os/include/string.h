#ifndef NEBULA_STRING_H
#define NEBULA_STRING_H

#include "types.h"

void  *memset(void *dest, int value, size_t count);
void  *memcpy(void *dest, const void *src, size_t count);
void  *memmove(void *dest, const void *src, size_t count);
int    memcmp(const void *a, const void *b, size_t count);

size_t strlen(const char *s);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
char  *strcpy(char *dest, const char *src);
char  *strncpy(char *dest, const char *src, size_t n);
size_t strlcpy(char *dest, const char *src, size_t size);
char  *strchr(const char *s, int c);

/* Parse a non-negative decimal integer; returns -1 on malformed input. */
int    atoi_safe(const char *s);

#endif /* NEBULA_STRING_H */
