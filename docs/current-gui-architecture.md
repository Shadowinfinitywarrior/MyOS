# Current MYOS GUI Architecture Audit

## Overview
This document describes the current state of the MYOS graphical subsystem as of the audit. The system is a functional but primitive framebuffer-based desktop with basic window management, software rendering, and PS/2 input handling.

---

## 1. Framebuffer Implementation

### Files
- `drivers/framebuffer.c` / `drivers/framebuffer.h`
- `gui/renderer.c` / `gui/renderer.h`
- `gui/blit.c` / `gui/blit.h`

### Hardware Interface
- **BGA (Bochs Graphics Adapter)** via I/O ports `0x01CE`/`0x01CF`
- **PCI BAR0** scanning for QEMU VGA devices:
  - Standard: `0x1234:0x1111`
  - VirtIO: `0x1AF4:0x1050`
- **Fallback physical address**: `0xFD000000`

### Configuration (Hardcoded)
- **Resolution**: 1024×768
- **Pixel Format**: 32-bit ARGB (4 bytes/pixel)
- **Pitch**: width × 4
- **Virtual Mapping**: `0xE0000000` when paging active
- **Max Backbuffer**: 1920×1080

### Double Buffering
- **Backbuffer**: Static `uint32_t backbuffer[1920*1080]` in `.bss`
- **Front Buffer**: Memory-mapped BGA LFB at `0xFD000000` (phys) / `0xE0000000` (virt)
- **Flush**: `fb_flush_rects()` copies dirty rects from backbuffer → LFB using `blit_copy()`
- **VSync**: Optional via VGA port `0x3DA` polling (`fb_wait_vsync()`)

### Key Data Structures
```c
typedef struct fb_info {
    uint32_t phys_addr, virt_addr;
    uint32_t width, height, pitch, bpp, depth;
    uint32_t refresh_rate, frame_count, last_vsync_time;
    bool vsync_enabled;
} fb_info_t;
```

---

## 2. Graphics Rendering Functions

### Renderer Abstraction (`gui/renderer.c`)
- **Backend Pattern**: `renderer_t` with `renderer_backend_t` function table
- **Backends**: Framebuffer (implemented), GPU (stub - returns NULL)
- **Context**: `fb_renderer_ctx_t` holds `fb_info_t*` and backbuffer pointer

### Primitives Implemented
| Function | Description |
|----------|-------------|
| `clear(color)` | Full backbuffer fill |
| `fill_rect(rect, color)` | Solid rectangle |
| `draw_rect(rect, color, thickness)` | Rectangle outline |
| `draw_line(x1,y1,x2,y2,color,thickness)` | Bresenham line |
| `fill_rounded_rect(rect, radius, color)` | Rounded fill |
| `draw_rounded_rect(rect, radius, color, thickness)` | Rounded outline |
| `fill_circle(cx,cy,radius,color)` | Circle fill |
| `draw_circle(cx,cy,radius,color,thickness)` | Circle outline |
| `fill_triangle(x1,y1,x2,y2,x3,y3,color)` | Barycentric fill |
| `blit(surface, src_rect, dst_x, dst_y)` | Alpha-blended blit |
| `blit_scaled()` | Scaled blit |
| `blit_tinted()` | Color-tinted blit |
| `fill_gradient_v/h()` | Vertical/horizontal gradients |
| `draw_shadow()` | Multi-radius drop shadow |

### Clipping & Transforms
- Clip rect per-renderer (`set_clip`/`clear_clip`)
- 2D transform matrix (6 floats: scale, rotate, translate)
- Blend modes: NONE, ALPHA, ADD, MULTIPLY

---

## 3. Low-Level Blitting (`gui/blit.c`)

### SSE2 Optimized
- `blit_copy()` - 128-bit aligned streaming stores
- `blit_blend()` - Fast-path for opaque runs, per-pixel alpha blend
- `blit_fill()` - Streaming stores for solid fill
- `blit_clip_rect()` - Bounds clipping

---

## 4. Text/Font Rendering (`gui/font.c`, `gui/font.h`)

### STB TrueType Integration
- **Atlas**: 1024×1024 → dynamic up to 4096×4096
- **Glyph Cache**: Per-face LRU linked list
- **Fallback**: Up to 4 fallback fonts chained
- **UTF-8**: Full decode/encode support

### Key Types
```c
typedef struct {
    int x, y, w, h;
    int advance_x, advance_y;
    int bearing_x, bearing_y;
} font_glyph_t;

typedef struct font_face {
    int size_px;
    stbtt_fontinfo info;
    font_glyph_t *glyphs;
    struct font_face *fallback;
} font_face_t;

typedef struct {
    font_face_t *face;
    font_face_t *fallbacks[4];
} font_t;
```

