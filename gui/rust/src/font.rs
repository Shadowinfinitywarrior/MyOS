//! Font rendering for the GUI system
//! Parses baked font format and provides text drawing functions

use crate::{Color, Rect};
use core::ptr;

/// Font metrics for a single glyph
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct GlyphMetrics {
    pub width: u16,
    pub height: u16,
    pub x_offset: i8,
    pub y_offset: i8,
    pub x_advance: u16,
    pub bitmap_offset: u32,
}

/// Sync wrapper for raw pointers - safe because we only initialize once and then read-only
#[derive(Debug)]
pub struct SyncPtr<T> {
    ptr: *const T,
}

impl<T> SyncPtr<T> {
    pub const fn new(ptr: *const T) -> Self {
        Self { ptr }
    }
    
    pub fn get(&self) -> *const T {
        self.ptr
    }
    
    pub fn set(&mut self, ptr: *const T) {
        self.ptr = ptr;
    }
}

impl<T> Copy for SyncPtr<T> {}
impl<T> Clone for SyncPtr<T> {
    fn clone(&self) -> Self {
        *self
    }
}

unsafe impl<T> Sync for SyncPtr<T> {}

/// Font structure matching the baked font format
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct Font {
    pub first_cp: u16,
    pub last_cp: u16,
    pub glyph_count: u16,
    pub pixel_size: u16,
    pub line_height: u16,
    pub glyph_widths: SyncPtr<u16>,
    pub glyph_heights: SyncPtr<u16>,
    pub glyph_x_offsets: SyncPtr<i8>,
    pub glyph_y_offsets: SyncPtr<i8>,
    pub glyph_x_advances: SyncPtr<u16>,
    pub glyph_bitmap_offsets: SyncPtr<u32>,
    pub bitmap_data: SyncPtr<u8>,
    pub bitmap_size: u32,
}

impl Font {
    /// Get glyph metrics for a codepoint
    pub fn get_glyph(&self, cp: u32) -> Option<GlyphMetrics> {
        if cp < self.first_cp as u32 || cp > self.last_cp as u32 {
            return None;
        }
        
        let idx = (cp - self.first_cp as u32) as usize;
        
        unsafe {
            Some(GlyphMetrics {
                width: *self.glyph_widths.get().add(idx),
                height: *self.glyph_heights.get().add(idx),
                x_offset: *self.glyph_x_offsets.get().add(idx),
                y_offset: *self.glyph_y_offsets.get().add(idx),
                x_advance: *self.glyph_x_advances.get().add(idx),
                bitmap_offset: *self.glyph_bitmap_offsets.get().add(idx),
            })
        }
    }
    
    /// Get glyph bitmap data
    pub fn get_glyph_bitmap(&self, metrics: &GlyphMetrics) -> &[u8] {
        unsafe {
            let start = self.bitmap_data.get().add(metrics.bitmap_offset as usize);
            let row_bytes = ((metrics.width as usize + 7) / 8).max(1);
            let size = row_bytes * metrics.height as usize;
            core::slice::from_raw_parts(start, size)
        }
    }
    
    /// Get the width of a text string
    pub fn text_width(&self, text: &str) -> i32 {
        let mut width = 0i32;
        for ch in text.chars() {
            if let Some(glyph) = self.get_glyph(ch as u32) {
                width += glyph.x_advance as i32;
            }
        }
        width
    }
    
    /// Draw text at position with color
    pub fn draw_text(&self, x: i32, y: i32, text: &str, color: Color) {
        let mut cursor_x = x;
        let baseline_y = y + self.line_height as i32;
        
        for ch in text.chars() {
            if let Some(glyph) = self.get_glyph(ch as u32) {
                self.draw_glyph(cursor_x, baseline_y, &glyph, color);
                cursor_x += glyph.x_advance as i32;
            }
        }
    }
    
    /// Draw text with background color
    pub fn draw_text_bg(&self, x: i32, y: i32, text: &str, fg: Color, bg: Color) {
        let width = self.text_width(text);
        let height = self.line_height as i32;
        
        // Draw background
        crate::fill_rect(&Rect::new(x, y, width, height), bg);
        
        // Draw text
        self.draw_text(x, y, text, fg);
    }
    
