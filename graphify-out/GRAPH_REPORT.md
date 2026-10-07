# Graph Report - myos  (2026-10-06)

## Corpus Check
- 644 files · ~316,579 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 125 file(s) not represented in the graph (top: (none) 28, .1 18, .asm 13)

## Summary
- 3931 nodes · 8889 edges · 256 communities (137 shown, 119 thin omitted)
- Extraction: 81% EXTRACTED · 19% INFERRED · 0% AMBIGUOUS · INFERRED: 1707 edges (avg confidence: 0.85)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `1a7ba089`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- printf
- puts
- libc.h
- tcp.c
- libc.c
- framebuffer.c
- process.c
- scene_graph.c
- terminal.c
- Font Glyph Indexing
- stbtt_fontinfo
- system.h
- vtty.c
- graphify_c_normalize.py
- types.h
- lib/stb_truetype.h
- browser.c
- System Settings GUI
- Font Edge Calculation
- pipe.c
- fat16.c
- mkbake/stb_truetype.h
- wm.c
- timer.h
- syscall.c
- desktop.c
- file_explorer.c
- stbtt_vertex
- drivers/input.c
- printf
- GUI Application Framework
- gui/surface.c
- stbtt_fontinfo
- xhci.c
- virtio.c
- Rectangle Packing Algorithm
- Texture Atlas Packing
- Sprite Sheet Layout
- desktop_boot.c
- Bitmap Allocation Utilities
- ttUSHORT
- kprintf
- term.c
- Font Name Metadata
- ttUSHORT
- Font Curve Flattening
- inb
- drivers/speaker.c
- 19.50 Technical Detail - Component Analysis 50
- CPU Register State
- alps.c
- mouse.c
- Synaptics Touchpad Driver
- elantech.c
- net.c
- CFF Font Buffer Parsing
- Font Data Stream Parsing
- Binary Font Buffer Utilities
- vfs.h
- API Reference Entry 120
- kmalloc
- init_phase9
- Math Library Functions
- gui/input.c
- VGA Graphics Library
- Font Shape Extraction
- strcpy
- unix_socket.c
- term_draw_tab_bar
- 19. System Call Interface
- tss.c
- kernel/socket.c
- stbtt_GetKerningTable
- Glyph Bounding Boxes
- Mutex and Pthreads
- acpi.c
- paging_map
- String and UI Utilities
- Ring Buffer Implementation
- MyOS - Complete Technical Documentation
- NE2000 Network Driver
- Linked List Implementation
- Font Bitmap Baking
- EFI Bootloader Interface
- Terminal Console Output
- process_create_user
- stbtt_BakeFontBitmap
- Charstring Command Execution
- stbtt_vertex
- Font Path Tracking
- Driver Registration System
- SIMD Memory Operations
- Read-Write Locks
- System Architecture Documentation
- printf.h
- 18. Detailed Component Analysis
- Toolchain Build Scripts
- E1000 Network Driver
- GPIO Driver Interface
- Watchdog Timer Driver
- ttSHORT
- Path String Utilities
- devfs.c
- QA Testing Scripts
- What You Must Do When Invoked
- QA Monitoring Tools
- Disk Image Creation
- Alias Command Utility
- 11. GUI Subsystem
- Job Scheduling Utility
- dhcp.c
- stdint.h
- gui_probe.py
- Calculator Utility
- Background Process Utility
- portald.c
- serial_printf
- spring.c
- Components
- graphify reference: extra exports and benchmark
- timer_get_ticks
- User Deletion Utility
- Kernel Module Dependencies
- mycomp_stub.c
- Shell Job Management
- mouse
- 10. Drivers
- sanitize_extraction.py
- Text Editor Utility
- graphify reference: query, path, explain
- web.c
- 16. Source File Index
- Shell Interpreter Utility
- Random Quote Utility
- In-Depth Analysis: Process Management Implementation
- In-Depth Analysis: Round-Robin Scheduler
- DNS Lookup Utility
- Host Identifier Utility
- In-Depth Analysis: Physical Memory Manager
- In-Depth Analysis: Virtual Memory Paging
- System Information Utility
- In-Depth Analysis: Kernel Heap Allocator
- In-Depth Analysis: Virtual Terminal
- In-Depth Analysis: Window Manager
- Line Merging Utility
- System Log Utility
- In-Depth Analysis: Desktop Environment
- In-Depth Analysis: Applications Framework
- Text Pager Utility
- Kernel Module Listing
- Lua Interpreter Utility
- Ext2 Filesystem Creation
- FAT Filesystem Creation
- Kernel Module Management
- Filesystem Mounting Utility
- News Reader Utility
- Process Priority Utility
- Node.js Runtime Utility
- Persistent Process Utility
- Partition Management Utility
- File Patching Utility
- Perl Interpreter Utility
- Memory Mapping Utility
- Service Management Utility
- Network Routing Utility
- Ruby Interpreter Utility
- System Runlevel Utility
- CPU Scheduling Utility
- System Info Display
- Stream Editor Utility
- Shell Variable Utility
- SMB Network Utility
- Shell Script Execution
- File Splitting Utility
- Secure Shell Utility
- Binary String Extraction
- hpet.c
- ide.c
- Privileged Command Execution
- ps2.c
- Network Packet Capture
- syscall64.c
- Network Path Utility
- Command Type Utility
- Resource Limit Utility
- MyLang Toolchain
- 15. Graphics & Rendering
- Database Update Utility
- 17. Build & Run Instructions
- Logged Users Utility
- 20. Complete Data Structure Reference
- Binary Location Utility
- User Login Utility
- NIS Domain Utility
- Archive Compression Utility
- Architecture Assessment Report
- GUI Stack Report
- Architectural Overview Document
- Boot Process Documentation
- System Architecture Assessment
- GUI Architecture Audit
- main
- Qmp
- graphify reference: add a URL and watch a folder
- graphify reference: commit hook and native CLAUDE.md integration
- graphify reference: incremental update and cluster-only
- Early Initialization (kernel_main_64 in kernel/kernel.c)
- QA Benchmark
- MyOS Manual QA Checklist
- graphify reference: GitHub clone and cross-repo merge
- graphify reference: transcribe video and audio
- CLAUDE.md
- extraction-spec.md
- README.md
- manual_checklist.md
- main
- batch.c
- bunzip2.c
- main
- clear.c
- cmp.c
- cron.c
- dbus-daemon.c
- dc.c
- df.c
- dnsdomainname.c
- egrep.c
- export.c
- main
- head.c
- hollywood.c
- ifconfig.c
- insmod.c
- iperf3.c
- iwconfig.c
- jobs.c
- main
- kswapd0.c
- od.c
- radio.c
- tail.c
- tar.c
- top.c
- tput.c
- tr.c
- unalias.c
- uniq.c
- update-rc.d.c
- wait.c
- webclient.c
- zsh.c