### Functions
- `font_load(path, size_px)` → `font_t*`
- `font_draw_text(surface, font, x, y, utf8, color, size_px)`
- `font_measure(font, text, &w, &h)`

---

## 4. Current Drawing Code (Blue Background + Icons)

### Location: `gui/wm2.c` → `wm2_init()`

```c
// Wallpaper: full-screen vertical gradient
g_wallpaper = surface_alloc(fb->width, fb->height, SURFACE_TYPE_WALLPAPER);
for (y = 0; y < h; y++) {
    t = (y * 4096) / (h - 1);
    r = lerp(top_r, bot_r, t);
    g = lerp(top_g, bot_g, t);
    b = lerp(top_b, bot_b, t);
    row[x] = 0xFF000000 | (r<<16)|(g<<8)|b;
}
// Vertical stripes every 4 pixels
for (x = 0; x < w; x += 4) for (y = 0; y < h; y++) {
    p = &pixels[y*w + x];
    *p = color + 0x040404;
}

// Desktop Icons (HARDCODED - 4 fixed rectangles)
const uint32_t icon_colors[] = { 0xFFE8A33D, 0xFF3D4148, 0xFF5A8DD6, 0xFFB05555 };
for (i = 0; i < 4; i++) {
    ix = 32 + i * (48 + 16);
    iy = 48;
    ui_fill_rounded_rect(g_wallpaper, ix, iy, 48, 48, 6, icon_colors[i]);
    // Highlight bar
    surface_fill_rect(g_wallpaper, &(rect_t){ix+2, iy+2, 44, 8}, 0x55FFFFFF);
}

// Taskbar
g_taskbar = surface_alloc(fb->width, 48, SURFACE_TYPE_PANEL);
ui_fill_rounded_rect(g_taskbar, 0, 0, w, 48, 0, panel_color);
// Start button at (6,6) size 72×36
```

### Hardcoded Elements (MUST BE REPLACED)
1. Four desktop icons at fixed positions with fixed colors
2. No icon images - just colored rounded rectangles
3. No icon actions/click handlers
4. Taskbar "Start" button draws text but click handler spawns demo launcher
5. Clock reads RTC but no date

---

## 5. Input Handling

### Keyboard (`drivers/keyboard.c`)
- **PS/2**: IRQ1 via `isr_register_handler(33, keyboard_callback)`
- **Scancode Set 1** with E0/E1 prefix handling
- **Key Repeat**: Configurable delay/rate
- **Modifiers**: Shift, Ctrl, Alt, Meta, CapsLock, NumLock, ScrollLock
- **Event Queue**: 256-entry circular buffer (`key_event_t`)

### Mouse (`drivers/mouse.c`)
- **PS/2**: IRQ12 via `isr_register_handler(44, mouse_callback)`
- **IntelliMouse**: Detects wheel via sample rate sequence (200,100,80)
- **5-button**: Partial support
- **Sensitivity/Acceleration**: Configurable (1-10, quadratic accel)
- **Event Queue**: 128-entry circular buffer (`mouse_event_t`)

### GUI Input Pipeline (`gui/input.c`)
```c
input_pump() {
    while (keyboard_get_event(&kev)) → push to ring (type 1/2/3)
    while (mouse_get_event(&mev))   → push to ring (type 4/5/6/7/8)
}
```

### Event Types
| Type | Meaning |
|------|---------|
| 1 | Key down |
| 2 | Key up |
| 3 | Key repeat |
| 4 | Mouse move (code=x, value=y) |
| 5 | Mouse button down (code=button) |
| 6 | Mouse button up (code=button) |
| 7 | Scroll vertical (value=delta) |
| 8 | Scroll horizontal (value=delta) |

---

## 6. Current Compositor (`gui/compositor.c`)

### Layer Types
```c
typedef enum {
    LAYER_TYPE_WALLPAPER = 0,
    LAYER_TYPE_WINDOW = 1,
    LAYER_TYPE_PANEL = 2,
    LAYER_TYPE_CURSOR = 3,
    LAYER_TYPE_POPUP = 4,
} layer_type_t;
```

### Damage Rect Compositing
- Per-surface damage rects (`surface_t::damage[128]`)
- Per-frame damage accumulation in compositor (`frame_damage[64]`)
- `compositor_frame()` iterates layers back-to-front, blits only damaged regions
- Cursor always on top (Z = INT_MAX)

