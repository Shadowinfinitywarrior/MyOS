# Graph Report - myos  (2026-10-06)

## Corpus Check
- cluster-only mode — file stats not available

## Summary
- 3190 nodes · 8117 edges · 206 communities (95 shown, 111 thin omitted)
- Extraction: 80% EXTRACTED · 20% INFERRED · 0% AMBIGUOUS · INFERRED: 1658 edges (avg confidence: 0.85)
- Token cost: 8,122 input · 2,366 output

## Graph Freshness
- Built from commit: `1a7ba089`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- Standard C Utilities
- Core Shell Commands
- File and Compression Tools
- Network Protocol Handling
- System Services and Audio
- Window Compositor Core
- Memory Management and Processes
- Window Animation and Physics
- Terminal Tab Management
- Font Glyph Indexing
- TrueType Font Parsing
- Hardware I/O and Procfs
- VGA Screen Output
- Python Build Scripts
- Font Rasterization Math
- Web Browser UI
- System Settings GUI
- Font Edge Calculation
- Real-Time Clock and Devfs
- FAT16 File System
- Font Baking Utilities
- Desktop Window Manager
- APIC Interrupt Handling
- Virtual File System
- Desktop Environment Shell
- File Explorer UI
- Font Table Metadata
- Input Device Management
- Hardware Timers and Audio
- GUI Application Framework
- Graphics Blitting Operations
- Font Codepoint Metrics
- xHCI USB Controller
- VirtIO Device Drivers
- Rectangle Packing Algorithm
- Texture Atlas Packing
- Sprite Sheet Layout
- Desktop System Headers
- Bitmap Allocation Utilities
- Font Table Lookup
- Storage and Audio Init
- Terminal Emulator Window
- Font Name Metadata
- Font Offset Mapping
- Font Curve Flattening
- Keyboard Driver
- Serial and Speaker Drivers
- Ext2 File System
- CPU Register State
- ALPS Touchpad Driver
- PS/2 Mouse Driver
- Synaptics Touchpad Driver
- Elantech Touchpad Driver
- ARP and Ethernet Networking
- CFF Font Buffer Parsing
- Font Data Stream Parsing
- Binary Font Buffer Utilities
- BGA Framebuffer Driver
- PCI Bus Enumeration
- Physical Memory Manager
- Ramfs and System Init
- Math Library Functions
- GUI Input Handling
- VGA Graphics Library
- Font Shape Extraction
- Browser Navigation Logic
- Unix Domain Sockets
- UI Drawing Primitives
- ATA and Partition Mounting
- GDT and IDT Initialization
- Kernel Socket Interface
- Font Kerning and Metrics
- Glyph Bounding Boxes
- Mutex and Pthreads
- ACPI Power Management
- USB HID Driver
- String and UI Utilities
- Ring Buffer Implementation
- Framebuffer Console
- NE2000 Network Driver
- Linked List Implementation
- Font Bitmap Baking
- EFI Bootloader Interface
- Terminal Console Output
- Shared Memory Management
- Font Quad Generation
- Charstring Command Execution
- Font Vertex Management
- Font Path Tracking
- Driver Registration System
- SIMD Memory Operations
- Read-Write Locks
- System Architecture Documentation
- NVMe Storage Driver
- Dynamic Linker
- Toolchain Build Scripts
- E1000 Network Driver
- GPIO Driver Interface
- Watchdog Timer Driver
- Font Kerning and Glyphs
- Path String Utilities
- Language Compiler Implementation
- QA Testing Scripts
- QA Monitoring Tools
- Disk Image Creation
- Alias Command Utility
- Audio Recording Utility
- Job Scheduling Utility
- Job Scheduling Daemon
- Pattern Processing Utility
- Text Banner Utility
- Calculator Utility
- Background Process Utility
- Multi-call Binary Utility
- Real-time Scheduling Utility
- System Clock Utility
- Text Filtering Utility
- Checksum Calculation Utility
- D-Bus Messaging Utility
- User Deletion Utility
- Kernel Module Dependencies
- File Comparison Utility
- Shell Job Management
- Kernel Log Utility
- Domain Name Utility
- Filesystem Check Utility
- Text Editor Utility
- Email Client Utility
- Disk Partitioning Utility
- File Type Utility
- Shell Interpreter Utility
- Random Quote Utility
- Memory Usage Utility
- Help Documentation Utility
- DNS Lookup Utility
- Host Identifier Utility
- Process Monitoring Utility
- Documentation Viewer Utility
- System Information Utility
- I/O Statistics Utility
- Network Performance Utility
- Wireless Interface Utility
- Line Merging Utility
- System Log Utility
- JavaScript Interpreter Utility
- Process Termination Utility
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
- Terminal Settings Utility
- User Switching Utility
- Privileged Command Execution
- System Init Utility
- Network Packet Capture
- Init Control Utility
- Network Path Utility
- Command Type Utility
- Resource Limit Utility
- Text Formatting Utility
- Archive Extraction Utility
- Database Update Utility
- System Uptime Utility
- Logged Users Utility
- User Activity Utility
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

