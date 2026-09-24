/* Stub definitions for SMP trampoline symbols.
   The real trampoline is hand-written assembly loaded by firmware;
   these weak symbols prevent linker errors when SMP is not used. */
#include "../include/system.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
__attribute__((weak)) uint8_t TRAMPOLINE_START[] = { 0xe9, 0x00, 0x00, 0x00 };
__attribute__((weak)) uint8_t TRAMPOLINE_END[]   = { 0xe9, 0x00, 0x00, 0x00 };