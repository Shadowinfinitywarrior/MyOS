#include "devfs.h"
#include "../kernel/heap.h"
#include "../kernel/timer.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#include "../drivers/screen.h"
#include "../drivers/keyboard.h"
#include "../drivers/rtc.h"
#include "../lib/ipc.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

extern char keyboard_getchar(void);

/* /dev/null — discards all writes, reads return EOF */
static int devnull_read(vfs_node_t *n, uint32_t off, uint32_t sz, void *buf) {
    (void)n; (void)off; (void)sz; (void)buf;
    return 0;
}
static int devnull_write(vfs_node_t *n, uint32_t off, uint32_t sz, const void *buf) {
    (void)n; (void)off; (void)buf;
    return sz;   /* Pretend we wrote everything */
}

/* /dev/zero — reads return zeros */
static int devzero_read(vfs_node_t *n, uint32_t off, uint32_t sz, void *buf) {
    (void)n; (void)off;
    memset(buf, 0, sz);
    return sz;
}
static int devzero_write(vfs_node_t *n, uint32_t off, uint32_t sz, const void *buf) {
    (void)n; (void)off; (void)buf;
    return sz;
}

/* /dev/random — simple PRNG */
static uint32_t prng_state = 12345;
static int devrandom_read(vfs_node_t *n, uint32_t off, uint32_t sz, void *buf) {
    (void)n; (void)off;
    uint8_t *p = (uint8_t *)buf;
    for (uint32_t i = 0; i < sz; i++) {
        prng_state = prng_state * 1103515245 + 12345;
        p[i] = (prng_state >> 16) & 0xFF;
    }
    return sz;
}

/* /dev/console — keyboard input, screen output */
static int devconsole_read(vfs_node_t *n, uint32_t off, uint32_t sz, void *buf) {
    (void)n; (void)off;
    char *p = (char *)buf;
    for (uint32_t i = 0; i < sz; i++) {
        p[i] = keyboard_getchar();
        if (p[i] == '\n') return i + 1;
    }
    return sz;
}
static int devconsole_write(vfs_node_t *n, uint32_t off, uint32_t sz, const void *buf) {
    (void)n; (void)off;
    const char *p = (const char *)buf;
    for (uint32_t i = 0; i < sz; i++)
        screen_putchar(p[i]);
    return sz;
}

/* /dev/rtc — returns current time as string on read */
static int devrtc_read(vfs_node_t *n, uint32_t off, uint32_t sz, void *buf) {
    (void)n; (void)off;
    datetime_t dt;
    rtc_get_time(&dt);
    char timebuf[32];
    ksprintf(timebuf, "%04d-%02d-%02d %02d:%02d:%02d\n",
             dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
    uint32_t len = MIN(sz, strlen(timebuf));
    memcpy(buf, timebuf, len);
    return len;
}

/* /dev/uptime — returns uptime as string */
static int devuptime_read(vfs_node_t *n, uint32_t off, uint32_t sz, void *buf) {
    (void)n; (void)off;
    uint32_t secs = timer_get_seconds();
    char buf2[32];
    ksprintf(buf2, "%u\n", secs);
    uint32_t len = MIN(sz, strlen(buf2));
    memcpy(buf, buf2, len);
    return len;
}

/* Device descriptor table. Nodes are created ONCE at devfs_init and cached:
 * vfs_resolve_path("/dev/...") must hand back a stable pointer without a
 * fresh allocation, otherwise every open/read/write leaks a node. */
enum {
    DEV_NULL,
    DEV_ZERO,
    DEV_RANDOM,
    DEV_CONSOLE,
    DEV_RTC,
    DEV_UPTIME,
    DEV_COUNT
};

static vfs_node_t *g_dev_nodes[DEV_COUNT] = {0};

static vfs_node_t *devfs_finddir(vfs_node_t *node, const char *name) {
    (void)node;

    struct dev_entry {
        const char *name;
        read_fn  read;
        write_fn write;
    } devices[] = {
        { "null",    devnull_read,    devnull_write    },
        { "zero",    devzero_read,    devzero_write    },
        { "random",  devrandom_read,  NULL             },
        { "console", devconsole_read, devconsole_write },
        { "rtc",     devrtc_read,     NULL             },
        { "uptime",  devuptime_read,  NULL             },
        { NULL, NULL, NULL }
    };

    for (int i = 0; devices[i].name; i++) {
        if (strcmp(name, devices[i].name) == 0) {
            return g_dev_nodes[i];
        }
    }
    return NULL;
}

static vfs_node_t *devfs_readdir(vfs_node_t *node, uint32_t index) {
    (void)node;
    const char *names[] = {
        "null", "zero", "random", "console", "rtc", "uptime", NULL
    };

    if (index >= 6) return NULL;

    vfs_node_t *dev = devfs_finddir(NULL, names[index]);
    return dev;
}

static struct dev_entry {
    const char *name;
    read_fn  read;
    write_fn write;
} g_devices[] = {
    { "null",    devnull_read,    devnull_write    },
    { "zero",    devzero_read,    devzero_write    },
    { "random",  devrandom_read,  NULL             },
    { "console", devconsole_read, devconsole_write },
    { "rtc",     devrtc_read,     NULL             },
    { "uptime",  devuptime_read,  NULL             },
    { NULL, NULL, NULL }
};

vfs_node_t *devfs_init(void) {
    /* Initialize IPC system before exposing /dev/shm/wm_pipe */
    ipc_init();

    for (int i = 0; g_devices[i].name; i++) {
        vfs_node_t *dev = (vfs_node_t *)kzalloc(sizeof(vfs_node_t));
        strncpy(dev->name, g_devices[i].name, VFS_NAME_MAX);
        dev->flags = VFS_FILE;
        dev->read = g_devices[i].read;
        dev->write = g_devices[i].write;
        g_dev_nodes[i] = dev;
    }

    vfs_node_t *root_dev = (vfs_node_t *)kzalloc(sizeof(vfs_node_t));
    strcpy(root_dev->name, "dev");
    root_dev->flags = VFS_DIRECTORY;
    root_dev->finddir = devfs_finddir;
    root_dev->readdir = devfs_readdir;

    kprintf("[DEVFS] Device filesystem initialized\n");
    kprintf("[DEVFS]   /dev/null, /dev/zero, /dev/random\n");
    kprintf("[DEVFS]   /dev/console, /dev/rtc, /dev/uptime\n");

    return root_dev;
}

