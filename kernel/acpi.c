#include "acpi.h"
#include "paging.h"
#include "../include/system.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

static acpi_rsdp_t *rsdp = NULL;
static acpi_rsdt_t *rsdt = NULL;
static acpi_fadt_t *fadt = NULL;
static acpi_madt_t *madt = NULL;
static int num_cpus = 0;
static uint32_t ioapic_address = 0;

static uint8_t acpi_checksum(void *ptr, uint32_t length) {
    uint8_t sum = 0;
    uint8_t *p = (uint8_t *)ptr;
    for (uint32_t i = 0; i < length; i++) sum += p[i];
    return sum;
}

static acpi_rsdp_t *find_rsdp(void) {
    uint16_t ebda_seg = *(volatile uint16_t *)0x040E;
    uint32_t ebda = (uint32_t)ebda_seg << 4;
    for (uint32_t addr = ebda; addr < ebda + 1024; addr += 16) {
        if (memcmp((void *)addr, "RSD PTR ", 8) == 0) {
            acpi_rsdp_t *c = (acpi_rsdp_t *)addr;
            if (acpi_checksum(c, 20) == 0) return c;
        }
    }
    for (uint32_t addr = 0xE0000; addr < 0x100000; addr += 16) {
        if (memcmp((void *)addr, "RSD PTR ", 8) == 0) {
            acpi_rsdp_t *c = (acpi_rsdp_t *)addr;
            if (acpi_checksum(c, 20) == 0) return c;
        }
    }
    return NULL;
}

void *acpi_find_table(const char *sig) {
    if (!rsdt) return NULL;
    uint32_t entries = (rsdt->header.length - sizeof(acpi_sdt_header_t)) / 4;
    for (uint32_t i = 0; i < entries; i++) {
        acpi_sdt_header_t *hdr = (acpi_sdt_header_t *)(uintptr_t)rsdt->entries[i];
        if (memcmp(hdr->signature, sig, 4) == 0) {
            if (acpi_checksum(hdr, hdr->length) == 0) return hdr;
        }
    }
    return NULL;
}

void acpi_init(void) {
    rsdp = find_rsdp();
    if (!rsdp) { kprintf("[ACPI] RSDP not found!\n"); return; }
    kprintf("[ACPI] RSDP found, OEM: %.6s, revision: %u\n", rsdp->oem_id, rsdp->revision);
    rsdt = (acpi_rsdt_t *)(uintptr_t)rsdp->rsdt_address;
    paging_map((uint32_t)rsdt, (uint32_t)rsdt, PAGE_PRESENT | PAGE_WRITE);
    fadt = (acpi_fadt_t *)acpi_find_table("FACP");
    if (fadt) kprintf("[ACPI] FADT found\n");
    madt = (acpi_madt_t *)acpi_find_table("APIC");
    if (madt) {
        kprintf("[ACPI] MADT found\n");
        uint8_t *ptr = madt->entries;
        uint8_t *end = (uint8_t *)madt + madt->header.length;
        while (ptr < end) {
            uint8_t type = ptr[0];
            uint8_t length = ptr[1];
            if (type == MADT_LOCAL_APIC) {
                madt_local_apic_t *lapic = (madt_local_apic_t *)ptr;
                if (lapic->flags & 1) num_cpus++;
            } else if (type == MADT_IOAPIC) {
                madt_ioapic_t *io = (madt_ioapic_t *)ptr;
                ioapic_address = io->ioapic_addr;
            }
            ptr += length;
        }
        kprintf("[ACPI] Detected %u CPU(s)\n", num_cpus);
    }
}

void acpi_shutdown(void) {
    kprintf("[ACPI] Initiating shutdown...\n");
    if (fadt && fadt->smi_command_port && fadt->acpi_enable) {
        outb(fadt->smi_command_port, fadt->acpi_enable);
    }
    outw(0x604, 0x2000);
    hang();
}

void acpi_reboot(void) {
    kprintf("[ACPI] Rebooting...\n");
    uint8_t good = 0x02;
    while (good & 0x02) good = inb(0x64);
    outb(0x64, 0xFE);
    hang();
}

int acpi_get_num_cpus(void) { return num_cpus > 0 ? num_cpus : 1; }
uint32_t acpi_get_ioapic_addr(void) { return ioapic_address; }