## God Nodes (most connected - your core abstractions)
1. `puts()` - 310 edges
2. `kprintf()` - 124 edges
3. `printf()` - 100 edges
4. `stbtt_fontinfo` - 72 edges
5. `stbtt_fontinfo` - 72 edges
6. `stbtt_fontinfo` - 72 edges
7. `inb()` - 42 edges
8. `init_phase9()` - 31 edges
9. `term_handle_key_down()` - 31 edges
10. `kfree()` - 29 edges

## Surprising Connections (you probably didn't know these)
- `irq_init()` --calls--> `kprintf()`  [INFERRED]
  kernel/irq.c → lib/printf.c
- `module_init()` --calls--> `kprintf()`  [INFERRED]
  kernel/module.c → lib/printf.c
- `slab_init()` --calls--> `kprintf()`  [INFERRED]
  kernel/slab.c → lib/printf.c
- `tty_init()` --calls--> `kprintf()`  [INFERRED]
  kernel/tty.c → lib/printf.c
- `pipe_init()` --calls--> `kprintf()`  [INFERRED]
  kernel/pipe.c → lib/printf.c

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Aspirational GUI Stack** — docs_interface, gui_compositor, gui_wm2, docs_current_gui_architecture [EXTRACTED 0.95]

## Communities (206 total, 111 thin omitted)

### Community 0 - "Standard C Utilities"
Cohesion: 0.01
Nodes (77): main(), main(), main(), main(), main(), main(), main(), main() (+69 more)

### Community 1 - "Core Shell Commands"
Cohesion: 0.01
Nodes (71): main(), main(), main(), main(), main(), main(), main(), main() (+63 more)

### Community 2 - "File and Compression Tools"
Cohesion: 0.02
Nodes (64): main(), main(), main(), main(), main(), main(), main(), main() (+56 more)

### Community 3 - "Network Protocol Handling"
Cohesion: 0.05
Nodes (61): strncasecmp(), arp_init(), htonl(), htons(), ntohl(), ntohs(), dhcp_discover(), dhcp_parse_msg_type() (+53 more)

### Community 4 - "System Services and Audio"
Cohesion: 0.04
Nodes (52): main(), main(), main(), main(), main(), main(), main(), main() (+44 more)

### Community 5 - "Window Compositor Core"
Cohesion: 0.07
Nodes (52): fb_add_damage(), fb_get_backbuffer(), fb_get_stride(), compositor_add_surface(), compositor_create(), compositor_destroy(), compositor_redraw_region(), compositor_remove_surface() (+44 more)

### Community 6 - "Memory Management and Processes"
Cohesion: 0.09
Nodes (55): ahci_init(), cli(), hang(), hlt(), read_eflags(), sti(), run_user_program(), run_user_program() (+47 more)

### Community 7 - "Window Animation and Physics"
Cohesion: 0.06
Nodes (37): spring1d_init(), spring1d_set_target(), spring1d_settled(), spring1d_step(), window_anim_init(), window_anim_tick(), comp_damage_add(), comp_damage_propagate() (+29 more)

### Community 8 - "Terminal Tab Management"
Cohesion: 0.09
Nodes (60): memcpy(), memmove(), strncmp(), term_add_tab(), term_atoi(), term_calc_content_area(), term_close_tab(), term_execute_builtin() (+52 more)

### Community 9 - "Font Glyph Indexing"
Cohesion: 0.08
Nodes (59): main(), main(), stbtt_BakeFontBitmap_internal(), stbtt_FindGlyphIndex(), stbtt_fontinfo, cff, charstrings, data (+51 more)

### Community 10 - "TrueType Font Parsing"
Cohesion: 0.08
Nodes (58): main(), stbtt_BakeFontBitmap_internal(), stbtt_FindGlyphIndex(), stbtt_fontinfo, cff, charstrings, data, fdselect (+50 more)

