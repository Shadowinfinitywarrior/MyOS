/*
 * MicroPython HAL port for MyOS
 * Provides low-level hardware access for MicroPython
 */

#ifndef MPHALPORT_H
#define MPHALPORT_H

#include "types.h"
#include "system.h"

// Standard types
typedef int mp_int_t;
typedef unsigned int mp_uint_t;
typedef long mp_off_t;

// Memory allocation - use kernel heap
#define MP_PLAT_ALLOC(sz, align) kmalloc(sz)
#define MP_PLAT_FREE(ptr) kfree(ptr)

// Atomic operations
#define MICROPY_BEGIN_ATOMIC_SECTION() ({ uint64_t flags = read_eflags(); cli(); flags; })
#define MICROPY_END_ATOMIC_SECTION(flags) write_eflags(flags)

// MP_HAL delay functions
void mp_hal_delay_ms(mp_uint_t ms);
void mp_hal_delay_us(mp_uint_t us);

// Time functions
mp_uint_t mp_hal_ticks_ms(void);
mp_uint_t mp_hal_ticks_us(void);
mp_uint_t mp_hal_ticks_cpu(void);

// Watchdog reset
void mp_hal_wdt_reset(void);

// I/O functions
void mp_hal_stdout_tx_strn(const char *str, size_t len);
void mp_hal_stdout_tx_str(const char *str);
int mp_hal_stdin_rx_chr(void);

// REPL support
void mp_hal_stdout_tx_strn_cooked(const char *str, size_t len);

// Interrupt handling
void mp_hal_disable_irq(void);
void mp_hal_enable_irq(void);

// Emergency exception buffer
void mp_init_emergency_exception_buf(void);

#endif // MPHALPORT_H