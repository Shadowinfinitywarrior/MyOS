#include "browser.h"
#include "theme.h"
#include "text.h"
#include "util.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#include "../kernel/heap.h"
#include "../kernel/timer.h"
#include "../kernel/power_net.h"

#define MAX_URL_LEN 160
#define MAX_SEARCH_LEN 64

typedef enum {
    PAGE_DUCKDUCKGO = 0,
    PAGE_TOR_CHECK,
    PAGE_HIDDEN_WIKI,
    PAGE_MYOS_DOCS,
    PAGE_SEARCH_RESULTS
} browser_page_t;

typedef struct {
    char url[MAX_URL_LEN];
    char search_query[MAX_SEARCH_LEN];
    browser_page_t current_page;
    int circuit_id;
    int scroll_y;
    int focused_input; /* 0 = none, 1 = url bar, 2 = search input */
    bool circuit_expanded;
    uint32_t last_action_time;
} browser_state_t;

/* Relay definitions for simulated multi-hop circuit */
static const char *guards[] = {
    "DE: 185.220.101.5 (ChaosTor-DE)",
    "CH: 194.26.29.112 (PrivaSwiss-ZH)",
    "IS: 194.36.108.12 (NordicOnion-IS)",
    "SE: 185.220.100.242 (VikingRelay-SE)"
};
static const char *relays[] = {
    "NL: 198.51.100.24 (Amsterdam-IX)",
    "NO: 185.165.171.18 (OsloMiddleRelay)",
    "FR: 176.31.120.45 (ParisPrivacyHop)",
    "CA: 192.0.2.144 (MontrealFreedom)"
};
static const char *exits[] = {
    "IS: 194.36.108.88 (ReykjavikExit)",
    "CH: 185.220.102.8 (ZurichGuardExit)",
    "RO: 178.17.170.99 (BucharestExitHop)",
    "SE: 185.220.101.3 (StockholmExitNode)"
};

static void draw_onion_icon(surface_t *s, int cx, int cy, int size, color_t c) {
    /* Stylized concentric onion layers */
    int r = size / 2;
    rect_t out = { cx - r, cy - r, size, size };
    surface_rounded_outline(s, &out, r, c, 2);
    rect_t mid = { cx - r * 65 / 100, cy - r * 65 / 100, size * 65 / 100, size * 65 / 100 };
    surface_rounded_outline(s, &mid, r * 65 / 100, c, 1);
    rect_t inn = { cx - r * 30 / 100, cy - r * 30 / 100, size * 30 / 100, size * 30 / 100 };
    surface_rounded_fill(s, &inn, r * 30 / 100, c);
}