## God Nodes (most connected - your core abstractions)
1. `puts()` - 310 edges
2. `19.50 Technical Detail - Component Analysis 50` - 201 edges
3. `kprintf()` - 125 edges
4. `vfs.h` - 121 edges
5. `API Reference Entry 120` - 101 edges
6. `printf()` - 100 edges
7. `stbtt_fontinfo` - 72 edges
8. `stbtt_fontinfo` - 72 edges
9. `stbtt_fontinfo` - 72 edges
10. `MyOS - Complete Technical Documentation` - 52 edges

## Surprising Connections (you probably didn't know these)
- `BIOS Boot Chain` --references--> `kernel_main_64()`  [INFERRED]
  OS_DOCUMENTATION.md → kernel/kernel.c
- `New stubs added` --references--> `kprintf()`  [INFERRED]
  drivers/README.md → lib/printf.c
- `Core Components` --references--> `desktop_boot()`  [INFERRED]
  OS_DOCUMENTATION.md → gui/desktop_boot.c
- `Early Initialization (kernel_main_64 in kernel/kernel.c)` --references--> `sti()`  [INFERRED]
  OS_DOCUMENTATION.md → include/system.h
- `Early Initialization (kernel_main_64 in kernel/kernel.c)` --references--> `gdt_init()`  [INFERRED]
  OS_DOCUMENTATION.md → kernel/gdt.c

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Aspirational GUI Stack** — docs_interface, gui_compositor, gui_wm2, docs_current_gui_architecture [EXTRACTED 0.95]

## Communities (256 total, 119 thin omitted)

### Community 0 - "printf"
Cohesion: 0.01
Nodes (78): main(), main(), main(), main(), main(), main(), main(), main() (+70 more)

### Community 1 - "puts"
Cohesion: 0.01
Nodes (70): main(), main(), main(), main(), main(), main(), main(), main() (+62 more)

### Community 2 - "libc.h"
Cohesion: 0.02
Nodes (63): main(), main(), main(), main(), main(), main(), main(), main() (+55 more)

### Community 3 - "tcp.c"
Cohesion: 0.10
Nodes (33): strncasecmp(), htonl(), htons(), ntohl(), ntohs(), http_body_complete(), http_build_request(), http_build_response() (+25 more)

### Community 4 - "libc.c"
Cohesion: 0.04
Nodes (50): main(), main(), main(), main(), main(), main(), main(), main() (+42 more)

### Community 5 - "framebuffer.c"
Cohesion: 0.05
Nodes (66): draw_char(), fbcon_clear(), fbcon_init(), fbcon_putchar(), fbcon_scroll(), fbcon_write(), bga_read(), bga_write() (+58 more)

### Community 6 - "process.c"
Cohesion: 0.16
Nodes (27): cli(), hang(), hlt(), sti(), kernel_main_64(), process_block(), process_exit(), process_get_by_pid() (+19 more)

### Community 7 - "scene_graph.c"
Cohesion: 0.21
Nodes (11): comp_damage_add(), comp_damage_propagate(), comp_handle_msg(), comp_hit_test(), comp_merge_node_damage(), comp_node_create(), comp_node_destroy(), comp_scene_create() (+3 more)

### Community 8 - "terminal.c"
Cohesion: 0.08
Nodes (63): kfree(), close(), memcpy(), memmove(), strncmp(), term_add_tab(), term_atoi(), term_calc_content_area() (+55 more)

### Community 9 - "Font Glyph Indexing"
Cohesion: 0.08
Nodes (59): main(), main(), stbtt_BakeFontBitmap_internal(), stbtt_FindGlyphIndex(), stbtt_fontinfo, cff, charstrings, data (+51 more)

### Community 10 - "stbtt_fontinfo"
Cohesion: 0.07
Nodes (66): main(), stbtt_BakeFontBitmap_internal(), stbtt_FindGlyphIndex(), stbtt_FindMatchingFont(), stbtt_fontinfo, cff, charstrings, data (+58 more)

### Community 11 - "system.h"
Cohesion: 0.06
Nodes (5): irq_init(), module_init(), signal_init(), slab_init(), tty_init()

### Community 12 - "vtty.c"
Cohesion: 0.09
Nodes (45): entry(), screen_clear(), screen_init(), screen_putchar(), screen_scroll(), screen_set_color(), screen_set_cursor(), screen_sync_vga() (+37 more)

### Community 13 - "graphify_c_normalize.py"
Cohesion: 0.15
Nodes (6): _blank(), install(), _patched(), _line_start(), _match_paren(), normalize_c()

### Community 14 - "types.h"
Cohesion: 0.07
Nodes (7): ahci_init(), find_free_region(), mmap_init(), shm_attach(), shm_create(), shm_destroy(), shm_init()

