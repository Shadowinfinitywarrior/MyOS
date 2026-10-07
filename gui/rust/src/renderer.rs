//! Rendering engine for the GUI system
//! Provides drawing primitives and surface management

use crate::{Color, Rect, Surface};

/// External kernel allocator
extern "C" {
    fn kmalloc(size: usize) -> *mut u8;
    fn kfree(ptr: *mut u8);
}

/// Simple math functions for no_std environment
mod math {
    /// Approximate sin using Taylor series
    pub fn sin(x: f32) -> f32 {
        let x = x % (2.0 * core::f32::consts::PI);
        let x2 = x * x;
        x - x * x2 / 6.0 + x * x2 * x2 / 120.0 - x * x2 * x2 * x2 / 5040.0
    }
    
    /// Approximate cos using Taylor series
    pub fn cos(x: f32) -> f32 {
        let x = x % (2.0 * core::f32::consts::PI);
        let x2 = x * x;
        1.0 - x2 / 2.0 + x2 * x2 / 24.0 - x2 * x2 * x2 / 720.0
    }
    
    /// Approximate sqrt using Newton's method
    pub fn sqrt(x: f32) -> f32 {
        if x <= 0.0 { return 0.0; }
        let mut guess = x;
        for _ in 0..10 {
            guess = 0.5 * (guess + x / guess);
        }
        guess
    }
}

/// Allocate zeroed memory from kernel heap
pub fn alloc_zeroed(size: usize) -> *mut u8 {
    unsafe {
        let ptr = kmalloc(size);
        if !ptr.is_null() {
            core::ptr::write_bytes(ptr, 0, size);
        }
        ptr
    }
}

/// Free memory back to kernel heap
pub fn free_mem(ptr: *mut u8) {
    unsafe { kfree(ptr); }
}

/// Color utilities
impl Color {
    /// Create color from ARGB components
    pub const fn from_argb(a: u8, r: u8, g: u8, b: u8) -> Color {
        Color(((a as u32) << 24) | ((r as u32) << 16) | ((g as u32) << 8) | (b as u32))
    }

    /// Create color from RGB components (alpha = 255)
    pub const fn from_rgb(r: u8, g: u8, b: u8) -> Color {
        Self::from_argb(255, r, g, b)
    }

    /// Get alpha component
    pub fn alpha(self) -> u8 {
        ((self.0 >> 24) & 0xFF) as u8
    }

    /// Get red component
    pub fn red(self) -> u8 {
        ((self.0 >> 16) & 0xFF) as u8
    }

    /// Get green component
    pub fn green(self) -> u8 {
        ((self.0 >> 8) & 0xFF) as u8
    }

    /// Get blue component
    pub fn blue(self) -> u8 {
        (self.0 & 0xFF) as u8
    }
}

/// Global framebuffer pointer (set by C kernel)
static mut FRAMEBUFFER: *mut u32 = core::ptr::null_mut();
static mut FB_WIDTH: i32 = 0;
static mut FB_HEIGHT: i32 = 0;
static mut FB_PITCH: u32 = 0;

/// Initialize the renderer with framebuffer information
pub fn init_renderer(fb: *mut u32, width: u32, height: u32, pitch: u32) {
    unsafe {
        FRAMEBUFFER = fb;
        FB_WIDTH = width as i32;
        FB_HEIGHT = height as i32;
        FB_PITCH = pitch;
    }
}

/// Get the framebuffer pointer
pub fn get_framebuffer() -> *mut u32 {
    unsafe { FRAMEBUFFER }
}

/// Get framebuffer dimensions
pub fn get_framebuffer_size() -> (i32, i32) {
    unsafe { (FB_WIDTH, FB_HEIGHT) }
}

/// Get framebuffer pitch
pub fn get_framebuffer_pitch() -> u32 {
    unsafe { FB_PITCH }
}

/// Check if a point is within bounds
fn in_bounds(x: i32, y: i32, width: i32, height: i32) -> bool {
    x >= 0 && y >= 0 && x < width && y < height
}

/// Set a pixel in the framebuffer
pub fn set_pixel(x: i32, y: i32, color: Color) {
    unsafe {
        if !in_bounds(x, y, FB_WIDTH, FB_HEIGHT) {
            return;
        }
        let pitch = FB_PITCH as usize / 4;
        let fb = FRAMEBUFFER;
        if !fb.is_null() {
            *fb.add((y as usize) * pitch + (x as usize)) = color.0;
        }
    }
}

/// Get a pixel from the framebuffer
pub fn get_pixel(x: i32, y: i32) -> Color {
    unsafe {
        if !in_bounds(x, y, FB_WIDTH, FB_HEIGHT) {
            return Color(0);
        }
        let pitch = FB_PITCH as usize / 4;
        let fb = FRAMEBUFFER;
        if fb.is_null() {
            return Color(0);
        }
        Color(*fb.add((y as usize) * pitch + (x as usize)))
    }
}

