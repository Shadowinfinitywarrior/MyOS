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
extern void desktop_boot(void);
extern void goshell_kernel_main(void);

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
extern const uint8_t _binary_build_user_cat_elf_start[];
extern const uint8_t _binary_build_user_cat_elf_end[];
extern const uint8_t _binary_build_user_echo_elf_start[];
extern const uint8_t _binary_build_user_echo_elf_end[];
extern const uint8_t _binary_build_user_pwd_elf_start[];
extern const uint8_t _binary_build_user_pwd_elf_end[];
extern const uint8_t _binary_build_user_date_elf_start[];
extern const uint8_t _binary_build_user_date_elf_end[];
extern const uint8_t _binary_build_user_whoami_elf_start[];
extern const uint8_t _binary_build_user_whoami_elf_end[];
extern const uint8_t _binary_build_user_uname_elf_start[];
extern const uint8_t _binary_build_user_uname_elf_end[];
extern const uint8_t _binary_build_user_kill_elf_start[];
extern const uint8_t _binary_build_user_kill_elf_end[];
extern const uint8_t _binary_build_user_touch_elf_start[];
extern const uint8_t _binary_build_user_touch_elf_end[];
extern const uint8_t _binary_build_user_calc_elf_start[];
extern const uint8_t _binary_build_user_calc_elf_end[];
extern const uint8_t _binary_build_user_df_elf_start[];
extern const uint8_t _binary_build_user_df_elf_end[];
extern const uint8_t _binary_build_user_env_elf_start[];
extern const uint8_t _binary_build_user_env_elf_end[];
extern const uint8_t _binary_build_user_version_elf_start[];
extern const uint8_t _binary_build_user_version_elf_end[];
extern const uint8_t _binary_build_user_help_elf_start[];
extern const uint8_t _binary_build_user_help_elf_end[];
extern const uint8_t _binary_build_user_ls_elf_start[];
extern const uint8_t _binary_build_user_ls_elf_end[];
extern const uint8_t _binary_build_user_free_elf_start[];
extern const uint8_t _binary_build_user_free_elf_end[];
extern const uint8_t _binary_build_user_ps_elf_start[];
extern const uint8_t _binary_build_user_ps_elf_end[];
extern const uint8_t _binary_build_user_uptime_elf_start[];
extern const uint8_t _binary_build_user_uptime_elf_end[];
extern const uint8_t _binary_build_user_reboot_elf_start[];
extern const uint8_t _binary_build_user_reboot_elf_end[];
extern const uint8_t _binary_build_user_shutdown_elf_start[];
extern const uint8_t _binary_build_user_shutdown_elf_end[];
extern const uint8_t _binary_ascii_txt_start[];
extern const uint8_t _binary_ascii_txt_end[];
extern const uint8_t _binary_build_user_compositor_elf_start[];
extern const uint8_t _binary_build_user_compositor_elf_end[];

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
    } else if (strcmp(name, "cat") == 0) {
        start = _binary_build_user_cat_elf_start;
        end = _binary_build_user_cat_elf_end;
    } else if (strcmp(name, "echo") == 0) {
        start = _binary_build_user_echo_elf_start;
        end = _binary_build_user_echo_elf_end;
    } else if (strcmp(name, "pwd") == 0) {
        start = _binary_build_user_pwd_elf_start;
        end = _binary_build_user_pwd_elf_end;
    } else if (strcmp(name, "date") == 0) {
        start = _binary_build_user_date_elf_start;
        end = _binary_build_user_date_elf_end;
    } else if (strcmp(name, "whoami") == 0) {
        start = _binary_build_user_whoami_elf_start;
        end = _binary_build_user_whoami_elf_end;
    } else if (strcmp(name, "uname") == 0) {
        start = _binary_build_user_uname_elf_start;
        end = _binary_build_user_uname_elf_end;
    } else if (strcmp(name, "kill") == 0) {
        start = _binary_build_user_kill_elf_start;
        end = _binary_build_user_kill_elf_end;
    } else if (strcmp(name, "touch") == 0) {
        start = _binary_build_user_touch_elf_start;
        end = _binary_build_user_touch_elf_end;
    } else if (strcmp(name, "calc") == 0) {
        start = _binary_build_user_calc_elf_start;
        end = _binary_build_user_calc_elf_end;
    } else if (strcmp(name, "df") == 0) {
        start = _binary_build_user_df_elf_start;
        end = _binary_build_user_df_elf_end;
    } else if (strcmp(name, "env") == 0) {
        start = _binary_build_user_env_elf_start;
        end = _binary_build_user_env_elf_end;
    } else if (strcmp(name, "version") == 0) {
        start = _binary_build_user_version_elf_start;
        end = _binary_build_user_version_elf_end;
    } else if (strcmp(name, "help") == 0) {
        start = _binary_build_user_help_elf_start;
        end = _binary_build_user_help_elf_end;
    } else if (strcmp(name, "ls") == 0) {
        start = _binary_build_user_ls_elf_start;
        end = _binary_build_user_ls_elf_end;
    } else if (strcmp(name, "free") == 0) {
        start = _binary_build_user_free_elf_start;
        end = _binary_build_user_free_elf_end;
    } else if (strcmp(name, "ps") == 0) {
        start = _binary_build_user_ps_elf_start;
        end = _binary_build_user_ps_elf_end;
    } else if (strcmp(name, "uptime") == 0) {
        start = _binary_build_user_uptime_elf_start;
        end = _binary_build_user_uptime_elf_end;
    } else if (strcmp(name, "reboot") == 0) {
        start = _binary_build_user_reboot_elf_start;
        end = _binary_build_user_reboot_elf_end;
    } else if (strcmp(name, "shutdown") == 0) {
        start = _binary_build_user_shutdown_elf_start;
        end = _binary_build_user_shutdown_elf_end;
    } else {
        kprintf("Unknown program '%s' (try hello, forkdemo, stacktrip, init, sh, cat, echo, pwd, date, whoami, uname, kill, touch, calc, df, env, version, help, ls, free, ps, uptime, reboot, shutdown)\n",
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

    extern void driver_seed_core(void);
    extern void pci_init(void);
    extern int ide_init(void);
    extern int e1000_init(void);
    extern int hda_init(void);
    extern int ac97_probe(void);
    extern void power_net_init(void);
    driver_seed_core();
    pci_init();
    ide_init();
    e1000_init();
    hda_init();
    ac97_probe();
    power_net_init();

    fb_init();
    screen_init(); // Re-initialize screen to pick up fbcon!

    print_splash();

    kprintf("[PHASE8] Calling init_start() to load /sbin/init\n");
    extern void init_start(void);
    init_start();

    vfs_node_t *bin_dir = ramfs_create_dir(root, "bin");
    
    /* Core commands */
    vfs_node_t *hello_node = ramfs_create_file(bin_dir, "hello");
    hello_node->length = (uint32_t)(_binary_build_user_hello_elf_end - _binary_build_user_hello_elf_start);
    ramfs_write(hello_node, 0, hello_node->length, _binary_build_user_hello_elf_start);
    
    vfs_node_t *forkdemo_node = ramfs_create_file(bin_dir, "forkdemo");
    forkdemo_node->length = (uint32_t)(_binary_build_user_forkdemo_elf_end - _binary_build_user_forkdemo_elf_start);
    ramfs_write(forkdemo_node, 0, forkdemo_node->length, _binary_build_user_forkdemo_elf_start);
    
    vfs_node_t *sh_node = ramfs_create_file(bin_dir, "sh");
    sh_node->length = (uint32_t)(_binary_build_user_sh_elf_end - _binary_build_user_sh_elf_start);
    ramfs_write(sh_node, 0, sh_node->length, _binary_build_user_sh_elf_start);

    /* File operations */
    vfs_node_t *cat_node = ramfs_create_file(bin_dir, "cat");
    cat_node->length = (uint32_t)(_binary_build_user_cat_elf_end - _binary_build_user_cat_elf_start);
    ramfs_write(cat_node, 0, cat_node->length, _binary_build_user_cat_elf_start);

    vfs_node_t *echo_node = ramfs_create_file(bin_dir, "echo");
    echo_node->length = (uint32_t)(_binary_build_user_echo_elf_end - _binary_build_user_echo_elf_start);
    ramfs_write(echo_node, 0, echo_node->length, _binary_build_user_echo_elf_start);

    vfs_node_t *pwd_node = ramfs_create_file(bin_dir, "pwd");
    pwd_node->length = (uint32_t)(_binary_build_user_pwd_elf_end - _binary_build_user_pwd_elf_start);
    ramfs_write(pwd_node, 0, pwd_node->length, _binary_build_user_pwd_elf_start);

    vfs_node_t *touch_node = ramfs_create_file(bin_dir, "touch");
    touch_node->length = (uint32_t)(_binary_build_user_touch_elf_end - _binary_build_user_touch_elf_start);
    ramfs_write(touch_node, 0, touch_node->length, _binary_build_user_touch_elf_start);

    vfs_node_t *ls_node = ramfs_create_file(bin_dir, "ls");
    ls_node->length = (uint32_t)(_binary_build_user_ls_elf_end - _binary_build_user_ls_elf_start);
    ramfs_write(ls_node, 0, ls_node->length, _binary_build_user_ls_elf_start);

    /* System info */
    vfs_node_t *date_node = ramfs_create_file(bin_dir, "date");
    date_node->length = (uint32_t)(_binary_build_user_date_elf_end - _binary_build_user_date_elf_start);
    ramfs_write(date_node, 0, date_node->length, _binary_build_user_date_elf_start);

    vfs_node_t *whoami_node = ramfs_create_file(bin_dir, "whoami");
    whoami_node->length = (uint32_t)(_binary_build_user_whoami_elf_end - _binary_build_user_whoami_elf_start);
    ramfs_write(whoami_node, 0, whoami_node->length, _binary_build_user_whoami_elf_start);

    vfs_node_t *uname_node = ramfs_create_file(bin_dir, "uname");
    uname_node->length = (uint32_t)(_binary_build_user_uname_elf_end - _binary_build_user_uname_elf_start);
    ramfs_write(uname_node, 0, uname_node->length, _binary_build_user_uname_elf_start);

    vfs_node_t *version_node = ramfs_create_file(bin_dir, "version");
    version_node->length = (uint32_t)(_binary_build_user_version_elf_end - _binary_build_user_version_elf_start);
    ramfs_write(version_node, 0, version_node->length, _binary_build_user_version_elf_start);

    /* Process management */
    vfs_node_t *ps_node = ramfs_create_file(bin_dir, "ps");
    ps_node->length = (uint32_t)(_binary_build_user_ps_elf_end - _binary_build_user_ps_elf_start);
    ramfs_write(ps_node, 0, ps_node->length, _binary_build_user_ps_elf_start);

    vfs_node_t *kill_node = ramfs_create_file(bin_dir, "kill");
    kill_node->length = (uint32_t)(_binary_build_user_kill_elf_end - _binary_build_user_kill_elf_start);
    ramfs_write(kill_node, 0, kill_node->length, _binary_build_user_kill_elf_start);

    vfs_node_t *free_node = ramfs_create_file(bin_dir, "free");
    free_node->length = (uint32_t)(_binary_build_user_free_elf_end - _binary_build_user_free_elf_start);
    ramfs_write(free_node, 0, free_node->length, _binary_build_user_free_elf_start);

    vfs_node_t *uptime_node = ramfs_create_file(bin_dir, "uptime");
    uptime_node->length = (uint32_t)(_binary_build_user_uptime_elf_end - _binary_build_user_uptime_elf_start);
    ramfs_write(uptime_node, 0, uptime_node->length, _binary_build_user_uptime_elf_start);

    /* Utilities */
    vfs_node_t *calc_node = ramfs_create_file(bin_dir, "calc");
    calc_node->length = (uint32_t)(_binary_build_user_calc_elf_end - _binary_build_user_calc_elf_start);
    ramfs_write(calc_node, 0, calc_node->length, _binary_build_user_calc_elf_start);

    vfs_node_t *df_node = ramfs_create_file(bin_dir, "df");
    df_node->length = (uint32_t)(_binary_build_user_df_elf_end - _binary_build_user_df_elf_start);
    ramfs_write(df_node, 0, df_node->length, _binary_build_user_df_elf_start);

    vfs_node_t *env_node = ramfs_create_file(bin_dir, "env");
    env_node->length = (uint32_t)(_binary_build_user_env_elf_end - _binary_build_user_env_elf_start);
    ramfs_write(env_node, 0, env_node->length, _binary_build_user_env_elf_start);

    vfs_node_t *help_node = ramfs_create_file(bin_dir, "help");
    help_node->length = (uint32_t)(_binary_build_user_help_elf_end - _binary_build_user_help_elf_start);
    ramfs_write(help_node, 0, help_node->length, _binary_build_user_help_elf_start);

    vfs_node_t *reboot_node = ramfs_create_file(bin_dir, "reboot");
    reboot_node->length = (uint32_t)(_binary_build_user_reboot_elf_end - _binary_build_user_reboot_elf_start);
    ramfs_write(reboot_node, 0, reboot_node->length, _binary_build_user_reboot_elf_start);

    vfs_node_t *shutdown_node = ramfs_create_file(bin_dir, "shutdown");
    shutdown_node->length = (uint32_t)(_binary_build_user_shutdown_elf_end - _binary_build_user_shutdown_elf_start);
    ramfs_write(shutdown_node, 0, shutdown_node->length, _binary_build_user_shutdown_elf_start);

    kprintf("[VFS] Populated /bin with hello, forkdemo, sh, cat, echo, pwd, touch, ls, date, whoami, uname, version, ps, kill, free, uptime, calc, df, env, help, reboot, shutdown\n");

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
        extern const uint8_t _binary_build_user_compositor_elf_start[];
        extern const uint8_t _binary_build_user_compositor_elf_end[];
        process_create_user("compositor", _binary_build_user_compositor_elf_start,
                            (uint64_t)(_binary_build_user_compositor_elf_end -
                                       _binary_build_user_compositor_elf_start));
    }

    /* Spawn Go/C shell (Phase 3 desktop shell) - embedded ELF - DISABLED for Phase 2 */
    /*
    {
        extern const uint8_t _binary_build_user_goshell_embed_o_start[];
        extern const uint8_t _binary_build_user_goshell_embed_o_end[];
        process_create_user("goshell", _binary_build_user_goshell_embed_o_start,
                            (uint64_t)(_binary_build_user_goshell_embed_o_end -
                                       _binary_build_user_goshell_embed_o_start));
    }
    */

    desktop_boot();
}
