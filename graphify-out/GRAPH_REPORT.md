# Graph Report - myos  (2026-09-24)

## Corpus Check
- 222 files · ~99,783 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 1524 nodes · 3660 edges · 98 communities (87 shown, 11 thin omitted)
- Extraction: 81% EXTRACTED · 19% INFERRED · 0% AMBIGUOUS · INFERRED: 685 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- syscall.c
- dhcp.c
- stbtt_fontinfo
- settings.c
- stb_truetype.h
- terminal.c
- kmalloc
- ext2.c
- devfs.c
- screen.c
- libc.c
- file_explorer.c
- browser.c
- apic.c
- stbtt_pack_context
- string.c
- ttUSHORT
- system.h
- keyboard.c
- kprintf
- registers
- printf.h
- types.h
- mouse.c
- xhci.c
- stbtt__buf
- strcpy
- math.h
- vga_gfx.c
- MYOS Current Architecture Assessment
- stbtt__run_charstring
- fb_get_info
- term_draw_tab_bar
- graphify_c_normalize.py
- gpt.c
- pci.c
- socket.c
- ttSHORT
- MyOS — Agent Task List: Demo OS → Real 64-bit GUI OS
- pthread.c
- Current MYOS GUI Architecture Audit
- ring_buffer.c
- framebuffer.c
- serial.c
- outb
- ne2k.c
- tss.c
- usb_hid.c
- list.c
- stbtt_BakeFontBitmap
- efi_main
- shm.c
- MyOS GUI Stack Public Interface
- ac97.c
- speaker.c
- ramfs.c
- rwlock.h
- Components
- usb.c
- xhci_enum_device
- emmintrin.h
- dynlink.c
- build_toolchain.sh
- MYOS Architecture Assessment — 2026-09-19
- xhci_ring_put
- screen.h
- syscall64.c
- tty.c
- sanitize_extraction.py
- pic.h
- tools/qa/run_qa.sh
- create_disk.sh
- tests/qa/run_qa.sh
- MyOS Boot Progress Documentation
- 1. Framebuffer Implementation
- 7. Window Management (`gui/wm2.c`)
- 5. Input Handling
- MyLang Toolchain
- 10. Widget Toolkit (Retained-Mode UI) (`gui/widget.c`, `gui/scene.c`)
- 12. Animation System (`gui/anim.c`)
- 13. Memory & Boot
- 2. Graphics Rendering Functions
- 4. Text/Font Rendering (`gui/font.c`, `gui/font.h`)
- 6. Current Compositor (`gui/compositor.c`)
- MyOS - Kernel From Scratch
- QA Benchmark
- MyOS Manual QA Checklist
- 11. Surface Management (`gui/surface.c`)
- 13. Interrupt Handling (`kernel/isr.c`, `kernel/idt.c`)
- 4. Current Drawing Code (Blue Background + Icons)
- 8. Desktop Shell (Minimal)
- manual_checklist.md

## God Nodes (most connected - your core abstractions)
1. `kprintf()` - 100 edges
2. `stbtt_fontinfo` - 72 edges
3. `term_handle_key_down()` - 31 edges
4. `kernel_main_64()` - 26 edges
5. `strcpy()` - 25 edges
6. `fb_draw_pixel()` - 24 edges
7. `inb()` - 24 edges
8. `registers` - 23 edges
9. `strlen()` - 22 edges
10. `outb()` - 21 edges

## Surprising Connections (you probably didn't know these)
- `ac97_init()` --calls--> `kprintf()`  [INFERRED]
  drivers/ac97.c → lib/printf.c
- `fb_wait_vsync()` --calls--> `inb()`  [INFERRED]
  drivers/framebuffer.c → include/system.h
- `nvme_init()` --calls--> `kprintf()`  [INFERRED]
  drivers/nvme.c → lib/printf.c
