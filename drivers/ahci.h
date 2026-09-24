#ifndef AHCI_H
#define AHCI_H

#include "../include/types.h"

#define AHCI_CAP 0x00
#define AHCI_GHC 0x04
#define AHCI_IS  0x08
#define AHCI_PI  0x0C
#define AHCI_VS  0x10

#define AHCI_GHC_AE BIT(31)
#define AHCI_GHC_IE BIT(1)
#define AHCI_GHC_HR BIT(0)

#define SATA_SIG_ATA 0x00000101

typedef struct ahci_cmd_header {
    uint8_t cfl:5;
    uint8_t a:1;
    uint8_t w:1;
    uint8_t p:1;
    uint8_t r:1;
    uint8_t b:1;
    uint8_t c:1;
    uint8_t reserved1:1;
    uint8_t pmp:4;
    uint16_t prdtl;
    uint32_t prdbc;
    uint32_t ctba;
    uint32_t ctbau;
    uint32_t reserved2[4];
} PACKED ahci_cmd_header_t;

typedef struct ahci_prd {
    uint32_t dba;
    uint32_t dbau;
    uint32_t reserved;
    uint32_t dbc:22;
    uint32_t reserved2:9;
    uint32_t i:1;
} PACKED ahci_prd_t;

typedef struct ahci_cmd_table {
    uint8_t cfis[64];
    uint8_t acmd[16];
    uint8_t reserved[48];
    ahci_prd_t prdt[8];
} PACKED ahci_cmd_table_t;

void ahci_init(uint32_t mmio_base);
int ahci_read(int port, uint32_t lba, uint32_t count, void *buf);
int ahci_write(int port, uint32_t lba, uint32_t count, const void *buf);
int ahci_get_port_count(void);

#endif
