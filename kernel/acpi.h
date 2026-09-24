#ifndef ACPI_H
#define ACPI_H

#include "../include/types.h"

typedef struct acpi_rsdp {
    char     signature[8];
    uint8_t  checksum;
    char     oem_id[6];
    uint8_t  revision;
    uint32_t rsdt_address;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t  ext_checksum;
    uint8_t  reserved[3];
} PACKED acpi_rsdp_t;

typedef struct acpi_sdt_header {
    char     signature[4];
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    char     oem_id[6];
    char     oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} PACKED acpi_sdt_header_t;

typedef struct acpi_rsdt {
    acpi_sdt_header_t header;
    uint32_t          entries[];
} PACKED acpi_rsdt_t;

typedef struct acpi_fadt {
    acpi_sdt_header_t header;
    uint32_t firmware_ctrl;
    uint32_t dsdt;
    uint8_t  reserved;
    uint8_t  preferred_pm_profile;
    uint16_t sci_interrupt;
    uint32_t smi_command_port;
    uint8_t  acpi_enable;
    uint8_t  acpi_disable;
    uint8_t  s4bios_req;
    uint8_t  pstate_control;
    uint32_t pm1a_event_block;
    uint32_t pm1b_event_block;
    uint32_t pm1a_control_block;
    uint32_t pm1b_control_block;
    uint32_t pm2_control_block;
    uint32_t pm_timer_block;
    uint32_t gpe0_block;
    uint32_t gpe1_block;
    uint8_t  pm1_event_length;
    uint8_t  pm1_control_length;
    uint8_t  pm2_control_length;
    uint8_t  pm_timer_length;
    uint8_t  gpe0_length;
    uint8_t  gpe1_length;
    uint8_t  gpe1_base;
    uint8_t  cstate_control;
    uint16_t worst_c2_latency;
    uint16_t worst_c3_latency;
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t  duty_offset;
    uint8_t  duty_width;
    uint8_t  day_alarm;
    uint8_t  month_alarm;
    uint8_t  century;
    uint16_t boot_arch_flags;
    uint8_t  reserved2;
    uint32_t flags;
} PACKED acpi_fadt_t;

typedef struct acpi_madt {
    acpi_sdt_header_t header;
    uint32_t          local_apic_addr;
    uint32_t          flags;
    uint8_t           entries[];
} PACKED acpi_madt_t;

#define MADT_LOCAL_APIC     0
#define MADT_IOAPIC         1
#define MADT_ISO            2
#define MADT_NMI            4
#define MADT_LOCAL_APIC64   5

typedef struct madt_local_apic {
    uint8_t  type;
    uint8_t  length;
    uint8_t  acpi_id;
    uint8_t  apic_id;
    uint32_t flags;
} PACKED madt_local_apic_t;

typedef struct madt_ioapic {
    uint8_t  type;
    uint8_t  length;
    uint8_t  ioapic_id;
    uint8_t  reserved;
    uint32_t ioapic_addr;
    uint32_t gsi_base;
} PACKED madt_ioapic_t;

void acpi_init(void);
void acpi_shutdown(void);
void acpi_reboot(void);
int  acpi_get_num_cpus(void);
uint32_t acpi_get_ioapic_addr(void);

void *acpi_find_table(const char *sig);
#endif