### Community 15 - "lib/stb_truetype.h"
Cohesion: 0.11
Nodes (35): stbtt__active_edge, next, stbtt__add_point(), stbtt__cuberoot(), stbtt__edge, invert, x0, x1 (+27 more)

### Community 16 - "browser.c"
Cohesion: 0.16
Nodes (34): fb_draw_pixel(), fb_get_info(), mouse_get_state(), browser_draw(), browser_draw_bookmarks_bar(), browser_draw_downloads(), browser_draw_nav_buttons(), browser_draw_page_content() (+26 more)

### Community 17 - "System Settings GUI"
Cohesion: 0.11
Nodes (39): strncpy(), add_button(), add_dropdown(), add_section(), add_slider(), add_toggle(), build_settings_scene(), draw_button() (+31 more)

### Community 18 - "Font Edge Calculation"
Cohesion: 0.09
Nodes (42): equal(), stbtt__active_edge, next, stbtt__add_point(), stbtt__compute_crossings_x(), stbtt__cuberoot(), stbtt__edge, invert (+34 more)

### Community 19 - "pipe.c"
Cohesion: 0.12
Nodes (13): pipe_create(), pipe_init(), pipe_is_readable(), pipe_is_writable(), pipe_read(), pipe_write(), ready_read(), ready_write() (+5 more)

### Community 20 - "fat16.c"
Cohesion: 0.22
Nodes (10): ata_read_sectors(), fat16_dir_finddir(), fat16_file_read(), fat16_format_name(), fat16_get_cluster(), fat16_mount(), fat16_read_cluster(), fat16_read_sector() (+2 more)

### Community 21 - "mkbake/stb_truetype.h"
Cohesion: 0.10
Nodes (25): emit(), equal(), my_stbtt_initfont(), my_stbtt_print(), stbtt__active_edge, next, stbtt__compute_crossings_x(), stbtt__cuberoot() (+17 more)

### Community 22 - "wm.c"
Cohesion: 0.09
Nodes (34): cursor_init(), desktop_boot(), desktop_shutdown(), apply_resize(), btn_x(), clamp_to_screen(), client_in_surface(), client_on_screen() (+26 more)

### Community 23 - "timer.h"
Cohesion: 0.09
Nodes (28): ac97_beep(), ac97_init(), apic_detect(), apic_eoi(), apic_get_id(), apic_init(), apic_read(), apic_send_ipi() (+20 more)

### Community 24 - "syscall.c"
Cohesion: 0.13
Nodes (23): virtio_net_receive(), virtio_net_send(), clac(), stac(), process_dump_all(), process_free_fd(), process_get_current(), copy_from_user() (+15 more)

### Community 25 - "desktop.c"
Cohesion: 0.13
Nodes (31): fb_get_stride(), blit_wallpaper(), desktop_main(), desktop_app_at(), desktop_click(), desktop_hover(), desktop_invalidate(), desktop_invalidate_rect() (+23 more)

### Community 26 - "file_explorer.c"
Cohesion: 0.12
Nodes (35): draw_address_bar(), draw_char_fb(), draw_file_list(), draw_rect_fb(), draw_round_rect_fb(), draw_sidebar(), draw_status_bar(), draw_string_fb() (+27 more)

### Community 27 - "stbtt_vertex"
Cohesion: 0.46
Nodes (7): stbtt__close_shape(), stbtt_FreeShape(), stbtt_GetCodepointShape(), stbtt_GetGlyphShape(), stbtt__GetGlyphShapeT2(), stbtt__GetGlyphShapeTT(), stbtt_setvertex()

### Community 28 - "drivers/input.c"
Cohesion: 0.09
Nodes (26): input_detect_all(), input_event_from_mouse(), input_find_device(), input_get_system(), input_init(), input_process_mouse_events(), input_register_device(), input_set_cursor() (+18 more)

### Community 29 - "printf"
Cohesion: 0.11
Nodes (4): hda_init(), i2c_init(), sdmmc_init(), spi_init()

### Community 30 - "GUI Application Framework"
Cohesion: 0.17
Nodes (21): about_paint(), files_event(), files_paint(), help_paint(), kv_line(), panel(), path_join(), sysinfo_event() (+13 more)

### Community 31 - "gui/surface.c"
Cohesion: 0.14
Nodes (32): blend_pixel(), blit_blend(), blit_copy(), blit_copy_alpha(), blit_copy_offset(), blit_fill(), blit_glyph_1bpp(), blit_vertical_gradient() (+24 more)

### Community 32 - "stbtt_fontinfo"
Cohesion: 0.08
Nodes (59): main(), main(), stbtt_BakeFontBitmap(), stbtt_BakeFontBitmap_internal(), stbtt_FindGlyphIndex(), stbtt_fontinfo, cff, charstrings (+51 more)

### Community 33 - "xhci.c"
Cohesion: 0.10
Nodes (36): keyboard_queue_key(), usb_hid_init(), usb_hid_kbd_report(), usb_hid_mouse_report(), usb_hid_parse_cfgdesc(), usb_hid_report(), usb_hid_set_dev(), usb_init() (+28 more)

### Community 34 - "virtio.c"
Cohesion: 0.07
Nodes (56): find_bga_pci_bar0(), pci_read(), pci_find_device(), pci_make_addr(), pci_read_cfg(), pci_read_cfg16(), pci_read_cfg8(), pci_write_cfg() (+48 more)

### Community 35 - "Rectangle Packing Algorithm"
Cohesion: 0.07
Nodes (26): stbrp_init_target(), stbrp_pack_rects(), stbrp_rect, h, id, w, was_packed, x (+18 more)

### Community 36 - "Texture Atlas Packing"
Cohesion: 0.07
Nodes (26): stbrp_init_target(), stbrp_pack_rects(), stbrp_rect, h, id, w, was_packed, x (+18 more)

