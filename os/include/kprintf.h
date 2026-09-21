#ifndef NEBULA_KPRINTF_H
#define NEBULA_KPRINTF_H

#include "types.h"

/*
 * A small printf for the kernel console. Supported conversions:
 *
 *   %d / %i  signed decimal        %u  unsigned decimal
 *   %x / %X  hexadecimal           %p  pointer (0x-prefixed, 8 digits)
 *   %s       string                %c  character
 *   %%       a literal percent
 *
 * Zero padding and a minimum field width are honoured, e.g. %08x.
 */

typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type)   __builtin_va_arg(ap, type)
#define va_end(ap)         __builtin_va_end(ap)

void kputchar(char c);
void kprintf(const char *fmt, ...);
void kvprintf(const char *fmt, va_list args);

/* Formats into a caller-supplied buffer; always NUL-terminates. */
int  ksnprintf(char *buf, size_t size, const char *fmt, ...);
int  kvsnprintf(char *buf, size_t size, const char *fmt, va_list args);

/* Print a message and halt the machine. Never returns. */
void panic(const char *fmt, ...) __attribute__((noreturn));

#endif /* NEBULA_KPRINTF_H */