/// Fill a rectangle with a solid color
pub fn fill_rect(rect: &Rect, color: Color) {
    let x_start = rect.x.max(0);
    let y_start = rect.y.max(0);
    let (fb_width, fb_height) = unsafe { (FB_WIDTH, FB_HEIGHT) };
    let x_end = (rect.x + rect.w as i32).min(fb_width as i32);
    let y_end = (rect.y + rect.h as i32).min(fb_height as i32);

    unsafe {
        let pitch = FB_PITCH as usize / 4;
        let fb = FRAMEBUFFER;
        if fb.is_null() {
            return;
        }

        for y in y_start..y_end {
            let row = fb.add((y as usize) * pitch);
            for x in x_start..x_end {
                *row.add(x as usize) = color.0;
            }
        }
    }
}

/// Draw a rectangle outline
pub fn draw_rect(rect: &Rect, color: Color, thickness: i32) {
    if thickness <= 0 {
        return;
    }

    // Top edge
    fill_rect(&Rect::new(rect.x, rect.y, rect.w, thickness), color);
    // Bottom edge
    fill_rect(&Rect::new(rect.x, rect.y + rect.h - thickness, rect.w, thickness), color);
    // Left edge
    fill_rect(&Rect::new(rect.x, rect.y, thickness, rect.h), color);
    // Right edge
    fill_rect(&Rect::new(rect.x + rect.w - thickness, rect.y, thickness, rect.h), color);
}

/// Clear the entire framebuffer with a color
pub fn clear(color: Color) {
    unsafe {
        let (width, height) = (FB_WIDTH, FB_HEIGHT);
        fill_rect(&Rect::new(0, 0, width, height), color);
    }
}

/// Bresenham's line algorithm
pub fn draw_line(x0: i32, y0: i32, x1: i32, y1: i32, color: Color) {
    let dx = (x1 - x0).abs();
    let dy = -(y1 - y0).abs();
    let sx = if x0 < x1 { 1 } else { -1 };
    let sy = if y0 < y1 { 1 } else { -1 };
    let mut err = dx + dy;

    let mut x = x0;
    let mut y = y0;

    loop {
        set_pixel(x, y, color);
        if x == x1 && y == y1 {
            break;
        }
        let e2 = 2 * err;
        if e2 >= dy {
            err += dy;
            x += sx;
        }
        if e2 <= dx {
            err += dx;
            y += sy;
        }
    }
}

/// Draw a line with thickness
pub fn draw_line_thick(x0: i32, y0: i32, x1: i32, y1: i32, color: Color, thickness: i32) {
    if thickness <= 1 {
        draw_line(x0, y0, x1, y1, color);
        return;
    }

    let half = thickness / 2;
    for i in -half..=half {
        draw_line(x0, y0 + i, x1, y1 + i, color);
    }
}

/// Fill a circle
pub fn fill_circle(cx: i32, cy: i32, radius: i32, color: Color) {
    let r2 = radius * radius;

    for y in -radius..=radius {
        for x in -radius..=radius {
            if x * x + y * y <= r2 {
                set_pixel(cx + x, cy + y, color);
            }
        }
    }
}

/// Draw a circle outline
pub fn draw_circle(cx: i32, cy: i32, radius: i32, color: Color, thickness: i32) {
    let r_outer = radius;
    let r_inner = radius - thickness;
    let r2_outer = r_outer * r_outer;
    let r2_inner = if r_inner > 0 { r_inner * r_inner } else { 0 };

    for y in -r_outer..=r_outer {
        for x in -r_outer..=r_outer {
            let d2 = x * x + y * y;
            if d2 <= r2_outer && d2 >= r2_inner {
                set_pixel(cx + x, cy + y, color);
            }
        }
    }
}