- `serial_read()` --calls--> `inb()`  [INFERRED]
  drivers/serial.c → include/system.h
- `serial_is_transmit_empty()` --calls--> `inb()`  [INFERRED]
  drivers/serial.c → include/system.h

## Import Cycles
- None detected.

## Communities (98 total, 11 thin omitted)

### Community 0 - "syscall.c"
Cohesion: 0.05
Nodes (84): ahci_init(), keyboard_getchar(), vfs_node_t, vfs_finddir(), vfs_read(), vfs_resolve_path(), vfs_set_root(), vfs_write() (+76 more)

### Community 1 - "dhcp.c"
Cohesion: 0.06
Nodes (35): timer_add(), arp_handle_packet(), arp_resolve(), arp_send_request(), arp_update(), dhcp_discover(), dhcp_parse_msg_type(), dhcp_parse_yiaddr() (+27 more)

### Community 2 - "stbtt_fontinfo"
Cohesion: 0.09
Nodes (57): main(), stbtt_BakeFontBitmap_internal(), stbtt_FindGlyphIndex(), stbtt_fontinfo, cff, charstrings, data, fdselect (+49 more)

### Community 3 - "settings.c"
Cohesion: 0.11
Nodes (43): setting_item_t, setting_type_t, strncpy(), add_button(), add_dropdown(), add_section(), add_slider(), add_toggle() (+35 more)

### Community 4 - "stb_truetype.h"
Cohesion: 0.08
Nodes (47): equal(), stbtt__active_edge, next, stbtt__add_point(), stbtt__compute_crossings_x(), stbtt__cuberoot(), stbtt__edge, invert (+39 more)

### Community 5 - "terminal.c"
Cohesion: 0.11
Nodes (52): term_tab_t, memcpy(), memmove(), term_atoi(), term_close_tab(), term_execute_builtin(), term_get_active_tab(), term_handle_key_down() (+44 more)

### Community 6 - "kmalloc"
Cohesion: 0.11
Nodes (27): ata_read_sectors(), ata_device_t, fat16_dir_entry_t, fat16_fs_t, ata_device_t, vfs_node_t, fat16_dir_finddir(), fat16_file_read() (+19 more)

### Community 7 - "ext2.c"
Cohesion: 0.12
Nodes (32): virtio_blk_init(), virtio_blk_read(), virtio_blk_write(), virtio_net_init(), virtio_net_receive(), virtio_net_send(), virtio_device_ready(), virtio_device_reset() (+24 more)

### Community 8 - "devfs.c"
Cohesion: 0.07
Nodes (34): datetime_t, rtc_get_time(), rtc_read_reg(), fd_set_t, vfs_node_t, devconsole_read(), devconsole_write(), devfs_finddir() (+26 more)

### Community 9 - "screen.c"
Cohesion: 0.08
Nodes (24): draw_char(), fbcon_clear(), fbcon_init(), fbcon_putchar(), fbcon_scroll(), fbcon_write(), fbterm_clear(), fbterm_init() (+16 more)

### Community 10 - "libc.c"
Cohesion: 0.10
Nodes (29): main(), main(), main(), main(), main(), pid_t, close(), exit() (+21 more)

### Community 11 - "file_explorer.c"
Cohesion: 0.12
Nodes (35): window_t, draw_char_fb(), draw_file_list(), draw_rect_fb(), draw_round_rect_fb(), draw_sidebar(), draw_status_bar(), draw_string_fb() (+27 more)

### Community 12 - "browser.c"
Cohesion: 0.18
Nodes (35): fb_draw_pixel(), mouse_get_state(), timer_get_ticks(), mouse_state_t, browser_draw(), browser_draw_bookmarks_bar(), browser_draw_downloads(), browser_draw_nav_buttons() (+27 more)

### Community 13 - "apic.c"
Cohesion: 0.10
Nodes (36): acpi_rsdp_t, acpi_checksum(), acpi_find_table(), acpi_get_num_cpus(), acpi_init(), find_rsdp(), apic_detect(), apic_eoi() (+28 more)

