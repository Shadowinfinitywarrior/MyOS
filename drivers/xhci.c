#include "xhci.h"
#include "pci.h"
#include "usb_hid.h"
#include "../kernel/isr.h"
#include "../kernel/pic.h"
#include "../lib/printf.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

#define XHCI_VENDOR  0x1B36   /* RedHat */
#define XHCI_DEVICE  0x000D   /* qemu-xhci */

/* ---- Capability registers (offset from BAR0) ---- */
#define XHC_CAP_CAPLENGTH    0x00
#define XHC_CAP_HCIVERSION   0x02
#define XHC_CAP_HCSPARAMS1   0x04
#define XHC_CAP_HCSPARAMS2   0x08
#define XHC_CAP_HCSPARAMS3   0x0C
#define XHC_CAP_HCCPARAMS1   0x10
/* Doorbell and Runtime offsets: spec puts them at 0x14/0x18 (QEMU 6.2 returns
 * the raw offset value, e.g. 0x2000 / 0x1000, in these dwords). */
#define XHC_CAP_DBOFF        0x14
#define XHC_CAP_RTSOFF       0x18

#define XHC_HCSP1_MAXSLOTS_MASK  0x000000FF
#define XHC_HCSP1_MAXINTRS_SHIFT 8
#define XHC_HCSP1_MAXPORTS_SHIFT 24

/* spec stores offsets in bits 31:16; QEMU 6.2 returns the raw offset value,
 * so accept either representation */
static uint32_t xhci_off_of(uint32_t raw) {
    uint32_t hi = (raw >> 16) & 0xFFFF;
    return hi ? hi : (raw & 0xFFFF);
}

/* ---- Operational registers (offset from oper) ---- */
#define XHC_OPER_USBCMD       0x00
#define XHC_OPER_USBSTS       0x04
#define XHC_OPER_CRCR         0x18   /* 64-bit, command ring control */
#define XHC_OPER_DCBAAP       0x30   /* 64-bit */
#define XHC_OPER_CONFIG       0x38

#define XHC_USBCMD_RS         0x00000001
#define XHC_USBCMD_HCRST      0x00000002
#define XHC_USBCMD_INTE       0x00000004
#define XHC_USBCMD_HSEE       0x00000008

#define XHC_USBSTS_HCH        0x00000001
#define XHC_USBSTS_HSE        0x00000004
#define XHC_USBSTS_EINT       0x00000008
#define XHC_USBSTS_PCD        0x00000010
#define XHC_USBSTS_CNR        0x00000800

#define XHC_CRCR_RCS          0x00000001
#define XHC_CRCR_CS           0x00000002
#define XHC_CRCR_CA           0x00000004

/* ---- Interrupter 0 registers (offset from runtime base + 0x20) ---- */
#define XHC_RUN_IMAN          0x20
#define XHC_RUN_IMOD          0x24
#define XHC_RUN_ERSTSZ        0x28
#define XHC_RUN_ERSTBA        0x30   /* 64-bit */
#define XHC_RUN_ERDP          0x38   /* 64-bit */

#define XHC_IMAN_IP           0x00000001
#define XHC_IMAN_IE           0x00000002
#define XHC_ERDP_EHB          0x00000008   /* QEMU quirk: EHB ack is bit 3, not bit 0 */

/* ---- PORTSC ---- */
#define XHC_PORT_CCS          0x00000001
#define XHC_PORT_PED          0x00000002
#define XHC_PORT_OCA          0x00000008
#define XHC_PORT_PR           0x00000010
#define XHC_PORT_PLS_SHIFT    5
#define XHC_PORT_PP           0x00000200
#define XHC_PORT_SPEED_SHIFT  10
#define XHC_PORT_SPEED_MASK   0x00003C00
#define XHC_PORT_CSC          0x00020000
#define XHC_PORT_PEC          0x00040000
#define XHC_PORT_WRC          0x00080000
#define XHC_PORT_OCC          0x00100000
#define XHC_PORT_PRC          0x00200000
#define XHC_PORT_PLC          0x00400000
#define XHC_PORT_CEC          0x00800000
#define XHC_PORT_CHANGE_BITS  (XHC_PORT_CSC | XHC_PORT_PEC | XHC_PORT_WRC | \
                               XHC_PORT_OCC | XHC_PORT_PRC | XHC_PORT_PLC | \
                               XHC_PORT_CEC)

/* ---- Ring geometry ---- */
#define XHC_CMD_RING_TRBS     32
#define XHC_EVENT_RING_TRBS   256
#define XHC_MAX_SLOTS_ALLOC   257   /* worst-case MaxSlots field is 8 bits + entry 0 */

typedef struct {
    uint32_t d[4];
} __attribute__((packed)) xhci_trb_t;

/* Static guest RAM buffers; this kernel has no MMU (CR0.PG off), so their
 * addresses ARE the physical addresses QEMU DMA's against. 64-byte aligned to
 * satisfy the CRCR/DCBAAP/ERSTBA alignment requirements. */
static xhci_trb_t xhci_cmd_ring[XHC_CMD_RING_TRBS] __attribute__((aligned(64)));
static xhci_trb_t xhci_ev_ring[XHC_EVENT_RING_TRBS] __attribute__((aligned(64)));
static uint64_t   xhci_dcbaap[XHC_MAX_SLOTS_ALLOC] __attribute__((aligned(64)));
static uint32_t   xhci_erst_entry[4] __attribute__((aligned(64)));

static uint32_t xhci_bar      = 0;
static uint32_t xhci_oper     = 0;
static uint32_t xhci_runt     = 0;
static uint32_t xhci_db       = 0;
static uint32_t xhci_maxslots = 0;
static uint32_t xhci_maxintrs = 0;
static uint32_t xhci_maxports = 0;
static uint8_t  xhci_irq_line = 0;
static uint32_t xhci_port_connected = 0;

