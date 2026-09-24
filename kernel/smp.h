#ifndef SMP_H
#define SMP_H

#include "../include/types.h"

#define MAX_CPUS 32

typedef struct {
    uint8_t apic_id;
    bool    active;
    bool    is_bsp;
} cpu_info_t;

typedef struct smp_apic_id {
    uint8_t id;
    uint8_t enabled;
} smp_apic_id_t;

typedef struct ap_startup {
    uint32_t stack;
    void     (*trampoline)(void);
} ap_startup_t;

void smp_init(void);
int smp_get_cpu_count(void);
uint8_t smp_get_current_id(void);
bool smp_start_ap(uint8_t apic_id);
void smp_ap_entry(void);
void smp_send_init_ipr(uint8_t apic_id);
void smp_send_startup_ipr(uint8_t apic_id, uint32_t vector);
cpu_info_t *smp_get_cpu(int index);

#endif
