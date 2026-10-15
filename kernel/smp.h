#ifndef SMP_H
#define SMP_H

#include "../include/types.h"
#include "../include/system.h"
#include "../include/spinlock.h"

#define MAX_CPUS 256
#define IPI_VECTOR_RESCHEDULE 0xFE

struct process;

typedef struct runqueue {
    spinlock_t lock;
    struct process *ready_head;
    uint32_t task_count;
    struct process *idle_task;
    struct process *current;
    uint64_t tick_count;
} runqueue_t;

typedef struct per_cpu_data {
    struct per_cpu_data *self;
    uint32_t cpu_id;
    uint32_t apic_id;
    runqueue_t rq;
    struct process *current_task;
    uint64_t irq_nesting;
    uint64_t kernel_stack_top;
    uint64_t tss_rsp0;
    uint8_t cache_line_pad[64 - sizeof(void*)*6];
} __attribute__((aligned(64))) per_cpu_data_t;

extern per_cpu_data_t per_cpu_data[MAX_CPUS];
uint8_t smp_get_current_id(void);

static inline per_cpu_data_t *get_this_cpu(void) {
    uint32_t id = (uint32_t)smp_get_current_id();
    if (id >= (uint32_t)MAX_CPUS) id = 0;
    return &per_cpu_data[id];
}

#define THIS_CPU() get_this_cpu()

typedef struct {
    uint8_t apic_id;
    bool    active;
    bool    is_bsp;
} cpu_info_t;

typedef struct smp_apic_id {
    uint8_t id;
    uint8_t enabled;
} smp_apic_id_t;

void smp_init(void);
int smp_get_cpu_count(void);
uint8_t smp_get_current_id(void);
bool smp_start_ap(uint8_t apic_id);
void smp_ap_entry(void);
cpu_info_t *smp_get_cpu(int index);

void ipi_reschedule(uint32_t target_cpu);
void ipi_reschedule_handler(registers_t *regs);

#endif