/* no MMU: dereference physical MMIO directly */
static void     xhci_w32(uint32_t base, uint32_t off, uint32_t val) { *(volatile uint32_t *)(uintptr_t)(base + off) = val; }
static uint32_t xhci_r32(uint32_t base, uint32_t off) { return *(volatile uint32_t *)(uintptr_t)(base + off); }

static void xhci_w64lo_hi(uint32_t base, uint32_t off, uint32_t lo, uint32_t hi) {
    xhci_w32(base, off, lo);
    xhci_w32(base, off + 4, hi);
}

static void xhci_busy_tick(void) {
    for (int i = 0; i < 8; i++)
        __asm__ __volatile__("nop");
    io_wait();
}

/* bounded wait for (reg & mask) == want; returns 1 on success, 0 on timeout */
static int xhci_spin_for(uint32_t base, uint32_t off, uint32_t mask, uint32_t want, uint32_t budget) {
    while (budget--) {
        if ((xhci_r32(base, off) & mask) == want)
            return 1;
        xhci_busy_tick();
    }
    return 0;
}

static const char *xhci_speed_name(uint32_t speed) {
    switch (speed) {
    case 1: return "FULL(1)";
    case 2: return "LOW(2)";
    case 3: return "HIGH(3)";
    case 4: return "SUPER(4)";
    default: return "?";
    }
}

static const char *xhci_pls_name(uint32_t pls) {
    switch (pls & 0xF) {
    case 0: return "U0";       case 1: return "U1";
    case 2: return "U2";       case 3: return "U3";
    case 4: return "Disabled"; case 5: return "RxDetect";
    case 6: return "Inactive"; case 7: return "Polling";
    case 8: return "Recovery"; case 9: return "HotReset";
    case 10: return "CompMode";case 11: return "TestMode";
    case 15: return "Resume";
    default: return "?";
    }
}

static uint32_t xhci_portsc_base(uint32_t port) {
    /* PORTSC for port N at oper + 0x400 + 0x10*(N-1) */
    return xhci_oper + 0x400 + 0x10 * (port - 1);
}

/* Port device bring-up: ensure power, issue a warm-less reset for connected
 * ports (drives QEMU to PED U0), clear the change bits, and print the result. */
static void xhci_probe_port(uint32_t port) {
    uint32_t pbase = xhci_portsc_base(port);
    uint32_t ps;

    /* ensure port power (QEMU powers ports on by default; be spec polite) */
    ps = xhci_r32(pbase, 0);
    if (!(ps & XHC_PORT_PP)) {
        xhci_w32(pbase, 0, ps | XHC_PORT_PP);
    }

    if (ps & XHC_PORT_CCS) {
        /* connected: run a port reset so the port gets PED + speed bits */
        xhci_w32(pbase, 0, XHC_PORT_PR);
        if (!xhci_spin_for(pbase, 0, XHC_PORT_PRC, XHC_PORT_PRC, 200000)) {
            kprintf("[XHCI] port %u: reset timeout PORTSC=0x%X\n", port, xhci_r32(pbase, 0));
        }
        /* pulse the R/W1C change bits to re-arm for the next change */
        ps = xhci_r32(pbase, 0);
        xhci_w32(pbase, 0, ps | XHC_PORT_CHANGE_BITS);
        if (!(ps & XHC_PORT_PED)) {
            kprintf("[XHCI] port %u: reset did not enable port; PORTSC=0x%X\n", port, ps);
        }
    } else {
        /* not connected: still clear stray change bits so they don't linger */
        xhci_w32(pbase, 0, ps | XHC_PORT_CHANGE_BITS);
    }
}

static void xhci_dump_ports(void) {
    xhci_port_connected = 0;
    for (uint32_t port = 1; port <= xhci_maxports; port++) {
        uint32_t pbase   = xhci_portsc_base(port);
        uint32_t ps      = xhci_r32(pbase, 0);
        uint32_t speed   = (ps & XHC_PORT_SPEED_MASK) >> XHC_PORT_SPEED_SHIFT;
        uint32_t pls     = (ps >> XHC_PORT_PLS_SHIFT) & 0xF;
        uint32_t ccs     = !!(ps & XHC_PORT_CCS);
        uint32_t ped     = !!(ps & XHC_PORT_PED);
        kprintf("[XHCI] port %u PORTSC=0x%X CCS=%u PED=%u PR=%u PLS=0x%X(%s) SPEED=0x%X(%s)%s\n",
                port, ps, ccs, ped, !!(ps & XHC_PORT_PR), pls, xhci_pls_name(pls),
                speed, xhci_speed_name(speed),
                ccs && ped ? " <== enabled, device present" : "");
        if (ccs) {
            xhci_port_connected |= (1u << port);
        }
    }

    kprintf("[XHCI] connected ports:");
    for (uint32_t port = 1; port <= xhci_maxports; port++)
        if (xhci_port_connected & (1u << port))
            kprintf(" %u", port);
    kprintf("\n");
}

