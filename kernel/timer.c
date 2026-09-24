#include "timer.h"
#include "../kernel/heap.h"
#include "../lib/printf.h"
#include "../kernel/pic.h"
#include "../kernel/isr.h"
#include "../kernel/scheduler.h"
#include "apic.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

#define MAX_CPUS 256
static volatile uint64_t per_cpu_ticks[MAX_CPUS] = {0};
static uint32_t timer_frequency_hz = 1000;
static int use_apic = 0;
static struct timer_event *timer_list = NULL;

static void timer_callback(registers_t *regs) {
    (void)regs;
    uint8_t cpu = use_apic ? apic_get_id() : 0;
    per_cpu_ticks[cpu]++;
    /* EOI first: scheduler_tick may context-switch inside this ISR and the
     * preempted task's EOI would otherwise be deferred until it is resumed,
     * leaving the LAPIC IS bit set and stalling further periodic ticks. */
    if (use_apic) {
        apic_eoi();
    }
    scheduler_tick();
    timer_tick_process();
}

static uint64_t timer_snapshot_ticks(void) {
    uint8_t cpu = use_apic ? apic_get_id() : 0;
    uint64_t eflags;
    __asm__ volatile("pushfq; popq %0" : "=r"(eflags));
    __asm__ volatile("cli");
    uint64_t t = per_cpu_ticks[cpu];
    __asm__ volatile("pushq %0; popfq" :: "r"(eflags));
    return t;
}

void timer_init(uint32_t frequency) {
    timer_frequency_hz = frequency;
    if (!apic_detect()) {
        use_apic = 0;
        kprintf("[TIMER] APIC not detected, falling back to PIT\n");
        isr_register_handler(IRQ0, timer_callback);
        pic_clear_mask(0);
        uint32_t divisor = 1193180 / frequency;
        outb(0x43, 0x36);
        outb(0x40, divisor & 0xFF);
        outb(0x40, (divisor >> 8) & 0xFF);
        kprintf("[TIMER] Initialized PIT at %u Hz\n", frequency);
        return;
    }
    use_apic = 1;
    apic_init();
    isr_register_handler(32, timer_callback);
    volatile uint32_t *apic = (volatile uint32_t *)0xFEE00000;
    apic[APIC_TIMER_DIV/4] = 0x03;
    uint32_t count = timer_frequency_hz;
    apic[APIC_TIMER_INIT/4] = count;
    apic[APIC_LVT_TIMER/4] = 0x20 | (1 << 17);
    kprintf("[TIMER] APIC timer initialized at %u Hz (approx)\n", frequency);
}

static inline uint64_t current_ticks(void) {
    uint8_t cpu = use_apic ? apic_get_id() : 0;
    return per_cpu_ticks[cpu];
}
uint64_t timer_get_ticks(void) { return current_ticks(); }
uint32_t timer_get_seconds(void) { return (uint32_t)(current_ticks() / 1000); }

void timer_sleep(uint32_t ms) {
    if (ms == 0) {
        return;
    }
    uint64_t start = current_ticks();
    while ((current_ticks() - start) < ms) {
        __asm__ volatile("sti; hlt");
    }
}

uint32_t timer_get_ms(void) {
    uint64_t t = timer_snapshot_ticks();
    uint64_t ms = t * 1000ULL / timer_frequency_hz;
    return (uint32_t)ms;
}

uint64_t timer_get_ms64(void) {
    uint64_t t = timer_snapshot_ticks();
    uint64_t ms = t * 1000ULL / timer_frequency_hz;
    return ms;
}

int timer_add(uint32_t delay_ms, timer_callback_t cb, void *arg) {
    if (!cb) return -1;
    struct timer_event *ev = (struct timer_event *)kmalloc(sizeof(struct timer_event));
    if (!ev) return -1;
    uint64_t eflags;
    __asm__ volatile("pushfq; popq %0" : "=r"(eflags));
    __asm__ volatile("cli");
    uint64_t now = timer_get_ms64();
    ev->expire_ms = now + delay_ms;
    ev->cb = cb;
    ev->arg = arg;
    ev->next = timer_list;
    timer_list = ev;
    __asm__ volatile("pushq %0; popfq" :: "r"(eflags));
    return 0;
}

void timer_del(struct timer_event *ev) {
    if (!ev) return;
    uint64_t eflags;
    __asm__ volatile("pushfq; popq %0" : "=r"(eflags));
    __asm__ volatile("cli");
    struct timer_event **cur = &timer_list;
    while (*cur) {
        if (*cur == ev) {
            *cur = ev->next;
            __asm__ volatile("pushq %0; popfq" :: "r"(eflags));
            kfree(ev);
            return;
        }
        cur = &(*cur)->next;
    }
    __asm__ volatile("pushq %0; popfq" :: "r"(eflags));
}

void timer_tick_process(void) {
    uint64_t now = timer_get_ms64();
    struct timer_event **cur = &timer_list;
    while (*cur) {
        struct timer_event *ev = *cur;
        if (ev->expire_ms <= now) {
            *cur = ev->next;
            if (ev->cb) ev->cb(ev->arg);
            kfree(ev);
        } else {
            cur = &(*cur)->next;
        }
    }
}