### Community 14 - "stbtt_pack_context"
Cohesion: 0.07
Nodes (29): stbrp_init_target(), stbrp_pack_rects(), stbrp_rect, h, id, w, was_packed, x (+21 more)

### Community 15 - "string.c"
Cohesion: 0.10
Nodes (14): bitmap_t, bitmap_clear(), bitmap_clear_range(), bitmap_find_contiguous(), bitmap_find_first_free(), bitmap_init(), bitmap_set(), bitmap_set_range() (+6 more)

### Community 16 - "ttUSHORT"
Cohesion: 0.17
Nodes (27): stbtt_CompareUTF8toUTF16_bigendian(), stbtt_CompareUTF8toUTF16_bigendian_internal(), stbtt__CompareUTF8toUTF16_bigendian_prefix(), stbtt__find_table(), stbtt_FindMatchingFont(), stbtt_FindMatchingFont_internal(), stbtt__get_svg(), stbtt__GetCoverageIndex() (+19 more)

### Community 18 - "keyboard.c"
Cohesion: 0.17
Nodes (19): registers_t, kbd_device_command(), kbd_wait_ibf(), kbd_wait_obf(), keyboard_callback(), keyboard_get_event(), keyboard_handle_key_down(), keyboard_handle_key_up() (+11 more)

### Community 19 - "kprintf"
Cohesion: 0.14
Nodes (22): usb_init(), virtio_init(), gpt_init_storage(), gpt_list_partitions(), init_phase8(), mmap_init(), socket_init(), registers_t (+14 more)

### Community 20 - "registers"
Cohesion: 0.09
Nodes (23): registers, cs, err_code, int_no, r10, r11, r12, r13 (+15 more)

### Community 21 - "printf.h"
Cohesion: 0.08
Nodes (6): nvme_init(), vfs_node_t, procfs_init(), irq_init(), module_init(), slab_init()

### Community 23 - "mouse.c"
Cohesion: 0.17
Nodes (15): apply_sensitivity(), registers_t, get_screen_bounds(), mouse_callback(), mouse_get_event(), mouse_init(), mouse_process_packet(), mouse_queue_event() (+7 more)

### Community 24 - "xhci.c"
Cohesion: 0.28
Nodes (19): registers_t, xhci_busy_tick(), xhci_ctl_xfer(), xhci_drain_events(), xhci_dump_ports(), xhci_enum_devices(), xhci_handle_xfer(), xhci_init() (+11 more)

### Community 25 - "stbtt__buf"
Cohesion: 0.38
Nodes (19): stbtt__buf_get(), stbtt__buf_get8(), stbtt__buf_peek8(), stbtt__buf_range(), stbtt__buf_seek(), stbtt__buf_skip(), stbtt__cff_get_index(), stbtt__cff_index_count() (+11 more)

### Community 26 - "strcpy"
Cohesion: 0.19
Nodes (22): keyboard_get_modifiers(), browser_add_tab(), browser_close_tab(), browser_go_back(), browser_go_forward(), browser_go_home(), browser_handle_key_down(), browser_handle_mouse_down() (+14 more)

### Community 28 - "vga_gfx.c"
Cohesion: 0.18
Nodes (11): gfx_clear(), gfx_draw_char(), gfx_draw_circle(), gfx_draw_line(), gfx_draw_rect(), gfx_draw_string(), gfx_fill_circle(), gfx_init() (+3 more)

### Community 29 - "MYOS Current Architecture Assessment"
Cohesion: 0.12
Nodes (15): Applications / Desktop Shell, Boot, Build and Boot Status, Dependency Graph Summary, Drivers, Filesystem / VFS, First Milestone Target, Graphics / Compositor (+7 more)