### Community 11 - "Hardware I/O and Procfs"
Cohesion: 0.05
Nodes (7): usb_probe(), inl(), inw(), irq_init(), module_init(), slab_init(), tty_init()

### Community 12 - "VGA Screen Output"
Cohesion: 0.09
Nodes (46): entry(), screen_clear(), screen_init(), screen_putchar(), screen_scroll(), screen_set_color(), screen_set_cursor(), screen_sync_vga() (+38 more)

### Community 13 - "Python Build Scripts"
Cohesion: 0.05
Nodes (19): _blank(), install(), _patched(), _line_start(), _match_paren(), normalize_c(), _is_host_tool(), main() (+11 more)

### Community 15 - "Font Rasterization Math"
Cohesion: 0.09
Nodes (42): equal(), stbtt__active_edge, next, stbtt__add_point(), stbtt__compute_crossings_x(), stbtt__cuberoot(), stbtt__edge, invert (+34 more)

### Community 16 - "Web Browser UI"
Cohesion: 0.13
Nodes (41): fb_get_info(), mouse_get_state(), timer_get_ticks(), browser_draw(), browser_draw_bookmarks_bar(), browser_draw_downloads(), browser_draw_nav_buttons(), browser_draw_page_content() (+33 more)

### Community 17 - "System Settings GUI"
Cohesion: 0.11
Nodes (39): strncpy(), add_button(), add_dropdown(), add_section(), add_slider(), add_toggle(), build_settings_scene(), draw_button() (+31 more)

### Community 18 - "Font Edge Calculation"
Cohesion: 0.09
Nodes (42): equal(), stbtt__active_edge, next, stbtt__add_point(), stbtt__compute_crossings_x(), stbtt__cuberoot(), stbtt__edge, invert (+34 more)

### Community 19 - "Real-Time Clock and Devfs"
Cohesion: 0.07
Nodes (27): rtc_get_time(), rtc_read_reg(), devconsole_read(), devconsole_write(), devfs_finddir(), devfs_init(), devfs_readdir(), devnull_read() (+19 more)

### Community 20 - "FAT16 File System"
Cohesion: 0.09
Nodes (32): ata_read_sectors(), fat16_dir_finddir(), fat16_file_read(), fat16_format_name(), fat16_get_cluster(), fat16_mount(), fat16_read_cluster(), fat16_read_sector() (+24 more)

### Community 21 - "Font Baking Utilities"
Cohesion: 0.08
Nodes (33): emit(), equal(), my_stbtt_initfont(), my_stbtt_print(), stbtt__active_edge, next, stbtt_BakeFontBitmap(), stbtt_CompareUTF8toUTF16_bigendian() (+25 more)

### Community 22 - "Desktop Window Manager"
Cohesion: 0.10
Nodes (31): cursor_init(), desktop_boot(), desktop_shutdown(), apply_resize(), btn_x(), clamp_to_screen(), client_in_surface(), client_on_screen() (+23 more)

### Community 23 - "APIC Interrupt Handling"
Cohesion: 0.09
Nodes (31): apic_detect(), apic_eoi(), apic_get_id(), apic_init(), apic_read(), apic_send_ipi(), apic_write(), ioapic_init() (+23 more)

### Community 24 - "Virtual File System"
Cohesion: 0.09
Nodes (35): virtio_net_send(), dcache_add_internal(), dcache_init(), dcache_lookup_internal(), vfs_dcache_add(), vfs_dcache_lookup(), vfs_finddir(), vfs_init() (+27 more)

### Community 25 - "Desktop Environment Shell"
Cohesion: 0.12
Nodes (32): app_open_about(), app_open_help(), app_open_sysinfo(), blit_wallpaper(), desktop_app_at(), desktop_click(), desktop_hover(), desktop_invalidate() (+24 more)

### Community 26 - "File Explorer UI"
Cohesion: 0.12
Nodes (36): fb_draw_pixel(), draw_address_bar(), draw_char_fb(), draw_file_list(), draw_rect_fb(), draw_round_rect_fb(), draw_sidebar(), draw_status_bar() (+28 more)

### Community 27 - "Font Table Metadata"
Cohesion: 0.08
Nodes (35): stbtt__close_shape(), stbtt_fontinfo, cff, charstrings, data, fdselect, fontdicts, fontstart (+27 more)

### Community 28 - "Input Device Management"
Cohesion: 0.10
Nodes (24): alps_create(), elantech_create(), input_detect_all(), input_find_device(), input_get_system(), input_init(), input_register_device(), input_unregister_device() (+16 more)

