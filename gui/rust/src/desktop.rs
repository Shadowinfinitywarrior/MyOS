//! Desktop shell implementation in Rust
//! Provides wallpaper, taskbar, start menu, desktop icons

use crate::{Color, Rect, fill_rect, draw_rect, fill_rounded_rect, blit_surface, font_ui, font_bold};

/// External C functions
extern "C" {
    fn timer_get_ticks() -> u64;
    fn acpi_shutdown() -> !;
    fn acpi_reboot() -> !;
}

/// Desktop shell state
struct DesktopShell {
    width: i32,
    height: i32,
    taskbar_height: i32,
    start_button_width: i32,
    menu_open: bool,
    menu_hover: i32,
    start_hover: bool,
    task_hover: i32,
    wallpaper_surface: Option<*mut crate::Surface>,
    clock_buffer: [u8; 32],
    last_clock_update: u64,
    icons: [DesktopIcon; 8],
    icon_count: usize,
}

#[derive(Clone, Copy)]
struct DesktopIcon {
    app_name: &'static str,
    x: i32,
    y: i32,
    hot: bool,
}

static mut DESKTOP: DesktopShell = DesktopShell {
    width: 0,
    height: 0,
    taskbar_height: 44,
    start_button_width: 92,
    menu_open: false,
    menu_hover: -1,
    start_hover: false,
    task_hover: -1,
    wallpaper_surface: None,
    clock_buffer: [0; 32],
    last_clock_update: 0,
    icons: [DesktopIcon { app_name: "", x: 0, y: 0, hot: false }; 8],
    icon_count: 0,
};

const DESKTOP_ICON_SIZE: i32 = 64;
const DESKTOP_ICON_SPACING: i32 = 20;
const DESKTOP_ICON_START_X: i32 = 20;
const DESKTOP_ICON_START_Y: i32 = 20;

/// Initialize the desktop shell
pub fn init_desktop_shell(width: i32, height: i32) {
    unsafe {
        DESKTOP.width = width;
        DESKTOP.height = height;
        DESKTOP.menu_open = false;
        DESKTOP.menu_hover = -1;
        DESKTOP.start_hover = false;
        DESKTOP.task_hover = -1;
        DESKTOP.wallpaper_surface = None;
        DESKTOP.icon_count = 0;
        
        // Initialize default icons
        let icon_apps = [
            ("Terminal", DESKTOP_ICON_START_X, DESKTOP_ICON_START_Y),
            ("Files", DESKTOP_ICON_START_X, DESKTOP_ICON_START_Y + DESKTOP_ICON_SIZE + DESKTOP_ICON_SPACING),
            ("System", DESKTOP_ICON_START_X, DESKTOP_ICON_START_Y + 2 * (DESKTOP_ICON_SIZE + DESKTOP_ICON_SPACING)),
            ("Help", DESKTOP_ICON_START_X, DESKTOP_ICON_START_Y + 3 * (DESKTOP_ICON_SIZE + DESKTOP_ICON_SPACING)),
        ];
        
        for (i, (name, x, y)) in icon_apps.iter().enumerate() {
            if i < 8 {
                DESKTOP.icons[i] = DesktopIcon {
                    app_name: name,
                    x: *x,
                    y: *y,
                    hot: false,
                };
                DESKTOP.icon_count += 1;
            }
        }
    }
}

