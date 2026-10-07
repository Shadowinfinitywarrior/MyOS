//! Window manager implementation
//! Handles window creation, destruction, z-order, focus, and decorations

use crate::{Color, Rect, Window, WindowId, WF_NO_DECOR};

pub const MAX_WINDOWS: usize = 16;
pub const TITLEBAR_HEIGHT: i32 = 30;
pub const BORDER_WIDTH: i32 = 1;
pub const MIN_WINDOW_WIDTH: i32 = 240;
pub const MIN_WINDOW_HEIGHT: i32 = 120;

struct WindowManager {
    windows: [Option<Window>; MAX_WINDOWS],
    window_count: usize,
    next_id: WindowId,
    focus: Option<usize>,
}

static mut WM: WindowManager = WindowManager {
    windows: [None; MAX_WINDOWS],
    window_count: 0,
    next_id: 1,
    focus: None,
};

/// Initialize the window manager
pub fn init_window_manager() {
    unsafe {
        WM.window_count = 0;
        WM.next_id = 1;
        WM.focus = None;
    }
}

/// Create a new window
pub fn create_window(title: &str, x: i32, y: i32, w: i32, h: i32) -> Option<WindowId> {
    unsafe {
        if WM.window_count >= MAX_WINDOWS {
            return None;
        }

        let mut w = w.max(MIN_WINDOW_WIDTH);
        let mut h = h.max(MIN_WINDOW_HEIGHT + TITLEBAR_HEIGHT);

        // Clamp to screen size
        let (screen_w, screen_h) = crate::get_framebuffer_size();
        if w > screen_w {
            w = screen_w;
        }
        if h > screen_h {
            h = screen_h;
        }

        let id = WM.next_id;
        WM.next_id += 1;

        // Copy title
        let mut title_bytes = [0u8; 64];
        let title_len = title.len().min(63);
        for (i, b) in title.as_bytes().iter().take(title_len).enumerate() {
            title_bytes[i] = *b;
        }

        let frame = Rect::new(x, y, w, h);
        let client = Rect::new(
            x + BORDER_WIDTH,
            y + TITLEBAR_HEIGHT + BORDER_WIDTH,
            w - BORDER_WIDTH * 2,
            h - TITLEBAR_HEIGHT - BORDER_WIDTH * 2,
        );

        let window = Window {
            id,
            title: title_bytes,
            frame,
            client,
            flags: 0,
            visible: true,
            focused: true,
        };

        WM.windows[WM.window_count] = Some(window);
        WM.window_count += 1;
        WM.focus = Some(WM.window_count - 1);

        Some(id)
    }
}

/// Destroy a window
pub fn destroy_window(id: WindowId) -> bool {
    unsafe {
        let mut idx = None;
        for i in 0..WM.window_count {
            if let Some(ref win) = WM.windows[i] {
                if win.id == id {
                    idx = Some(i);
                    break;
                }
            }
        }

        if let Some(idx) = idx {
            // Shift remaining windows
            for i in idx..WM.window_count - 1 {
                WM.windows[i] = WM.windows[i + 1];
            }
            WM.windows[WM.window_count - 1] = None;
            WM.window_count -= 1;

            // Update focus
            if WM.focus == Some(idx) {
                WM.focus = if WM.window_count > 0 {
                    Some(WM.window_count - 1)
                } else {
                    None
                };
            }

            return true;
        }
        false
    }
}

/// Get window count
pub fn window_count() -> usize {
    unsafe { WM.window_count }
}

/// Get window by index
pub fn get_window(index: usize) -> Option<&'static Window> {
    unsafe {
        if index < WM.window_count {
            WM.windows[index].as_ref()
        } else {
            None
        }
    }
}

/// Get window by ID
pub fn find_window(id: WindowId) -> Option<&'static Window> {
    unsafe {
        for i in 0..WM.window_count {
            if let Some(ref win) = WM.windows[i] {
                if win.id == id {
                    return WM.windows[i].as_ref();
                }
            }
        }
        None
    }
}

/// Get focused window
pub fn get_focused() -> Option<&'static Window> {
    unsafe {
        if let Some(idx) = WM.focus {
            if idx < WM.window_count {
                WM.windows[idx].as_ref()
            } else {
                None
            }
        } else {
            None
        }
    }
}

/// Set focus to a window
pub fn set_focus(id: WindowId) -> bool {
    unsafe {
        for i in 0..WM.window_count {
            if let Some(ref win) = WM.windows[i] {
                if win.id == id {
                    // Unfocus current
                    if let Some(old_idx) = WM.focus {
                        if old_idx < WM.window_count {
                            if let Some(ref mut win) = WM.windows[old_idx] {
                                win.focused = false;
                            }
                        }
                    }

                    // Focus new
                    if let Some(ref mut win) = WM.windows[i] {
                        win.focused = true;
                    }
                    WM.focus = Some(i);

                    // Move to top of z-order
                    let window = WM.windows[i];
                    for j in i..WM.window_count - 1 {
                        WM.windows[j] = WM.windows[j + 1];
                    }
                    WM.windows[WM.window_count - 1] = window;

                    return true;
                }
            }
        }
        false
    }
}

/// Invalidate a window (mark for redraw)
pub fn invalidate_window(_id: WindowId) {
    // TODO: Implement damage tracking
}

