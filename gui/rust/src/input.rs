//! Input handling for the GUI system
//! Provides keyboard and mouse event queue with C FFI integration

use crate::WindowId;
use core::sync::atomic::{AtomicU32, Ordering};

/// Maximum number of events in the queue
const EVENT_QUEUE_SIZE: usize = 256;

/// Keyboard event types
#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum KeyEventType {
    Down = 1,
    Up = 2,
    Repeat = 3,
}

/// Mouse event types
#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MouseEventType {
    Move = 4,
    ButtonDown = 5,
    ButtonUp = 6,
    ScrollV = 7,
    ScrollH = 8,
}

/// Keyboard event
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct KeyEvent {
    pub event_type: u8,
    pub scancode: u8,
    pub ascii: u8,
    pub modifiers: u32,
}

/// Mouse event
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct MouseEvent {
    pub event_type: u8,
    pub x: i32,
    pub y: i32,
    pub button: u8,
    pub delta: i32,
}

/// Unified input event
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub enum InputEvent {
    Keyboard(KeyEvent),
    Mouse(MouseEvent),
}

/// Event queue entry
#[repr(C)]
#[derive(Clone, Copy)]
union EventEntry {
    key: KeyEvent,
    mouse: MouseEvent,
    raw: [u8; 24], // Max size of either event
}

/// Circular event queue
struct EventQueue {
    buffer: [EventEntry; EVENT_QUEUE_SIZE],
    head: AtomicU32,
    tail: AtomicU32,
    is_keyboard: [bool; EVENT_QUEUE_SIZE], // Track event type
}

static mut EVENT_QUEUE: EventQueue = EventQueue {
    buffer: [EventEntry { raw: [0; 24] }; EVENT_QUEUE_SIZE],
    head: AtomicU32::new(0),
    tail: AtomicU32::new(0),
    is_keyboard: [false; EVENT_QUEUE_SIZE],
};

/// Mouse cursor position
static mut MOUSE_X: i32 = 0;
static mut MOUSE_Y: i32 = 0;

/// Initialize the input system
pub fn init_input() {
    unsafe {
        EVENT_QUEUE.head.store(0, Ordering::Relaxed);
        EVENT_QUEUE.tail.store(0, Ordering::Relaxed);
        MOUSE_X = 0;
        MOUSE_Y = 0;
    }
}

/// Check if there are pending events
pub fn has_events() -> bool {
    unsafe {
        let head = EVENT_QUEUE.head.load(Ordering::Acquire);
        let tail = EVENT_QUEUE.tail.load(Ordering::Acquire);
        head != tail
    }
}

/// Get the next event from the queue
pub fn get_event() -> Option<InputEvent> {
    unsafe {
        let tail = EVENT_QUEUE.tail.load(Ordering::Acquire);
        let head = EVENT_QUEUE.head.load(Ordering::Acquire);
        
        if head == tail {
            return None;
        }
        
        let is_key = EVENT_QUEUE.is_keyboard[tail as usize];
        let event = if is_key {
            InputEvent::Keyboard(EVENT_QUEUE.buffer[tail as usize].key)
        } else {
            InputEvent::Mouse(EVENT_QUEUE.buffer[tail as usize].mouse)
        };
        
        EVENT_QUEUE.tail.store((tail + 1) % EVENT_QUEUE_SIZE as u32, Ordering::Release);
        Some(event)
    }
}

/// Process all pending events and dispatch to windows
pub fn process_events() {
    while let Some(event) = get_event() {
        match event {
            InputEvent::Keyboard(key) => handle_key_event(key),
            InputEvent::Mouse(mouse) => handle_mouse_event(mouse),
        }
    }
}

/// Handle keyboard event - route to focused window
fn handle_key_event(event: KeyEvent) {
    // Get focused window and send event
    if let Some(window) = crate::get_focused() {
        // In a full implementation, this would send to the window's event handler
        // For now, we just track the event
        let _ = (window, event);
    }
}

/// Handle mouse event - hit test and route to appropriate window
fn handle_mouse_event(event: MouseEvent) {
    unsafe {
        MOUSE_X = event.x;
        MOUSE_Y = event.y;
    }
    
    // Hit test to find window under cursor
    if let Some(window_id) = crate::hit_test(event.x, event.y) {
        if event.event_type == MouseEventType::ButtonDown as u8 {
            // Set focus on click
            crate::set_focus(window_id);
        }
        
        // In a full implementation, send event to window
        if let Some(window) = crate::find_window(window_id) {
            let _ = (window, event);
        }
    } else {
        // Click on desktop - could close menus, etc.
        if event.event_type == MouseEventType::ButtonDown as u8 {
            // Unfocus all windows if clicking on desktop background
            // crate::set_focus(0); // Would need a way to unfocus
        }
    }
}

/// Get current mouse position
pub fn get_mouse_position() -> (i32, i32) {
    unsafe { (MOUSE_X, MOUSE_Y) }
}

// ========================================================================
// FFI exports for C drivers
// ========================================================================

/// Push a keyboard event from C driver
/// event_type: 1=down, 2=up, 3=repeat
#[no_mangle]
pub extern "C" fn rust_gui_push_key_event(
    event_type: u8,
    scancode: u8,
    ascii: u8,
    modifiers: u32,
) {
    unsafe {
        let head = EVENT_QUEUE.head.load(Ordering::Relaxed);
        let next_head = (head + 1) % EVENT_QUEUE_SIZE as u32;
        let tail = EVENT_QUEUE.tail.load(Ordering::Relaxed);
        
        // Check for overflow
        if next_head == tail {
            return; // Queue full, drop event
        }
        
        let entry = &mut EVENT_QUEUE.buffer[head as usize];
        entry.key = KeyEvent {
            event_type,
            scancode,
            ascii,
            modifiers,
        };
        EVENT_QUEUE.is_keyboard[head as usize] = true;
        EVENT_QUEUE.head.store(next_head, Ordering::Release);
    }
}

/// Push a mouse event from C driver
/// event_type: 4=move, 5=down, 6=up, 7=scroll_v, 8=scroll_h
#[no_mangle]
pub extern "C" fn rust_gui_push_mouse_event(
    event_type: u8,
    x: i32,
    y: i32,
    button: u8,
    delta: i32,
) {
    unsafe {
        let head = EVENT_QUEUE.head.load(Ordering::Relaxed);
        let next_head = (head + 1) % EVENT_QUEUE_SIZE as u32;
        let tail = EVENT_QUEUE.tail.load(Ordering::Relaxed);
        
        // Check for overflow
        if next_head == tail {
            return; // Queue full, drop event
        }
        
        let entry = &mut EVENT_QUEUE.buffer[head as usize];
        entry.mouse = MouseEvent {
            event_type,
            x,
            y,
            button,
            delta,
        };
        EVENT_QUEUE.is_keyboard[head as usize] = false;
        EVENT_QUEUE.head.store(next_head, Ordering::Release);
    }
}

/// Get current mouse position (for C code)
#[no_mangle]
pub extern "C" fn rust_gui_get_mouse_position(x: *mut i32, y: *mut i32) {
    unsafe {
        if !x.is_null() { *x = MOUSE_X; }
        if !y.is_null() { *y = MOUSE_Y; }
    }
}

/// Check if a key is currently pressed (for C code)
#[no_mangle]
pub extern "C" fn rust_gui_is_key_pressed(scancode: u8) -> bool {
    // In a full implementation, we'd track key state
    // For now, return false
    let _ = scancode;
    false
}