void xhci_init(uint32_t bar) {
    xhci_bar = bar;

    /* Re-locate PCI info (location/vendor/device already printed by usb_init)
     * so we can report the INTx line for Stage B. */
    pci_dev_info_t info;
    if (!pci_find_device(XHCI_VENDOR, XHCI_DEVICE, &info)) {
        kprintf("[XHCI] FAIL: controller vanished from PCI config space\n");
        return;
    }
    xhci_irq_line = info.irq_line;
    kprintf("[XHCI] INTx: IRQ line %u -> Stage B will claim vector %u (32 + irq line)\n",
            xhci_irq_line, 32u + (xhci_irq_line & 0xFu));

    /* ---- 1. capability registers ---- */
    uint32_t caplen32 = xhci_r32(bar, XHC_CAP_CAPLENGTH);
    uint32_t caplen   = caplen32 & 0x3F;
    if (caplen == 0) caplen = 0x40;   /* broken-revision fallback */
    uint32_t hciver   = (caplen32 >> 16) & 0xFFFF;
    xhci_oper = bar + caplen;

    uint32_t hcsp1 = xhci_r32(bar, XHC_CAP_HCSPARAMS1);
    uint32_t hcsp2 = xhci_r32(bar, XHC_CAP_HCSPARAMS2);
    uint32_t hcsp3 = xhci_r32(bar, XHC_CAP_HCSPARAMS3);
    uint32_t hccp1 = xhci_r32(bar, XHC_CAP_HCCPARAMS1);
    uint32_t dboff_cap = xhci_r32(bar, XHC_CAP_DBOFF);
    uint32_t rtsoff_cap = xhci_r32(bar, XHC_CAP_RTSOFF);

    xhci_maxslots = hcsp1 & XHC_HCSP1_MAXSLOTS_MASK;
    xhci_maxintrs = (hcsp1 >> XHC_HCSP1_MAXINTRS_SHIFT) & 0xFF;
    xhci_maxports = (hcsp1 >> XHC_HCSP1_MAXPORTS_SHIFT) & 0xFF;

    if (xhci_maxslots > XHC_MAX_SLOTS_ALLOC) {
        kprintf("[XHCI] FAIL: MaxSlots=%u exceeds static DCBAAP allocation\n", xhci_maxslots);
        return;
    }

    uint32_t dboff  = xhci_off_of(dboff_cap);
    uint32_t rtsoff = xhci_off_of(rtsoff_cap);
    if (dboff  == 0) dboff  = 0x2000;
    if (rtsoff == 0) rtsoff = 0x1000;
    xhci_db   = bar + dboff;
    xhci_runt = bar + rtsoff;

    kprintf("[XHCI] BAR0=0x%X CAPLENGTH=0x%X HCIVERSION=0x%X oper=0x%X\n",
            bar, caplen, hciver, xhci_oper);
    kprintf("[XHCI] HCSPARAMS1=0x%X MaxSlots=%u MaxIntrs=%u MaxPorts=%u\n",
            hcsp1, xhci_maxslots, xhci_maxintrs, xhci_maxports);
    kprintf("[XHCI] HCSPARAMS2=0x%X HCSPARAMS3=0x%X HCCPARAMS1=0x%X\n", hcsp2, hcsp3, hccp1);
    kprintf("[XHCI] DBOFF=0x%X -> doorbell base 0x%X  RTSOFF=0x%X -> runtime base 0x%X\n",
            dboff_cap, xhci_db, rtsoff_cap, xhci_runt);

    /* ---- 2. host controller reset ---- */
    xhci_w32(xhci_oper, XHC_OPER_USBCMD, XHC_USBCMD_HCRST);
    if (!xhci_spin_for(xhci_oper, XHC_OPER_USBSTS, XHC_USBSTS_HCH, XHC_USBSTS_HCH, 1000000)) {
        kprintf("[XHCI] FAIL: host did not halt after reset USBSTS=0x%X\n",
                xhci_r32(xhci_oper, XHC_OPER_USBSTS));
    }
    xhci_w32(xhci_oper, XHC_OPER_USBCMD, 0);
    kprintf("[XHCI] reset done: USBSTS=0x%X (HCH=%u)\n",
            xhci_r32(xhci_oper, XHC_OPER_USBSTS),
            !!(xhci_r32(xhci_oper, XHC_OPER_USBSTS) & XHC_USBSTS_HCH));

    /* ---- 3. command ring (32 TRBs, first TRB cycle=1) ---- */
    for (int i = 0; i < XHC_CMD_RING_TRBS; i++) {
        xhci_cmd_ring[i].d[0] = 0;
        xhci_cmd_ring[i].d[1] = 0;
        xhci_cmd_ring[i].d[2] = 0;
        xhci_cmd_ring[i].d[3] = 0;
    }
    xhci_cmd_ring[0].d[3] = 1;   /* command TRB cycle state starts at 1 */
    uint32_t crcr_lo = (uint32_t)(uintptr_t)xhci_cmd_ring;
    xhci_w64lo_hi(xhci_oper, XHC_OPER_CRCR, crcr_lo | XHC_CRCR_RCS, 0);
    kprintf("[XHCI] command ring base 0x%X (CRCR=0x%X)\n",
            (uint32_t)(uintptr_t)xhci_cmd_ring,
            xhci_r32(xhci_oper, XHC_OPER_CRCR));

    /* ---- 4. DCBAAP: MaxSlots+1 zeroed 64-bit entries; QEMU never touches [0] ---- */
    for (int i = 0; i <= (int)xhci_maxslots; i++)
        xhci_dcbaap[i] = 0;
    xhci_w64lo_hi(xhci_oper, XHC_OPER_DCBAAP,
                  (uint32_t)(uintptr_t)xhci_dcbaap,
                  (uint32_t)(((uint64_t)(uintptr_t)xhci_dcbaap) >> 32));
    kprintf("[XHCI] DCBAAP=0x%X (%u slot entries, zeroed)\n",
            (uint32_t)(uintptr_t)xhci_dcbaap, xhci_maxslots + 1);

    /* ---- 5. CONFIG = MaxSlotsEn ---- */
    xhci_w32(xhci_oper, XHC_OPER_CONFIG, xhci_maxslots);
    kprintf("[XHCI] CONFIG=0x%X (MaxSlotsEn=%u)\n",
            xhci_r32(xhci_oper, XHC_OPER_CONFIG), xhci_maxslots);

    /* ---- 6. event ring (256 TRBs) + ERST segment + ERDP + IMAN ---- */
    for (int i = 0; i < XHC_EVENT_RING_TRBS; i++) {
        xhci_ev_ring[i].d[0] = 0;
        xhci_ev_ring[i].d[1] = 0;
        xhci_ev_ring[i].d[2] = 0;
        xhci_ev_ring[i].d[3] = 0;
    }
    uint32_t erval = (uint32_t)(uintptr_t)xhci_ev_ring;
    xhci_erst_entry[0] = erval;                          /* DW0: ring addr low */
    xhci_erst_entry[1] = 0;                              /* DW1: high */
    xhci_erst_entry[2] = (uint32_t)XHC_EVENT_RING_TRBS;  /* DW2: TRB count; QEMU 6.2 rejects <<16 form (>4096) */
    xhci_erst_entry[3] = 0;                              /* DW3 */

    xhci_w32(xhci_runt, XHC_RUN_ERSTSZ, 1);              /* exactly 1 segment (QEMU) */
    xhci_w64lo_hi(xhci_runt, XHC_RUN_ERSTBA, (uint32_t)(uintptr_t)xhci_erst_entry, 0);
    /* ERDP = ring start; QEMU requires the EHB ack bit (3) set on write */
    xhci_w64lo_hi(xhci_runt, XHC_RUN_ERDP, erval | XHC_ERDP_EHB, 0);

    xhci_w32(xhci_runt, XHC_RUN_IMAN, XHC_IMAN_IE);
    xhci_w32(xhci_runt, XHC_RUN_IMOD, 0);
    kprintf("[XHCI] event ring base 0x%X ERSTSZ=1 ERSTBA=0x%X ERDP=0x%X|EHB IMAN IE=%u\n",
            erval, (uint32_t)(uintptr_t)xhci_erst_entry,
            xhci_r32(xhci_runt, XHC_RUN_ERDP),
            !!(xhci_r32(xhci_runt, XHC_RUN_IMAN) & XHC_IMAN_IE));

    /* ---- 7. start the host ---- */
    xhci_w32(xhci_oper, XHC_OPER_USBCMD, XHC_USBCMD_RS | XHC_USBCMD_HSEE);
    if (!xhci_spin_for(xhci_oper, XHC_OPER_USBSTS, XHC_USBSTS_CNR, 0, 1000000)) {
        kprintf("[XHCI] FAIL: controller not ready CNR=1 USBSTS=0x%X\n",
                xhci_r32(xhci_oper, XHC_OPER_USBSTS));
    }
    uint32_t usbsts = xhci_r32(xhci_oper, XHC_OPER_USBSTS);
    if (usbsts & XHC_USBSTS_HSE) {
        kprintf("[XHCI] FAIL: host system error HSE=1 USBSTS=0x%X\n", usbsts);
    }
    kprintf("[XHCI] host started: USBCMD=0x%X USBSTS=0x%X HCH=%u CNR=%u HSE=%u\n",
            xhci_r32(xhci_oper, XHC_OPER_USBCMD), usbsts,
            !!(usbsts & XHC_USBSTS_HCH),
            !!(usbsts & XHC_USBSTS_CNR),
            !!(usbsts & XHC_USBSTS_HSE));

    /* let the controller settle, then power/reset/scan every port */
    for (int i = 0; i < 200000; i++)
        xhci_busy_tick();
    for (uint32_t port = 1; port <= xhci_maxports; port++) {
        xhci_probe_port(port);
    }
    xhci_dump_ports();

    kprintf("[XHCI] Stage A complete\n");
}

