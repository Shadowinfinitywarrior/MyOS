#include "libc.h"

int main(void) {
    puts("hwinfo - hardware information");
    puts("CPU: x86_64 (QEMU)");
    puts("Memory: 2GB");
    puts("Graphics: Bochs VGA");
    puts("Network: Virtio-Net");
    puts("Storage: Virtio-Blk");
    puts("Audio: AC97/HDA");
    return 0;
}
