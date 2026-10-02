#include "../lib/string.h"
#include "../include/system.h"
#include "socket.h"
#include "../drivers/keyboard.h"
#include "../drivers/mouse.h"
#include "../drivers/usb.h"
#include "../drivers/framebuffer.h"
#include "gpt.h"
#include "process.h"
#include "scheduler.h"
#include "../lib/printf.h"
#include "../drivers/serial.h"
#include "../drivers/virtio.h"
#include "../drivers/virtio_blk.h"
#include "../drivers/virtio_net.h"
#include "../fs/vfs.h"
#include "../fs/devfs.h"
#include "../fs/ramfs.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

extern void serial_printf(const char *fmt, ...);
extern void gpt_init_storage(void);
extern void screen_init(void);
extern void screen_clear(void);

/* Embedded user-space programs (build/user/hello.elf, build/user/forkdemo.elf,
 * build/user/stacktrip.elf). These live in the read-only ELF sections produced
 * by the Makefile's ld -r -b binary embed rule and are referenced by the boot
 * spawns AND the on-demand Phase 2.5 launcher below. */
extern const uint8_t _binary_build_user_hello_elf_start[];
extern const uint8_t _binary_build_user_hello_elf_end[];
extern const uint8_t _binary_build_user_forkdemo_elf_start[];
extern const uint8_t _binary_build_user_forkdemo_elf_end[];
extern const uint8_t _binary_build_user_stacktrip_elf_start[];
extern const uint8_t _binary_build_user_stacktrip_elf_end[];
extern const uint8_t _binary_build_user_init_elf_start[];
extern const uint8_t _binary_build_user_init_elf_end[];
extern const uint8_t _binary_build_user_sh_elf_start[];
extern const uint8_t _binary_build_user_sh_elf_end[];
extern const uint8_t _binary_ascii_txt_start[];
extern const uint8_t _binary_ascii_txt_end[];

/* Phase 2.5 — on-demand user-program launcher. Maps a program name to its
 * embedded ELF binary and spawns it as a fresh user process. The shell `run`
 * dispatch (below) feeds this: `run hello`, `run forkdemo`, and
 * `run stacktrip` execute the Phase 2.4 QA binaries on demand — no boot-time
 * spawn hack required. */
void run_user_program(const char *name) {
    const uint8_t *start, *end;

    if (strcmp(name, "hello") == 0) {
        start = _binary_build_user_hello_elf_start;
        end = _binary_build_user_hello_elf_end;
    } else if (strcmp(name, "forkdemo") == 0) {
        start = _binary_build_user_forkdemo_elf_start;
        end = _binary_build_user_forkdemo_elf_end;
    } else if (strcmp(name, "stacktrip") == 0) {
        start = _binary_build_user_stacktrip_elf_start;
        end = _binary_build_user_stacktrip_elf_end;
    } else if (strcmp(name, "init") == 0) {
        start = _binary_build_user_init_elf_start;
        end = _binary_build_user_init_elf_end;
    } else if (strcmp(name, "sh") == 0) {
        start = _binary_build_user_sh_elf_start;
        end = _binary_build_user_sh_elf_end;
    } else {
        kprintf("Unknown program '%s' (try hello, forkdemo, stacktrip, init, sh)\n",
                name);
        return;
    }
    kprintf("[PHASE2.5] Launcher: starting user program '%s'\n", name);
    process_create_user(name, start, (uint64_t)(end - start));
}


void print_splash(void) {
    kprintf("\n");
    /* Print embedded ASCII banner */
    const uint8_t *ascii_start = _binary_ascii_txt_start;
    const uint8_t *ascii_end = _binary_ascii_txt_end;
    size_t ascii_len = ascii_end - ascii_start;
    for (size_t i = 0; i < ascii_len; i++) {
        kputchar(ascii_start[i]);
    }
    kprintf("\n");
}

void init_phase8(void) {
    kprintf("[PHASE8] Init start\n");

    vfs_node_t *root = ramfs_init();
    vfs_node_t *dev = devfs_init();
    ramfs_mount_dev(root, dev);
    vfs_set_root(root);
    kprintf("[VFS] Root filesystem initialized with /dev\n");

    virtio_init();
    virtio_blk_init();
    {
        uint64_t bcap = virtio_blk_get_capacity();
        if (bcap) {
            uint8_t probe[512];
            if (virtio_blk_read(0, 1, probe) == 0) {
                uint32_t sum = 0;
                for (int i = 0; i < 512; i++) sum += probe[i];
                kprintf("[VIRTIO-BLK] selftest sector0 read ok (%llu sectors) magic=%02x%02x%02x%02x sum=%u\n",
                        (unsigned long long)bcap, probe[0], probe[1], probe[2], probe[3], sum);
            } else {
                kprintf("[VIRTIO-BLK] selftest sector0 read FAILED\n");
            }
        } else {
            kprintf("[VIRTIO-BLK] selftest skipped: no device\n");
        }
    }
    virtio_net_init();
    socket_init();

    keyboard_init();
    mouse_init();
    usb_init();

    fb_init();
    screen_init(); // Re-initialize screen to pick up fbcon!

    print_splash();

    kprintf("[PHASE8] Calling init_start() to load /sbin/init\n");
    extern void init_start(void);
    init_start();

    vfs_node_t *bin_dir = ramfs_create_dir(root, "bin");
    
    vfs_node_t *hello_node = ramfs_create_file(bin_dir, "hello");
    hello_node->length = (uint32_t)(_binary_build_user_hello_elf_end - _binary_build_user_hello_elf_start);
    ramfs_write(hello_node, 0, hello_node->length, _binary_build_user_hello_elf_start);
    
    vfs_node_t *forkdemo_node = ramfs_create_file(bin_dir, "forkdemo");
    forkdemo_node->length = (uint32_t)(_binary_build_user_forkdemo_elf_end - _binary_build_user_forkdemo_elf_start);
    ramfs_write(forkdemo_node, 0, forkdemo_node->length, _binary_build_user_forkdemo_elf_start);
    
    vfs_node_t *sh_node = ramfs_create_file(bin_dir, "sh");
    sh_node->length = (uint32_t)(_binary_build_user_sh_elf_end - _binary_build_user_sh_elf_start);
    ramfs_write(sh_node, 0, sh_node->length, _binary_build_user_sh_elf_start);

    kprintf("[VFS] Populated /bin with hello, forkdemo, sh\n");

    kprintf("[PHASE8] Spawning embedded init as fallback\n");
    process_create_user("init",
                        _binary_build_user_init_elf_start,
                        (uint64_t)(_binary_build_user_init_elf_end -
                                   _binary_build_user_init_elf_start));

    kprintf("[PHASE8] Spawning user 'sh' process (Interactive CLI)\n");
    process_create_user("sh",
                        _binary_build_user_sh_elf_start,
                        (uint64_t)(_binary_build_user_sh_elf_end -
                                   _binary_build_user_sh_elf_start));

    /* stacktrip (user/stacktrip.c) is a Phase 2.4 QA artifact: its guard-page
     * fault was verified during development but the test binary is NOT
     * auto-spawned at boot. Keep user/stacktrip.c and its build wiring so an
     * interactive launcher can run it on demand. */

    kprintf("[PHASE8] Init complete\n");

    /* Start the graphical session last, so the whole kernel and userspace are
     * already up when the first window appears. desktop_boot() never returns;
     * if there is no usable framebuffer it returns immediately and the text
     * console continues as before. */
    {
        extern void desktop_boot(void);
        desktop_boot();
    }
}