/* Set of context links and event/command TRB opcodes (fields in bits 15:10 of
 * the control dword). Event types arrive in the same bits of event TRBs. */
#define XHC_TRB_NORMAL       1
#define XHC_TRB_SETUP        2
#define XHC_TRB_DATA         3
#define XHC_TRB_STATUS       4
#define XHC_TRB_LINK         6

#define XHC_TRB_ENABLE_SLOT  9
#define XHC_TRB_DISABLE_SLOT 10
#define XHC_TRB_ADDRESS_DEV  11
#define XHC_TRB_CONFIGURE_EP 12

#define XHC_EVENT_TRANSFER   32
#define XHC_EVENT_CC         33
#define XHC_EVENT_PSC        34

#define XHC_CC_SUCCESS       1
#define XHC_CC_SHORT_PACKET  13

/* Limits for the static per-slot allocations. QEMU arms 2 devices; 16 slots is
 * generous headroom. Buffers are arrays in the flat kernel so their addresses
 * ARE the DMA addresses QEMU reads/writes. */
#define XHC_MAX_DEV          16
#define XHC_CTL_RING_TRBS    16
#define XHC_IN_RING_TRBS     16

/* 512B output context (slot + 15 EP contexts at 32B stride each) */
static uint32_t xhci_devctx[XHC_MAX_DEV][128]   __attribute__((aligned(64)));
/* 256B input context (control + slot + EP0 + EP1 at 32B stride) */
static uint32_t xhci_inctx[XHC_MAX_DEV][64]     __attribute__((aligned(64)));
static xhci_trb_t xhci_ctl_trbs[XHC_MAX_DEV][XHC_CTL_RING_TRBS] __attribute__((aligned(64)));
static xhci_trb_t xhci_in_trbs[XHC_MAX_DEV][XHC_IN_RING_TRBS]   __attribute__((aligned(64)));
static uint8_t xhci_setup[XHC_MAX_DEV][8]     __attribute__((aligned(4)));
static uint8_t xhci_ctlbuf[XHC_MAX_DEV][512]  __attribute__((aligned(4)));
static uint8_t xhci_inbuf[XHC_MAX_DEV][16]    __attribute__((aligned(4)));

typedef struct {
    xhci_trb_t *trbs;
    uint16_t    size;
    uint16_t    enq;      /* next free software index (never == size-1) */
    uint8_t     ccs;      /* cycle bit applied to the next enqueue */
} xhci_ring_t;

static xhci_ring_t xhci_ctlr[XHC_MAX_DEV];
static xhci_ring_t xhci_inr[XHC_MAX_DEV];

static volatile uint32_t xhci_events_processed = 0;
static volatile uint32_t xhci_er_idx = 0;
static uint8_t  xhci_er_ccs = 1;

