#include "ahci.h"
#include "../kernel/paging.h"
#include "../kernel/pmm.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

#define MAX_AHCI_PORTS 8

typedef struct hba_port {
    uint32_t clb, clbu, fb, fbu, is, ie, cmd;
    uint32_t reserved0;
    uint32_t tfd;
    uint32_t sig;
    uint32_t ssts;
    uint32_t sctl;
    uint32_t serr;
    uint32_t sact;
    uint32_t ci;
    uint32_t sntf;
    uint32_t fbs;
    uint32_t reserved1[11];
} PACKED hba_port_t;

typedef struct hba_mem {
    uint32_t cap, ghc, is, pi, vs;
    uint32_t ccc_ctl, ccc_pts, em_loc, em_ctl, cap2, bohc;
    uint8_t reserved[0xA0-0x2C];
    hba_port_t ports[32];
} PACKED hba_mem_t;

static hba_mem_t *hba = NULL;
static int num_ports = 0;

void ahci_init(uint32_t mmio_base) {
    for (uint32_t addr = mmio_base & ~0xFFF; addr < mmio_base + 0x2000; addr += 4096) {
        paging_map(addr, addr, PAGE_PRESENT | PAGE_WRITE | PAGE_NOCACHE);
    }
    hba = (hba_mem_t *)(uintptr_t)mmio_base;
    hba->ghc |= AHCI_GHC_AE;
    uint32_t pi = hba->pi;
    kprintf("[AHCI] PI=0x%08X\n", pi);
    num_ports = 0;
    for (int i = 0; i < 32 && num_ports < MAX_AHCI_PORTS; i++) {
        if (!(pi & (1 << i))) continue;
        hba_port_t *port = &hba->ports[i];
        uint32_t ssts = port->ssts;
        uint8_t det = ssts & 0x0F;
        if (det != 3) continue;
        if (port->sig != SATA_SIG_ATA) continue;
        uint32_t cl_phys = pmm_alloc_page();
        port->clb = cl_phys;
        uint32_t fb_phys = pmm_alloc_page();
        port->fb = fb_phys;
        port->cmd = 0x300; /* FRE+ST */
        num_ports++;
        kprintf("[AHCI] Port %d active\n", i);
    }
    kprintf("[AHCI] %d ports\n", num_ports);
}

int ahci_read(int port_idx, uint32_t lba, uint32_t count, void *buf) { (void)port_idx;(void)lba;(void)count;(void)buf; return 0; }
int ahci_write(int port_idx, uint32_t lba, uint32_t count, const void *buf) { (void)port_idx;(void)lba;(void)count;(void)buf; return 0; }
int ahci_get_port_count(void) { return num_ports; }