### Current Layer Stack (bottom → top)
1. Wallpaper surface (full screen, gradient + icons drawn onto it)
2. Window surfaces (z-order managed)
3. Taskbar panel (fixed Y = height - 48)
4. Cursor (12×20, hardcoded arrow bitmap)

---

## 7. Window Management (`gui/wm2.c`)

### Window Structure
```c
typedef struct wm2_window {
    uint32_t id;
    surface_t *surface;
    widget_t *scene_root;
    char title[64];
    int flags;           // VISIBLE, FOCUSED, MINIMIZED, MAXIMIZED, CLOSING
    int x, y, w, h;
    int min_w, min_h, max_w, max_h;
    int prev_x, prev_y, prev_w, prev_h;  // for restore
    int z_order;
    int desktop;         // 0-3 virtual desktops
    int anim_progress;
    input_event_t events[32];
    int event_head, event_tail;
} wm2_window_t;
```

### Flags
- `WM2_FLAG_VISIBLE` (0x01)
- `WM2_FLAG_FOCUSED` (0x02)
- `WM2_FLAG_MINIMIZED` (0x04)
- `WM2_FLAG_MAXIMIZED` (0x08)
- `WM2_FLAG_CLOSING` (0x80)

### Operations Implemented
- `wm2_create_window()` - allocates surface, draws chrome, adds to compositor
- `wm2_destroy_window()` - removes from compositor, frees surface/scene
- `wm2_close_window()` - starts close animation
- `wm2_minimize()` / `wm2_restore()` - hide/show with animation
- `wm2_maximize()` / `wm2_restore_maximize()` - saves/restores geometry
- `wm2_switch_desktop()` - moves surfaces between compositor

### Hit Testing & Interaction
```c
wm2_hit_test(x, y) → topmost window at coordinates
```
- Title bar (y < 30): drag move
- Close button (top-right): close
- Minimize button: minimize
- Maximize button: toggle maximize
- Borders (8px): resize from edges/corners

### Animation
- `anim_start_ex()` with easing (EASE_OUT_CUBIC, 200ms)
- Window open: 0→1000 progress
- Window close: progress→0

---

## 8. Desktop Shell (Minimal)

### Current State (`wm2_init()`)
- **Wallpaper**: Gradient surface at Z=0
- **Icons**: 4 hardcoded colored squares drawn onto wallpaper (no interaction)
- **Taskbar**: Single panel at bottom with:
  - Start button (6,6 → 78,42) - click opens launcher window
  - Clock (RTC time, updates every 60 frames)
  - Placeholder "Welcome" text
- **Cursor**: 12×20 arrow bitmap, always top Z

### Start Menu (click on Start button)
- Creates "Start Menu" window at (200,200) 320×360
- Lists app registry entries (hardcoded 5 apps)
- Click anywhere launches "Files" (hardcoded)

---

## 9. Application Registry (`gui/app_registry.c`)

### Static Entries
```c
static const app_entry_t apps[] = {
    { "Files", "File Manager", "files", "/system/apps/files" },
    { "Terminal", "Terminal Emulator", "terminal", "/system/apps/terminal" },
    { "Settings", "System Settings", "settings", "/system/apps/settings" },
    { "Task Manager", "Process Manager", "taskmanager", "/system/apps/taskmanager" },
    { "System Information", "System Info", "sysinfo", "/system/apps/sysinfo" },
};
```
**Currently**: All launchers just create stub windows with static text.

---

## 10. Widget Toolkit (Retained-Mode UI) (`gui/widget.c`, `gui/scene.c`)

### Scene Graph
```c
typedef struct widget {
    widget_type_t type;
    rect_t rect;
    struct widget *parent;
    struct widget *first_child, *last_child;
    struct widget *next_sibling, *prev_sibling;
} widget_t;
```

### Types
- `WIDGET_BOX`, `WIDGET_BUTTON`, `WIDGET_LABEL`
- `WIDGET_TEXT`, `WIDGET_SLIDER`, `WIDGET_SCROLL`
- `WIDGET_IMAGE`, `WIDGET_CHECKBOX`

### Layout
- `layout_run()` - simple recursive pass (children inherit parent rect)
- No constraint-based layout yet

---

## 11. Surface Management (`gui/surface.c`)

### Surface Structure
```c
typedef struct surface {
    uint32_t *pixels;
    int w, h, pitch;
    int x, y;
    surface_type_t type;
    int z_order;
    bool visible;
    rect_t damage[128];
    int damage_count;
    int full_damage;
    // hierarchy
    surface_t *parent, *children, *next_sibling, *prev_sibling;
} surface_t;
```