/// Fill a rounded rectangle
pub fn fill_rounded_rect(rect: &Rect, radius: i32, color: Color) {
    if radius <= 0 {
        fill_rect(rect, color);
        return;
    }

    let mut r = radius;
    if r * 2 > rect.w {
        r = rect.w / 2;
    }
    if r * 2 > rect.h {
        r = rect.h / 2;
    }

    // Fill body
    let body = Rect::new(rect.x, rect.y + r, rect.w, rect.h - r * 2);
    fill_rect(&body, color);

    // Fill top and bottom
    let top = Rect::new(rect.x + r, rect.y, rect.w - r * 2, r);
    let bottom = Rect::new(rect.x + r, rect.y + rect.h - r, rect.w - r * 2, r);
    fill_rect(&top, color);
    fill_rect(&bottom, color);

    // Fill corners
    let r2 = r * r;
    let corners = [
        (rect.x + r, rect.y + r),
        (rect.x + rect.w as i32 - r - 1, rect.y + r),
        (rect.x + r, rect.y + rect.h as i32 - r - 1),
        (rect.x + rect.w as i32 - r - 1, rect.y + rect.h as i32 - r - 1),
    ];

    for (k, &(cx, cy)) in corners.iter().enumerate() {
        for dy in 0..r {
            for dx in 0..r {
                let px = if k & 1 == 0 { cx + dx } else { cx + r - 1 - dx };
                let py = if k & 2 == 0 { cy + dy } else { cy + r - 1 - dy };
                let ex = px - cx;
                let ey = py - cy;
                if ex * ex + ey * ey <= r2 {
                    set_pixel(px, py, color);
                }
            }
        }
    }
}

/// Draw a rounded rectangle outline
pub fn draw_rounded_rect(rect: &Rect, radius: i32, color: Color, thickness: i32) {
    if radius <= 0 || thickness <= 0 {
        draw_rect(rect, color, thickness);
        return;
    }

    let mut r = radius;
    if r * 2 > rect.w as i32 {
        r = rect.w as i32 / 2;
    }
    if r * 2 > rect.h as i32 {
        r = rect.h as i32 / 2;
    }

    // Draw top and bottom edges
    let top = Rect::new(rect.x + r, rect.y, rect.w - r * 2, thickness);
    let bottom = Rect::new(rect.x + r, rect.y + rect.h - thickness, rect.w - r * 2, thickness);
    fill_rect(&top, color);
    fill_rect(&bottom, color);

    // Draw left and right edges
    let left = Rect::new(rect.x, rect.y + r, thickness, rect.h - r * 2);
    let right = Rect::new(rect.x + rect.w - thickness, rect.y + r, thickness, rect.h - r * 2);
    fill_rect(&left, color);
    fill_rect(&right, color);

    // Draw corner arcs
    let r2_outer = r * r;
    let r_inner = r - thickness;
    let r2_inner = if r_inner > 0 { r_inner * r_inner } else { 0 };

    let corners = [
        (rect.x + r, rect.y + r, 1, 1),
        (rect.x + rect.w as i32 - r - 1, rect.y + r, -1, 1),
        (rect.x + r, rect.y + rect.h as i32 - r - 1, 1, -1),
        (rect.x + rect.w as i32 - r - 1, rect.y + rect.h as i32 - r - 1, -1, -1),
    ];

    for (cx, cy, sx, sy) in corners {
        for dy in 0..r {
            for dx in 0..r {
                let ox = sx * dx;
                let oy = sy * dy;
                let d2 = ox * ox + oy * oy;
                if d2 <= r2_outer && d2 >= r2_inner {
                    set_pixel(cx + ox, cy + oy, color);
                }
            }
        }
    }
}

/// Alpha blend two colors
fn alpha_blend(src: Color, dst: Color, alpha: u8) -> Color {
    if alpha == 255 {
        return src;
    }
    if alpha == 0 {
        return dst;
    }

    let a = alpha as u32;
    let na = 255 - a;

    let sr = src.red() as u32;
    let sg = src.green() as u32;
    let sb = src.blue() as u32;

    let dr = dst.red() as u32;
    let dg = dst.green() as u32;
    let db = dst.blue() as u32;

    let r = ((sr * a + dr * na) / 255) as u8;
    let g = ((sg * a + dg * na) / 255) as u8;
    let b = ((sb * a + db * na) / 255) as u8;

    Color::from_rgb(r, g, b)
}

/// Blit a surface to the framebuffer
pub fn blit_surface(surface: &Surface, dst_x: i32, dst_y: i32) {
    unsafe {
        let pitch = FB_PITCH as usize / 4;
        let fb = FRAMEBUFFER;
        if fb.is_null() || surface.pixels.is_null() {
            return;
        }

        let src_pitch = (surface.pitch / 4) as usize;

        for y in 0..surface.h as i32 {
            let dy = dst_y + y;
            if dy < 0 || dy >= FB_HEIGHT {
                continue;
            }
            let dst_row = fb.add((dy as usize) * pitch);
            let src_row = surface.pixels.add((y as usize) * src_pitch);

            for x in 0..surface.w as i32 {
                let dx = dst_x + x;
                if dx < 0 || dx >= FB_WIDTH {
                    continue;
                }
                *dst_row.add(dx as usize) = *src_row.add(x as usize);
            }
        }
    }
}