static volatile uint32_t xhci_cmd_done = 0;
static volatile uint32_t xhci_cmd_ccode = 0xFF;
static volatile uint32_t xhci_cmd_sid = 0;
static volatile uint32_t xhci_cmd_trb_addr = 0;
static uint32_t xhci_cmd_tail = 0;
static uint8_t  xhci_cmd_ccs = 1;

static uint8_t  xhci_ctl_done[XHC_MAX_DEV];
static uint8_t  xhci_ctl_cc[XHC_MAX_DEV];
static uint32_t xhci_ctl_stat_addr[XHC_MAX_DEV];

static uint8_t  xhci_ep1_done[XHC_MAX_DEV];
static uint8_t  xhci_ep1_cc[XHC_MAX_DEV];
static uint32_t xhci_ep1_trb_addr[XHC_MAX_DEV];
static uint16_t xhci_ep1_maxp[XHC_MAX_DEV];
static uint8_t  xhci_ep1_interval[XHC_MAX_DEV];
static uint8_t  xhci_slot_present[XHC_MAX_DEV];

static uint32_t xhci_irq_count = 0;

/* ---- transfer ring helpers (Link TRB at the last slot, cycle-toggling) ---- */

static void xhci_ring_link(xhci_ring_t *r) {
    uint32_t base = (uint32_t)(uintptr_t)r->trbs;
    r->trbs[r->size - 1].d[0] = base;
    r->trbs[r->size - 1].d[1] = 0;
    r->trbs[r->size - 1].d[2] = 0;
    r->trbs[r->size - 1].d[3] = (uint32_t)r->ccs | (1u << 1) |
                                ((uint32_t)XHC_TRB_LINK << 10);
}

static void xhci_ring_init(xhci_ring_t *r, xhci_trb_t *trbs, uint16_t size) {
    r->trbs = trbs;
    r->size = size;
    r->enq  = 0;
    r->ccs  = 1;
    for (uint16_t i = 0; i < size; i++) {
        trbs[i].d[0] = 0; trbs[i].d[1] = 0;
        trbs[i].d[2] = 0; trbs[i].d[3] = 0;
    }
    xhci_ring_link(r);
}

/* Place a TRB; returns the physical index of the written slot. Rewrites the
 * Link TRB just before the enqueue crosses into it. */
static uint16_t xhci_ring_put(xhci_ring_t *r, const xhci_trb_t *t) {
    if (r->enq == r->size - 1) r->enq = 0;
    if (r->enq == r->size - 2) xhci_ring_link(r);
    uint16_t idx = r->enq;
    r->trbs[idx] = *t;
    r->trbs[idx].d[3] |= (uint32_t)r->ccs;
    r->enq++;
    if (r->enq == r->size - 1) {
        r->enq = 0;
        r->ccs ^= 1;
    }
    return idx;
}

/* ---- event ring processing ---- */

static void xhci_prime_ep1(uint8_t sid);

static void xhci_handle_xfer(uint32_t sid, uint32_t epid, uint32_t trb,
                             uint32_t ccode, uint32_t rem) {
    if (sid == 0 || sid >= XHC_MAX_DEV) return;

    if (epid == 1) {
        /* EP0 control: only the IOC status TRB is awaited */
        if (trb == xhci_ctl_stat_addr[sid]) {
            xhci_ctl_cc[sid] = (uint8_t)ccode;
            xhci_ctl_done[sid] = 1;
        }
        return;
    }

    if (epid == 3) {
        /* EP1 IN (HID report ring) */
        if (!xhci_slot_present[sid]) return;
        if (trb != xhci_ep1_trb_addr[sid]) return;
        xhci_ep1_cc[sid] = (uint8_t)ccode;
        if (ccode == XHC_CC_SUCCESS || ccode == XHC_CC_SHORT_PACKET) {
            uint32_t got = xhci_ep1_maxp[sid] > rem ? xhci_ep1_maxp[sid] - rem : 0;
            usb_hid_report(sid, xhci_inbuf[sid], got, (uint8_t)epid);
            xhci_prime_ep1(sid);   /* keep the input ring armed */
        } else {
            xhci_ep1_done[sid] = 1;
            kprintf("[XHCI] slot %u EP1 cc=0x%X rem=%u (stopped)\n", sid, ccode, rem);
        }
        return;
    }
}

/* Consume every valid event; then ack EHB and advance the ERDP register.
 * Single threaded by design: either called from the ISR, or during polling
 * enumeration while the PIC mask / INTE keep interrupts off. */
static void xhci_drain_events(void) {
    for (;;) {
        xhci_trb_t *e = &xhci_ev_ring[xhci_er_idx];
        if ((e->d[3] & 0x1) != xhci_er_ccs) break;
        uint32_t type = (e->d[3] >> 10) & 0x3F;
        switch (type) {
        case XHC_EVENT_CC: {
            uint32_t cc   = (e->d[2] >> 24) & 0xFF;
            uint32_t sid  = (e->d[3] >> 24) & 0xFF;
            uint32_t ptr  = e->d[0];
            if (ptr == xhci_cmd_trb_addr) {
                xhci_cmd_ccode = cc;
                xhci_cmd_sid   = sid;
                xhci_cmd_done  = 1;
            }
            break;
        }
        case XHC_EVENT_TRANSFER: {
            uint32_t cc   = (e->d[2] >> 24) & 0xFF;
            uint32_t rem  = e->d[2] & 0xFFFFFF;
            uint32_t sid  = (e->d[3] >> 24) & 0xFF;
            uint32_t epid = (e->d[3] >> 16) & 0xFF;
            uint32_t trb  = e->d[0];
            xhci_handle_xfer(sid, epid, trb, cc, rem);
            break;
        }
        case XHC_EVENT_PSC:
        default:
            break;   /* port status changes are serviced by the enum path */
        }
        xhci_er_idx++;
        if (xhci_er_idx == XHC_EVENT_RING_TRBS) {
            xhci_er_idx = 0;
            xhci_er_ccs ^= 1;
        }
        xhci_events_processed++;
    }
    xhci_w64lo_hi(xhci_runt, XHC_RUN_ERDP,
                  ((uint32_t)(uintptr_t)xhci_ev_ring + xhci_er_idx * 16) | XHC_ERDP_EHB,
                  0);
}