### Community 30 - "stbtt__run_charstring"
Cohesion: 0.27
Nodes (16): stbtt__close_shape(), stbtt__csctx_close_shape(), stbtt__csctx_rccurve_to(), stbtt__csctx_rline_to(), stbtt__csctx_rmove_to(), stbtt__csctx_v(), stbtt_FreeShape(), stbtt_GetCodepointShape() (+8 more)

### Community 31 - "fb_get_info"
Cohesion: 0.17
Nodes (13): fb_get_info(), fb_info_t, browser_init_window(), memset(), settings_init_window(), term_add_tab(), term_calc_content_area(), term_init() (+5 more)

### Community 32 - "term_draw_tab_bar"
Cohesion: 0.22
Nodes (15): strcat(), draw_char_scaled(), draw_line(), draw_round_rect(), draw_shadow(), draw_string_ellipsis(), draw_string_scaled(), fill_rect() (+7 more)

### Community 33 - "graphify_c_normalize.py"
Cohesion: 0.19
Nodes (12): _blank(), install(), _line_start(), _match_paren(), normalize_c(), C/C++ source normalizer for graphify's tree-sitter extraction. tree-sitter-c…, Monkeypatch graphify's generic extractor to normalize C/C++ first.…, Replace buf[start:end] with spaces, preserving newlines and length. (+4 more)

### Community 34 - "gpt.c"
Cohesion: 0.15
Nodes (7): vfs_node_t, ext4_mount(), gpt_mount_ext4(), mount_ext4_for_partitions(), gpt_get_partition(), gpt_get_partition_count(), partition_info_t

### Community 35 - "pci.c"
Cohesion: 0.29
Nodes (10): pci_read(), pci_find_device(), pci_make_addr(), pci_read_cfg(), pci_read_cfg16(), pci_read_cfg8(), pci_write_cfg(), inl() (+2 more)

### Community 36 - "socket.c"
Cohesion: 0.32
Nodes (11): alloc_socket(), get_socket(), sys_accept(), sys_bind(), sys_connect(), sys_listen(), sys_socket(), sys_socket_close() (+3 more)

### Community 37 - "ttSHORT"
Cohesion: 0.18
Nodes (13): stbtt_GetFontBoundingBox(), stbtt_GetFontVMetricsOS2(), stbtt__GetGlyfOffset(), stbtt_GetGlyphBox(), stbtt__GetGlyphInfoT2(), stbtt_GetKerningTable(), stbtt_IsGlyphEmpty(), stbtt_kerningentry (+5 more)

### Community 38 - "MyOS — Agent Task List: Demo OS → Real 64-bit GUI OS"
Cohesion: 0.14
Nodes (13): Directive 0 — Ground Rules (apply to every task), Execution Protocol, First Three Tasks (start here), MyOS — Agent Task List: Demo OS → Real 64-bit GUI OS, Phase 1 — x86_64 Port & Boot (blocks everything), Phase 2 — Memory & Paging (blocks processes/FS/GUI buffers), Phase 3 — Scheduler, Processes, Syscalls, Phase 4 — Drivers (storage, display, net, input) (+5 more)

### Community 39 - "pthread.c"
Cohesion: 0.21
Nodes (8): mutex_init(), mutex_lock(), mutex_unlock(), pthread_create(), pthread_join(), mutex_t, pthread_start_t, pthread_t

### Community 40 - "Current MYOS GUI Architecture Audit"
Cohesion: 0.18
Nodes (10): 14. Current Limitations, 15. Files by Responsibility, 16. Resolution & Pixel Format Summary, 17. Build & Test Commands, 3. Low-Level Blitting (`gui/blit.c`), 9. Application Registry (`gui/app_registry.c`), Current MYOS GUI Architecture Audit, Overview (+2 more)

### Community 41 - "ring_buffer.c"
Cohesion: 0.35
Nodes (9): ring_buffer_available(), ring_buffer_clear(), ring_buffer_get(), ring_buffer_init(), ring_buffer_put(), ring_buffer_read(), ring_buffer_used(), ring_buffer_write() (+1 more)

