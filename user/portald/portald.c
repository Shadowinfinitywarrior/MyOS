#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "mydp/protocol.h"

static volatile int running = 1;
static int state = 0;
static uint64_t seq = 0;
static uint32_t next_surface_id = 1;
static uint32_t surfaces[256];
static int surface_count = 0;

int portald_init(void)
{
    running = 1;
    state = 0;
    seq = 0;
    next_surface_id = 1;
    surface_count = 0;
    return 0;
}

void portald_shutdown(void)
{
    running = 0;
    state = 3;
}

static void portald_handle_hello(const mydp_header_t *hdr)
{
    (void)hdr;
    if (state == 0) {
        state = 1;
    }
}

static void portald_handle_create_surface(const mydp_header_t *hdr)
{
    (void)hdr;
    if (surface_count < 256) {
        surfaces[surface_count] = next_surface_id++;
        ++surface_count;
    }
}

static void portald_handle_damage(const mydp_header_t *hdr)
{
    (void)hdr;
}

static void portald_handle_commit(const mydp_header_t *hdr)
{
    (void)hdr;
}

static void portald_handle_close_surface(const mydp_header_t *hdr)
{
    (void)hdr;
    if (surface_count > 0) {
        --surface_count;
    }
}

static void portald_process_message(const mydp_header_t *hdr)
{
    if (!hdr) {
        return;
    }
    if (hdr->magic != MYDP_MAGIC) {
        return;
    }
    seq = hdr->seq;
    switch (hdr->type) {
    case MYDP_HELLO:
        portald_handle_hello(hdr);
        break;
    case MYDP_CREATE_SURFACE:
        portald_handle_create_surface(hdr);
        break;
    case MYDP_DAMAGE:
        portald_handle_damage(hdr);
        break;
    case MYDP_COMMIT:
        portald_handle_commit(hdr);
        break;
    case MYDP_CLOSE_SURFACE:
        portald_handle_close_surface(hdr);
        break;
    default:
        break;
    }
}

void portald_run(void)
{
    mydp_header_t hdr;
    hdr.magic = MYDP_MAGIC;
    hdr.version = MYDP_VERSION;
    hdr.type = MYDP_HELLO;
    hdr.length = 0;
    hdr.seq = 0;
    while (running) {
        portald_process_message(&hdr);
        hdr.seq++;
    }
}

int main(void)
{
    portald_init();
    portald_run();
    portald_shutdown();
    return 0;
}