/* ---- command TRB submission (one in flight at a time) ---- */

static uint32_t xhci_cmd(uint32_t op, uint32_t slotid,
                         uint32_t p0, uint32_t p1, const char *what) {
    xhci_trb_t trb;
    trb.d[0] = p0;
    trb.d[1] = p1;
    trb.d[2] = 0;
    trb.d[3] = ((uint32_t)(slotid & 0xFF) << 24) | ((uint32_t)op << 10);
    trb.d[3] |= xhci_cmd_ccs;

    xhci_cmd_ring[xhci_cmd_tail] = trb;
    xhci_cmd_trb_addr = (uint32_t)(uintptr_t)&xhci_cmd_ring[xhci_cmd_tail];
    xhci_cmd_done = 0;
    xhci_cmd_ccode = 0xFF;

    xhci_w32(xhci_db, 0, 0);   /* ring doorbell 0 (command target) */

    uint32_t waited = 0;
    while (!xhci_cmd_done && waited < 3000000) {
        xhci_drain_events();
        waited++;
        xhci_busy_tick();
    }
    if (!xhci_cmd_done)
        kprintf("[XHCI] cmd %s TIMEOUT (op=0x%X sid=%u) events=%u\n",
                what, op, slotid, xhci_events_processed);

    xhci_cmd_tail = (xhci_cmd_tail + 1) % XHC_CMD_RING_TRBS;
    if (xhci_cmd_tail == 0) xhci_cmd_ccs ^= 1;
    return xhci_cmd_done ? xhci_cmd_ccode : 0xFF;
}

/* ---- EP0 control transfer (setup + data + status with IOC) ---- */

static int xhci_ctl_xfer(uint8_t sid, uint8_t bmrt, uint8_t breq,
                         uint16_t wv, uint16_t wi, uint16_t wl, uint8_t *dbuf) {
    xhci_ring_t *r = &xhci_ctlr[sid];

    /* headroom: setup+data+status <= slots before the Link TRB */
    if ((uint16_t)(r->enq + 3) > (uint16_t)(r->size - 1)) {
        kprintf("[XHCI] slot %u: EP0 ring full\n", sid);
        return -1;
    }

    uint8_t *s = xhci_setup[sid];
    s[0] = bmrt; s[1] = breq;
    s[2] = wv & 0xFF;         s[3] = wv >> 8;
    s[4] = wi & 0xFF;         s[5] = wi >> 8;
    s[6] = wl & 0xFF;         s[7] = wl >> 8;

    xhci_trb_t t;
    uint16_t sidx;

    /* SETUP (IDT: the 8 setup bytes live IN the TRB itself) */
    t.d[0] = (uint32_t)s[0] | ((uint32_t)s[1] << 8) |
             ((uint32_t)s[2] << 16) | ((uint32_t)s[3] << 24);
    t.d[1] = (uint32_t)s[4] | ((uint32_t)s[5] << 8) |
             ((uint32_t)s[6] << 16) | ((uint32_t)s[7] << 24);
    t.d[2] = 8;
    t.d[3] = (1u << 6) | ((uint32_t)XHC_TRB_SETUP << 10);
    xhci_ring_put(r, &t);

    /* DATA (direction follows bmrt bit 7) */
    if (wl) {
        t.d[0] = dbuf ? (uint32_t)(uintptr_t)dbuf : 0;
        t.d[1] = 0;
        t.d[2] = wl;
        t.d[3] = ((uint32_t)XHC_TRB_DATA << 10) | ((bmrt & 0x80) ? 0x10000u : 0);
        xhci_ring_put(r, &t);
    }

    /* STATUS (opposite direction, IOC for the event) */
    t.d[0] = 0;
    t.d[1] = 0;
    t.d[2] = 0;
    t.d[3] = (1u << 5) | ((uint32_t)XHC_TRB_STATUS << 10) |
             ((bmrt & 0x80) ? 0 : 0x10000u);
    sidx = xhci_ring_put(r, &t);

    xhci_ctl_stat_addr[sid] = (uint32_t)(uintptr_t)&xhci_ctl_trbs[sid][sidx];
    xhci_ctl_done[sid] = 0;
    xhci_ctl_cc[sid] = 0xFF;

    xhci_w32(xhci_db, sid * 4, 1);   /* EP0 target */

    uint32_t waited = 0;
    while (!xhci_ctl_done[sid] && waited < 4000000) {
        xhci_drain_events();
        waited++;
        xhci_busy_tick();
    }
    if (!xhci_ctl_done[sid]) {
        kprintf("[XHCI] slot %u: EP0 xfer %X:%X TIMEOUT\n", sid, bmrt, breq);
        return -1;
    }
    if (xhci_ctl_cc[sid] != XHC_CC_SUCCESS) {
        kprintf("[XHCI] slot %u: EP0 xfer %X:%X cc=0x%X\n",
                sid, bmrt, breq, xhci_ctl_cc[sid]);
        return -1;
    }
    return 0;
}

/* ---- context construction ---- */

static void xhci_clear_input(uint8_t sid) {
    for (int i = 0; i < 64; i++) xhci_inctx[sid][i] = 0;
}

