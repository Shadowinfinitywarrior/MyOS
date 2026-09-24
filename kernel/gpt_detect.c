#include "gpt.h"
#include "../drivers/ata.h"
#include "../lib/printf.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

static int ata_read_callback(uint64_t lba, uint32_t count, void* buf){
    /* Use first ATA device for now */
    extern ata_device_t* ata_devices[];
    if(!ata_devices[0]) return -1;
    return ata_read_sectors(ata_devices[0], (uint32_t)lba, (uint8_t)count, buf);
}

void gpt_init_storage(void){
    int n = gpt_detect(ata_read_callback);
    if(n>0){
        kprintf("[GPT] Detected %d partitions\n", n);
        gpt_list_partitions();
    } else {
        kprintf("[GPT] No GPT found, fallback to MBR\n");
    }
}
