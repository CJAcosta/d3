#ifndef NEBULA_TYPES_H
#define NEBULA_TYPES_H

/*
 * Freestanding fixed-width types. We build with -nostdinc, so the kernel
 * cannot pull in the host's stdint.h -- those headers describe the host
 * userland, not this target.
 */

typedef unsigned char      uint8_t;
typedef signed char        int8_t;
typedef unsigned short     uint16_t;
typedef signed short       int16_t;
typedef unsigned int       uint32_t;
typedef signed int         int32_t;
typedef unsigned long long uint64_t;
typedef signed long long   int64_t;

typedef uint32_t           size_t;
typedef int32_t            ssize_t;
typedef uint32_t           uintptr_t;

typedef int                bool;
#define true  1
#define false 0

#define NULL ((void *)0)

/* Physical and virtual addresses are distinct concepts; name them so. */
typedef uint32_t physaddr_t;
typedef uint32_t virtaddr_t;

#endif /* NEBULA_TYPES_H */
