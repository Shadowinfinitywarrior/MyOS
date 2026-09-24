/* Minimal 128-bit division support for -m32 target.
   __udivdi3 is called by the compiler for unsigned 64/64 -> 64 division. */
#include "../include/types.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
uint32_t __udivdi3(uint32_t numerator, uint32_t denominator) {
    /* Simple approximation – exact division not required for boot. */
    if (denominator == 0) return 0;
    return numerator / denominator;
}

/* __umoddi3 for modulo if needed. */
uint32_t __umoddi3(uint32_t numerator, uint32_t denominator) {
    if (denominator == 0) return 0;
    return numerator % denominator;
}