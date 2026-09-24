#include "gpt.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#include "../kernel/heap.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
static gpt_header_t gpt_header;
static partition_info_t partitions[GPT_MAX_PARTITIONS];
static int num_partitions=0;
static bool gpt_valid=false;
int gpt_detect(int (*read_fn)(uint64_t lba,uint32_t count,void* buf)){
    num_partitions=0; gpt_valid=false;
    uint8_t mbr[512]; if(read_fn(0,1,mbr)!=0) return -1;
    if(mbr[510]!=0x55||mbr[511]!=0xAA) return -1;
    if(mbr[450]!=0xEE) return -1;
    if(read_fn(GPT_HEADER_LBA,1,&gpt_header)!=0) return -1;
    if(gpt_header.signature!=GPT_SIGNATURE) return -1;
    gpt_valid=true;
    kprintf("[GPT] Valid GPT found, %u partitions\n",gpt_header.num_partitions);
    uint32_t es=gpt_header.partition_entry_size;
    uint32_t sectors=(gpt_header.num_partitions*es+511)/512;
    uint8_t* buf=(uint8_t*)kmalloc(sectors*512);
    if(!buf) return -1;
    read_fn(gpt_header.partition_entry_lba,sectors,buf);
    for(uint32_t i=0;i<gpt_header.num_partitions && i<GPT_MAX_PARTITIONS;i++){
        gpt_entry_t* e=(gpt_entry_t*)(buf+i*es);
        bool empty=true; for(int j=0;j<16;j++) if(e->type_guid[j]){empty=false;break;}
        if(empty) continue;
        partition_info_t* p=&partitions[num_partitions];
        p->index=i; p->start_lba=e->first_lba; p->end_lba=e->last_lba;
        p->size_sectors=e->last_lba-e->first_lba+1; p->attributes=e->attributes;
        p->valid=true; memcpy(p->type_guid,e->type_guid,16);
        for(int k=0;k<35;k++){char ch=e->name[k]; if(!ch) break; p->name[k]=ch;}
        p->name[35]=0; num_partitions++;
    }
    kfree(buf);
    return num_partitions;
}
int gpt_get_partition_count(void){ return num_partitions; }
partition_info_t* gpt_get_partition(int i){ if(i>=0&&i<num_partitions) return &partitions[i]; return NULL; }
void gpt_list_partitions(void){
    if(!gpt_valid){ kprintf("No GPT\n"); return; }
    kprintf("\n # Start End Size(MB) Name\n");
    for(int i=0;i<num_partitions;i++){
        partition_info_t* p=&partitions[i];
        uint64_t mb=(p->size_sectors*512)/(1024*1024);
        kprintf("%2d %lu %lu %lu %s\n",p->index,p->start_lba,p->end_lba,mb,p->name);
    }
}