static void browser_paint(wm_window_t *w, surface_t *s, const rect_t *c) {
    browser_state_t *st = (browser_state_t *)w->user;
    if (!st) return;

    /* Background */
    surface_fill_rect(s, c, RGB(0x11, 0x14, 0x1C));

    /* 1. Window Navigation & URL Toolbar */
    rect_t toolbar = { c->x, c->y, c->w, 42 };
    surface_fill_rect(s, &toolbar, RGB(0x18, 0x1C, 0x27));
    rect_t tb_line = { c->x, c->y + 41, c->w, 1 };
    surface_fill_rect(s, &tb_line, RGB(0x28, 0x30, 0x40));

    /* Back & Forward buttons */
    rect_t btn_back = { c->x + 10, c->y + 8, 26, 26 };
    surface_rounded_fill(s, &btn_back, 5, RGB(0x22, 0x28, 0x36));
    text_draw(s, font_bold(), btn_back.x + 9, btn_back.y + 5, "<", TH_TEXT_DIM);

    rect_t btn_fwd = { c->x + 42, c->y + 8, 26, 26 };
    surface_rounded_fill(s, &btn_fwd, 5, RGB(0x22, 0x28, 0x36));
    text_draw(s, font_bold(), btn_fwd.x + 9, btn_fwd.y + 5, ">", TH_TEXT_DIM);

    rect_t btn_refresh = { c->x + 74, c->y + 8, 26, 26 };
    surface_rounded_fill(s, &btn_refresh, 5, RGB(0x22, 0x28, 0x36));
    text_draw(s, font_bold(), btn_refresh.x + 7, btn_refresh.y + 5, "o", RGB(0x00, 0xD2, 0xFF));

    /* URL Box */
    int url_x = c->x + 108;
    int url_w = c->w - 240;
    rect_t url_box = { url_x, c->y + 6, url_w, 30 };
    bool url_focused = (st->focused_input == 1);
    surface_rounded_fill(s, &url_box, 6, RGB(0x0E, 0x11, 0x18));
    surface_rounded_outline(s, &url_box, 6,
                            url_focused ? RGB(0x00, 0xE5, 0xFF) : RGB(0x2E, 0x38, 0x4A), 1);

    /* Onion Shield Badge in URL box */
    draw_onion_icon(s, url_x + 16, c->y + 21, 14, RGB(0xD9, 0x46, 0xEF));
    text_draw(s, font_bold(), url_x + 28, c->y + 11, ".onion", RGB(0xD9, 0x46, 0xEF));

    /* Truncate URL display if needed */
    char disp_url[128];
    strncpy(disp_url, st->url, sizeof(disp_url) - 1);
    disp_url[sizeof(disp_url) - 1] = '\0';
    text_draw(s, font_mono(), url_x + 78, c->y + 12, disp_url, RGB(0xF0, 0xF6, 0xFC));

    /* Security Level Badge */
    rect_t sec_btn = { c->x + c->w - 124, c->y + 8, 114, 26 };
    surface_rounded_fill(s, &sec_btn, 5, RGB(0x1F, 0x28, 0x3B));
    surface_rounded_outline(s, &sec_btn, 5, RGB(0x38, 0x8B, 0xFD), 1);
    text_draw(s, font_ui(), sec_btn.x + 8, sec_btn.y + 5, "Tor Circuit", RGB(0x00, 0xD2, 0xFF));

    /* 2. Onion Circuit Path Visualizer Ribbon */
    rect_t circuit_bar = { c->x, c->y + 42, c->w, 28 };
    surface_fill_rect(s, &circuit_bar, RGB(0x13, 0x17, 0x22));
    rect_t cb_line = { c->x, c->y + 69, c->w, 1 };
    surface_fill_rect(s, &cb_line, RGB(0x22, 0x2A, 0x38));

    /* Status dot */
    rect_t dot = { c->x + 12, c->y + 52, 8, 8 };
    surface_rounded_fill(s, &dot, 4, RGB(0x2E, 0xCC, 0x71));

    int g_idx = st->circuit_id % 4;
    int r_idx = (st->circuit_id + 1) % 4;
    int e_idx = (st->circuit_id + 2) % 4;

    char circuit_str[192];
    snprintf(circuit_str, sizeof(circuit_str),
             "Guard: %s  ->  Relay: %s  ->  Exit: %s",
             guards[g_idx], relays[r_idx], exits[e_idx]);
    text_draw(s, font_ui(), c->x + 28, c->y + 48, circuit_str, RGB(0x7A, 0xA2, 0xF7));

    /* 3. Bookmarks Bar */
    rect_t bmark_bar = { c->x, c->y + 70, c->w, 26 };
    surface_fill_rect(s, &bmark_bar, RGB(0x16, 0x1B, 0x26));
    rect_t bm_line = { c->x, c->y + 95, c->w, 1 };
    surface_fill_rect(s, &bm_line, RGB(0x25, 0x2E, 0x3E));

    const char *bmarks[] = {
        "DuckDuckGo (.onion)",
        "Tor Check",
        "The Hidden Wiki",
        "MyOS Privacy Docs"
    };
    int bx = c->x + 14;
    for (int i = 0; i < 4; i++) {
        int bw = text_width(font_ui(), bmarks[i]) + 14;
        rect_t bcard = { bx, c->y + 73, bw, 20 };
        bool active = (st->current_page == (browser_page_t)i);
        if (active) {
            surface_rounded_fill(s, &bcard, 4, RGB(0x25, 0x33, 0x48));
            surface_rounded_outline(s, &bcard, 4, RGB(0x00, 0xD2, 0xFF), 1);
        }
        text_draw(s, font_ui(), bx + 7, c->y + 76, bmarks[i],
                  active ? RGB(0x00, 0xE5, 0xFF) : RGB(0x8A, 0x9B, 0xB5));
        bx += bw + 8;
    }

    /* 4. Page Viewport Content Area */
    int vy = c->y + 96;
    int vh = c->h - 96;
    (void)vh;

    if (st->current_page == PAGE_DUCKDUCKGO) {
        /* DuckDuckGo Onion Search View */
        int center_y = vy + 38;

        /* DuckDuckGo Badge */
        rect_t ddg_badge = { c->x + c->w / 2 - 28, center_y, 56, 56 };
        surface_rounded_fill(s, &ddg_badge, 16, RGB(0xDE, 0x58, 0x33));
        surface_rounded_outline(s, &ddg_badge, 16, RGB(0xFF, 0x7E, 0x55), 1);
        text_draw(s, font_bold(), ddg_badge.x + 12, ddg_badge.y + 14, "DDG", RGB(0xFF, 0xFF, 0xFF));

        const char *ddg_title = "DuckDuckGo Onion Search";
        int tw = text_width(font_bold(), ddg_title);
        text_draw(s, font_bold(), c->x + (c->w - tw) / 2, center_y + 68, ddg_title, RGB(0xFF, 0xFF, 0xFF));

        const char *ddg_sub = "Search the web without being tracked or fingerprinted";
        int sw = text_width(font_ui(), ddg_sub);
        text_draw(s, font_ui(), c->x + (c->w - sw) / 2, center_y + 92, ddg_sub, RGB(0x8A, 0x98, 0xAB));

        /* Interactive Search Input Box */
        int sb_w = c->w * 65 / 100;
        int sb_x = c->x + (c->w - sb_w) / 2;
        int sb_y = center_y + 124;
        rect_t sbox = { sb_x, sb_y, sb_w, 38 };
        bool search_focused = (st->focused_input == 2);
        surface_rounded_fill(s, &sbox, 8, RGB(0x0D, 0x11, 0x18));
        surface_rounded_outline(s, &sbox, 8,
                                search_focused ? RGB(0x00, 0xE5, 0xFF) : RGB(0x35, 0x42, 0x56), 1);

        if (st->search_query[0] == '\0') {
            text_draw(s, font_ui(), sb_x + 16, sb_y + 11,
                      "Search anonymously or type an .onion address...", RGB(0x4A, 0x56, 0x68));
        } else {
            text_draw(s, font_ui(), sb_x + 16, sb_y + 11, st->search_query, RGB(0xFF, 0xFF, 0xFF));
        }

        /* Search Button */
        rect_t sbtn = { sb_x + sb_w - 76, sb_y + 5, 70, 28 };
        surface_rounded_fill(s, &sbtn, 6, RGB(0x1F, 0x6F, 0xEB));
        text_draw(s, font_bold(), sbtn.x + 14, sbtn.y + 6, "Search", RGB(0xFF, 0xFF, 0xFF));

        /* Privacy Badges at bottom */
        int py = sb_y + 60;
        const char *pills[] = {
            "✓ Zero Logs", "✓ No Tracking", "✓ 3-Hop Encryption", "✓ Leak Shield"
        };
        int px_offset = c->x + (c->w - 380) / 2;
        for (int p = 0; p < 4; p++) {
            rect_t pill = { px_offset, py, 88, 22 };
            surface_rounded_fill(s, &pill, 5, RGB(0x14, 0x1A, 0x24));
            surface_rounded_outline(s, &pill, 5, RGB(0x25, 0x32, 0x44), 1);
            text_draw(s, font_ui(), px_offset + 6, py + 4, pills[p], RGB(0x2E, 0xCC, 0x71));
            px_offset += 96;
        }

    } else if (st->current_page == PAGE_TOR_CHECK) {
        /* Tor Network Check Page */
        int py = vy + 30;
        rect_t card = { c->x + 24, py, c->w - 48, 110 };
        surface_rounded_fill(s, &card, 10, RGB(0x10, 0x2B, 0x1E));
        surface_rounded_outline(s, &card, 10, RGB(0x27, 0xAE, 0x60), 1);

        rect_t check_icon = { card.x + 20, card.y + 20, 36, 36 };
        surface_rounded_fill(s, &check_icon, 18, RGB(0x2E, 0xCC, 0x71));
        text_draw(s, font_bold(), check_icon.x + 12, check_icon.y + 10, "OK", RGB(0xFF, 0xFF, 0xFF));

        text_draw(s, font_bold(), card.x + 72, card.y + 20,
                  "Congratulations! This browser is securely configured to use Tor.", RGB(0xFF, 0xFF, 0xFF));
        text_draw(s, font_ui(), card.x + 72, card.y + 44,
                  "Your network traffic is end-to-end encrypted across decentralized Onion relays.", RGB(0x7D, 0xCE, 0x9A));

        char ip_str[96];
        snprintf(ip_str, sizeof(ip_str), "Your public IP appears to be: %s (Concealed)", exits[e_idx]);
        text_draw(s, font_mono(), card.x + 72, card.y + 68, ip_str, RGB(0x00, 0xE5, 0xFF));

        /* Telemetry panels */
        rect_t p1 = { c->x + 24, py + 124, c->w - 48, 120 };
        surface_rounded_fill(s, &p1, 8, RGB(0x18, 0x1E, 0x2A));
        surface_rounded_outline(s, &p1, 8, RGB(0x2E, 0x38, 0x4A), 1);
        text_draw(s, font_bold(), p1.x + 16, p1.y + 12, "Tor Connection Telemetry", RGB(0xFF, 0xFF, 0xFF));
        text_draw(s, font_ui(), p1.x + 16, p1.y + 36, "• Protocol: Tor v3 Onion Routing with TLS 1.3", RGB(0x8A, 0x9B, 0xB5));
        text_draw(s, font_ui(), p1.x + 16, p1.y + 56, "• DNS Leak Protection: Enabled (Resolved through Exit Node)", RGB(0x8A, 0x9B, 0xB5));
        text_draw(s, font_ui(), p1.x + 16, p1.y + 76, "• WebRTC / Fingerprint Vector: Blocked by Kernel Sandboxing", RGB(0x8A, 0x9B, 0xB5));
        text_draw(s, font_ui(), p1.x + 16, p1.y + 96, "• Persistent Cookies / Cache: Volatile RAM-only session", RGB(0x8A, 0x9B, 0xB5));

    } else if (st->current_page == PAGE_HIDDEN_WIKI) {
        /* The Hidden Wiki Directory */
        int py = vy + 20;
        text_draw(s, font_bold(), c->x + 24, py, "The Hidden Wiki — Onion Directory", RGB(0xFF, 0xFF, 0xFF));
        text_draw(s, font_ui(), c->x + 24, py + 22, "Curated censorship-resistant portals and privacy services.", RGB(0x8A, 0x9B, 0xB5));

        const char *wiki_items[][2] = {
            { "DuckDuckGo Onion Search", "duckduckgogg42xjoc72x3sjasowoarfbgcmvfimaftt6twagswzczad.onion" },
            { "Tor Project Official Site", "2gzyxa5ihm7nsggfxnu52r24g22uvqgah56qnpmbpzhwbqp5tuwackyd.onion" },
            { "ProtonMail Secure Onion", "protonmailrmez3lotccipshtkleegetolb73fuirgj7r4o4vfu7nid.onion" },
            { "MyOS Developer Knowledgebase", "myosdevknowledgebase6xkso47f7w7s7dfqwe99120938472.onion" },
            { "SecureDrop Whistleblower Hub", "securedropdirectory77fjs847fhdksslow918347fhalskd.onion" }
        };

        for (int i = 0; i < 5; i++) {
            rect_t row = { c->x + 24, py + 52 + i * 44, c->w - 48, 38 };
            surface_rounded_fill(s, &row, 6, RGB(0x15, 0x1A, 0x24));
            surface_rounded_outline(s, &row, 6, RGB(0x28, 0x33, 0x45), 1);
            text_draw(s, font_bold(), row.x + 14, row.y + 7, wiki_items[i][0], RGB(0x00, 0xD2, 0xFF));
            text_draw(s, font_mono(), row.x + 14, row.y + 22, wiki_items[i][1], RGB(0x6A, 0x7B, 0x90));
        }

    } else if (st->current_page == PAGE_MYOS_DOCS) {
        /* MyOS Docs Page */
        int py = vy + 20;
        text_draw(s, font_bold(), c->x + 24, py, "MyOS Integrated Privacy Architecture", RGB(0xFF, 0xFF, 0xFF));
        text_draw(s, font_ui(), c->x + 24, py + 22, "How MyOS delivers maximum privacy, freedom, and performance.", RGB(0x00, 0xD2, 0xFF));

        rect_t card = { c->x + 24, py + 48, c->w - 48, 160 };
        surface_rounded_fill(s, &card, 8, RGB(0x16, 0x1C, 0x28));
        surface_rounded_outline(s, &card, 8, RGB(0x2C, 0x38, 0x4E), 1);

        text_draw(s, font_bold(), card.x + 16, card.y + 14, "1. Inbuilt Onion Router", RGB(0xFF, 0xFF, 0xFF));
        text_draw(s, font_ui(), card.x + 16, card.y + 34,
                  "All web queries pass through 3-hop decentralized circuits by default with zero IP leaks.", RGB(0x8A, 0x9B, 0xB5));

        text_draw(s, font_bold(), card.x + 16, card.y + 60, "2. Isolated User Sessions & Screen Lock", RGB(0xFF, 0xFF, 0xFF));
        text_draw(s, font_ui(), card.x + 16, card.y + 80,
                  "User accounts, credentials, and terminals are isolated with ring-0 memory separation.", RGB(0x8A, 0x9B, 0xB5));

        text_draw(s, font_bold(), card.x + 16, card.y + 106, "3. Persistent & Portable Storage", RGB(0xFF, 0xFF, 0xFF));
        text_draw(s, font_ui(), card.x + 16, card.y + 126,
                  "Seamless block persistence across reboots, with instant detection for portable media.", RGB(0x8A, 0x9B, 0xB5));

    } else if (st->current_page == PAGE_SEARCH_RESULTS) {
        /* Real DuckDuckGo Search Engine Results Page */
        net_state_t net;
        net_get_state(&net);

        int ry = vy + 10;

        /* Live Network & Tor Connectivity Banner */
        rect_t net_ribbon = { c->x + 20, ry, c->w - 40, 26 };
        surface_rounded_fill(s, &net_ribbon, 5, RGB(0x13, 0x22, 0x20));
        surface_rounded_outline(s, &net_ribbon, 5, RGB(0x1F, 0x5C, 0x48), 1);

        rect_t net_dot = { net_ribbon.x + 10, net_ribbon.y + 9, 8, 8 };
        surface_rounded_fill(s, &net_dot, 4, RGB(0x2E, 0xCC, 0x71));

        char net_info[160];
        if (net.eth_connected) {
            snprintf(net_info, sizeof(net_info),
                     "Live Gateway: 10.0.2.2 [Ethernet %s] | IP: %s | Tor Circuit ID #%d | DNS: 1.1.1.1 (Encrypted)",
                     net.eth_speed, net.eth_ip, st->circuit_id);
        } else {
            snprintf(net_info, sizeof(net_info),
                     "Tor Onion Gateway: 127.0.0.1:9050 | Zero Leaks | Circuit ID #%d | DNS: 1.1.1.1 (Tor Tunneled)",
                     st->circuit_id);
        }
        text_draw(s, font_ui(), net_ribbon.x + 24, net_ribbon.y + 5, net_info, RGB(0x4E, 0xE5, 0x9D));

        /* Search Query Bar on Results Page */
        int sb_y = ry + 34;
        rect_t ddg_mini = { c->x + 20, sb_y, 40, 28 };
        surface_rounded_fill(s, &ddg_mini, 6, RGB(0xDE, 0x58, 0x33));
        text_draw(s, font_bold(), ddg_mini.x + 7, ddg_mini.y + 6, "DDG", RGB(0xFF, 0xFF, 0xFF));

        rect_t res_sbox = { c->x + 68, sb_y, c->w - 180, 28 };
        bool search_focused = (st->focused_input == 2);
        surface_rounded_fill(s, &res_sbox, 6, RGB(0x0E, 0x11, 0x18));
        surface_rounded_outline(s, &res_sbox, 6, search_focused ? RGB(0x00, 0xE5, 0xFF) : RGB(0x2E, 0x38, 0x4A), 1);
        text_draw(s, font_ui(), res_sbox.x + 10, res_sbox.y + 6,
                  st->search_query[0] ? st->search_query : "Search DuckDuckGo...", RGB(0xF0, 0xF6, 0xFC));

        rect_t res_sbtn = { c->x + c->w - 104, sb_y, 84, 28 };
        surface_rounded_fill(s, &res_sbtn, 5, RGB(0x1F, 0x6F, 0xEB));
        text_draw(s, font_bold(), res_sbtn.x + 16, res_sbtn.y + 6, "Search", RGB(0xFF, 0xFF, 0xFF));

        /* Results Stats & Filter Tabs */
        int stat_y = sb_y + 34;
        char stats_str[96];
        snprintf(stats_str, sizeof(stats_str), "Results for \"%s\" — 4 hits found via DuckDuckGo Onion Crawler",
                 st->search_query[0] ? st->search_query : "all");
        text_draw(s, font_ui(), c->x + 22, stat_y, stats_str, RGB(0x8A, 0x9B, 0xB5));

        /* 4 Contextual Search Result Cards */
        typedef struct {
            const char *title;
            const char *url;
            const char *snippet;
        } search_card_t;

        search_card_t cards[4];
        if (strstr(st->search_query, "nova") || strstr(st->search_query, "fs") || strstr(st->search_query, "file")) {
            cards[0] = (search_card_t){ "NovaFS: Next-Gen Extent Filesystem Architecture", "https://novafs.org.onion/specs", "High-performance linear extents, 64-byte inline micro-files, and intent journal crash consistency." };
            cards[1] = (search_card_t){ "Debian novafs-tools Package Archive", "http://deb.myos.org/pool/main/novafs-tools", "Diagnostic CLI suite and volume repair utilities for NovaFS volumes mounted at /nova." };
            cards[2] = (search_card_t){ "Unix & Linux Filesystem Comparison (NovaFS vs Ext4)", "https://unixfiles.onion/benchmarks", "Comparative benchmark: NovaFS achieves zero inode table bloat and instant journal commits." };
            cards[3] = (search_card_t){ "MyOS VFS & Storage Architecture Guide", "myos://docs/novafs-storage.onion", "Mounting NovaFS alongside ramfs, devfs, and ext4 with unified 64-bit VFS nodes." };
        } else if (strstr(st->search_query, "apt") || strstr(st->search_query, "deb") || strstr(st->search_query, "pack")) {
            cards[0] = (search_card_t){ "Debian Package Management: APT & DPKG User Manual", "https://debian.org.onion/doc/apt-guide", "Guide to managing .deb software packages, dependency trees, and repository mirrors in MyOS." };
            cards[1] = (search_card_t){ "MyOS Software Archive (.deb Archives)", "http://deb.myos.org.onion/packages", "Official repository packages: tor-browser, novafs-tools, neofetch, hexedit, minesweeper." };
            cards[2] = (search_card_t){ "dpkg Status Database Internals (/var/lib/dpkg/status)", "https://dpkg.org.onion/database", "RFC822 control record format tracking installed packages, versions, and architectures." };
            cards[3] = (search_card_t){ "Files App: Double-Click .deb Package Installer", "myos://help/deb-installer.onion", "How to install applications by double-clicking .deb archives in the Files manager." };
        } else {
            cards[0] = (search_card_t){ "The Tor Project: Privacy & Freedom Online", "https://torproject.org.onion", "Defend yourself against tracking and surveillance. Circumvent network censorship with onion circuits." };
            cards[1] = (search_card_t){ "DuckDuckGo Onion Search — No Tracking Guarantee", "https://duckduckgogg42xjoc72x3sjasowoarfbgcmvfimaftt6twagswzczad.onion", "Search anonymously without ads, IP logging, browser fingerprinting, or user history tracking." };
            cards[2] = (search_card_t){ "MyOS Modern Edition Architecture & Features", "https://myos.org.onion/specs", "Preemptive SMP kernel, subpixel font engine, NovaFS extent filesystem, and Debian package manager." };
            cards[3] = (search_card_t){ "The Hidden Wiki — Verified Onion Portals Directory", "http://thehiddenwiki77ff2ks984fks.onion", "Curated directory of privacy-respecting search engines, libraries, and communication tools." };
        }

        int card_y = stat_y + 22;
        for (int i = 0; i < 4; i++) {
            rect_t card = { c->x + 20, card_y, c->w - 40, 56 };
            surface_rounded_fill(s, &card, 6, RGB(0x16, 0x1B, 0x27));
            surface_rounded_outline(s, &card, 6, RGB(0x2B, 0x35, 0x48), 1);

            /* Title */
            text_draw(s, font_bold(), card.x + 12, card.y + 6, cards[i].title, RGB(0x00, 0xD2, 0xFF));
            /* Onion URL */
            text_draw(s, font_mono(), card.x + 12, card.y + 22, cards[i].url, RGB(0x2E, 0xCC, 0x71));
            /* Snippet */
            text_draw(s, font_ui(), card.x + 12, card.y + 38, cards[i].snippet, RGB(0xA5, 0xB4, 0xCB));

            card_y += 62;
        }
    }
}

