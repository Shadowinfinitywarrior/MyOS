#![no_std]
#![no_main]

use core::panic::PanicInfo;

mod renderer;
mod window;
mod input;
mod font;
mod desktop;

// Re-export public API
pub use renderer::{
    alloc_zeroed, free_mem,
    clear, draw_circle, draw_line, draw_line_thick, draw_rect, draw_rounded_rect,
    fill_circle, fill_ellipse, fill_gradient_h, fill_gradient_v, fill_rect, fill_rounded_rect,
    get_framebuffer, get_framebuffer_pitch, get_framebuffer_size, init_renderer,
    set_pixel, blit_surface, blit_surface_alpha,
    draw_ellipse, draw_text,
};
pub use window::{
    compose_frame, create_window, destroy_window, find_window, get_focused,
    get_window, hit_test, hit_test_resize, init_window_manager, invalidate_window,
    set_focus, set_window_title, get_window_rect, set_window_rect, window_count,
    ResizeEdge,
    BORDER_WIDTH, TITLEBAR_HEIGHT, MIN_WINDOW_WIDTH, MIN_WINDOW_HEIGHT,
};
pub use input::{get_event, has_events, init_input, process_events, get_mouse_position};
pub use font::{Font, GlyphMetrics, font_ui, font_mono, font_bold, font_blocks, rust_gui_init_fonts,
               rust_gui_draw_text, rust_gui_draw_text_bg, rust_gui_text_width, rust_gui_draw_text_font,
               rust_gui_draw_text_clipped};
pub use desktop::{rust_gui_init_desktop, rust_gui_compose_desktop, rust_gui_desktop_mouse_move, rust_gui_desktop_mouse_click, rust_gui_desktop_key};

/// Panic handler for no_std environment
#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}

/// Basic types for the GUI system
#[repr(transparent)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Color(pub u32);  // ARGB format

pub type WindowId = u64;

/// Rectangle structure (matches C rect_t)
#[repr(C)]
#[derive(Debug, Clone, Copy, Default)]
pub struct Rect {
    pub x: i32,
    pub y: i32,
    pub w: i32,
    pub h: i32,
}

impl Rect {
    pub const fn new(x: i32, y: i32, w: i32, h: i32) -> Self {
        Rect { x, y, w, h }
    }

    pub fn is_empty(&self) -> bool {
        self.w <= 0 || self.h <= 0
    }
}

/// Surface structure (matches C surface_t from gui/surface.h)
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct Surface {
    pub pixels: *mut u32,
    pub w: i32,
    pub h: i32,
    pub pitch: i32,
}

/// Window structure
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct Window {
    pub id: WindowId,
    pub title: [u8; 64],
    pub frame: Rect,
    pub client: Rect,
    pub flags: u32,
    pub visible: bool,
    pub focused: bool,
}

/// Window flags
pub const WF_MINIMIZED: u32 = 1 << 0;
pub const WF_MAXIMIZED: u32 = 1 << 1;
pub const WF_RESIZING: u32 = 1 << 2;
pub const WF_DRAGGING: u32 = 1 << 3;
pub const WF_NO_DECOR: u32 = 1 << 4;
pub const WF_MODAL: u32 = 1 << 5;

// FFI exports for C

/// Initialize the Rust GUI system
#[no_mangle]
pub extern "C" fn rust_gui_init(width: u32, height: u32) -> i32 {
    // Initialize with dummy framebuffer for now (will be set by C later)
    init_renderer(core::ptr::null_mut(), width, height, width * 4);
    init_window_manager();
    init_input();
    0  // Success
}

/// Create a new window
#[no_mangle]
pub extern "C" fn rust_gui_create_window(
    title: *const u8,
    x: i32,
    y: i32,
    w: i32,
    h: i32,
) -> WindowId {
    // Convert C string to Rust string
    let title_str = if !title.is_null() {
        unsafe {
            let mut len = 0;
            while *title.add(len) != 0 && len < 63 {
                len += 1;
            }
            let slice = core::slice::from_raw_parts(title, len);
            core::str::from_utf8_unchecked(slice)
        }
    } else {
        "Untitled"
    };

    match create_window(title_str, x, y, w, h) {
        Some(id) => id,
        None => 0,
    }
}

/// Destroy a window
#[no_mangle]
pub extern "C" fn rust_gui_destroy_window(id: WindowId) {
    destroy_window(id);
}