    /// Draw text clipped to a rectangle
    pub fn draw_text_clipped(&self, rect: &Rect, x: i32, y: i32, text: &str, color: Color) {
        let mut cursor_x = x;
        let baseline_y = y + self.line_height as i32;
        
        for ch in text.chars() {
            if let Some(glyph) = self.get_glyph(ch as u32) {
                let glyph_x = cursor_x + glyph.x_offset as i32;
                let glyph_y = baseline_y - glyph.y_offset as i32;
                
                // Check if glyph is visible in clip rect
                if glyph_x + glyph.width as i32 >= rect.x
                    && glyph_x < rect.x + rect.w
                    && glyph_y + glyph.height as i32 >= rect.y
                    && glyph_y < rect.y + rect.h {
                    
                    self.draw_glyph_clipped(rect, glyph_x, glyph_y, &glyph, color);
                }
                cursor_x += glyph.x_advance as i32;
            }
        }
    }
    
    /// Draw a single glyph
    fn draw_glyph(&self, x: i32, y: i32, glyph: &GlyphMetrics, color: Color) {
        let glyph_x = x + glyph.x_offset as i32;
        let glyph_y = y - glyph.y_offset as i32;
        
        let bitmap = self.get_glyph_bitmap(glyph);
        let row_bytes = ((glyph.width as usize + 7) / 8).max(1);
        
        for row in 0..glyph.height as usize {
            let src_row = &bitmap[row * row_bytes..];
            let dst_y = glyph_y + row as i32;
            
            for col in 0..glyph.width as usize {
                let byte_idx = col / 8;
                let bit_idx = 7 - (col % 8);
                if (src_row[byte_idx] >> bit_idx) & 1 != 0 {
                    crate::set_pixel(glyph_x + col as i32, dst_y, color);
                }
            }
        }
    }
    
    /// Draw a single glyph with clipping
    fn draw_glyph_clipped(&self, clip: &Rect, x: i32, y: i32, glyph: &GlyphMetrics, color: Color) {
        let bitmap = self.get_glyph_bitmap(glyph);
        let row_bytes = ((glyph.width as usize + 7) / 8).max(1);
        
        for row in 0..glyph.height as usize {
            let dst_y = y + row as i32;
            if dst_y < clip.y || dst_y >= clip.y + clip.h {
                continue;
            }
            
            let src_row = &bitmap[row * row_bytes..];
            
            for col in 0..glyph.width as usize {
                let dst_x = x + col as i32;
                if dst_x < clip.x || dst_x >= clip.x + clip.w {
                    continue;
                }
                
                let byte_idx = col / 8;
                let bit_idx = 7 - (col % 8);
                if (src_row[byte_idx] >> bit_idx) & 1 != 0 {
                    crate::set_pixel(dst_x, dst_y, color);
                }
            }
        }
    }
}

// ========================================================================
// Font definitions - these match the C baked font headers
// ========================================================================

/// UI Font (DejaVu Sans 15px)
pub static mut font_ui: Font = Font {
    first_cp: 32,
    last_cp: 126,
    glyph_count: 95,
    pixel_size: 15,
    line_height: 15,
    glyph_widths: SyncPtr::new(ptr::null()),
    glyph_heights: SyncPtr::new(ptr::null()),
    glyph_x_offsets: SyncPtr::new(ptr::null()),
    glyph_y_offsets: SyncPtr::new(ptr::null()),
    glyph_x_advances: SyncPtr::new(ptr::null()),
    glyph_bitmap_offsets: SyncPtr::new(ptr::null()),
    bitmap_data: SyncPtr::new(ptr::null()),
    bitmap_size: 0,
};

/// Mono Font (DejaVu Sans Mono 15px)
pub static mut font_mono: Font = Font {
    first_cp: 32,
    last_cp: 126,
    glyph_count: 95,
    pixel_size: 15,
    line_height: 15,
    glyph_widths: SyncPtr::new(ptr::null()),
    glyph_heights: SyncPtr::new(ptr::null()),
    glyph_x_offsets: SyncPtr::new(ptr::null()),
    glyph_y_offsets: SyncPtr::new(ptr::null()),
    glyph_x_advances: SyncPtr::new(ptr::null()),
    glyph_bitmap_offsets: SyncPtr::new(ptr::null()),
    bitmap_data: SyncPtr::new(ptr::null()),
    bitmap_size: 0,
};