### Community 29 - "Hardware Timers and Audio"
Cohesion: 0.06
Nodes (7): hda_init(), hpet_init(), i2c_init(), ide_init(), ps2_init(), sdmmc_init(), spi_init()

### Community 30 - "GUI Application Framework"
Cohesion: 0.17
Nodes (21): about_paint(), files_event(), files_paint(), help_paint(), kv_line(), panel(), path_join(), sysinfo_event() (+13 more)

### Community 31 - "Graphics Blitting Operations"
Cohesion: 0.18
Nodes (27): blend_pixel(), blit_blend(), blit_copy(), blit_copy_alpha(), blit_copy_offset(), blit_fill(), blit_glyph_1bpp(), blit_vertical_gradient() (+19 more)

### Community 32 - "Font Codepoint Metrics"
Cohesion: 0.17
Nodes (31): main(), main(), stbtt_BakeFontBitmap_internal(), stbtt_FindGlyphIndex(), stbtt_GetCodepointBitmapBox(), stbtt_GetCodepointBitmapBoxSubpixel(), stbtt_GetCodepointBox(), stbtt_GetCodepointHMetrics() (+23 more)

### Community 33 - "xHCI USB Controller"
Cohesion: 0.19
Nodes (27): xhci_address_device(), xhci_busy_tick(), xhci_clear_input(), xhci_cmd(), xhci_configure_ep(), xhci_ctl_xfer(), xhci_disable_slot(), xhci_drain_events() (+19 more)

### Community 34 - "VirtIO Device Drivers"
Cohesion: 0.20
Nodes (23): config_u32(), config_u64(), submit(), virtio_blk_init(), virtio_net_init(), virtio_net_irq(), virtio_net_receive(), virtio_device_ready() (+15 more)

### Community 35 - "Rectangle Packing Algorithm"
Cohesion: 0.07
Nodes (26): stbrp_init_target(), stbrp_pack_rects(), stbrp_rect, h, id, w, was_packed, x (+18 more)

### Community 36 - "Texture Atlas Packing"
Cohesion: 0.07
Nodes (26): stbrp_init_target(), stbrp_pack_rects(), stbrp_rect, h, id, w, was_packed, x (+18 more)

### Community 37 - "Sprite Sheet Layout"
Cohesion: 0.07
Nodes (26): stbrp_init_target(), stbrp_pack_rects(), stbrp_rect, h, id, w, was_packed, x (+18 more)

### Community 38 - "Desktop System Headers"
Cohesion: 0.11
Nodes (6): desktop_main(), rect_inset(), rect_make(), rect_offset(), rect_overlap(), rect_union()

### Community 39 - "Bitmap Allocation Utilities"
Cohesion: 0.11
Nodes (13): bitmap_clear(), bitmap_clear_range(), bitmap_find_contiguous(), bitmap_find_first_free(), bitmap_init(), bitmap_set(), bitmap_set_range(), bitmap_test() (+5 more)

### Community 40 - "Font Table Lookup"
Cohesion: 0.17
Nodes (23): stbtt_CompareUTF8toUTF16_bigendian(), stbtt_CompareUTF8toUTF16_bigendian_internal(), stbtt__CompareUTF8toUTF16_bigendian_prefix(), stbtt__find_table(), stbtt_FindMatchingFont(), stbtt_FindMatchingFont_internal(), stbtt__get_svg(), stbtt__GetCoverageIndex() (+15 more)

### Community 41 - "Storage and Audio Init"
Cohesion: 0.13
Nodes (19): ac97_beep(), ac97_init(), procfs_init(), gpt_init_storage(), gpt_list_partitions(), print_splash(), print_splash(), mmap_init() (+11 more)

### Community 42 - "Terminal Emulator Window"
Cohesion: 0.15
Nodes (16): launch_terminal(), term_attach(), term_event(), term_notify_dirty(), term_of(), term_open(), term_open_shell(), term_paint() (+8 more)

### Community 43 - "Font Name Metadata"
Cohesion: 0.18
Nodes (22): stbtt_CompareUTF8toUTF16_bigendian(), stbtt_CompareUTF8toUTF16_bigendian_internal(), stbtt__CompareUTF8toUTF16_bigendian_prefix(), stbtt__find_table(), stbtt_FindMatchingFont_internal(), stbtt__get_svg(), stbtt__GetCoverageIndex(), stbtt_GetFontNameString() (+14 more)

