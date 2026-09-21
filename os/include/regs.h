#ifndef NEBULA_REGS_H
#define NEBULA_REGS_H

#include "types.h"

/*
 * The CPU state an interrupt stub hands to C code.
 *
 * The field order mirrors the exact push order in isr.S, from the lowest
 * stack address upwards:
 *
 *   pusha (edi first, eax last) -> ds -> int_no -> err_code -> CPU frame
 *
 * esp_dummy is the value pusha recorded for ESP; it describes the stack
 * mid-push and is never useful, so it is named to discourage its use.
 * useresp/ss are only present when the interrupt crossed a privilege
 * boundary, so they must not be read for a ring-0 interrupt.
 */
struct registers {
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
    uint32_t ds;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags;
    uint32_t useresp, ss;
};

#endif /* NEBULA_REGS_H */