### Community 42 - "framebuffer.c"
Cohesion: 0.27
Nodes (8): bga_read(), bga_write(), fb_detect(), fb_init(), fb_wait_vsync(), find_bga_pci_bar0(), inw(), outw()

### Community 43 - "serial.c"
Cohesion: 0.22
Nodes (7): serial_init(), serial_is_transmit_empty(), serial_read(), serial_write(), put_num(), serial_printf(), test_all_stubs()

### Community 44 - "outb"
Cohesion: 0.31
Nodes (9): hang(), io_wait(), outb(), acpi_reboot(), acpi_shutdown(), pic_clear_mask(), pic_init(), pic_send_eoi() (+1 more)

### Community 45 - "ne2k.c"
Cohesion: 0.36
Nodes (8): ne2k_detect(), ne2k_inb(), ne2k_init(), ne2k_irq_handler(), ne2k_outb(), ne2k_recv(), ne2k_send(), ne2k_dev_t

### Community 46 - "tss.c"
Cohesion: 0.19
Nodes (8): gdt_init(), gdt_set_gate(), gdt_set_tss(), idt_init(), idt_load(), idt_set_gate(), load_tss(), tss_init()

### Community 47 - "usb_hid.c"
Cohesion: 0.24
Nodes (7): mouse_input(), usb_hid_init(), usb_hid_mouse_report(), usb_hid_parse_cfgdesc(), usb_hid_report(), usb_hid_set_dev(), usb_hid_dev_t

### Community 48 - "list.c"
Cohesion: 0.36
Nodes (7): list_add(), list_add_tail(), list_count(), list_empty(), list_init(), list_remove(), list_node_t

### Community 49 - "stbtt_BakeFontBitmap"
Cohesion: 0.22
Nodes (9): my_stbtt_initfont(), my_stbtt_print(), stbtt_BakeFontBitmap(), stbtt_GetBakedQuad(), stbtt_GetPackedQuad(), stbtt_PackFontRange(), stbtt_aligned_quad, stbtt_bakedchar (+1 more)

### Community 50 - "efi_main"
Cohesion: 0.32
Nodes (6): efi_main(), efi_print(), CHAR16, EFI_HANDLE, EFI_STATUS, EFI_SYSTEM_TABLE

### Community 53 - "shm.c"
Cohesion: 0.25
Nodes (4): shm_attach(), shm_create(), shm_destroy(), shm_init()

### Community 54 - "MyOS GUI Stack Public Interface"
Cohesion: 0.18
Nodes (10): gui/anim.h, gui/blit.h, gui/compositor.h, gui/font.h, gui/input.h, gui/rect.h, gui/scene.h, gui/surface.h (+2 more)

### Community 56 - "speaker.c"
Cohesion: 0.43
Nodes (5): speaker_beep(), speaker_init(), speaker_off(), speaker_on(), speaker_play_note()

### Community 57 - "ramfs.c"
Cohesion: 0.60
Nodes (5): vfs_node_t, ramfs_add_child(), ramfs_finddir(), ramfs_init(), ramfs_mount_dev()

### Community 58 - "rwlock.h"
Cohesion: 0.48
Nodes (6): rwlock_init(), rwlock_rdlock(), rwlock_rdunlock(), rwlock_wrlock(), rwlock_wrunlock(), rwlock_t

### Community 59 - "Components"
Cohesion: 0.20
Nodes (9): 1. Compiler, 2. Composer, 3. Interpreter, 4. Runtime Requirements, Build Targets, Components, File Layout, MyLang Toolchain Requirements (+1 more)

### Community 61 - "xhci_enum_device"
Cohesion: 0.60
Nodes (6): xhci_address_device(), xhci_clear_input(), xhci_cmd(), xhci_configure_ep(), xhci_disable_slot(), xhci_enum_device()