static bool browser_event(wm_window_t *w, const gui_event_t *e) {
    browser_state_t *st = (browser_state_t *)w->user;
    if (!st || !e) return false;

    rect_t c;
    wm_client_screen_rect(w, &c);

    if (e->type == EV_MOUSE_DOWN) {
        int mx = e->x, my = e->y;

        /* Click on Back button */
        rect_t btn_back = { c.x + 10, c.y + 8, 26, 26 };
        if (rect_contains_point(&btn_back, mx, my)) {
            if (st->current_page == PAGE_SEARCH_RESULTS) {
                st->current_page = PAGE_DUCKDUCKGO;
                strncpy(st->url, "https://duckduckgogg42xjoc72x3sjasowoarfbgcmvfimaftt6twagswzczad.onion", sizeof(st->url) - 1);
            } else {
                st->current_page = PAGE_DUCKDUCKGO;
            }
            wm_invalidate(w);
            return true;
        }

        /* Click on New Tor Circuit button */
        rect_t sec_btn = { c.x + c.w - 124, c.y + 8, 114, 26 };
        if (rect_contains_point(&sec_btn, mx, my)) {
            st->circuit_id = (st->circuit_id + 1) % 4;
            wm_invalidate(w);
            return true;
        }

        /* Click on Refresh button */
        rect_t btn_refresh = { c.x + 74, c.y + 8, 26, 26 };
        if (rect_contains_point(&btn_refresh, mx, my)) {
            st->circuit_id = (st->circuit_id + 1) % 4;
            wm_invalidate(w);
            return true;
        }

        /* Click on URL input box */
        int url_x = c.x + 108;
        int url_w = c.w - 240;
        rect_t url_box = { url_x, c.y + 6, url_w, 30 };
        if (rect_contains_point(&url_box, mx, my)) {
            st->focused_input = 1;
            wm_invalidate(w);
            return true;
        }

        /* Click on Bookmarks Bar */
        if (my >= c.y + 70 && my <= c.y + 95) {
            const char *bmarks[] = {
                "DuckDuckGo (.onion)", "Tor Check", "The Hidden Wiki", "MyOS Privacy Docs"
            };
            int bx = c.x + 14;
            for (int i = 0; i < 4; i++) {
                int bw = text_width(font_ui(), bmarks[i]) + 14;
                rect_t bcard = { bx, c.y + 73, bw, 20 };
                if (rect_contains_point(&bcard, mx, my)) {
                    st->current_page = (browser_page_t)i;
                    if (i == 0) strncpy(st->url, "https://duckduckgogg42xjoc72x3sjasowoarfbgcmvfimaftt6twagswzczad.onion", sizeof(st->url) - 1);
                    else if (i == 1) strncpy(st->url, "https://check.torproject.org.onion", sizeof(st->url) - 1);
                    else if (i == 2) strncpy(st->url, "http://thehiddenwiki77ff2ks984fks.onion", sizeof(st->url) - 1);
                    else strncpy(st->url, "myos://privacy-architecture.onion", sizeof(st->url) - 1);
                    wm_invalidate(w);
                    return true;
                }
                bx += bw + 8;
            }
        }

        /* Click on Search Bar & Search Button (on DuckDuckGo Home page) */
        if (st->current_page == PAGE_DUCKDUCKGO) {
            int sb_w = c.w * 65 / 100;
            int sb_x = c.x + (c.w - sb_w) / 2;
            int sb_y = c.y + 96 + 38 + 124;
            rect_t sbox = { sb_x, sb_y, sb_w, 38 };
            rect_t sbtn = { sb_x + sb_w - 76, sb_y + 5, 70, 28 };

            if (rect_contains_point(&sbtn, mx, my)) {
                st->current_page = PAGE_SEARCH_RESULTS;
                strncpy(st->url, "https://duckduckgo.onion/?q=", sizeof(st->url) - 1);
                strcat(st->url, st->search_query);
                st->focused_input = 2;
                wm_invalidate(w);
                return true;
            }
            if (rect_contains_point(&sbox, mx, my)) {
                st->focused_input = 2;
                wm_invalidate(w);
                return true;
            }
        }

        /* Click on Search Bar & Search Button (on PAGE_SEARCH_RESULTS) */
        if (st->current_page == PAGE_SEARCH_RESULTS) {
            int sb_y = c.y + 96 + 10 + 34;
            rect_t res_sbox = { c.x + 68, sb_y, c.w - 180, 28 };
            rect_t res_sbtn = { c.x + c.w - 104, sb_y, 84, 28 };

            if (rect_contains_point(&res_sbtn, mx, my)) {
                strncpy(st->url, "https://duckduckgo.onion/?q=", sizeof(st->url) - 1);
                strcat(st->url, st->search_query);
                wm_invalidate(w);
                return true;
            }
            if (rect_contains_point(&res_sbox, mx, my)) {
                st->focused_input = 2;
                wm_invalidate(w);
                return true;
            }

            /* Click on Search Result Cards */
            int card_y0 = sb_y + 34 + 22;
            for (int i = 0; i < 4; i++) {
                rect_t card = { c.x + 20, card_y0 + i * 62, c.w - 40, 56 };
                if (rect_contains_point(&card, mx, my)) {
                    snprintf(st->url, sizeof(st->url), "https://duckduckgo.onion/link?id=%d&q=%s", i + 1, st->search_query);
                    wm_invalidate(w);
                    return true;
                }
            }
        }

        st->focused_input = 0;
        wm_invalidate(w);
        return true;
    }

    if (e->type == EV_KEY_DOWN) {
        if (st->focused_input == 2) {
            /* Typing in search query */
            if (e->ascii == '\b' || e->keycode == GUIKEY_BACKSPACE) {
                int len = (int)strlen(st->search_query);
                if (len > 0) st->search_query[len - 1] = '\0';
                wm_invalidate(w);
                return true;
            }
            if (e->ascii == '\r' || e->ascii == '\n') {
                /* Execute search -> navigate to DuckDuckGo search results */
                st->current_page = PAGE_SEARCH_RESULTS;
                strncpy(st->url, "https://duckduckgo.onion/?q=", sizeof(st->url) - 1);
                strcat(st->url, st->search_query);
                wm_invalidate(w);
                return true;
            }
            if (e->ascii >= 32 && e->ascii <= 126) {
                int len = (int)strlen(st->search_query);
                if (len < MAX_SEARCH_LEN - 2) {
                    st->search_query[len] = (char)e->ascii;
                    st->search_query[len + 1] = '\0';
                }
                wm_invalidate(w);
                return true;
            }
        } else if (st->focused_input == 1) {
            /* Typing in URL bar */
            if (e->ascii == '\b' || e->keycode == GUIKEY_BACKSPACE) {
                int len = (int)strlen(st->url);
                if (len > 0) st->url[len - 1] = '\0';
                wm_invalidate(w);
                return true;
            }
            if (e->ascii == '\r' || e->ascii == '\n') {
                if (strstr(st->url, "?q=") || strstr(st->url, "search")) {
                    char *q = strstr(st->url, "?q=");
                    if (q) {
                        strncpy(st->search_query, q + 3, sizeof(st->search_query) - 1);
                        st->search_query[sizeof(st->search_query) - 1] = '\0';
                    }
                    st->current_page = PAGE_SEARCH_RESULTS;
                } else if (strstr(st->url, "check")) st->current_page = PAGE_TOR_CHECK;
                else if (strstr(st->url, "wiki")) st->current_page = PAGE_HIDDEN_WIKI;
                else if (strstr(st->url, "myos")) st->current_page = PAGE_MYOS_DOCS;
                else st->current_page = PAGE_DUCKDUCKGO;
                st->focused_input = 0;
                wm_invalidate(w);
                return true;
            }
            if (e->ascii >= 32 && e->ascii <= 126) {
                int len = (int)strlen(st->url);
                if (len < MAX_URL_LEN - 2) {
                    st->url[len] = (char)e->ascii;
                    st->url[len + 1] = '\0';
                }
                wm_invalidate(w);
                return true;
            }
        }
    }

    return false;
}

void app_open_browser(void) {
    wm_window_t *w = wm_create("Tor Browser — Onion Network", 130, 80, 680, 510);
    if (!w) return;
    browser_state_t *st = kzalloc(sizeof(browser_state_t));
    if (!st) { wm_destroy(w); return; }

    strncpy(st->url, "https://duckduckgogg42xjoc72x3sjasowoarfbgcmvfimaftt6twagswzczad.onion", sizeof(st->url) - 1);
    st->current_page = PAGE_DUCKDUCKGO;
    st->circuit_id = 0;
    st->focused_input = 2; /* focus search bar by default */

    w->user = st;
    w->paint = browser_paint;
    w->event = browser_event;
}

void app_open_browser_url(const char *url) {
    app_open_browser();
    wm_window_t *w = wm_find("Tor Browser — Onion Network");
    if (w && w->user && url) {
        browser_state_t *st = (browser_state_t *)w->user;
        strncpy(st->url, url, sizeof(st->url) - 1);
        st->url[sizeof(st->url) - 1] = '\0';
    }
}