/// Draw the wallpaper
fn draw_wallpaper() {
    unsafe {
        let (width, height) = (DESKTOP.width, DESKTOP.height);
        
        // Create or reuse wallpaper surface
        if DESKTOP.wallpaper_surface.is_none() {
            let surface = crate::rust_gui_create_surface(width, height);
            DESKTOP.wallpaper_surface = Some(surface);
            
            if !surface.is_null() {
                // Render gradient wallpaper to surface
                let surface_ref = &*surface;
                let pixels = surface_ref.pixels as *mut u32;
                let stride = (surface_ref.pitch / 4) as i32;
                
                // Three-stop vertical gradient
                for y in 0..height {
                    let half = height / 2;
                    let from = if y < half { Color::from_rgb(25, 25, 35) } else { Color::from_rgb(35, 35, 45) };
                    let to = if y < half { Color::from_rgb(35, 35, 45) } else { Color::from_rgb(20, 20, 30) };
                    
                    let t = if y < half { (y as u32 * 255) / half as u32 } else { ((y - half) as u32 * 255) / (height - half) as u32 };
                    
                    let fr = from.red() as u32;
                    let fg = from.green() as u32;
                    let fb = from.blue() as u32;
                    let tr = to.red() as u32;
                    let tg = to.green() as u32;
                    let tb = to.blue() as u32;
                    
                    let r = fr + ((tr - fr) * t) / 255;
                    let g = fg + ((tg - fg) * t) / 255;
                    let b = fb + ((tb - fb) * t) / 255;
                    
                    let color = Color::from_rgb(r as u8, g as u8, b as u8);
                    
                    let row = pixels.offset((y * stride) as isize);
                    for x in 0..width {
                        *row.offset(x as isize) = color.0;
                    }
                }
                
                // Radial accent glow
                let cx = width / 2;
                let cy = height / 3;
                let rad = if width < height { width } else { height };
                let rad2 = (rad * rad) as u32;
                
                for y in 0..height {
                    let dy = y - cy;
                    let dy2 = (dy * dy) as u32;
                    let row = pixels.offset((y * stride) as isize);
                    
                    for x in 0..width {
                        let dx = x - cx;
                        let d2 = dy2 + (dx * dx) as u32;
                        if d2 > rad2 { continue; }
                        
                        let t = (255 - (d2 * 255 / rad2)) / 6;
                        let current = Color(*row.offset(x as isize));
                        
                        let r = current.red() as u32 + (((100 - current.red() as u32) * t) / 255);
                        let g = current.green() as u32 + (((150 - current.green() as u32) * t) / 255);
                        let b = current.blue() as u32 + (((200 - current.blue() as u32) * t) / 255);
                        
                        *row.offset(x as isize) = Color::from_rgb(r as u8, g as u8, b as u8).0;
                    }
                }
            }
        }
        
        // Blit wallpaper to framebuffer
        if let Some(surface) = DESKTOP.wallpaper_surface {
            blit_surface(&*surface, 0, 0);
        }
    }
}

/// Draw the taskbar
fn draw_taskbar() {
    unsafe {
        let width = DESKTOP.width;
        let height = DESKTOP.height;
        let taskbar_h = DESKTOP.taskbar_height;
        let taskbar_y = height - taskbar_h;
        
        // Taskbar background
        let taskbar_rect = Rect::new(0, taskbar_y, width, taskbar_h);
        fill_rect(&taskbar_rect, Color::from_rgb(30, 30, 35));
        
        // Top border of taskbar
        draw_rect(&Rect::new(0, taskbar_y, width, 1), Color::from_rgb(60, 60, 70), 1);
        
        // Start button
        let start_rect = Rect::new(4, taskbar_y + 4, DESKTOP.start_button_width, taskbar_h - 8);
        let start_color = if DESKTOP.start_hover || DESKTOP.menu_open {
            Color::from_rgb(60, 100, 140)
        } else {
            Color::from_rgb(45, 45, 55)
        };
        fill_rounded_rect(&start_rect, 6, start_color);
        
        // Start button text
        font_bold.draw_text(start_rect.x + 10, start_rect.y + 4, "Start", Color::from_rgb(255, 255, 255));
        
        // Task buttons (window list)
        let mut task_x = DESKTOP.start_button_width + 16;
        let task_btn_w = 160;
        let task_btn_h = taskbar_h - 8;
        
        for i in 0..crate::window_count() {
            if let Some(window) = crate::get_window(i) {
                if !window.visible { continue; }
                
                let task_rect = Rect::new(task_x, taskbar_y + 4, task_btn_w, task_btn_h);
                let is_hovered = DESKTOP.task_hover == i as i32;
                let is_focused = window.focused;
                
                let task_color = if is_focused {
                    Color::from_rgb(80, 120, 180)
                } else if is_hovered {
                    Color::from_rgb(55, 55, 65)
                } else {
                    Color::from_rgb(40, 40, 50)
                };
                fill_rounded_rect(&task_rect, 4, task_color);
                
                // Window title (truncated)
                let title_len = window.title.iter().position(|&b| b == 0).unwrap_or(64);
                let title_str = core::str::from_utf8(&window.title[..title_len]).unwrap_or("Untitled");
                font_ui.draw_text(task_rect.x + 8, task_rect.y + 4, title_str, Color::from_rgb(220, 220, 220));
                
                task_x += task_btn_w + 4;
            }
        }
        
        // Clock (right side)
        update_clock();
        let clock_str = core::str::from_utf8(&DESKTOP.clock_buffer).unwrap_or("00:00");
        let clock_width = font_ui.text_width(clock_str);
        let clock_x = width - clock_width - 20;
        let clock_y = taskbar_y + 4;
        font_ui.draw_text(clock_x, clock_y, clock_str, Color::from_rgb(200, 200, 200));
    }
}