### Community 62 - "emmintrin.h"
Cohesion: 0.47
Nodes (4): _mm_loadu_si128(), _mm_set1_epi32(), _mm_stream_si128(), __m128i

### Community 64 - "build_toolchain.sh"
Cohesion: 0.40
Nodes (4): PATH, PREFIX, build_toolchain.sh script, TARGET

### Community 65 - "MYOS Architecture Assessment — 2026-09-19"
Cohesion: 0.22
Nodes (8): Dependency graph (high level), Executive summary, First implementation milestone, Incomplete / broken / demo-only, Migration plan aligned to plan.md, MYOS Architecture Assessment — 2026-09-19, Repository structure, What works

### Community 66 - "xhci_ring_put"
Cohesion: 0.70
Nodes (5): xhci_ring_init(), xhci_ring_link(), xhci_ring_put(), xhci_ring_t, xhci_trb_t

### Community 67 - "screen.h"
Cohesion: 0.32
Nodes (3): mylang_program_t, emit(), mylang_compile()

### Community 68 - "syscall64.c"
Cohesion: 0.60
Nodes (3): rdmsr(), syscall_init_64(), wrmsr()

### Community 70 - "sanitize_extraction.py"
Cohesion: 0.60
Nodes (4): _is_host_tool(), main(), Extraction post-pass that cleans edge-integrity issues graphify's auditor…, sanitize()

### Community 71 - "pic.h"
Cohesion: 0.25
Nodes (5): isr_handler_t, registers_t, isr_handler(), isr_init(), isr_register_handler()

### Community 78 - "MyOS Boot Progress Documentation"
Cohesion: 0.33
Nodes (5): Boot Progress Steps (code inspection), Changes Made, MyOS Boot Progress Documentation, QEMU Launch Verification, Test Result

### Community 79 - "1. Framebuffer Implementation"
Cohesion: 0.33
Nodes (6): 1. Framebuffer Implementation, Configuration (Hardcoded), Double Buffering, Files, Hardware Interface, Key Data Structures

### Community 80 - "7. Window Management (`gui/wm2.c`)"
Cohesion: 0.33
Nodes (6): 7. Window Management (`gui/wm2.c`), Animation, Flags, Hit Testing & Interaction, Operations Implemented, Window Structure

### Community 81 - "5. Input Handling"
Cohesion: 0.40
Nodes (5): 5. Input Handling, Event Types, GUI Input Pipeline (`gui/input.c`), Keyboard (`drivers/keyboard.c`), Mouse (`drivers/mouse.c`)

### Community 82 - "MyLang Toolchain"
Cohesion: 0.40
Nodes (4): Build, MyLang Toolchain, Requirements, Usage

### Community 83 - "10. Widget Toolkit (Retained-Mode UI) (`gui/widget.c`, `gui/scene.c`)"
Cohesion: 0.50
Nodes (4): 10. Widget Toolkit (Retained-Mode UI) (`gui/widget.c`, `gui/scene.c`), Layout, Scene Graph, Types

### Community 84 - "12. Animation System (`gui/anim.c`)"
Cohesion: 0.50
Nodes (4): 12. Animation System (`gui/anim.c`), Easing Functions, Integration, Pool Allocator

### Community 85 - "13. Memory & Boot"
Cohesion: 0.50
Nodes (4): 13. Memory & Boot, Heap (`kernel/heap.c`), PMM (`kernel/pmm.c`), VMM (`kernel/paging.c`)

### Community 86 - "2. Graphics Rendering Functions"
Cohesion: 0.50
Nodes (4): 2. Graphics Rendering Functions, Clipping & Transforms, Primitives Implemented, Renderer Abstraction (`gui/renderer.c`)