### Community 44 - "Font Offset Mapping"
Cohesion: 0.18
Nodes (22): stbtt_CompareUTF8toUTF16_bigendian_internal(), stbtt__CompareUTF8toUTF16_bigendian_prefix(), stbtt__find_table(), stbtt_FindMatchingFont_internal(), stbtt__get_svg(), stbtt__GetCoverageIndex(), stbtt_GetFontNameString(), stbtt_GetFontOffsetForIndex_internal() (+14 more)

### Community 45 - "Font Curve Flattening"
Cohesion: 0.13
Nodes (23): stbtt__add_point(), stbtt__edge, invert, x0, x1, y0, y1, stbtt_FlattenCurves() (+15 more)

### Community 46 - "Keyboard Driver"
Cohesion: 0.13
Nodes (16): kbd_device_command(), kbd_wait_ibf(), kbd_wait_obf(), keyboard_callback(), keyboard_get_event(), keyboard_getchar(), keyboard_handle_key_down(), keyboard_handle_key_up() (+8 more)

### Community 47 - "Serial and Speaker Drivers"
Cohesion: 0.13
Nodes (18): serial_init(), serial_is_transmit_empty(), serial_read(), serial_write(), speaker_beep(), speaker_init(), speaker_off(), speaker_on() (+10 more)

### Community 48 - "Ext2 File System"
Cohesion: 0.23
Nodes (17): virtio_blk_read(), virtio_blk_write(), ext2_alloc_block(), ext2_create_dir_entry(), ext2_create_file(), ext2_find_dir_entry(), ext2_find_free_block(), ext2_find_free_inode() (+9 more)

### Community 49 - "CPU Register State"
Cohesion: 0.09
Nodes (23): registers, cs, err_code, int_no, r10, r11, r12, r13 (+15 more)

### Community 50 - "ALPS Touchpad Driver"
Cohesion: 0.21
Nodes (18): alps_decode_packet_v1_v2(), alps_decode_packet_v3_v4(), alps_disable(), alps_enable(), alps_handle_irq(), alps_init(), alps_poll(), alps_query_byte() (+10 more)

### Community 51 - "PS/2 Mouse Driver"
Cohesion: 0.18
Nodes (14): fb_wait_vsync(), apply_sensitivity(), get_screen_bounds(), mouse_callback(), mouse_init(), mouse_input(), mouse_process_packet(), mouse_queue_event() (+6 more)

### Community 52 - "Synaptics Touchpad Driver"
Cohesion: 0.25
Nodes (17): abs_int(), synaptics_create(), synaptics_decode_packet(), synaptics_disable(), synaptics_enable(), synaptics_handle_irq(), synaptics_init(), synaptics_poll() (+9 more)

### Community 53 - "Elantech Touchpad Driver"
Cohesion: 0.26
Nodes (15): elantech_decode_packet(), elantech_disable(), elantech_enable(), elantech_handle_irq(), elantech_init(), elantech_poll(), elantech_query_byte(), elantech_read_data() (+7 more)

### Community 54 - "ARP and Ethernet Networking"
Cohesion: 0.15
Nodes (8): arp_handle_packet(), arp_resolve(), arp_send_request(), arp_update(), eth_send(), net_send(), web_query(), web_search()

### Community 55 - "CFF Font Buffer Parsing"
Cohesion: 0.38
Nodes (18): stbtt__buf_get(), stbtt__buf_get8(), stbtt__buf_peek8(), stbtt__buf_range(), stbtt__buf_seek(), stbtt__buf_skip(), stbtt__cff_get_index(), stbtt__cff_index_count() (+10 more)

### Community 56 - "Font Data Stream Parsing"
Cohesion: 0.38
Nodes (18): stbtt__buf_get(), stbtt__buf_get8(), stbtt__buf_peek8(), stbtt__buf_range(), stbtt__buf_seek(), stbtt__buf_skip(), stbtt__cff_get_index(), stbtt__cff_index_count() (+10 more)

### Community 57 - "Binary Font Buffer Utilities"
Cohesion: 0.38
Nodes (18): stbtt__buf_get(), stbtt__buf_get8(), stbtt__buf_peek8(), stbtt__buf_range(), stbtt__buf_seek(), stbtt__buf_skip(), stbtt__cff_get_index(), stbtt__cff_index_count() (+10 more)

