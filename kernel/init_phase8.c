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
    } else {
        kprintf("Unknown program '%s' (try hello, forkdemo, stacktrip)\n",
                name);
        return;
    }
    kprintf("[PHASE2.5] Launcher: starting user program '%s'\n", name);
    process_create_user(name, start, (uint64_t)(end - start));
}

#define CMD_MAX_LEN 128

void shell_dummy(void) {
    kprintf("\nWelcome to MyOS (Headless Shell)\n");
    kprintf("Type 'help' to see available commands.\n\n");
    kprintf("myos> ");

    char cmd[CMD_MAX_LEN];
    int cmd_len = 0;

    while (1) {
        key_event_t event;
        if (keyboard_get_event(&event)) {
            if (event.type == KEY_EVENT_DOWN || event.type == KEY_EVENT_REPEAT) {
                kprintf("[KEY] ascii=%d keycode=%d\n", event.ascii, event.keycode);
                if (event.ascii >= ' ' && event.ascii <= '~') {
                    if (cmd_len < CMD_MAX_LEN - 1) {
                        cmd[cmd_len++] = event.ascii;
                        kprintf("%c", event.ascii);
                    }
                } else if (event.keycode == KEY_BACKSPACE && cmd_len > 0) {
                    cmd_len--;
                    kprintf("\b \b");
                } else if (event.keycode == KEY_ENTER) {
                    cmd[cmd_len] = '\0';
                    kprintf("\n");

                    if (cmd_len > 0) {
                        if (strcmp(cmd, "help") == 0) {
                            kprintf("Available commands:\n");
                            kprintf("  help       - Show this message\n");
                            kprintf("  clear      - Clear the screen\n");
                            kprintf("  reboot     - Reboot the system\n");
                            kprintf("  ls         - List directory contents\n");
                            kprintf("  cat        - Concatenate and display files\n");
                            kprintf("  ps         - Show process status\n");
                            kprintf("  shutdown   - Power off the system\n");
                            kprintf("  kbdrate    - Set keyboard repeat rate\n");
                            kprintf("  gui_demo   - Launch GUI demonstration\n");
                            kprintf("  httpd      - Start HTTP server\n");
                            kprintf("  dhcpclient - Obtain IP via DHCP\n");
                        } else if (strcmp(cmd, "clear") == 0) {
                            screen_clear();
                        } else if (strcmp(cmd, "reboot") == 0) {
                            outb(0x64, 0xFE);
                            hlt();
                        } else if (strcmp(cmd, "run") == 0 ||
                                   strncmp(cmd, "run ", 4) == 0) {
                            run_user_program(cmd + 4);
                        } else {
                            kprintf("Unknown command: %s\n", cmd);
                        }
                    }
                    cmd_len = 0;
                    kprintf("myos> ");
                }
            }
        } else {
            hlt();
        }
    }
}

void print_splash(void) {
    kprintf("\n");
    kprintf("  ____                _        _   _              \n");
    kprintf(" |  _ \\ ___  ___| |_ ___| |_ (_) | |_ ___  ___   \n");
    kprintf(" | |_) / _ \\/ __| __/ __| __| | | __/ _ \\/ __|  \n");
    kprintf(" |  __/  __/\\__ \\ || (__| |_  | | ||  __/\\__ \\  \n");
    kprintf(" |_|   \\___||___/\\__\\___|\\__| |_|\\__\\___||___/  \n");
    kprintf("\n");
    kprintf(" Welcome to MyOS - a simple hobby operating system\n");
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

    kprintf("[PHASE8] Spawning user-space 'hello' process\n");
    process_create_user("hello",
                        _binary_build_user_hello_elf_start,
                        (uint64_t)(_binary_build_user_hello_elf_end -
                                   _binary_build_user_hello_elf_start));

    kprintf("[PHASE8] Spawning user 'forkdemo' process\n");
    process_create_user("forkdemo",
                        _binary_build_user_forkdemo_elf_start,
                        (uint64_t)(_binary_build_user_forkdemo_elf_end -
                                   _binary_build_user_forkdemo_elf_start));

    /* stacktrip (user/stacktrip.c) is a Phase 2.4 QA artifact: its guard-page
     * fault was verified during development but the test binary is NOT
     * auto-spawned at boot. Keep user/stacktrip.c and its build wiring so an
     * interactive launcher can run it on demand. */

    kprintf("[PHASE8] Spawning shell thread\n");
    process_create_kernel("shell", shell_dummy);

    kprintf("[PHASE8] Init complete\n");
}
