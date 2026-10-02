#include "../include/mydp/protocol.h"
#include "../include/stddef.h"
#include "../include/stdint.h"

int main(void)
{
    mydp_header_t hdr = {0};
    hdr.magic = MYDP_MAGIC;
    hdr.version = MYDP_VERSION;
    hdr.type = MYDP_HELLO;
    hdr.length = 0;
    hdr.seq = 1;

    (void)hdr;
    /* Stub: would send via socket */
    return 0;
}