### Community 58 - "BGA Framebuffer Driver"
Cohesion: 0.20
Nodes (15): bga_read(), bga_write(), fb_clear_damage(), fb_copy_to_lfb(), fb_detect(), fb_dst(), fb_fill(), fb_flush() (+7 more)

### Community 59 - "PCI Bus Enumeration"
Cohesion: 0.22
Nodes (14): pci_find_device(), pci_make_addr(), pci_read_cfg(), pci_read_cfg16(), pci_read_cfg8(), pci_write_cfg(), find_virtio_cap(), map_bar() (+6 more)

### Community 60 - "Physical Memory Manager"
Cohesion: 0.14
Nodes (16): isr_init(), kernel_main_64(), bitmap_clear(), bitmap_set(), pmm_get_free_pages(), pmm_get_total_pages(), pmm_init(), pmm_reserve_range() (+8 more)

### Community 61 - "Ramfs and System Init"
Cohesion: 0.28
Nodes (14): usb_init(), virtio_blk_get_capacity(), ramfs_add_child(), ramfs_create_dir(), ramfs_create_file(), ramfs_find(), ramfs_finddir(), ramfs_init() (+6 more)

### Community 63 - "GUI Input Handling"
Cohesion: 0.14
Nodes (9): input_event_from_mouse(), input_process_mouse_events(), input_set_cursor(), mouse_get_event(), mouse_get_position(), input_mouse_y(), input_poll(), input_pump() (+1 more)

### Community 64 - "VGA Graphics Library"
Cohesion: 0.18
Nodes (11): gfx_clear(), gfx_draw_char(), gfx_draw_circle(), gfx_draw_line(), gfx_draw_rect(), gfx_draw_string(), gfx_fill_circle(), gfx_init() (+3 more)

### Community 65 - "Font Shape Extraction"
Cohesion: 0.27
Nodes (14): stbtt__close_shape(), stbtt__csctx_close_shape(), stbtt__csctx_rccurve_to(), stbtt__csctx_rline_to(), stbtt__csctx_rmove_to(), stbtt__csctx_v(), stbtt_FreeShape(), stbtt_GetCodepointShape() (+6 more)

### Community 66 - "Browser Navigation Logic"
Cohesion: 0.27
Nodes (15): keyboard_get_modifiers(), browser_add_tab(), browser_close_tab(), browser_go_back(), browser_go_forward(), browser_handle_key_down(), browser_handle_mouse_down(), browser_init_url_bar() (+7 more)

### Community 67 - "Unix Domain Sockets"
Cohesion: 0.27
Nodes (12): process_yield(), alloc_socket(), get_socket(), sys_accept(), sys_bind(), sys_connect(), sys_listen(), sys_socket() (+4 more)

### Community 68 - "UI Drawing Primitives"
Cohesion: 0.24
Nodes (14): draw_char_scaled(), draw_line(), draw_round_rect(), draw_shadow(), draw_string_ellipsis(), draw_string_scaled(), fill_rect(), fill_round_rect() (+6 more)

### Community 69 - "ATA and Partition Mounting"
Cohesion: 0.17
Nodes (5): ext4_mount(), gpt_mount_ext4(), mount_ext4_for_partitions(), gpt_get_partition(), gpt_get_partition_count()

### Community 70 - "GDT and IDT Initialization"
Cohesion: 0.23
Nodes (8): gdt_init(), gdt_set_gate(), gdt_set_tss(), idt_init(), idt_load(), idt_set_gate(), load_tss(), tss_init()

### Community 71 - "Kernel Socket Interface"
Cohesion: 0.28
Nodes (11): alloc_socket(), get_socket(), socket_init(), sys_accept(), sys_bind(), sys_connect(), sys_listen(), sys_socket() (+3 more)

### Community 72 - "Font Kerning and Metrics"
Cohesion: 0.18
Nodes (12): stbtt_GetFontBoundingBox(), stbtt_GetFontVMetricsOS2(), stbtt__GetGlyfOffset(), stbtt_GetGlyphBox(), stbtt__GetGlyphInfoT2(), stbtt_GetKerningTable(), stbtt_IsGlyphEmpty(), stbtt_kerningentry (+4 more)

### Community 73 - "Glyph Bounding Boxes"
Cohesion: 0.18
Nodes (12): stbtt_GetFontBoundingBox(), stbtt_GetFontVMetricsOS2(), stbtt__GetGlyfOffset(), stbtt_GetGlyphBox(), stbtt__GetGlyphInfoT2(), stbtt_GetKerningTable(), stbtt_IsGlyphEmpty(), stbtt_kerningentry (+4 more)

