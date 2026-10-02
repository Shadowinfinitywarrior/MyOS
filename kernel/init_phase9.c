#include "../lib/string.h"
#include "../include/system.h"
#include "unix_socket.h"
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
#include "../drivers/driver.h"
#include "../drivers/ps2.h"
#include "../drivers/hpet.h"
#include "../drivers/wdt.h"
#include "../drivers/i2c.h"
#include "../drivers/spi.h"
#include "../drivers/ide.h"
#include "../drivers/sdmmc.h"
#include "../drivers/e1000.h"
#include "../drivers/hda.h"
#include "../drivers/gpio.h"
#include "../drivers/ac97.h"
#include "../fs/vfs.h"
#include "../fs/devfs.h"
#include "../fs/ramfs.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

extern const uint8_t _binary_ascii_txt_start[];
extern const uint8_t _binary_ascii_txt_end[];

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
extern const uint8_t _binary_build_user_myinit_elf_start[];
extern const uint8_t _binary_build_user_myinit_elf_end[];
extern const uint8_t _binary_build_user_devmgr_elf_start[];
extern const uint8_t _binary_build_user_devmgr_elf_end[];
extern const uint8_t _binary_build_user_logd_elf_start[];
extern const uint8_t _binary_build_user_logd_elf_end[];
extern const uint8_t _binary_build_user_netmgr_elf_start[];
extern const uint8_t _binary_build_user_netmgr_elf_end[];
extern const uint8_t _binary_build_user_audiosrv_elf_start[];
extern const uint8_t _binary_build_user_audiosrv_elf_end[];
extern const uint8_t _binary_build_user_powersrv_elf_start[];
extern const uint8_t _binary_build_user_powersrv_elf_end[];
extern const uint8_t _binary_build_user_mountd_elf_start[];
extern const uint8_t _binary_build_user_mountd_elf_end[];



/* Phase 2.5 — on-demand user-program launcher. Maps a program name to its
 * embedded ELF binary and spawns it as a fresh user process. The `run`
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
    } else {
        kprintf("Unknown program '%s' (try hello, forkdemo, stacktrip, init)\n",
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

void init_phase9(void) {
    kprintf("[PHASE9] Init start\n");
    serial_printf("[PHASE9] calling ramfs_init\n");

    vfs_node_t *root = ramfs_init();
    serial_printf("[PHASE9] ramfs_init done root=0x%lx\n", (uint64_t)root);
    vfs_node_t *dev = devfs_init();
    serial_printf("[PHASE9] devfs_init done dev=0x%lx\n", (uint64_t)dev);
    ramfs_mount_dev(root, dev);
    vfs_set_root(root);
    kprintf("[VFS] Root filesystem initialized with /dev\n");

    // Populate ramfs. /sbin holds background daemons that myinit starts;
    // /bin holds the interactive programs, so the
    // paths myinit execve()s actually resolve.
    vfs_node_t *sbin = ramfs_create_dir(root, "sbin");
    vfs_node_t *bin = ramfs_create_dir(root, "bin");

    vfs_node_t *f;
    f = ramfs_create_file(sbin, "myinit");
    ramfs_write(f, 0, (uint32_t)(_binary_build_user_myinit_elf_end - _binary_build_user_myinit_elf_start), _binary_build_user_myinit_elf_start);

    f = ramfs_create_file(sbin, "devmgr");
    ramfs_write(f, 0, (uint32_t)(_binary_build_user_devmgr_elf_end - _binary_build_user_devmgr_elf_start), _binary_build_user_devmgr_elf_start);

    f = ramfs_create_file(sbin, "logd");
    ramfs_write(f, 0, (uint32_t)(_binary_build_user_logd_elf_end - _binary_build_user_logd_elf_start), _binary_build_user_logd_elf_start);

    f = ramfs_create_file(sbin, "netmgr");
    ramfs_write(f, 0, (uint32_t)(_binary_build_user_netmgr_elf_end - _binary_build_user_netmgr_elf_start), _binary_build_user_netmgr_elf_start);

    f = ramfs_create_file(sbin, "audiosrv");
    ramfs_write(f, 0, (uint32_t)(_binary_build_user_audiosrv_elf_end - _binary_build_user_audiosrv_elf_start), _binary_build_user_audiosrv_elf_start);

    f = ramfs_create_file(sbin, "powersrv");
    ramfs_write(f, 0, (uint32_t)(_binary_build_user_powersrv_elf_end - _binary_build_user_powersrv_elf_start), _binary_build_user_powersrv_elf_start);

    f = ramfs_create_file(sbin, "mountd");
    ramfs_write(f, 0, (uint32_t)(_binary_build_user_mountd_elf_end - _binary_build_user_mountd_elf_start), _binary_build_user_mountd_elf_start);

    f = ramfs_create_file(bin, "hello");
    ramfs_write(f, 0, (uint32_t)(_binary_build_user_hello_elf_end - _binary_build_user_hello_elf_start), _binary_build_user_hello_elf_start);

    f = ramfs_create_file(bin, "forkdemo");
    ramfs_write(f, 0, (uint32_t)(_binary_build_user_forkdemo_elf_end - _binary_build_user_forkdemo_elf_start), _binary_build_user_forkdemo_elf_start);

    f = ramfs_create_file(bin, "stacktrip");
    ramfs_write(f, 0, (uint32_t)(_binary_build_user_stacktrip_elf_end - _binary_build_user_stacktrip_elf_start), _binary_build_user_stacktrip_elf_start);

    f = ramfs_create_file(bin, "init");
    ramfs_write(f, 0, (uint32_t)(_binary_build_user_init_elf_end - _binary_build_user_init_elf_start), _binary_build_user_init_elf_start);


    serial_printf("[PHASE9] calling virtio_init\n");
    virtio_init();
    serial_printf("[PHASE9] calling virtio_blk_init\n");
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
    unix_socket_init();

    /* Register the drivers that are always compiled in, then probe the
     * optional hardware. Each probe adds (or skips) itself in the registry
     * that the "drivers" shell command prints. PS/2 runs before the keyboard
     * driver because its self-test resets the controller. */
    driver_seed_core();
    ps2_init();
    keyboard_init();
    usb_init();
    hpet_init();
    wdt_init();
    ide_init();
    sdmmc_init();
    i2c_init();
    spi_init();
    gpio_init();
    e1000_init();
    hda_init();
    ac97_probe();

    print_splash();

    /* /sbin/init is not a real program (user/init.c is a placeholder), so skip
     * init_start(): it would print a "Calling init_start()" line and then do
     * nothing. PID 1 is myinit. */
    kprintf("[PHASE9] Spawning PID 1 myinit...\n");
    process_create_user("myinit",
                        _binary_build_user_myinit_elf_start,
                        (uint64_t)(_binary_build_user_myinit_elf_end -
                                   _binary_build_user_myinit_elf_start));

    /* stacktrip (user/stacktrip.c) is a Phase 2.4 QA artifact: its guard-page
     * fault was verified during development but the test binary is NOT
     * auto-spawned at boot. Keep user/stacktrip.c and its build wiring so an
     * interactive launcher can run it on demand. */

    kprintf("[PHASE9] Init complete\n");
}
