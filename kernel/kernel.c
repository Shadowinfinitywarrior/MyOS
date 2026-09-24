#include "kernel.h"
#include "../include/system.h"
#include "../drivers/screen.h"
#include "../drivers/serial.h"
#include "init.h"
#include "scheduler.h"
#include "process.h"
#include "isr.h"
#include "pic.h"
#include "idt.h"
#include "timer.h"
#include "gdt.h"
#include "tss.h"
#include "paging.h"
#include "pmm.h"
#include "heap.h"
#include "../lib/printf.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
extern void init_phase8(void);
extern void syscall_init(void);
extern void syscall_init_64(void);
extern void serial_printf(const char *fmt, ...);

void kernel_panic(const char *msg, const char *file, int line){
    (void)file;
    (void)line;
    screen_set_color(15,4);
    screen_write("KERNEL PANIC\n");
    screen_write(msg);
    hang();
}

extern char __bss_start[];
extern char __bss_end[];

void kernel_main_64(void){
    __asm__ volatile ("outb %%al, %%dx" : : "a"(0x41), "d"(0xE9));
    
    char *bss = __bss_start;
    while (bss < __bss_end) {
        *bss++ = 0;
    }

    serial_init(COM1);
    serial_write('K');
    serial_write('\n');
    
    screen_init();
    serial_write('S');
    serial_write('\n');
    
    screen_write("MyOS starting...\n");
    serial_write('1');
    serial_write('\n');
    
    screen_write("Kernel loaded at 1MB\n");
    serial_write('2');
    serial_write('\n');

    gdt_init();
    tss_init();
    {
        uint64_t mmap_ptr = 0;
        uint64_t mmap_cnt = 0;
        /* Bootloader stores E820 pointer at 0x4FF8 and count at 0x4FFC */
        {
            uint32_t ptr32 = *(uint32_t*)(uintptr_t)0x4FF8;
            mmap_ptr = (uint64_t)ptr32;
        }
        mmap_cnt = *(uint32_t*)(uintptr_t)0x4FFC;
        kprintf("mmap_ptr=0x%lx cnt=%lu\n", (unsigned long)mmap_ptr, (unsigned long)mmap_cnt);
        pmm_init(mmap_ptr, mmap_cnt);
        /* Reserve the early boot stack (entry sets RSP=0x900000 and works
         * downward) so the PMM never allocates over live boot state. */
        int boot_reserved = pmm_reserve_range(0x8FE000UL, 0x902000UL);
        kprintf("[PMM] reserved boot stack frames: %d\n", boot_reserved);
    }
    heap_init(0,0);
    extern char __kernel_end[];
    kprintf("[KERNEL] __kernel_end=0x%lx\n", (unsigned long)(uintptr_t)__kernel_end);
    kprintf("[KERNEL] HEAP_BASE=0x4000000 (fixed)\n");
    paging_init();
    kprintf("paging enabled\n");
    kprintf("higher-half kernel\n");
    serial_printf("paging enabled\n");
    /* Interrupt subsystem: PIC remap + IDT + PIT timer.
     * Must run before any sti() so the live PIT IRQ0 is handled. */
    isr_init();
    pic_init();
    idt_init();
    syscall_init();
    syscall_init_64();
    timer_init(1000);

    /* Process and scheduler subsystem (PID 0 = idle) */
    process_init();
    scheduler_init();

    init_phase8();
    serial_write('3');
    serial_write('\n');
    
    screen_write("Desktop init complete\n");
    serial_write('4');
    serial_write('\n');
    
    /* Enable interrupts and start scheduler */
    sti();
    serial_write('5');
    serial_write('\n');

    /* Hand the CPU to the first runnable process (Desktop).

     * Interrupts must stay masked across the very first context switch:
     * if a timer tick landed right after scheduler_start(), the switch
     * would happen inside the ISR chain and idle would be saved at a
     * tick-dispatch position instead of here, producing a non-deterministic
     * first schedule. We only reach the sti() below once we are switched
     * back to idle, which is exactly what should re-enable interrupts. */
    cli();
    scheduler_start();
    scheduler_schedule();
    sti();

    for(;;) {
        hlt();
    }
}