/// Update clock buffer
fn update_clock() {
    unsafe {
        // In a real OS, we'd get time from RTC
        // For now, use a simple counter
        let ticks = timer_get_ticks() / 100; // Assuming 100Hz timer
        let hours = (ticks / 3600) % 24;
        let minutes = (ticks / 60) % 60;
        let seconds = ticks % 60;
        
        // Manual format without alloc
        let h1 = (hours / 10) as u8 + b'0';
        let h2 = (hours % 10) as u8 + b'0';
        let m1 = (minutes / 10) as u8 + b'0';
        let m2 = (minutes % 10) as u8 + b'0';
        let s1 = (seconds / 10) as u8 + b'0';
        let s2 = (seconds % 10) as u8 + b'0';
        
        DESKTOP.clock_buffer[0] = h1;
        DESKTOP.clock_buffer[1] = h2;
        DESKTOP.clock_buffer[2] = b':';
        DESKTOP.clock_buffer[3] = m1;
        DESKTOP.clock_buffer[4] = m2;
        DESKTOP.clock_buffer[5] = b':';
        DESKTOP.clock_buffer[6] = s1;
        DESKTOP.clock_buffer[7] = s2;
        DESKTOP.clock_buffer[8] = 0;
        DESKTOP.last_clock_update = ticks;
    }
}

/// Draw the start menu
fn draw_start_menu() {
    unsafe {
        if !DESKTOP.menu_open { return; }
        
        let width = DESKTOP.width;
        let height = DESKTOP.height;
        let taskbar_h = DESKTOP.taskbar_height;
        let menu_w = 280;
        let menu_h = 350;
        let menu_x = 4;
        let menu_y = height - taskbar_h - menu_h;
        
        // Menu background
        let menu_rect = Rect::new(menu_x, menu_y, menu_w, menu_h);
        fill_rounded_rect(&menu_rect, 8, Color::from_rgb(35, 35, 40));
        draw_rect(&menu_rect, Color::from_rgb(80, 80, 90), 1);
        
        // Menu items
        let items = [
            ("Terminal", "Open shell"),
            ("Files", "Browse disks"),
            ("Settings", "System settings"),
            ("Help", "Keys and mouse"),
            ("About", "System info"),
            ("", ""), // Separator
            ("Shutdown", "Power off"),
            ("Reboot", "Restart"),
        ];
        
        for (i, (name, desc)) in items.iter().enumerate() {
            let item_y = menu_y + 8 + (i as i32) * 40;
            let item_rect = Rect::new(menu_x + 4, item_y, menu_w - 8, 36);
            
            let is_hovered = DESKTOP.menu_hover == i as i32;
            
            if is_hovered {
                fill_rounded_rect(&item_rect, 4, Color::from_rgb(55, 55, 65));
            }
            
            if !name.is_empty() {
                font_ui.draw_text(item_rect.x + 10, item_rect.y + 2, name, Color::from_rgb(255, 255, 255));
                font_ui.draw_text(item_rect.x + 10, item_rect.y + 18, desc, Color::from_rgb(160, 160, 160));
            } else {
                // Separator line
                draw_rect(&Rect::new(menu_x + 20, item_y + 18, menu_w - 40, 1), Color::from_rgb(60, 60, 70), 1);
            }
        }
    }
}