/* Input context layout (32B stride): [0]=control, [1]=slot, [2]=EP0, [3]=EP1 */
static int xhci_address_device(uint8_t sid, uint32_t port) {
    xhci_clear_input(sid);
    uint32_t *ictx = xhci_inctx[sid];

    ictx[0] = 0;                          /* Drop = none */
    ictx[1] = 0x03;                       /* Add: slot + EP0 */

    uint32_t *slot = ictx + 8;            /* slot context @ +32 */
    slot[0] = 1u << 27;                   /* ContextEntries = 1 (EP0 only) */
    slot[1] = (port & 0xFF) << 16;        /* Root Hub Port Number */
    slot[2] = 0;
    slot[3] = 0;

    uint32_t *ep0 = ictx + 16;            /* EP0 context @ +64 */
    ep0[0] = 0;                           /* Interval 0, state 0 */
    ep0[1] = (4u << 3) | (64u << 16);     /* CONTROL, MaxPacketSize 64 */
    ep0[2] = (uint32_t)(uintptr_t)xhci_ctl_trbs[sid] | 1;   /* TR Dequeue + DCS */
    ep0[3] = 0;

    uint32_t cc = xhci_cmd(XHC_TRB_ADDRESS_DEV, sid,
                           (uint32_t)(uintptr_t)xhci_inctx[sid], 0, "AddressDevice");
    if (cc != XHC_CC_SUCCESS)
        kprintf("[XHCI] slot %u: AddressDevice cc=0x%X\n", sid, cc);
    return cc == XHC_CC_SUCCESS ? 0 : -1;
}

static int xhci_configure_ep(uint8_t sid) {
    xhci_clear_input(sid);
    uint32_t *ictx = xhci_inctx[sid];

    ictx[0] = 0;                          /* Drop = none */
    ictx[1] = 0x09;                       /* Add: slot + EP1 (bit3) */

    uint32_t *slot = ictx + 8;
    slot[0] = 2u << 27;                   /* ContextEntries = 2 (EP0 + EP1) */

    uint32_t *ep1 = ictx + 32;            /* EP1 context @ +128 (ctx idx 4) */
    ep1[0] = ((uint32_t)xhci_ep1_interval[sid] << 16);     /* Interval */
    ep1[1] = (7u << 3) | ((uint32_t)xhci_ep1_maxp[sid] << 16);  /* INTR_IN */
    ep1[2] = (uint32_t)(uintptr_t)xhci_in_trbs[sid] | 1;    /* TR Dequeue + DCS */
    ep1[3] = 0;

    uint32_t cc = xhci_cmd(XHC_TRB_CONFIGURE_EP, sid,
                           (uint32_t)(uintptr_t)xhci_inctx[sid], 0, "ConfigureEndpoint");
    if (cc != XHC_CC_SUCCESS)
        kprintf("[XHCI] slot %u: ConfigureEndpoint cc=0x%X\n", sid, cc);
    return cc == XHC_CC_SUCCESS ? 0 : -1;
}

static void xhci_disable_slot(uint8_t sid) {
    xhci_cmd(XHC_TRB_DISABLE_SLOT, sid, 0, 0, "DisableSlot");
    xhci_dcbaap[sid] = 0;
    xhci_slot_present[sid] = 0;
}

/* Prime the EP1-IN ring with one IOC Normal TRB and ring the doorbell. */
static void xhci_prime_ep1(uint8_t sid) {
    xhci_ring_t *r = &xhci_inr[sid];
    xhci_trb_t t;
    t.d[0] = (uint32_t)(uintptr_t)xhci_inbuf[sid];
    t.d[1] = 0;
    t.d[2] = xhci_ep1_maxp[sid];
    t.d[3] = (1u << 5) | ((uint32_t)XHC_TRB_NORMAL << 10);
    uint16_t idx = xhci_ring_put(r, &t);
    xhci_ep1_trb_addr[sid] = (uint32_t)(uintptr_t)&xhci_in_trbs[sid][idx];
    xhci_ep1_done[sid] = 0;
    xhci_ep1_cc[sid] = 0xFF;
    xhci_w32(xhci_db, sid * 4, 3);   /* EP1 IN target */
}

static int xhci_enum_device(uint32_t port) {
    uint8_t sid;
    uint32_t cc;

    cc = xhci_cmd(XHC_TRB_ENABLE_SLOT, 0, 0, 0, "EnableSlot");
    if (cc != XHC_CC_SUCCESS || xhci_cmd_sid == 0 || xhci_cmd_sid >= XHC_MAX_DEV) {
        kprintf("[XHCI] slot: Enable Slot failed cc=0x%X sid=%u\n", cc, xhci_cmd_sid);
        return -1;
    }
    sid = (uint8_t)xhci_cmd_sid;
    kprintf("[XHCI] slot %u: Enable Slot OK (port %u, cc=1)\n", sid, port);

    /* Zero the output context QEMU writes into, then point DCBAAP[sid] at it */
    for (int i = 0; i < 128; i++) xhci_devctx[sid][i] = 0;
    xhci_dcbaap[sid] = (uint64_t)(uintptr_t)xhci_devctx[sid];

    if (xhci_address_device(sid, port) != 0) {
        kprintf("[XHCI] slot %u: Address Device FAILED\n", sid);
        xhci_disable_slot(sid);
        return -1;
    }
    kprintf("[XHCI] slot %u: Address Device OK\n", sid);

    /* Fetch the configuration descriptor (wLength=512, one burst) */
    if (xhci_ctl_xfer(sid, 0x80, 6, 0x0200, 0, 512, xhci_ctlbuf[sid]) != 0) {
        kprintf("[XHCI] slot %u: GET_DESCRIPTOR(config) FAILED\n", sid);
        xhci_disable_slot(sid);
        return -1;
    }
    uint16_t total = xhci_ctlbuf[sid][2] | (uint16_t)(xhci_ctlbuf[sid][3] << 8);
    if (total == 0 || total > 512) total = 512;
    kprintf("[XHCI] slot %u: GET_DESCRIPTOR(config) OK (%u bytes)\n", sid, total);

    /* Locate the boot HID interface + interrupt IN endpoint */
    usb_hid_dev_t dev;
    if (!usb_hid_parse_cfgdesc(xhci_ctlbuf[sid], total, &dev)) {
        kprintf("[XHCI] slot %u: no boot HID interface found\n", sid);
        xhci_disable_slot(sid);
        return -1;
    }
    usb_hid_set_dev(sid, &dev);
    kprintf("[XHCI] slot %u: HID dev=%s EP=0x%X maxp=%u interval=%u\n",
            sid, dev.type == USB_HID_MOUSE ? "MOUSE" : "KBD",
            dev.ep_addr, dev.max_packet, dev.interval);

    /* Select configuration 1 */
    if (xhci_ctl_xfer(sid, 0x00, 9, 1, 0, 0, NULL) != 0) {
        kprintf("[XHCI] slot %u: SET_CONFIGURATION FAILED\n", sid);
        xhci_disable_slot(sid);
        return -1;
    }
    kprintf("[XHCI] slot %u: SET_CONFIGURATION(1) OK\n", sid);

    /* Configure EP1 IN as an interrupt endpoint feeding the HID report ring */
    xhci_ep1_maxp[sid] = dev.max_packet ? dev.max_packet : (dev.type == USB_HID_MOUSE ? 4 : 8);
    xhci_ep1_interval[sid] = dev.interval ? dev.interval : 8;
    if (xhci_configure_ep(sid) != 0) {
        kprintf("[XHCI] slot %u: Configure Endpoint FAILED\n", sid);
        xhci_disable_slot(sid);
        return -1;
    }
    kprintf("[XHCI] slot %u: Configure Endpoint OK (EP1 IN maxp=%u)\n",
            sid, xhci_ep1_maxp[sid]);

    xhci_slot_present[sid] = 1;
    xhci_prime_ep1(sid);
    kprintf("[XHCI] slot %u: EP1-IN primed ring=0x%X buf=0x%X\n",
            sid, (uint32_t)(uintptr_t)xhci_in_trbs[sid],
            (uint32_t)(uintptr_t)xhci_inbuf[sid]);
    return 0;
}