### Community 37 - "Sprite Sheet Layout"
Cohesion: 0.07
Nodes (26): stbrp_init_target(), stbrp_pack_rects(), stbrp_rect, h, id, w, was_packed, x (+18 more)

### Community 39 - "Bitmap Allocation Utilities"
Cohesion: 0.11
Nodes (13): bitmap_clear(), bitmap_clear_range(), bitmap_find_contiguous(), bitmap_find_first_free(), bitmap_init(), bitmap_set(), bitmap_set_range(), bitmap_test() (+5 more)

### Community 40 - "ttUSHORT"
Cohesion: 0.17
Nodes (23): stbtt_CompareUTF8toUTF16_bigendian(), stbtt_CompareUTF8toUTF16_bigendian_internal(), stbtt__CompareUTF8toUTF16_bigendian_prefix(), stbtt__find_table(), stbtt_FindMatchingFont_internal(), stbtt__get_svg(), stbtt__GetCoverageIndex(), stbtt_GetFontNameString() (+15 more)

### Community 41 - "kprintf"
Cohesion: 0.16
Nodes (18): ext4_mount(), gpt_init_storage(), gpt_mount_ext4(), mount_ext4_for_partitions(), gpt_detect(), gpt_get_partition(), gpt_get_partition_count(), gpt_list_partitions() (+10 more)

### Community 42 - "term.c"
Cohesion: 0.17
Nodes (14): term_attach(), term_event(), term_notify_dirty(), term_of(), term_open(), term_open_shell(), term_paint(), term_service() (+6 more)

### Community 43 - "Font Name Metadata"
Cohesion: 0.18
Nodes (22): stbtt_CompareUTF8toUTF16_bigendian(), stbtt_CompareUTF8toUTF16_bigendian_internal(), stbtt__CompareUTF8toUTF16_bigendian_prefix(), stbtt__find_table(), stbtt_FindMatchingFont_internal(), stbtt__get_svg(), stbtt__GetCoverageIndex(), stbtt_GetFontNameString() (+14 more)

### Community 44 - "ttUSHORT"
Cohesion: 0.17
Nodes (23): stbtt_CompareUTF8toUTF16_bigendian(), stbtt_CompareUTF8toUTF16_bigendian_internal(), stbtt__CompareUTF8toUTF16_bigendian_prefix(), stbtt__find_table(), stbtt_FindMatchingFont(), stbtt_FindMatchingFont_internal(), stbtt__get_svg(), stbtt__GetCoverageIndex() (+15 more)

### Community 45 - "Font Curve Flattening"
Cohesion: 0.13
Nodes (23): stbtt__add_point(), stbtt__edge, invert, x0, x1, y0, y1, stbtt_FlattenCurves() (+15 more)

### Community 46 - "inb"
Cohesion: 0.12
Nodes (24): kbd_device_command(), kbd_wait_ibf(), kbd_wait_obf(), keyboard_callback(), keyboard_handle_key_up(), keyboard_init(), keyboard_set_repeat_enabled(), keyboard_update_leds() (+16 more)

### Community 47 - "drivers/speaker.c"
Cohesion: 0.43
Nodes (5): speaker_beep(), speaker_init(), speaker_off(), speaker_on(), speaker_play_note()

### Community 48 - "19.50 Technical Detail - Component Analysis 50"
Cohesion: 0.01
Nodes (201): 19.50 Technical Detail - Component Analysis 50, Detail Block 1, Detail Block 10, Detail Block 100, Detail Block 101, Detail Block 102, Detail Block 103, Detail Block 104 (+193 more)

### Community 49 - "CPU Register State"
Cohesion: 0.09
Nodes (23): registers, cs, err_code, int_no, r10, r11, r12, r13 (+15 more)

### Community 50 - "alps.c"
Cohesion: 0.18
Nodes (19): alps_create(), alps_decode_packet_v1_v2(), alps_decode_packet_v3_v4(), alps_disable(), alps_enable(), alps_handle_irq(), alps_init(), alps_poll() (+11 more)

### Community 51 - "mouse.c"
Cohesion: 0.17
Nodes (8): apply_sensitivity(), get_screen_bounds(), mouse_callback(), mouse_input(), mouse_process_packet(), mouse_queue_event(), mouse_set_position(), isr_init()

### Community 52 - "Synaptics Touchpad Driver"
Cohesion: 0.25
Nodes (17): abs_int(), synaptics_create(), synaptics_decode_packet(), synaptics_disable(), synaptics_enable(), synaptics_handle_irq(), synaptics_init(), synaptics_poll() (+9 more)

### Community 53 - "elantech.c"
Cohesion: 0.26
Nodes (16): elantech_create(), elantech_decode_packet(), elantech_disable(), elantech_enable(), elantech_handle_irq(), elantech_init(), elantech_poll(), elantech_query_byte() (+8 more)

### Community 54 - "net.c"
Cohesion: 0.11
Nodes (20): arp_handle_packet(), arp_init(), arp_resolve(), arp_send_request(), arp_update(), eth_init(), eth_recv(), eth_send() (+12 more)

### Community 55 - "CFF Font Buffer Parsing"
Cohesion: 0.38
Nodes (18): stbtt__buf_get(), stbtt__buf_get8(), stbtt__buf_peek8(), stbtt__buf_range(), stbtt__buf_seek(), stbtt__buf_skip(), stbtt__cff_get_index(), stbtt__cff_index_count() (+10 more)

### Community 56 - "Font Data Stream Parsing"
Cohesion: 0.38
Nodes (18): stbtt__buf_get(), stbtt__buf_get8(), stbtt__buf_peek8(), stbtt__buf_range(), stbtt__buf_seek(), stbtt__buf_skip(), stbtt__cff_get_index(), stbtt__cff_index_count() (+10 more)