/// Draw desktop icons
fn draw_desktop_icons() {
    unsafe {
        for i in 0..DESKTOP.icon_count {
            let icon = DESKTOP.icons[i];
            let icon_rect = Rect::new(icon.x, icon.y, DESKTOP_ICON_SIZE, DESKTOP_ICON_SIZE);
            
            // Icon background (hover effect)
            if icon.hot {
                let bg_rect = Rect::new(icon.x - 4, icon.y - 4, DESKTOP_ICON_SIZE + 8, DESKTOP_ICON_SIZE + 24);
                fill_rounded_rect(&bg_rect, 8, Color::from_rgb(50, 50, 60));
            }
            
            // Icon image (placeholder - colored square)
            let icon_color = match icon.app_name {
                "Terminal" => Color::from_rgb(100, 180, 100),
                "Files" => Color::from_rgb(100, 150, 220),
                "System" => Color::from_rgb(220, 150, 100),
                "Help" => Color::from_rgb(200, 200, 100),
                "About" => Color::from_rgb(180, 100, 180),
                _ => Color::from_rgb(150, 150, 150),
            };
            fill_rounded_rect(&Rect::new(icon.x + 8, icon.y + 8, DESKTOP_ICON_SIZE - 16, DESKTOP_ICON_SIZE - 16), 8, icon_color);
            
            // Icon label
            let label_y = icon.y + DESKTOP_ICON_SIZE + 2;
            let label_width = font_ui.text_width(icon.app_name);
            let label_x = icon.x + (DESKTOP_ICON_SIZE - label_width) / 2;
            font_ui.draw_text(label_x, label_y, icon.app_name, Color::from_rgb(255, 255, 255));
        }
    }
}

/// Compose the full desktop frame
pub fn compose_desktop() {
    draw_wallpaper();
    draw_desktop_icons();
    draw_taskbar();
    draw_start_menu();
    
    // Draw C windows on top
    crate::compose_frame();
}

/// Handle mouse move for desktop shell
pub fn desktop_handle_mouse_move(x: i32, y: i32) {
    unsafe {
        // Check start button hover
        let height = DESKTOP.height;
        let taskbar_h = DESKTOP.taskbar_height;
        let taskbar_y = height - taskbar_h;
        let start_rect = Rect::new(4, taskbar_y + 4, DESKTOP.start_button_width, taskbar_h - 8);
        
        DESKTOP.start_hover = x >= start_rect.x && x < start_rect.x + start_rect.w
            && y >= start_rect.y && y < start_rect.y + start_rect.h;
        
        // Check task buttons hover
        let mut task_x = DESKTOP.start_button_width + 16;
        let task_btn_w = 160;
        DESKTOP.task_hover = -1;
        
        for i in 0..crate::window_count() {
            if let Some(window) = crate::get_window(i) {
                if !window.visible { continue; }
                let task_rect = Rect::new(task_x, taskbar_y + 4, task_btn_w, taskbar_h - 8);
                if x >= task_rect.x && x < task_rect.x + task_rect.w
                    && y >= task_rect.y && y < task_rect.y + task_rect.h {
                    DESKTOP.task_hover = i as i32;
                    break;
                }
                task_x += task_btn_w + 4;
            }
        }
        
        // Check menu items hover
        if DESKTOP.menu_open {
            let menu_w = 280;
            let menu_h = 350;
            let menu_x = 4;
            let menu_y = height - taskbar_h - menu_h;
            
            DESKTOP.menu_hover = -1;
            for i in 0..8 {
                let item_y = menu_y + 8 + i * 40;
                let item_rect = Rect::new(menu_x + 4, item_y, menu_w - 8, 36);
                if x >= item_rect.x && x < item_rect.x + item_rect.w
                    && y >= item_rect.y && y < item_rect.y + item_rect.h {
                    DESKTOP.menu_hover = i as i32;
                    break;
                }
            }
        }
        
        // Check icon hover
        for i in 0..DESKTOP.icon_count {
            let icon = DESKTOP.icons[i];
            let icon_rect = Rect::new(icon.x - 4, icon.y - 4, DESKTOP_ICON_SIZE + 8, DESKTOP_ICON_SIZE + 24);
            DESKTOP.icons[i].hot = x >= icon_rect.x && x < icon_rect.x + icon_rect.w
                && y >= icon_rect.y && y < icon_rect.y + icon_rect.h;
        }
    }
}