/// Bold Font (DejaVu Sans Bold 15px)
pub static mut font_bold: Font = Font {
    first_cp: 32,
    last_cp: 126,
    glyph_count: 95,
    pixel_size: 15,
    line_height: 15,
    glyph_widths: SyncPtr::new(ptr::null()),
    glyph_heights: SyncPtr::new(ptr::null()),
    glyph_x_offsets: SyncPtr::new(ptr::null()),
    glyph_y_offsets: SyncPtr::new(ptr::null()),
    glyph_x_advances: SyncPtr::new(ptr::null()),
    glyph_bitmap_offsets: SyncPtr::new(ptr::null()),
    bitmap_data: SyncPtr::new(ptr::null()),
    bitmap_size: 0,
};

/// Blocks Font (Unicode block elements)
pub static mut font_blocks: Font = Font {
    first_cp: 9600,
    last_cp: 9631,
    glyph_count: 32,
    pixel_size: 15,
    line_height: 15,
    glyph_widths: SyncPtr::new(ptr::null()),
    glyph_heights: SyncPtr::new(ptr::null()),
    glyph_x_offsets: SyncPtr::new(ptr::null()),
    glyph_y_offsets: SyncPtr::new(ptr::null()),
    glyph_x_advances: SyncPtr::new(ptr::null()),
    glyph_bitmap_offsets: SyncPtr::new(ptr::null()),
    bitmap_data: SyncPtr::new(ptr::null()),
    bitmap_size: 0,
};

// ========================================================================
// Font initialization - called from C after fonts are linked
// ========================================================================

/// Initialize font pointers from C symbols
/// This is called from C after the font objects are linked in
#[no_mangle]
pub extern "C" fn rust_gui_init_fonts(
    ui_gw: *const u16, ui_gh: *const u16, ui_xo: *const i8, ui_yo: *const i8,
    ui_xa: *const u16, ui_bo: *const u32, ui_bitmap: *const u8, ui_bitmap_size: u32,
    mono_gw: *const u16, mono_gh: *const u16, mono_xo: *const i8, mono_yo: *const i8,
    mono_xa: *const u16, mono_bo: *const u32, mono_bitmap: *const u8, mono_bitmap_size: u32,
    bold_gw: *const u16, bold_gh: *const u16, bold_xo: *const i8, bold_yo: *const i8,
    bold_xa: *const u16, bold_bo: *const u32, bold_bitmap: *const u8, bold_bitmap_size: u32,
    blocks_gw: *const u16, blocks_gh: *const u16, blocks_xo: *const i8, blocks_yo: *const i8,
    blocks_xa: *const u16, blocks_bo: *const u32, blocks_bitmap: *const u8, blocks_bitmap_size: u32,
) {
    unsafe {
        // UI Font
        font_ui.glyph_widths.ptr = ui_gw;
        font_ui.glyph_heights.ptr = ui_gh;
        font_ui.glyph_x_offsets.ptr = ui_xo;
        font_ui.glyph_y_offsets.ptr = ui_yo;
        font_ui.glyph_x_advances.ptr = ui_xa;
        font_ui.glyph_bitmap_offsets.ptr = ui_bo;
        font_ui.bitmap_data.ptr = ui_bitmap;
        font_ui.bitmap_size = ui_bitmap_size;
        
        // Mono Font
        font_mono.glyph_widths.ptr = mono_gw;
        font_mono.glyph_heights.ptr = mono_gh;
        font_mono.glyph_x_offsets.ptr = mono_xo;
        font_mono.glyph_y_offsets.ptr = mono_yo;
        font_mono.glyph_x_advances.ptr = mono_xa;
        font_mono.glyph_bitmap_offsets.ptr = mono_bo;
        font_mono.bitmap_data.ptr = mono_bitmap;
        font_mono.bitmap_size = mono_bitmap_size;
        
        // Bold Font
        font_bold.glyph_widths.ptr = bold_gw;
        font_bold.glyph_heights.ptr = bold_gh;
        font_bold.glyph_x_offsets.ptr = bold_xo;
        font_bold.glyph_y_offsets.ptr = bold_yo;
        font_bold.glyph_x_advances.ptr = bold_xa;
        font_bold.glyph_bitmap_offsets.ptr = bold_bo;
        font_bold.bitmap_data.ptr = bold_bitmap;
        font_bold.bitmap_size = bold_bitmap_size;
        
        // Blocks Font
        font_blocks.glyph_widths.ptr = blocks_gw;
        font_blocks.glyph_heights.ptr = blocks_gh;
        font_blocks.glyph_x_offsets.ptr = blocks_xo;
        font_blocks.glyph_y_offsets.ptr = blocks_yo;
        font_blocks.glyph_x_advances.ptr = blocks_xa;
        font_blocks.glyph_bitmap_offsets.ptr = blocks_bo;
        font_blocks.bitmap_data.ptr = blocks_bitmap;
        font_blocks.bitmap_size = blocks_bitmap_size;
    }
}