### Community 57 - "Binary Font Buffer Utilities"
Cohesion: 0.38
Nodes (18): stbtt__buf_get(), stbtt__buf_get8(), stbtt__buf_peek8(), stbtt__buf_range(), stbtt__buf_seek(), stbtt__buf_skip(), stbtt__cff_get_index(), stbtt__cff_index_count() (+10 more)

### Community 58 - "vfs.h"
Cohesion: 0.02
Nodes (120): API Reference Entry 1, API Reference Entry 10, API Reference Entry 100, API Reference Entry 101, API Reference Entry 102, API Reference Entry 103, API Reference Entry 104, API Reference Entry 105 (+112 more)

### Community 59 - "API Reference Entry 120"
Cohesion: 0.02
Nodes (101): API Reference Entry 120, Extended Technical Note 1, Extended Technical Note 10, Extended Technical Note 100, Extended Technical Note 11, Extended Technical Note 12, Extended Technical Note 13, Extended Technical Note 14 (+93 more)

### Community 60 - "kmalloc"
Cohesion: 0.20
Nodes (19): read_eflags(), heap_ensure_init(), heap_enter(), heap_exit(), heap_init(), kmalloc(), bitmap_clear(), bitmap_set() (+11 more)

### Community 61 - "init_phase9"
Cohesion: 0.27
Nodes (15): virtio_blk_get_capacity(), devfs_init(), ramfs_add_child(), ramfs_create_dir(), ramfs_create_file(), ramfs_find(), ramfs_finddir(), ramfs_init() (+7 more)

### Community 63 - "gui/input.c"
Cohesion: 0.13
Nodes (11): mouse_get_position(), cursor_draw(), cursor_set_shape(), cursor_shape(), put(), shape_for(), input_mouse_x(), input_mouse_y() (+3 more)

### Community 64 - "VGA Graphics Library"
Cohesion: 0.18
Nodes (11): gfx_clear(), gfx_draw_char(), gfx_draw_circle(), gfx_draw_line(), gfx_draw_rect(), gfx_draw_string(), gfx_fill_circle(), gfx_init() (+3 more)

### Community 65 - "Font Shape Extraction"
Cohesion: 0.27
Nodes (14): stbtt__close_shape(), stbtt__csctx_close_shape(), stbtt__csctx_rccurve_to(), stbtt__csctx_rline_to(), stbtt__csctx_rmove_to(), stbtt__csctx_v(), stbtt_FreeShape(), stbtt_GetCodepointShape() (+6 more)

### Community 66 - "strcpy"
Cohesion: 0.22
Nodes (20): keyboard_get_modifiers(), browser_add_tab(), browser_close_tab(), browser_go_back(), browser_go_forward(), browser_go_home(), browser_handle_key_down(), browser_handle_mouse_down() (+12 more)

### Community 67 - "unix_socket.c"
Cohesion: 0.25
Nodes (11): alloc_socket(), get_socket(), sys_accept(), sys_bind(), sys_connect(), sys_listen(), sys_socket(), sys_socket_close() (+3 more)

### Community 68 - "term_draw_tab_bar"
Cohesion: 0.22
Nodes (15): strcat(), draw_char_scaled(), draw_line(), draw_round_rect(), draw_shadow(), draw_string_ellipsis(), draw_string_scaled(), fill_rect() (+7 more)

### Community 69 - "19. System Call Interface"
Cohesion: 0.04
Nodes (51): 19.10 Technical Detail - Component Analysis 10, 19.11 Technical Detail - Component Analysis 11, 19.12 Technical Detail - Component Analysis 12, 19.13 Technical Detail - Component Analysis 13, 19.14 Technical Detail - Component Analysis 14, 19.15 Technical Detail - Component Analysis 15, 19.16 Technical Detail - Component Analysis 16, 19.17 Technical Detail - Component Analysis 17 (+43 more)

### Community 70 - "tss.c"
Cohesion: 0.19
Nodes (9): gdt_init(), gdt_set_gate(), gdt_set_tss(), idt_init(), idt_load(), idt_set_gate(), load_tss(), tss_init() (+1 more)

### Community 71 - "kernel/socket.c"
Cohesion: 0.25
Nodes (11): alloc_socket(), get_socket(), socket_init(), sys_accept(), sys_bind(), sys_connect(), sys_listen(), sys_socket() (+3 more)

### Community 72 - "stbtt_GetKerningTable"
Cohesion: 0.40
Nodes (5): stbtt_GetKerningTable(), stbtt_kerningentry, advance, glyph1, glyph2

### Community 73 - "Glyph Bounding Boxes"
Cohesion: 0.18
Nodes (12): stbtt_GetFontBoundingBox(), stbtt_GetFontVMetricsOS2(), stbtt__GetGlyfOffset(), stbtt_GetGlyphBox(), stbtt__GetGlyphInfoT2(), stbtt_GetKerningTable(), stbtt_IsGlyphEmpty(), stbtt_kerningentry (+4 more)

### Community 74 - "Mutex and Pthreads"
Cohesion: 0.21
Nodes (5): mutex_init(), mutex_lock(), mutex_unlock(), pthread_create(), pthread_join()

### Community 75 - "acpi.c"
Cohesion: 0.29
Nodes (6): acpi_checksum(), acpi_find_table(), acpi_get_num_cpus(), acpi_init(), acpi_reboot(), find_rsdp()

### Community 76 - "paging_map"
Cohesion: 0.16
Nodes (27): elf64_phdr_off(), elf_load(), elf_load32(), elf_load64(), elf_phdr_off(), elf_validate(), elf_validate32(), elf_validate64() (+19 more)

### Community 77 - "String and UI Utilities"
Cohesion: 0.25
Nodes (8): desktop_format_clock(), desktop_init(), term_register_service(), ui_bytes(), ui_cat(), ui_cat_num(), ui_strcpy(), ui_utoa()

