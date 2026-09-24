#ifndef NE2K_H
#define NE2K_H

#include "../include/types.h"

#define NE2K_IO_BASE 0x300
#define NE2K_IRQ     5
#define NE2K_MEM_BASE 0xD0000

#define NE2K_CMD_REG    0x00
#define NE2K_DATA_PTR   0x01
#define NE2K_BCNT_LO    0x02
#define NE2K_BCNT_HI    0x03
#define NE2K_RESET      0x06
#define NE2K_INTR_MASK  0x07

#define NE2K_CMD_WRITE  0x40
#define NE2K_CMD_READ   0x80
#define NE2K_CMD_STOP   0x01
#define NE2K_CMD_START  0x02

typedef struct ne2k_dev {
    uint16_t io_base;
    uint8_t  irq;
    uint8_t  mac[6];
    uint8_t *shared_ram;
    uint16_t page_current;
} ne2k_dev_t;

bool    ne2k_detect(void);
int     ne2k_init(uint16_t io_base, uint8_t irq);
int     ne2k_send(ne2k_dev_t *dev, uint8_t *buf, uint16_t len);
int     ne2k_recv(ne2k_dev_t *dev, uint8_t *buf, uint16_t max_len);
void    ne2k_irq_handler(void);

#endif