/// Invalidate a window (mark for redraw)
/// x, y, w, h specify the damaged region (currently unused, full window invalidated)
#[no_mangle]
pub extern "C" fn rust_gui_invalidate_window(id: WindowId, _x: i32, _y: i32, _w: i32, _h: i32) {
    invalidate_window(id);
}

/// Render a frame
#[no_mangle]
pub extern "C" fn rust_gui_render_frame() -> i32 {
    process_events();
    compose_frame();
    0  // Success
}

/// Set the framebuffer pointer (called from C)
#[no_mangle]
pub extern "C" fn rust_gui_set_framebuffer(fb: *mut u32, width: u32, height: u32, pitch: u32) {
    init_renderer(fb, width, height, pitch);
}

/// Get the window count
#[no_mangle]
pub extern "C" fn rust_gui_window_count() -> i32 {
    window_count() as i32
}

/// Get a window by index
#[no_mangle]
pub extern "C" fn rust_gui_get_window(index: i32) -> *mut Window {
    if index < 0 {
        return core::ptr::null_mut();
    }
    match get_window(index as usize) {
        Some(w) => w as *const Window as *mut Window,
        None => core::ptr::null_mut(),
    }
}

/// Focus a window
#[no_mangle]
pub extern "C" fn rust_gui_focus_window(id: WindowId) -> i32 {
    if set_focus(id) { 1 } else { 0 }
}

/// Set window title
#[no_mangle]
pub extern "C" fn rust_gui_set_window_title(id: WindowId, title: *const u8) -> i32 {
    let title_str = if !title.is_null() {
        unsafe {
            let mut len = 0;
            while *title.add(len) != 0 && len < 63 {
                len += 1;
            }
            let slice = core::slice::from_raw_parts(title, len);
            core::str::from_utf8_unchecked(slice)
        }
    } else {
        ""
    };

    if set_window_title(id, title_str) { 1 } else { 0 }
}

/// Get window frame rectangle
#[no_mangle]
pub extern "C" fn rust_gui_get_window_rect(id: WindowId, rect: *mut Rect) -> i32 {
    if rect.is_null() {
        return -1;
    }
    unsafe {
        let mut r = Rect::default();
        if get_window_rect(id, &mut r) {
            *rect = r;
            1
        } else {
            0
        }
    }
}

/// Set window frame rectangle (move/resize)
#[no_mangle]
pub extern "C" fn rust_gui_set_window_rect(id: WindowId, rect: *const Rect) -> i32 {
    if rect.is_null() {
        return -1;
    }
    unsafe {
        if set_window_rect(id, &*rect) { 1 } else { 0 }
    }
}

// ---- Surface management (for Go shell) ------------------------------------

static mut SURFACES: [Option<Surface>; 16] = [None; 16];
static mut NEXT_SURFACE_ID: usize = 1;

/// Create a new drawing surface
#[no_mangle]
pub extern "C" fn rust_gui_create_surface(width: i32, height: i32) -> *mut Surface {
    if width <= 0 || height <= 0 {
        return core::ptr::null_mut();
    }

    unsafe {
        let pitch = (width as u32 * 4) as i32;
        let size = (pitch as usize) * (height as usize);
        let pixels = crate::alloc_zeroed(size);
        if pixels.is_null() {
            return core::ptr::null_mut();
        }

        let surface = Surface {
            pixels: pixels as *mut u32,
            w: width,
            h: height,
            pitch,
        };

        for i in 0..SURFACES.len() {
            if SURFACES[i].is_none() {
                SURFACES[i] = Some(surface);
                return SURFACES[i].as_mut().unwrap() as *mut Surface;
            }
        }
        core::ptr::null_mut()
    }
}

/// Blit a surface to the framebuffer
#[no_mangle]
pub extern "C" fn rust_gui_blit_surface(surface: *mut Surface, x: i32, y: i32) -> i32 {
    if surface.is_null() {
        return -1;
    }
    unsafe {
        let surf = &*surface;
        let fb = get_framebuffer();
        if fb.is_null() {
            return -1;
        }
        crate::blit_surface(surf, x, y);
        0
    }
}

/// Get framebuffer info
#[no_mangle]
pub extern "C" fn rust_gui_get_fb_info(width: *mut i32, height: *mut i32, pitch: *mut i32) {
    unsafe {
        let (w, h) = get_framebuffer_size();
        let p = get_framebuffer_pitch() as i32;
        if !width.is_null() { *width = w; }
        if !height.is_null() { *height = h; }
        if !pitch.is_null() { *pitch = p; }
    }
}

// ---- Rendering test functions (for verification) --------------------------