/// Handle mouse click for desktop shell
pub fn desktop_handle_mouse_click(x: i32, y: i32, button: u8) {
    unsafe {
        if button != 1 { return; } // Left click only
        
        let height = DESKTOP.height;
        let taskbar_h = DESKTOP.taskbar_height;
        let taskbar_y = height - taskbar_h;
        
        // Start button click
        let start_rect = Rect::new(4, taskbar_y + 4, DESKTOP.start_button_width, taskbar_h - 8);
        if x >= start_rect.x && x < start_rect.x + start_rect.w
            && y >= start_rect.y && y < start_rect.y + start_rect.h {
            DESKTOP.menu_open = !DESKTOP.menu_open;
            return;
        }
        
        // Task button click
        let mut task_x = DESKTOP.start_button_width + 16;
        let task_btn_w = 160;
        
        for i in 0..crate::window_count() {
            if let Some(window) = crate::get_window(i) {
                if !window.visible { continue; }
                let task_rect = Rect::new(task_x, taskbar_y + 4, task_btn_w, taskbar_h - 8);
                if x >= task_rect.x && x < task_rect.x + task_rect.w
                    && y >= task_rect.y && y < task_rect.y + task_rect.h {
                    crate::set_focus(window.id);
                    return;
                }
                task_x += task_btn_w + 4;
            }
        }
        
        // Menu item click
        if DESKTOP.menu_open {
            let menu_w = 280;
            let menu_h = 350;
            let menu_x = 4;
            let menu_y = height - taskbar_h - menu_h;
            
            for i in 0..8 {
                let item_y = menu_y + 8 + (i as i32) * 40;
                let item_rect = Rect::new(menu_x + 4, item_y, menu_w - 8, 36);
                if x >= item_rect.x && x < item_rect.x + item_rect.w
                    && y >= item_rect.y && y < item_rect.y + item_rect.h {
                    
                    match i {
                        0 => { crate::rust_gui_create_window(b"Terminal\0".as_ptr(), 100, 100, 800, 600); }
                        1 => { crate::rust_gui_create_window(b"Files\0".as_ptr(), 200, 100, 800, 600); }
                        2 => { crate::rust_gui_create_window(b"System\0".as_ptr(), 150, 150, 600, 400); }
                        3 => { crate::rust_gui_create_window(b"Help\0".as_ptr(), 200, 200, 500, 400); }
                        4 => { crate::rust_gui_create_window(b"About\0".as_ptr(), 300, 150, 500, 350); }
                        6 => unsafe { acpi_shutdown(); }
                        7 => unsafe { acpi_reboot(); }
                        _ => {}
                    }
                    DESKTOP.menu_open = false;
                    return;
                }
            }
            
            // Click outside menu closes it
            DESKTOP.menu_open = false;
        }
        
        // Icon click
        for i in 0..DESKTOP.icon_count {
            let icon = DESKTOP.icons[i];
            let icon_rect = Rect::new(icon.x - 4, icon.y - 4, DESKTOP_ICON_SIZE + 8, DESKTOP_ICON_SIZE + 24);
            if x >= icon_rect.x && x < icon_rect.x + icon_rect.w
                && y >= icon_rect.y && y < icon_rect.y + icon_rect.h {
                
                match icon.app_name {
                    "Terminal" => { crate::rust_gui_create_window(b"Terminal\0".as_ptr(), 100, 100, 800, 600); }
                    "Files" => { crate::rust_gui_create_window(b"Files\0".as_ptr(), 200, 100, 800, 600); }
                    "System" => { crate::rust_gui_create_window(b"System\0".as_ptr(), 150, 150, 600, 400); }
                    "Help" => { crate::rust_gui_create_window(b"Help\0".as_ptr(), 200, 200, 500, 400); }
                    "About" => { crate::rust_gui_create_window(b"About\0".as_ptr(), 300, 150, 500, 350); }
                    _ => {}
                }
                return;
            }
        }
    }
}

/// Handle keyboard input for desktop shell
pub fn desktop_handle_key(key: u8, pressed: bool) {
    unsafe {
        if pressed && key == 0x5B { // Left Meta/Win key
            DESKTOP.menu_open = !DESKTOP.menu_open;
        }
        if pressed && key == 0x01 && DESKTOP.menu_open { // Escape
            DESKTOP.menu_open = false;
        }
    }
}

// FFI exports
#[no_mangle]
pub extern "C" fn rust_gui_init_desktop(width: i32, height: i32) {
    init_desktop_shell(width, height);
}

#[no_mangle]
pub extern "C" fn rust_gui_compose_desktop() {
    compose_desktop();
}

#[no_mangle]
pub extern "C" fn rust_gui_desktop_mouse_move(x: i32, y: i32) {
    desktop_handle_mouse_move(x, y);
}

#[no_mangle]
pub extern "C" fn rust_gui_desktop_mouse_click(x: i32, y: i32, button: u8) {
    desktop_handle_mouse_click(x, y, button);
}

#[no_mangle]
pub extern "C" fn rust_gui_desktop_key(key: u8, pressed: bool) {
    desktop_handle_key(key, pressed);
}