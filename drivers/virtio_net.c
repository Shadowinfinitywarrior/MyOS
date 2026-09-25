#include "virtio_net.h"
#include "virtio.h"
#include "../lib/printf.h"
#include "../net/net.h"
#include "../kernel/isr.h"
#include "../kernel/heap.h"
#include "../kernel/paging.h"
#include "../lib/string.h"

static virtio_device_t *g_net_dev = NULL;
static uint8_t g_mac[6] = {0};
static uint8_t *g_rx_buffers[16];
static uint32_t g_rx_buffer_phys[16];
static uint8_t g_tx_buffer[2048];

static void virtio_net_irq(registers_t *regs) {
    (void)regs;
    if (!g_net_dev) return;
    virtqueue_t *rxq = g_net_dev->queues[0];
    if (!rxq) return;
    uint32_t len;
    int id = virtqueue_get_buf(rxq, &len);
    while (id >= 0) {
        virtqueue_t *txq = g_net_dev->queues[1];
        if (txq) {
            int idx = virtqueue_get_buf(txq, NULL);
            while (idx >= 0) {
                virtqueue_release_buf(txq, (uint16_t)idx);
                idx = virtqueue_get_buf(txq, NULL);
            }
        }
        id = virtqueue_get_buf(rxq, &len);
    }
}

int virtio_net_init(void) {
    g_net_dev = virtio_find_device(VIRTIO_DEV_NET);
    if (!g_net_dev) {
        kprintf("[VIRTIO-NET] no device\n");
        return -1;
    }

    virtio_device_reset(g_net_dev);
    virtio_set_status(g_net_dev, VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER);

    uint64_t features = virtio_get_features(g_net_dev);
    features &= ~(1ULL << VIRTIO_F_VERSION_1);
    virtio_set_features(g_net_dev, features);
    virtio_set_status(g_net_dev, VIRTIO_STATUS_ACK | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_OK);

    uint16_t mac_off = g_net_dev->transitional ? 0 : 8;
    virtio_read_config(g_net_dev, mac_off, g_mac, 6);
    kprintf("[VIRTIO-NET] MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
            g_mac[0], g_mac[1], g_mac[2], g_mac[3], g_mac[4], g_mac[5]);

    if (virtio_setup_queue(g_net_dev, 0, 16) < 0) {
        kprintf("[VIRTIO-NET] rx queue setup failed\n");
        return -1;
    }
    if (virtio_setup_queue(g_net_dev, 1, 16) < 0) {
        kprintf("[VIRTIO-NET] tx queue setup failed\n");
        return -1;
    }

    virtqueue_t *rxq = g_net_dev->queues[0];
    for (int i = 0; i < 16; i++) {
        g_rx_buffers[i] = (uint8_t *)kmalloc(2048);
        if (!g_rx_buffers[i]) return -1;
        g_rx_buffer_phys[i] = paging_get_physical((uint64_t)(uintptr_t)g_rx_buffers[i]);
        vring_desc_t sg;
        sg.addr = g_rx_buffer_phys[i];
        sg.len = 2048;
        sg.flags = VRING_DESC_F_WRITE;
        virtqueue_add_buf(rxq, &sg, 0, 1);
    }
    virtqueue_kick(g_net_dev, 0);

    if (g_net_dev->irq) {
        isr_register_handler(g_net_dev->irq + 32, virtio_net_irq);
    }

    virtio_device_ready(g_net_dev);
    kprintf("[VIRTIO-NET] init complete\n");
    return 0;
}

int virtio_net_send(uint8_t *data, uint16_t len) {
    if (!g_net_dev || len > 2048) return -1;
    virtqueue_t *txq = g_net_dev->queues[1];
    if (!txq) return -1;

    stac();
    memcpy(g_tx_buffer, data, len);
    clac();

    vring_desc_t sg;
    sg.addr = paging_get_physical((uint64_t)(uintptr_t)g_tx_buffer);
    sg.len = len;
    sg.flags = 0;
    virtqueue_add_buf(g_net_dev->queues[1], &sg, 1, 0);
    virtqueue_kick(g_net_dev, 1);
    return len;
}

int virtio_net_receive(uint8_t *buf, uint16_t max_len) {
    if (!g_net_dev) return 0;
    virtqueue_t *rxq = g_net_dev->queues[0];
    if (!rxq) return 0;

    uint32_t len = 0;
    int id = virtqueue_get_buf(rxq, &len);
    if (id < 0) return 0;
    if (len > (uint32_t)max_len) len = max_len;

    uint16_t idx = id % 16;
    stac();
    memcpy(buf, g_rx_buffers[idx], len);
    clac();

    virtqueue_release_buf(rxq, (uint16_t)id);
    vring_desc_t sg;
    sg.addr = g_rx_buffer_phys[idx];
    sg.len = 2048;
    sg.flags = VRING_DESC_F_WRITE;
    virtqueue_add_buf(g_net_dev->queues[0], &sg, 0, 1);
    virtqueue_kick(g_net_dev, 0);
    return len;
}

const uint8_t *virtio_net_mac(void) {
    return g_mac;
}