/// Blit a surface with alpha blending
pub fn blit_surface_alpha(surface: &Surface, dst_x: i32, dst_y: i32) {
    unsafe {
        let pitch = FB_PITCH as usize / 4;
        let fb = FRAMEBUFFER;
        if fb.is_null() || surface.pixels.is_null() {
            return;
        }

        let src_pitch = (surface.pitch / 4) as usize;

        for y in 0..surface.h as i32 {
            let dy = dst_y + y;
            if dy < 0 || dy >= FB_HEIGHT {
                continue;
            }
            let dst_row = fb.add((dy as usize) * pitch);
            let src_row = surface.pixels.add((y as usize) * src_pitch);

            for x in 0..surface.w as i32 {
                let dx = dst_x + x;
                if dx < 0 || dx >= FB_WIDTH {
                    continue;
                }
                let src_pixel = Color(*src_row.add(x as usize));
                let dst_pixel = Color(*dst_row.add(dx as usize));
                let alpha = src_pixel.alpha();
                *dst_row.add(dx as usize) = alpha_blend(src_pixel, dst_pixel, alpha).0;
            }
        }
    }
}

/// Vertical gradient
pub fn fill_gradient_v(rect: &Rect, top: Color, bottom: Color) {
    let rect = *rect;
    if rect.h <= 0 {
        return;
    }

    for y in 0..rect.h {
        let t = (y as u32 * 255) / rect.h as u32;
        let r = top.red() as u32 + ((bottom.red() as u32 - top.red() as u32) * t / 255);
        let g = top.green() as u32 + ((bottom.green() as u32 - top.green() as u32) * t / 255);
        let b = top.blue() as u32 + ((bottom.blue() as u32 - top.blue() as u32) * t / 255);
        let color = Color::from_rgb(r as u8, g as u8, b as u8);
        fill_rect(&Rect::new(rect.x, rect.y + y, rect.w, 1), color);
    }
}

/// Horizontal gradient
pub fn fill_gradient_h(rect: &Rect, left: Color, right: Color) {
    let rect = *rect;
    if rect.w <= 0 {
        return;
    }

    for x in 0..rect.w {
        let t = (x as u32 * 255) / rect.w as u32;
        let r = left.red() as u32 + ((right.red() as u32 - left.red() as u32) * t / 255);
        let g = left.green() as u32 + ((right.green() as u32 - left.green() as u32) * t / 255);
        let b = left.blue() as u32 + ((right.blue() as u32 - left.blue() as u32) * t / 255);
        let color = Color::from_rgb(r as u8, g as u8, b as u8);
        fill_rect(&Rect::new(rect.x + x, rect.y, 1, rect.h), color);
    }
}

/// Draw an ellipse outline
pub fn draw_ellipse(cx: i32, cy: i32, rx: i32, ry: i32, color: Color, thickness: i32) {
    if thickness <= 0 {
        return;
    }

    // Use midpoint ellipse algorithm for outline
    // For filled ellipse, use fill_ellipse
    
    // Draw as approximation using multiple circles/lines for now
    // Full implementation would use proper ellipse algorithm
    if rx == ry {
        draw_circle(cx, cy, rx, color, thickness);
        return;
    }

    // Approximate ellipse with line segments
    let segments = 64;
    for i in 0..segments {
        let angle1 = (i as f32 * 2.0 * core::f32::consts::PI) / segments as f32;
        let angle2 = ((i + 1) as f32 * 2.0 * core::f32::consts::PI) / segments as f32;
        
        let x1 = cx + (rx as f32 * math::cos(angle1)) as i32;
        let y1 = cy + (ry as f32 * math::sin(angle1)) as i32;
        let x2 = cx + (rx as f32 * math::cos(angle2)) as i32;
        let y2 = cy + (ry as f32 * math::sin(angle2)) as i32;
        
        draw_line_thick(x1, y1, x2, y2, color, thickness);
    }
}

/// Fill an ellipse
pub fn fill_ellipse(cx: i32, cy: i32, rx: i32, ry: i32, color: Color) {
    if rx <= 0 || ry <= 0 {
        return;
    }

    let rx2 = rx * rx;
    let ry2 = ry * ry;
    let two_rx2 = 2 * rx2;
    let two_ry2 = 2 * ry2;

    for y in -ry..=ry {
        let y2 = y * y;
        // Calculate x range for this y
        let x_max = if ry2 > 0 {
            math::sqrt((rx2 * (ry2 - y2) / ry2) as f32) as i32
        } else {
            0
        };
        
        for x in -x_max..=x_max {
            set_pixel(cx + x, cy + y, color);
        }
    }
}

/// Draw text on surface (placeholder - uses font system)
pub fn draw_text(x: i32, y: i32, text: &str, color: Color, font_size: i32) {
    // This would use the font system to render text
    // For now, it's a placeholder that the font module handles
    // The actual implementation is in font.rs
    // We call font.draw_text from the FFI wrapper
}