### Community 78 - "Ring Buffer Implementation"
Cohesion: 0.35
Nodes (8): ring_buffer_available(), ring_buffer_clear(), ring_buffer_get(), ring_buffer_init(), ring_buffer_put(), ring_buffer_read(), ring_buffer_used(), ring_buffer_write()

### Community 79 - "MyOS - Complete Technical Documentation"
Cohesion: 0.06
Nodes (31): 12. Virtual Terminal (vtty), 13. Userspace, 14. Networking, 1. Introduction, 2. Project Overview, 3. Build System, Build Configuration, Detailed Walkthrough: drivers/framebuffer.c (+23 more)

### Community 80 - "NE2000 Network Driver"
Cohesion: 0.36
Nodes (7): ne2k_detect(), ne2k_inb(), ne2k_init(), ne2k_irq_handler(), ne2k_outb(), ne2k_recv(), ne2k_send()

### Community 81 - "Linked List Implementation"
Cohesion: 0.36
Nodes (6): list_add(), list_add_tail(), list_count(), list_empty(), list_init(), list_remove()

### Community 82 - "Font Bitmap Baking"
Cohesion: 0.22
Nodes (6): my_stbtt_initfont(), my_stbtt_print(), stbtt_BakeFontBitmap(), stbtt_GetBakedQuad(), stbtt_GetPackedQuad(), stbtt_PackFontRange()

### Community 84 - "Terminal Console Output"
Cohesion: 0.32
Nodes (4): fbterm_clear(), fbterm_init(), fbterm_putchar(), fbterm_write()

### Community 87 - "process_create_user"
Cohesion: 0.12
Nodes (27): dcache_add_internal(), dcache_init(), dcache_lookup_internal(), vfs_dcache_add(), vfs_dcache_lookup(), vfs_finddir(), vfs_init(), vfs_read() (+19 more)

### Community 88 - "stbtt_BakeFontBitmap"
Cohesion: 0.22
Nodes (6): my_stbtt_initfont(), my_stbtt_print(), stbtt_BakeFontBitmap(), stbtt_GetBakedQuad(), stbtt_GetPackedQuad(), stbtt_PackFontRange()

### Community 89 - "Charstring Command Execution"
Cohesion: 0.61
Nodes (7): stbtt__csctx_close_shape(), stbtt__csctx_rccurve_to(), stbtt__csctx_rline_to(), stbtt__csctx_rmove_to(), stbtt__csctx_v(), stbtt__run_charstring(), stbtt__track_vertex()

### Community 90 - "stbtt_vertex"
Cohesion: 0.24
Nodes (12): equal(), stbtt__close_shape(), stbtt__compute_crossings_x(), stbtt_FreeShape(), stbtt_GetCodepointSDF(), stbtt_GetCodepointShape(), stbtt_GetGlyphSDF(), stbtt_GetGlyphShape() (+4 more)

### Community 91 - "Font Path Tracking"
Cohesion: 0.61
Nodes (7): stbtt__csctx_close_shape(), stbtt__csctx_rccurve_to(), stbtt__csctx_rline_to(), stbtt__csctx_rmove_to(), stbtt__csctx_v(), stbtt__run_charstring(), stbtt__track_vertex()

### Community 92 - "Driver Registration System"
Cohesion: 0.38
Nodes (4): copy_field(), driver_get(), driver_register(), driver_seed_core()

### Community 93 - "SIMD Memory Operations"
Cohesion: 0.43
Nodes (4): _mm_loadu_si128(), _mm_set1_epi32(), _mm_storeu_si128(), _mm_stream_si128()

### Community 94 - "Read-Write Locks"
Cohesion: 0.48
Nodes (5): rwlock_init(), rwlock_rdlock(), rwlock_rdunlock(), rwlock_wrlock(), rwlock_wrunlock()

### Community 95 - "System Architecture Documentation"
Cohesion: 0.33
Nodes (6): MyOS Architecture, Transformation Roadmap, Phase 0 System Audit, GUI Stack Public Interface, Compositor, Window Manager (WM2)

### Community 96 - "printf.h"
Cohesion: 0.07
Nodes (9): nvme_init(), procfs_init(), elf_dynlink_load(), emit(), mylang_compile(), mylang_compose(), mylang_interpret(), mylang_alloc() (+1 more)

### Community 97 - "18. Detailed Component Analysis"
Cohesion: 0.07
Nodes (28): 18.10 Implementation Notes - Section 3, 18.11 Implementation Notes - Section 4, 18.12 Implementation Notes - Section 5, 18.13 Implementation Notes - Section 6, 18.14 Implementation Notes - Section 7, 18.15 Implementation Notes - Section 8, 18.16 Implementation Notes - Section 9, 18.17 Implementation Notes - Section 10 (+20 more)

### Community 98 - "Toolchain Build Scripts"
Cohesion: 0.40
Nodes (4): PATH, PREFIX, build_toolchain.sh script, TARGET

### Community 102 - "ttSHORT"
Cohesion: 0.18
Nodes (12): stbtt_GetFontBoundingBox(), stbtt_GetFontVMetricsOS2(), stbtt__GetGlyfOffset(), stbtt_GetGlyphBox(), stbtt__GetGlyphInfoT2(), stbtt_GetKerningTable(), stbtt_IsGlyphEmpty(), stbtt_kerningentry (+4 more)

### Community 103 - "Path String Utilities"
Cohesion: 0.40
Nodes (3): main(), main(), strrchr()

### Community 104 - "devfs.c"
Cohesion: 0.12
Nodes (21): keyboard_getchar(), rtc_get_time(), rtc_read_reg(), devconsole_read(), devconsole_write(), devfs_finddir(), devfs_readdir(), devnull_read() (+13 more)

### Community 105 - "QA Testing Scripts"
Cohesion: 0.83
Nodes (3): check(), check_absent(), run_qa.sh script

