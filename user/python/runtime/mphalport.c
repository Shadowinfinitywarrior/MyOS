/*
 * MicroPython HAL port implementation for MyOS
 * Provides low-level hardware access for MicroPython
 */

#include "mphalport.h"
#include "../../../user/libc.h"
#include "../../../drivers/keyboard.h"
#include "../../../drivers/screen.h"
#include "../../../kernel/timer.h"

extern char keyboard_getchar(void);

// Delay functions using kernel timer
void mp_hal_delay_ms(mp_uint_t ms) {
    sleep_ms(ms);
}

void mp_hal_delay_us(mp_uint_t us) {
    // Busy wait for microseconds
    uint64_t start = timer_get_ticks();
    // Assuming timer ticks are in milliseconds, convert
    while ((timer_get_ticks() - start) * 1000 < us) {
        // Spin
        __asm__ __volatile__("pause");
    }
}

// Time functions - return milliseconds since boot
mp_uint_t mp_hal_ticks_ms(void) {
    return timer_get_ticks();
}

mp_uint_t mp_hal_ticks_us(void) {
    return timer_get_ticks() * 1000;
}

mp_uint_t mp_hal_ticks_cpu(void) {
    // Use TSC if available, otherwise use timer ticks
    uint64_t tsc;
    __asm__ __volatile__("rdtsc" : "=A"(tsc));
    return (mp_uint_t)tsc;
}

void mp_hal_wdt_reset(void) {
    // No hardware watchdog in MyOS yet
}

// I/O functions - use kernel console
void mp_hal_stdout_tx_strn(const char *str, size_t len) {
    write(1, str, len);
}

void mp_hal_stdout_tx_str(const char *str) {
    while (*str) {
        putchar(*str++);
    }
}

void mp_hal_stdout_tx_strn_cooked(const char *str, size_t len) {
    // For REPL - convert \n to \r\n
    for (size_t i = 0; i < len; i++) {
        if (str[i] == '\n') {
            putchar('\r');
        }
        putchar(str[i]);
    }
}

int mp_hal_stdin_rx_chr(void) {
    // Non-blocking check for keyboard input
    char c = keyboard_getchar();
    return c ? c : -1;
}

void mp_hal_disable_irq(void) {
    cli();
}

void mp_hal_enable_irq(void) {
    sti();
}

void mp_init_emergency_exception_buf(void) {
    // Emergency exception buffer is statically allocated in MicroPython
    // We don't need to do anything special here
}