### Community 74 - "Mutex and Pthreads"
Cohesion: 0.21
Nodes (5): mutex_init(), mutex_lock(), mutex_unlock(), pthread_create(), pthread_join()

### Community 75 - "ACPI Power Management"
Cohesion: 0.25
Nodes (7): acpi_checksum(), acpi_find_table(), acpi_get_num_cpus(), acpi_init(), acpi_reboot(), acpi_shutdown(), find_rsdp()

### Community 76 - "USB HID Driver"
Cohesion: 0.24
Nodes (7): keyboard_queue_key(), usb_hid_init(), usb_hid_kbd_report(), usb_hid_mouse_report(), usb_hid_parse_cfgdesc(), usb_hid_report(), usb_hid_set_dev()

### Community 77 - "String and UI Utilities"
Cohesion: 0.25
Nodes (8): desktop_format_clock(), desktop_init(), term_register_service(), ui_bytes(), ui_cat(), ui_cat_num(), ui_strcpy(), ui_utoa()

### Community 78 - "Ring Buffer Implementation"
Cohesion: 0.35
Nodes (8): ring_buffer_available(), ring_buffer_clear(), ring_buffer_get(), ring_buffer_init(), ring_buffer_put(), ring_buffer_read(), ring_buffer_used(), ring_buffer_write()

### Community 79 - "Framebuffer Console"
Cohesion: 0.29
Nodes (6): draw_char(), fbcon_clear(), fbcon_init(), fbcon_putchar(), fbcon_scroll(), fbcon_write()

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

### Community 87 - "Shared Memory Management"
Cohesion: 0.25
Nodes (4): shm_attach(), shm_create(), shm_destroy(), shm_init()

### Community 88 - "Font Quad Generation"
Cohesion: 0.25
Nodes (5): my_stbtt_initfont(), my_stbtt_print(), stbtt_BakeFontBitmap(), stbtt_GetBakedQuad(), stbtt_GetPackedQuad()

### Community 89 - "Charstring Command Execution"
Cohesion: 0.61
Nodes (7): stbtt__csctx_close_shape(), stbtt__csctx_rccurve_to(), stbtt__csctx_rline_to(), stbtt__csctx_rmove_to(), stbtt__csctx_v(), stbtt__run_charstring(), stbtt__track_vertex()

### Community 90 - "Font Vertex Management"
Cohesion: 0.46
Nodes (7): stbtt__close_shape(), stbtt_FreeShape(), stbtt_GetCodepointShape(), stbtt_GetGlyphShape(), stbtt__GetGlyphShapeT2(), stbtt__GetGlyphShapeTT(), stbtt_setvertex()

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

### Community 98 - "Toolchain Build Scripts"
Cohesion: 0.40
Nodes (4): PATH, PREFIX, build_toolchain.sh script, TARGET

### Community 102 - "Font Kerning and Glyphs"
Cohesion: 0.40
Nodes (5): stbtt_GetKerningTable(), stbtt_kerningentry, advance, glyph1, glyph2

### Community 103 - "Path String Utilities"
Cohesion: 0.40
Nodes (3): main(), main(), strrchr()

### Community 105 - "QA Testing Scripts"
Cohesion: 0.83
Nodes (3): check(), check_absent(), run_qa.sh script