### Community 106 - "What You Must Do When Invoked"
Cohesion: 0.08
Nodes (24): For /graphify add and --watch, For /graphify query, For the commit hook and native CLAUDE.md integration, For --update and --cluster-only, /graphify, Honesty Rules, Interpreter guard for subcommands, Part A - Structural extraction for code files (+16 more)

### Community 110 - "11. GUI Subsystem"
Cohesion: 0.11
Nodes (19): app_open_about(), app_open_files(), app_open_help(), app_open_sysinfo(), launch_about(), launch_files(), launch_help(), launch_sysinfo() (+11 more)

### Community 112 - "dhcp.c"
Cohesion: 0.20
Nodes (12): dhcp_discover(), dhcp_parse_msg_type(), dhcp_parse_yiaddr(), dhcp_renew(), dhcp_send_request(), dhcp_set_chaddr(), dhcp_set_xid(), dns_resolve() (+4 more)

### Community 113 - "stdint.h"
Cohesion: 0.25
Nodes (4): gesture_tick(), input_pump(), inputd_init(), main()

### Community 114 - "gui_probe.py"
Cohesion: 0.18
Nodes (4): find_cursor(), goto(), _rel1(), _shoot()

### Community 117 - "portald.c"
Cohesion: 0.38
Nodes (10): main(), portald_handle_close_surface(), portald_handle_commit(), portald_handle_create_surface(), portald_handle_damage(), portald_handle_hello(), portald_init(), portald_process_message() (+2 more)

### Community 118 - "serial_printf"
Cohesion: 0.22
Nodes (9): serial_write(), isr_handler(), paging_dump_dirs(), put_num(), serial_printf(), 8. Interrupts, Exceptions & Syscalls, ISRs - kernel/isr.c (97 lines), kernel/isr64.asm (83 lines), Syscalls - kernel/syscall.c, kernel/syscall64.c (97 lines each) (+1 more)

### Community 119 - "spring.c"
Cohesion: 0.36
Nodes (6): spring1d_init(), spring1d_set_target(), spring1d_settled(), spring1d_step(), window_anim_init(), window_anim_tick()

### Community 120 - "Components"
Cohesion: 0.20
Nodes (9): 1. Compiler, 2. Composer, 3. Interpreter, 4. Runtime Requirements, Build Targets, Components, File Layout, MyLang Toolchain Requirements (+1 more)

### Community 121 - "graphify reference: extra exports and benchmark"
Cohesion: 0.22
Nodes (8): graphify reference: extra exports and benchmark, Step 6b - Wiki (only if --wiki flag), Step 7 - Neo4j export (only if --neo4j or --neo4j-push flag), Step 7a - FalkorDB export (only if --falkordb or --falkordb-push flag), Step 7b - SVG export (only if --svg flag), Step 7c - GraphML export (only if --graphml flag), Step 7d - MCP server (only if --mcp flag), Step 8 - Token reduction benchmark (only if total_words > 5000)

### Community 122 - "timer_get_ticks"
Cohesion: 0.28
Nodes (7): keyboard_get_event(), keyboard_handle_key_down(), keyboard_keycode_to_ascii(), keyboard_process_repeat(), keyboard_queue_event(), timer_get_ticks(), browser_update()

### Community 125 - "mycomp_stub.c"
Cohesion: 0.33
Nodes (6): comp_damage_add(), comp_scene_init(), main(), mydp_handle_message(), mydp_log(), mydp_server_init()

### Community 127 - "mouse"
Cohesion: 0.25
Nodes (5): Existing drivers, myos Drivers, New stubs added, mouse(), _rel()

### Community 128 - "10. Drivers"
Cohesion: 0.25
Nodes (8): 10. Drivers, Audio, Display & Graphics, Input Devices, Networking, Serial & RTC, Storage, USB

### Community 129 - "sanitize_extraction.py"
Cohesion: 0.38
Nodes (3): _is_host_tool(), main(), sanitize()

### Community 131 - "graphify reference: query, path, explain"
Cohesion: 0.33
Nodes (5): For /graphify explain, For /graphify path, graphify reference: query, path, explain, Step 0 — Constrained query expansion (REQUIRED before traversal), Step 1 — Traversal

### Community 132 - "web.c"
Cohesion: 0.47
Nodes (3): web_fetch(), web_query(), web_search()

### Community 133 - "16. Source File Index"
Cohesion: 0.33
Nodes (6): 16. Source File Index, Core Kernel (11 files), Drivers (major ones), Filesystem (6 files), GUI (11 files), Networking (9 files)

### Community 136 - "In-Depth Analysis: Process Management Implementation"
Cohesion: 0.33
Nodes (6): Concurrency Considerations, Design Overview, In-Depth Analysis: Process Management Implementation, Integration Points, Internal Data Structures, Key Algorithms

### Community 137 - "In-Depth Analysis: Round-Robin Scheduler"
Cohesion: 0.33
Nodes (6): Concurrency Considerations, Design Overview, In-Depth Analysis: Round-Robin Scheduler, Integration Points, Internal Data Structures, Key Algorithms

### Community 140 - "In-Depth Analysis: Physical Memory Manager"
Cohesion: 0.33
Nodes (6): Concurrency Considerations, Design Overview, In-Depth Analysis: Physical Memory Manager, Integration Points, Internal Data Structures, Key Algorithms

### Community 141 - "In-Depth Analysis: Virtual Memory Paging"
Cohesion: 0.33
Nodes (6): Concurrency Considerations, Design Overview, In-Depth Analysis: Virtual Memory Paging, Integration Points, Internal Data Structures, Key Algorithms

### Community 143 - "In-Depth Analysis: Kernel Heap Allocator"
Cohesion: 0.33
Nodes (6): Concurrency Considerations, Design Overview, In-Depth Analysis: Kernel Heap Allocator, Integration Points, Internal Data Structures, Key Algorithms