// ========================================================================
// Public API functions (matching C interface)
// ========================================================================

/// Get the UI font
#[no_mangle]
pub extern "C" fn rust_gui_font_ui() -> *const Font {
    unsafe { &font_ui }
}

/// Get the mono font
#[no_mangle]
pub extern "C" fn rust_gui_font_mono() -> *const Font {
    unsafe { &font_mono }
}

/// Get the bold font
#[no_mangle]
pub extern "C" fn rust_gui_font_bold() -> *const Font {
    unsafe { &font_bold }
}

/// Get the blocks font
#[no_mangle]
pub extern "C" fn rust_gui_font_blocks() -> *const Font {
    unsafe { &font_blocks }
}

/// Draw text using UI font
#[no_mangle]
pub extern "C" fn rust_gui_draw_text(x: i32, y: i32, text: *const u8, color: Color) {
    unsafe {
        if text.is_null() { return; }
        let mut len = 0;
        while *text.add(len) != 0 { len += 1; }
        let slice = core::slice::from_raw_parts(text, len);
        if let Ok(s) = core::str::from_utf8(slice) {
            font_ui.draw_text(x, y, s, color);
        }
    }
}

/// Draw text with background
#[no_mangle]
pub extern "C" fn rust_gui_draw_text_bg(x: i32, y: i32, text: *const u8, fg: Color, bg: Color) {
    unsafe {
        if text.is_null() { return; }
        let mut len = 0;
        while *text.add(len) != 0 { len += 1; }
        let slice = core::slice::from_raw_parts(text, len);
        if let Ok(s) = core::str::from_utf8(slice) {
            font_ui.draw_text_bg(x, y, s, fg, bg);
        }
    }
}

/// Get text width
#[no_mangle]
pub extern "C" fn rust_gui_text_width(text: *const u8) -> i32 {
    unsafe {
        if text.is_null() { return 0; }
        let mut len = 0;
        while *text.add(len) != 0 { len += 1; }
        let slice = core::slice::from_raw_parts(text, len);
        if let Ok(s) = core::str::from_utf8(slice) {
            font_ui.text_width(s)
        } else { 0 }
    }
}

/// Draw text with specific font
#[no_mangle]
pub extern "C" fn rust_gui_draw_text_font(font: *const Font, x: i32, y: i32, text: *const u8, color: Color) {
    unsafe {
        if font.is_null() || text.is_null() { return; }
        let mut len = 0;
        while *text.add(len) != 0 { len += 1; }
        let slice = core::slice::from_raw_parts(text, len);
        if let Ok(s) = core::str::from_utf8(slice) {
            (*font).draw_text(x, y, s, color);
        }
    }
}

/// Draw text clipped to rect
#[no_mangle]
pub extern "C" fn rust_gui_draw_text_clipped(
    font: *const Font,
    clip_x: i32, clip_y: i32, clip_w: i32, clip_h: i32,
    x: i32, y: i32, text: *const u8, color: Color
) {
    unsafe {
        if font.is_null() || text.is_null() { return; }
        let mut len = 0;
        while *text.add(len) != 0 { len += 1; }
        let slice = core::slice::from_raw_parts(text, len);
        if let Ok(s) = core::str::from_utf8(slice) {
            let clip = Rect::new(clip_x, clip_y, clip_w, clip_h);
            (*font).draw_text_clipped(&clip, x, y, s, color);
        }
    }
}