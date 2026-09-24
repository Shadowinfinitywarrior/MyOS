#include "init.h"
#include "../lib/printf.h"
#include "process.h"
#include "../fs/vfs.h"
#include "heap.h"
void init_start(void){
    vfs_node_t *node = vfs_resolve_path("/sbin/init");
    if(node){
        uint8_t *data = kmalloc(node->length);
        vfs_read(node,0,node->length,data);
        process_create_user("init",data,node->length);
        kfree(data);
    }
}