/// Set window title
pub fn set_window_title(id: WindowId, title: &str) -> bool {
    unsafe {
        for i in 0..WM.window_count {
            if let Some(ref mut win) = WM.windows[i] {
                if win.id == id {
                    let mut title_bytes = [0u8; 64];
                    let title_len = title.len().min(63);
                    for (i, b) in title.as_bytes().iter().take(title_len).enumerate() {
                        title_bytes[i] = *b;
                    }
                    win.title = title_bytes;
                    return true;
                }
            }
        }
        false
    }
}

/// Get window frame rectangle
pub fn get_window_rect(id: WindowId, rect: &mut Rect) -> bool {
    unsafe {
        for i in 0..WM.window_count {
            if let Some(ref win) = WM.windows[i] {
                if win.id == id {
                    *rect = win.frame;
                    return true;
                }
            }
        }
        false
    }
}

/// Set window frame rectangle (move/resize)
pub fn set_window_rect(id: WindowId, rect: &Rect) -> bool {
    unsafe {
        for i in 0..WM.window_count {
            if let Some(ref mut win) = WM.windows[i] {
                if win.id == id {
                    let w = rect.w.max(MIN_WINDOW_WIDTH);
                    let h = rect.h.max(MIN_WINDOW_HEIGHT + TITLEBAR_HEIGHT);

                    let (screen_w, screen_h) = crate::get_framebuffer_size();
                    let w = w.min(screen_w);
                    let h = h.min(screen_h);

                    win.frame = Rect::new(rect.x, rect.y, w, h);
                    win.client = Rect::new(
                        rect.x + BORDER_WIDTH,
                        rect.y + TITLEBAR_HEIGHT + BORDER_WIDTH,
                        w - BORDER_WIDTH * 2,
                        h - TITLEBAR_HEIGHT - BORDER_WIDTH * 2,
                    );
                    return true;
                }
            }
        }
        false
    }
}

/// Draw window decorations
pub fn draw_decorations(window: &Window) {
    if window.flags & WF_NO_DECOR != 0 {
        return;
    }

    let frame = window.frame;
    let titlebar_color = if window.focused {
        Color::from_rgb(60, 60, 70)
    } else {
        Color::from_rgb(45, 45, 55)
    };

    // Title bar background
    let titlebar_rect = Rect::new(
        frame.x,
        frame.y,
        frame.w,
        TITLEBAR_HEIGHT,
    );
    crate::fill_rect(&titlebar_rect, titlebar_color);

    // Border
    crate::draw_rect(&frame, Color::from_rgb(100, 100, 100), BORDER_WIDTH);

    // Title text
    let title_color = if window.focused {
        Color::from_rgb(255, 255, 255)
    } else {
        Color::from_rgb(180, 180, 180)
    };

    // Convert title bytes to string
    let title_len = window.title.iter().position(|&b| b == 0).unwrap_or(64);
    let title_str = core::str::from_utf8(&window.title[..title_len]).unwrap_or("Untitled");

    unsafe {
        crate::font_bold.draw_text(frame.x + 10, frame.y + 8, title_str, title_color);
    }
}

/// Draw all windows (compositor)
pub fn compose_frame() {
    unsafe {
        for i in 0..WM.window_count {
            if let Some(ref window) = WM.windows[i] {
                if !window.visible {
                    continue;
                }

                // Draw decorations
                draw_decorations(window);

                // Draw client area (placeholder - solid color)
                let client_color = if window.focused {
                    Color::from_rgb(40, 40, 50)
                } else {
                    Color::from_rgb(30, 30, 40)
                };
                crate::fill_rect(&window.client, client_color);
            }
        }
    }
}

/// Hit test for window at position
pub fn hit_test(x: i32, y: i32) -> Option<WindowId> {
    unsafe {
        // Check windows in reverse z-order (top to bottom)
        for i in (0..WM.window_count).rev() {
            if let Some(ref window) = WM.windows[i] {
                if !window.visible {
                    continue;
                }

                if x >= window.frame.x
                    && x < window.frame.x + window.frame.w as i32
                    && y >= window.frame.y
                    && y < window.frame.y + window.frame.h as i32
                {
                    return Some(window.id);
                }
            }
        }
        None
    }
}

/// Resize edge for hit testing
#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ResizeEdge {
    None = 0,
    North = 1,
    South = 2,
    East = 3,
    West = 4,
    NorthEast = 5,
    NorthWest = 6,
    SouthEast = 7,
    SouthWest = 8,
}

/// Hit test for resize edge
pub fn hit_test_resize(x: i32, y: i32, window: &Window) -> ResizeEdge {
    const EDGE_THRESHOLD: i32 = 8;

    let frame = window.frame;
    let mut edge = ResizeEdge::None;

    // Check edges
    if y < frame.y + EDGE_THRESHOLD {
        edge = ResizeEdge::North;
    } else if y >= frame.y + frame.h as i32 - EDGE_THRESHOLD {
        edge = ResizeEdge::South;
    }

    if x < frame.x + EDGE_THRESHOLD {
        edge = match edge {
            ResizeEdge::North => ResizeEdge::NorthWest,
            ResizeEdge::South => ResizeEdge::SouthWest,
            _ => ResizeEdge::West,
        };
    } else if x >= frame.x + frame.w as i32 - EDGE_THRESHOLD {
        edge = match edge {
            ResizeEdge::North => ResizeEdge::NorthEast,
            ResizeEdge::South => ResizeEdge::SouthEast,
            _ => ResizeEdge::East,
        };
    }

    edge
}