## Knowledge Gaps
- **188 isolated node(s):** `cff`, `charstrings`, `data`, `fdselect`, `fontdicts` (+183 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 464 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **111 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `puts()` connect `Core Shell Commands` to `Standard C Utilities`, `File and Compression Tools`, `System Services and Audio`, `Terminal Tab Management`, `Browser Navigation Logic`, `Path String Utilities`, `Alias Command Utility`, `Audio Recording Utility`, `Job Scheduling Utility`, `Job Scheduling Daemon`, `Pattern Processing Utility`, `Text Banner Utility`, `Calculator Utility`, `Background Process Utility`, `Multi-call Binary Utility`, `Real-time Scheduling Utility`, `System Clock Utility`, `Text Filtering Utility`, `Checksum Calculation Utility`, `D-Bus Messaging Utility`, `User Deletion Utility`, `Kernel Module Dependencies`, `File Comparison Utility`, `Shell Job Management`, `Kernel Log Utility`, `Domain Name Utility`, `Filesystem Check Utility`, `Text Editor Utility`, `Email Client Utility`, `Disk Partitioning Utility`, `File Type Utility`, `Shell Interpreter Utility`, `Random Quote Utility`, `Memory Usage Utility`, `Help Documentation Utility`, `DNS Lookup Utility`, `Host Identifier Utility`, `Process Monitoring Utility`, `Documentation Viewer Utility`, `System Information Utility`, `I/O Statistics Utility`, `Network Performance Utility`, `Wireless Interface Utility`, `Line Merging Utility`, `System Log Utility`, `JavaScript Interpreter Utility`, `Process Termination Utility`, `Text Pager Utility`, `Kernel Module Listing`, `Lua Interpreter Utility`, `Ext2 Filesystem Creation`, `FAT Filesystem Creation`, `Kernel Module Management`, `Filesystem Mounting Utility`, `News Reader Utility`, `Process Priority Utility`, `Node.js Runtime Utility`, `Persistent Process Utility`, `Partition Management Utility`, `File Patching Utility`, `Perl Interpreter Utility`, `Memory Mapping Utility`, `Service Management Utility`, `Network Routing Utility`, `Ruby Interpreter Utility`, `System Runlevel Utility`, `CPU Scheduling Utility`, `System Info Display`, `Stream Editor Utility`, `Shell Variable Utility`, `SMB Network Utility`, `Shell Script Execution`, `File Splitting Utility`, `Secure Shell Utility`, `Binary String Extraction`, `Terminal Settings Utility`, `User Switching Utility`, `Privileged Command Execution`, `System Init Utility`, `Network Packet Capture`, `Init Control Utility`, `Network Path Utility`, `Command Type Utility`, `Resource Limit Utility`, `Text Formatting Utility`, `Archive Extraction Utility`, `Database Update Utility`, `System Uptime Utility`, `Logged Users Utility`, `User Activity Utility`, `Binary Location Utility`, `User Login Utility`, `NIS Domain Utility`, `Archive Compression Utility`?**
  _High betweenness centrality (0.093) - this node is a cross-community bridge._
- **Why does `putchar()` connect `System Services and Audio` to `Font Codepoint Metrics`, `Standard C Utilities`, `Core Shell Commands`, `Font Glyph Indexing`, `TrueType Font Parsing`?**
  _High betweenness centrality (0.074) - this node is a cross-community bridge._
- **Why does `kprintf()` connect `Storage and Audio Init` to `Network Protocol Handling`, `Memory Management and Processes`, `Hardware I/O and Procfs`, `Web Browser UI`, `System Settings GUI`, `Real-Time Clock and Devfs`, `FAT16 File System`, `Desktop Window Manager`, `APIC Interrupt Handling`, `Virtual File System`, `File Explorer UI`, `Input Device Management`, `Hardware Timers and Audio`, `xHCI USB Controller`, `VirtIO Device Drivers`, `Keyboard Driver`, `Serial and Speaker Drivers`, `ALPS Touchpad Driver`, `PS/2 Mouse Driver`, `Synaptics Touchpad Driver`, `Elantech Touchpad Driver`, `BGA Framebuffer Driver`, `PCI Bus Enumeration`, `Physical Memory Manager`, `Ramfs and System Init`, `Browser Navigation Logic`, `Unix Domain Sockets`, `ATA and Partition Mounting`, `GDT and IDT Initialization`, `Kernel Socket Interface`, `Mutex and Pthreads`, `ACPI Power Management`, `USB HID Driver`, `Framebuffer Console`, `NE2000 Network Driver`, `Terminal Console Output`, `Shared Memory Management`, `NVMe Storage Driver`, `Dynamic Linker`, `E1000 Network Driver`, `GPIO Driver Interface`, `Watchdog Timer Driver`?**
  _High betweenness centrality (0.037) - this node is a cross-community bridge._
- **Are the 306 inferred relationships involving `puts()` (e.g. with `main()` and `main()`) actually correct?**
  _`puts()` has 306 INFERRED edges - model-reasoned connections that need verification._
- **Are the 119 inferred relationships involving `kprintf()` (e.g. with `ac97_beep()` and `ac97_init()`) actually correct?**
  _`kprintf()` has 119 INFERRED edges - model-reasoned connections that need verification._
- **Are the 97 inferred relationships involving `printf()` (e.g. with `main()` and `main()`) actually correct?**
  _`printf()` has 97 INFERRED edges - model-reasoned connections that need verification._
- **What connects `cff`, `charstrings`, `data` to the rest of the system?**
  _188 weakly-connected nodes found - possible documentation gaps or missing edges._