### Damage Tracking
- `surface_damage(s, rect)` - clips to surface bounds, merges if >128
- `surface_clear_damage()` - resets for next frame

---

## 12. Animation System (`gui/anim.c`)

### Pool Allocator
- Fixed pool of 64 `anim_node` structs
- `anim_start_ex(target_ptr, from, to, duration_ms, easing, on_done, invalidate_cb, ctx)`

### Easing Functions
- `EASING_LINEAR`
- `EASING_EASE_OUT_CUBIC`
- `EASING_EASE_IN_OUT_QUAD`
- `EASING_SPRING`

### Integration
- `anim_tick()` called each frame from `wm2_run()`
- Progress calculated via `timer_get_ms()`
- Calls invalidate callback on value change

---

## 13. Interrupt Handling (`kernel/isr.c`, `kernel/idt.c`)

### IDT
- 256 gates (64-bit)
- IRQs remapped: PIC1 → 0x20-0x27, PIC2 → 0x28-0x2F
- IRQ1 (keyboard) → vector 33
- IRQ12 (mouse) → vector 44

### Handler Flow
```c
isr_handler(regs) {
    if (vector >= 32 && vector <= 47) pic_send_eoi(vector - 32);
    if (handlers[vector]) handlers[vector](regs);
    else if (vector < 32) panic();
}
```

---

## 13. Memory & Boot

### PMM (`kernel/pmm.c`)
- E820 memory map from bootloader
- Bitmap allocator (1 bit per 4KB page)
- Fallback region after `__kernel_end`

### VMM (`kernel/paging.c`)
- 4-level page tables (PML4→PDPT→PD→PT)
- Higher-half kernel at `0xFFFFFFFF80000000`
- Identity map for first 1GB + framebuffer at `0xE0000000`

### Heap (`kernel/heap.c`)
- Fixed base: `0x4000000` (64MB)
- Simple bump allocator with free list

---

## 14. Current Limitations

| Area | Limitation |
|------|------------|
| **Desktop Icons** | Hardcoded 4 colored rectangles, no images, no click actions |
| **Window Chrome** | Drawn directly on window surface at create time (no dynamic redraw on theme change) |
| **Theme** | No theme system - colors hardcoded throughout |
| **Font** | Single global font face, no per-widget font selection |
| **Layout** | No constraint solver - absolute positioning only |
| **Input** | No text input composition, no IME |
| **Cursor** | Single hardcoded arrow, no cursor shapes (ibeam, resize, etc.) |
| **GPU** | No acceleration path, `renderer_create_gpu()` returns NULL |
| **VSync** | Software polling, no hardware page flip |
| **Compositor** | CPU-only blitting, no hardware overlays |
| **Windows** | No modal dialogs, no window snapping, no workspace overview |
| **Shell** | No application launcher, no file associations, no notifications |

---

## 15. Files by Responsibility

| Component | Files |
|-----------|-------|
| Framebuffer | `drivers/framebuffer.c/h` |
| Renderer | `gui/renderer.c/h`, `gui/blit.c/h` |
| Font | `gui/font.c/h` |
| Compositor | `gui/compositor.c/h` |
| Window Manager | `gui/wm2.c/h` |
| Surfaces | `gui/surface.c/h` |
| Widgets | `gui/widget.c/h`, `gui/scene.c/h` |
| Animation | `gui/anim.c/h` |
| Input | `drivers/keyboard.c/h`, `drivers/mouse.c/h`, `gui/input.c/h` |
| Desktop/Apps | `gui/wm2.c` (launchers), `gui/app_registry.c/h` |
| Taskbar/Clock | `gui/wm2.c` |
| Interrupts | `kernel/isr.c/h`, `kernel/idt.c/h`, `kernel/pic.c/h` |
| Memory | `kernel/pmm.c`, `kernel/paging.c`, `kernel/heap.c` |

---

## 16. Resolution & Pixel Format Summary

| Property | Value |
|----------|-------|
| Width | 1024 |
| Height | 768 |
| BPP | 32 |
| Format | ARGB (0xAARRGGBB) |
| Pitch | 4096 bytes |
| Backbuffer | 1920×1080 max (static) |
| LFB Phys | 0xFD000000 |
| LFB Virt | 0xE0000000 |

---

## 17. Build & Test Commands

```bash
make clean && make          # Build kernel + disk image
make run                    # QEMU with GTK display
make run-headless           # QEMU serial only (for CI)
make qa                     # Run QA tests
```