### Community 144 - "In-Depth Analysis: Virtual Terminal"
Cohesion: 0.33
Nodes (6): Concurrency Considerations, Design Overview, In-Depth Analysis: Virtual Terminal, Integration Points, Internal Data Structures, Key Algorithms

### Community 145 - "In-Depth Analysis: Window Manager"
Cohesion: 0.33
Nodes (6): Concurrency Considerations, Design Overview, In-Depth Analysis: Window Manager, Integration Points, Internal Data Structures, Key Algorithms

### Community 148 - "In-Depth Analysis: Desktop Environment"
Cohesion: 0.33
Nodes (6): Concurrency Considerations, Design Overview, In-Depth Analysis: Desktop Environment, Integration Points, Internal Data Structures, Key Algorithms

### Community 149 - "In-Depth Analysis: Applications Framework"
Cohesion: 0.33
Nodes (6): Concurrency Considerations, Design Overview, In-Depth Analysis: Applications Framework, Integration Points, Internal Data Structures, Key Algorithms

### Community 183 - "syscall64.c"
Cohesion: 0.60
Nodes (4): rdmsr(), syscall_init_64(), syscall_set_kernel_stack(), wrmsr()

### Community 187 - "MyLang Toolchain"
Cohesion: 0.40
Nodes (4): Build, MyLang Toolchain, Requirements, Usage

### Community 188 - "15. Graphics & Rendering"
Cohesion: 0.40
Nodes (5): 15. Graphics & Rendering, Blitting (gui/blit.c), Font System, Framebuffer, Surface System (gui/surface.c)

### Community 190 - "17. Build & Run Instructions"
Cohesion: 0.40
Nodes (5): 17. Build & Run Instructions, Building, Prerequisites, Running in QEMU, Testing

### Community 192 - "20. Complete Data Structure Reference"
Cohesion: 0.40
Nodes (5): 20. Complete Data Structure Reference, desktop.h, process.h, vtty.h, wm.h

### Community 206 - "main"
Cohesion: 0.40
Nodes (3): analyze_ppm(), main(), type_keys()

### Community 208 - "graphify reference: add a URL and watch a folder"
Cohesion: 0.50
Nodes (3): For /graphify add, For --watch, graphify reference: add a URL and watch a folder

### Community 209 - "graphify reference: commit hook and native CLAUDE.md integration"
Cohesion: 0.50
Nodes (3): For git commit hook, For native CLAUDE.md integration, graphify reference: commit hook and native CLAUDE.md integration

### Community 210 - "graphify reference: incremental update and cluster-only"
Cohesion: 0.50
Nodes (3): For --cluster-only, For --update (incremental re-extraction), graphify reference: incremental update and cluster-only

### Community 211 - "Early Initialization (kernel_main_64 in kernel/kernel.c)"
Cohesion: 0.50
Nodes (4): 4. Boot Process, BIOS Boot Chain, Early Initialization (kernel_main_64 in kernel/kernel.c), UEFI Boot

### Community 212 - "QA Benchmark"
Cohesion: 0.50
Nodes (3): After rtfix, Baseline (before rtfix), QA Benchmark

### Community 213 - "MyOS Manual QA Checklist"
Cohesion: 0.50
Nodes (3): MyOS Manual QA Checklist, Perf Bar, Visual & Interaction Checks

## Knowledge Gaps
- **852 isolated node(s):** `build_toolchain.sh script`, `PREFIX`, `TARGET`, `PATH`, `r15` (+847 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 1145 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **119 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `MyOS - Complete Technical Documentation` connect `MyOS - Complete Technical Documentation` to `10. Drivers`, `16. Source File Index`, `process.c`, `In-Depth Analysis: Process Management Implementation`, `In-Depth Analysis: Round-Robin Scheduler`, `In-Depth Analysis: Physical Memory Manager`, `In-Depth Analysis: Virtual Memory Paging`, `In-Depth Analysis: Kernel Heap Allocator`, `In-Depth Analysis: Virtual Terminal`, `In-Depth Analysis: Window Manager`, `In-Depth Analysis: Desktop Environment`, `In-Depth Analysis: Applications Framework`, `wm.c`, `kmalloc`, `15. Graphics & Rendering`, `17. Build & Run Instructions`, `20. Complete Data Structure Reference`, `19. System Call Interface`, `Early Initialization (kernel_main_64 in kernel/kernel.c)`, `process_create_user`, `18. Detailed Component Analysis`, `11. GUI Subsystem`, `serial_printf`?**
  _High betweenness centrality (0.255) - this node is a cross-community bridge._
- **Why does `19. System Call Interface` connect `19. System Call Interface` to `19.50 Technical Detail - Component Analysis 50`, `MyOS - Complete Technical Documentation`?**
  _High betweenness centrality (0.123) - this node is a cross-community bridge._
- **Why does `19.50 Technical Detail - Component Analysis 50` connect `19.50 Technical Detail - Component Analysis 50` to `19. System Call Interface`?**
  _High betweenness centrality (0.115) - this node is a cross-community bridge._
- **Are the 306 inferred relationships involving `puts()` (e.g. with `main()` and `main()`) actually correct?**
  _`puts()` has 306 INFERRED edges - model-reasoned connections that need verification._
- **Are the 120 inferred relationships involving `kprintf()` (e.g. with `ac97_beep()` and `ac97_init()`) actually correct?**
  _`kprintf()` has 120 INFERRED edges - model-reasoned connections that need verification._
- **What connects `build_toolchain.sh script`, `PREFIX`, `TARGET` to the rest of the system?**
  _852 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `printf` be split into smaller, more focused modules?**
  _Cohesion score 0.013156777862660216 - nodes in this community are weakly interconnected._