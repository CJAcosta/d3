/*
 * string.c -- the handful of libc string routines the kernel needs.
 *
 * GCC emits calls to memset/memcpy/memmove/memcmp for struct assignment
 * and array initialisation even with -ffreestanding, so these four must
 * exist under exactly these names no matter what else we use.
 */

#include "string.h"

void *memset(void *dest, int value, size_t count)
{
    uint8_t *d = (uint8_t *)dest;
    uint8_t  v = (uint8_t)value;

    while (count--)
        *d++ = v;

    return dest;
}

void *memcpy(void *dest, const void *src, size_t count)
{
    uint8_t       *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;

    while (count--)
        *d++ = *s++;

    return dest;
}

void *memmove(void *dest, const void *src, size_t count)
{
    uint8_t       *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;

    if (d == s || count == 0)
        return dest;

    /* Copy backwards when the regions overlap with dest above src. */
    if (d < s) {
        while (count--)
            *d++ = *s++;
    } else {
        d += count;
        s += count;
        while (count--)
            *--d = *--s;
    }

    return dest;
}

int memcmp(const void *a, const void *b, size_t count)
{
    const uint8_t *p = (const uint8_t *)a;
    const uint8_t *q = (const uint8_t *)b;

    while (count--) {
        if (*p != *q)
            return (int)*p - (int)*q;
        p++;
        q++;
    }

    return 0;
}

size_t strlen(const char *s)
{
    size_t n = 0;

    while (s[n])
        n++;

    return n;
}

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }

    return (int)(uint8_t)*a - (int)(uint8_t)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
    while (n && *a && *a == *b) {
        a++;
        b++;
        n--;
    }

    if (n == 0)
        return 0;

    return (int)(uint8_t)*a - (int)(uint8_t)*b;
}

char *strcpy(char *dest, const char *src)
{
    char *out = dest;

    while ((*dest++ = *src++))
        ;

    return out;
}

char *strncpy(char *dest, const char *src, size_t n)
{
    size_t i = 0;

    for (; i < n && src[i]; i++)
        dest[i] = src[i];
    for (; i < n; i++)
        dest[i] = '\0';

    return dest;
}

/*
 * strlcpy semantics: always NUL-terminates (given size > 0) and returns
 * the length of src, so callers can detect truncation. Safer than
 * strncpy, which may leave the destination unterminated.
 */
size_t strlcpy(char *dest, const char *src, size_t size)
{
    size_t len = strlen(src);

    if (size > 0) {
        size_t copy = (len >= size) ? size - 1 : len;
        memcpy(dest, src, copy);
        dest[copy] = '\0';
    }

    return len;
}

char *strchr(const char *s, int c)
{
    while (*s) {
        if (*s == (char)c)
            return (char *)s;
        s++;
    }

    return (c == '\0') ? (char *)s : NULL;
}

int atoi_safe(const char *s)
{
    int value = 0;

    if (!s || !*s)
        return -1;

    while (*s) {
        if (*s < '0' || *s > '9')
            return -1;
        value = value * 10 + (*s - '0');
        if (value < 0)          /* overflowed into the sign bit */
            return -1;
        s++;
    }

    return value;
}