### Community 87 - "4. Text/Font Rendering (`gui/font.c`, `gui/font.h`)"
Cohesion: 0.50
Nodes (4): 4. Text/Font Rendering (`gui/font.c`, `gui/font.h`), Functions, Key Types, STB TrueType Integration

### Community 88 - "6. Current Compositor (`gui/compositor.c`)"
Cohesion: 0.50
Nodes (4): 6. Current Compositor (`gui/compositor.c`), Current Layer Stack (bottom → top), Damage Rect Compositing, Layer Types

### Community 89 - "MyOS - Kernel From Scratch"
Cohesion: 0.50
Nodes (3): Build, MyOS - Kernel From Scratch, Structure

### Community 90 - "QA Benchmark"
Cohesion: 0.50
Nodes (3): After rtfix, Baseline (before rtfix), QA Benchmark

### Community 91 - "MyOS Manual QA Checklist"
Cohesion: 0.50
Nodes (3): MyOS Manual QA Checklist, Perf Bar, Visual & Interaction Checks

### Community 92 - "11. Surface Management (`gui/surface.c`)"
Cohesion: 0.67
Nodes (3): 11. Surface Management (`gui/surface.c`), Damage Tracking, Surface Structure

### Community 93 - "13. Interrupt Handling (`kernel/isr.c`, `kernel/idt.c`)"
Cohesion: 0.67
Nodes (3): 13. Interrupt Handling (`kernel/isr.c`, `kernel/idt.c`), Handler Flow, IDT

### Community 94 - "4. Current Drawing Code (Blue Background + Icons)"
Cohesion: 0.67
Nodes (3): 4. Current Drawing Code (Blue Background + Icons), Hardcoded Elements (MUST BE REPLACED), Location: `gui/wm2.c` → `wm2_init()`

### Community 95 - "8. Desktop Shell (Minimal)"
Cohesion: 0.67
Nodes (3): 8. Desktop Shell (Minimal), Current State (`wm2_init()`), Start Menu (click on Start button)

## Knowledge Gaps
- **187 isolated node(s):** `build_toolchain.sh script`, `PREFIX`, `TARGET`, `PATH`, `r15` (+182 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **11 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `kprintf()` connect `kprintf` to `syscall.c`, `dhcp.c`, `settings.c`, `kmalloc`, `ext2.c`, `devfs.c`, `screen.c`, `file_explorer.c`, `browser.c`, `apic.c`, `system.h`, `keyboard.c`, `printf.h`, `mouse.c`, `xhci.c`, `strcpy`, `gpt.c`, `pthread.c`, `framebuffer.c`, `outb`, `ne2k.c`, `tss.c`, `usb_hid.c`, `shm.c`, `ac97.c`, `speaker.c`, `xhci_enum_device`, `dynlink.c`, `syscall64.c`, `tty.c`, `pic.h`?**
  _High betweenness centrality (0.057) - this node is a cross-community bridge._
- **Why does `registers` connect `registers` to `system.h`?**
  _High betweenness centrality (0.030) - this node is a cross-community bridge._
- **Why does `putchar()` connect `libc.c` to `stbtt_fontinfo`?**
  _High betweenness centrality (0.012) - this node is a cross-community bridge._
- **Are the 95 inferred relationships involving `kprintf()` (e.g. with `ac97_beep()` and `ac97_init()`) actually correct?**
  _`kprintf()` has 95 INFERRED edges - model-reasoned connections that need verification._
- **Are the 2 inferred relationships involving `term_handle_key_down()` (e.g. with `keyboard_get_modifiers()` and `keyboard_keycode_to_ascii()`) actually correct?**
  _`term_handle_key_down()` has 2 INFERRED edges - model-reasoned connections that need verification._
- **What connects `build_toolchain.sh script`, `PREFIX`, `TARGET` to the rest of the system?**
  _187 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `syscall.c` be split into smaller, more focused modules?**
  _Cohesion score 0.05352743561030235 - nodes in this community are weakly interconnected._