int xhci_enum_devices(void) {
    if (!xhci_bar) {
        kprintf("[XHCI] enum: controller not initialized\n");
        return -1;
    }

    /* Reset polling state; any events queued during Stage A (port resets)
     * are consumed here before the first command. */
    xhci_er_idx = 0;
    xhci_er_ccs = 1;
    xhci_cmd_tail = 0;
    xhci_cmd_ccs = 1;
    xhci_drain_events();

    for (int i = 0; i < XHC_MAX_DEV; i++) {
        xhci_ctl_done[i] = 0;
        xhci_ep1_done[i] = 0;
        xhci_ep1_trb_addr[i] = 0;
        xhci_slot_present[i] = 0;
        xhci_ring_init(&xhci_ctlr[i], xhci_ctl_trbs[i], XHC_CTL_RING_TRBS);
        xhci_ring_init(&xhci_inr[i], xhci_in_trbs[i], XHC_IN_RING_TRBS);
    }

    uint32_t connected = 0;
    for (uint32_t port = 1; port <= xhci_maxports; port++) {
        uint32_t ps    = xhci_r32(xhci_portsc_base(port), 0);
        uint32_t speed = (ps & XHC_PORT_SPEED_MASK) >> XHC_PORT_SPEED_SHIFT;
        if (ps & XHC_PORT_CCS) {
            connected++;
            kprintf("[XHCI] enum: port %u device present CCS PED=%u speed=0x%X(%s)\n",
                    port, !!(ps & XHC_PORT_PED), speed, xhci_speed_name(speed));
        }
    }

    if (!connected) {
        kprintf("[XHCI] enum: no devices to enumerate\n");
        return 0;
    }

    int ok = 0;
    for (uint32_t port = 1; port <= xhci_maxports; port++) {
        uint32_t ps = xhci_r32(xhci_portsc_base(port), 0);
        if (ps & XHC_PORT_CCS) {
            if (xhci_enum_device(port) == 0) ok++;
        }
    }

    /* IRQ path: enable the interrupt and unmask the line only after polling
     * enumeration is fully drained (no reentrancy on the event ring). */
    xhci_w32(xhci_oper, XHC_OPER_USBCMD,
             xhci_r32(xhci_oper, XHC_OPER_USBCMD) | XHC_USBCMD_INTE);
    isr_register_handler(32u + (xhci_irq_line & 0xFu), xhci_isr);
    pic_clear_mask(xhci_irq_line & 0xFu);

    kprintf("[XHCI] IRQ path enabled: vector %u (PCI IRQ %u)\n",
            32u + (xhci_irq_line & 0xFu), xhci_irq_line);
    kprintf("[XHCI] Stage B complete: %u device(s) enumerated\n", ok);
    return ok;
}

/* Stage B interrupt handler: clear RW1C pending bits, drain the event ring
 * (arming the HID ring as reports complete), then ack IP/EHB. */
void xhci_isr(registers_t *regs) {
    (void)regs;
    if (!xhci_bar) return;

    uint32_t usbsts = xhci_r32(xhci_oper, XHC_OPER_USBSTS);
    if (usbsts & (XHC_USBSTS_EINT | XHC_USBSTS_PCD | XHC_USBSTS_HSE))
        xhci_w32(xhci_oper, XHC_OPER_USBSTS,
                 usbsts & (XHC_USBSTS_EINT | XHC_USBSTS_PCD | XHC_USBSTS_HSE));

    xhci_drain_events();

    uint32_t iman = xhci_r32(xhci_runt, XHC_RUN_IMAN);
    xhci_w32(xhci_runt, XHC_RUN_IMAN, iman | XHC_IMAN_IP);   /* RW1C */

    xhci_irq_count++;
    kprintf("[XHCI] IRQ %u: USBSTS=0x%X events=%u\n",
            xhci_irq_count, usbsts, xhci_events_processed);
}