/// Test: clear screen with color
#[no_mangle]
pub extern "C" fn rust_gui_test_clear(color: Color) {
    clear(color);
}

/// Test: draw a test pattern
#[no_mangle]
pub extern "C" fn rust_gui_test_pattern() {
    let (width, height) = get_framebuffer_size();

    // Draw gradient background
    fill_gradient_v(&Rect::new(0, 0, width, height),
                    Color::from_rgb(30, 30, 40),
                    Color::from_rgb(60, 60, 80));

    // Draw some rectangles
    fill_rect(&Rect::new(100, 100, 200, 150), Color::from_rgb(100, 150, 200));
    draw_rounded_rect(&Rect::new(150, 150, 100, 100), 20, Color::from_rgb(255, 255, 255), 3);

    // Draw a circle
    fill_circle(500, 200, 50, Color::from_rgb(200, 100, 100));
    draw_circle(500, 200, 50, Color::from_rgb(255, 255, 255), 2);

    // Draw lines
    draw_line_thick(50, 400, 300, 500, Color::from_rgb(255, 200, 100), 5);
    draw_line_thick(300, 500, 550, 400, Color::from_rgb(100, 255, 100), 5);
    draw_line_thick(550, 400, 350, 300, Color::from_rgb(100, 100, 255), 5);

    // Test window manager
    create_window("Test Window 1", 50, 50, 400, 300);
    create_window("Test Window 2", 500, 100, 350, 250);
    compose_frame();
}

// ---- Drawing primitives for Python Qt bindings -----------------------------

/// Draw a rectangle outline on a surface
#[no_mangle]
pub extern "C" fn rust_gui_draw_rect(surface: *mut Surface, x: i32, y: i32, w: i32, h: i32, color: u32, width: i32) {
    if surface.is_null() || width <= 0 {
        return;
    }
    unsafe {
        let surf = &*surface;
        let rect = Rect::new(x, y, w, h);
        crate::draw_rect(&rect, Color(color), width);
    }
}

/// Fill a rectangle on a surface
#[no_mangle]
pub extern "C" fn rust_gui_fill_rect(surface: *mut Surface, x: i32, y: i32, w: i32, h: i32, color: u32) {
    if surface.is_null() {
        return;
    }
    unsafe {
        let surf = &*surface;
        let rect = Rect::new(x, y, w, h);
        crate::fill_rect(&rect, Color(color));
    }
}

/// Draw a line on a surface
#[no_mangle]
pub extern "C" fn rust_gui_draw_line(surface: *mut Surface, x1: i32, y1: i32, x2: i32, y2: i32, color: u32, width: i32) {
    if surface.is_null() || width <= 0 {
        return;
    }
    unsafe {
        let surf = &*surface;
        crate::draw_line_thick(x1, y1, x2, y2, Color(color), width);
    }
}

/// Draw text on a surface
#[no_mangle]
pub extern "C" fn rust_gui_draw_text_on_surface(surface: *mut Surface, x: i32, y: i32, text: *const u8, color: u32, font_size: i32) {
    if surface.is_null() || text.is_null() {
        return;
    }
    unsafe {
        let surf = &*surface;
        let mut len = 0;
        while *text.add(len) != 0 && len < 1024 {
            len += 1;
        }
        let slice = core::slice::from_raw_parts(text, len);
        if let Ok(text_str) = core::str::from_utf8(slice) {
            // Use the font system to draw text
            // For now, use the bold font
            crate::font_bold.draw_text(x, y, text_str, Color(color));
        }
    }
}

/// Draw an ellipse on a surface
#[no_mangle]
pub extern "C" fn rust_gui_draw_ellipse(surface: *mut Surface, cx: i32, cy: i32, rx: i32, ry: i32, color: u32, width: i32) {
    if surface.is_null() || width <= 0 {
        return;
    }
    unsafe {
        let surf = &*surface;
        // Draw ellipse using multiple circles or a custom algorithm
        // For simplicity, draw as a circle if rx == ry, otherwise approximate
        if rx == ry {
            crate::draw_circle(cx, cy, rx, Color(color), width);
        } else {
            // Approximate ellipse with multiple circles or line segments
            // For now, just draw a circle with average radius
            let avg_r = (rx + ry) / 2;
            crate::draw_circle(cx, cy, avg_r, Color(color), width);
        }
    }
}

/// Set focus to a window
#[no_mangle]
pub extern "C" fn rust_gui_set_focus(id: WindowId) -> i32 {
    if set_focus(id) { 1 } else { 0 }
}
