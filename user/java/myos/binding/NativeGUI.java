package myos.binding;

/**
 * JNI-like bridge to the MyOS Rust GUI system.
 * This class provides native methods that map to Rust GUI FFI functions
 * via syscalls. When compiled with GraalVM native-image, the native
 * implementations call the appropriate kernel syscalls which forward
 * to the Rust GUI.
 */
public final class NativeGUI {

    // Window flags (must match include/rust_gui.h and gui/rust/src/lib.rs)
    public static final int WF_MINIMIZED = 1 << 0;
    public static final int WF_MAXIMIZED = 1 << 1;
    public static final int WF_RESIZING  = 1 << 2;
    public static final int WF_DRAGGING  = 1 << 3;
    public static final int WF_NO_DECOR  = 1 << 4;
    public static final int WF_MODAL     = 1 << 5;

    // Event types (must match gui/rust/src/input.rs)
    public static final byte EV_KEY_DOWN       = 1;
    public static final byte EV_KEY_UP         = 2;
    public static final byte EV_KEY_REPEAT     = 3;
    public static final byte EV_MOUSE_MOVE     = 4;
    public static final byte EV_MOUSE_DOWN     = 5;
    public static final byte EV_MOUSE_UP       = 6;
    public static final byte EV_SCROLL_VERTICAL = 7;
    public static final byte EV_SCROLL_HORIZONTAL = 8;

    // Mouse buttons
    public static final byte MOUSE_LEFT   = 1;
    public static final byte MOUSE_MIDDLE = 2;
    public static final byte MOUSE_RIGHT  = 3;

    // Key modifiers
    public static final int KMOD_SHIFT   = 1 << 0;
    public static final int KMOD_CTRL    = 1 << 1;
    public static final int KMOD_ALT     = 1 << 2;
    public static final int KMOD_META    = 1 << 3;

    // Key codes (subset)
    public static final int GUIKEY_ENTER      = 0x0D;
    public static final int GUIKEY_BACKSPACE  = 0x08;
    public static final int GUIKEY_TAB        = 0x09;
    public static final int GUIKEY_ESCAPE     = 0x1B;
    public static final int GUIKEY_UP         = 0x48;
    public static final int GUIKEY_DOWN       = 0x50;
    public static final int GUIKEY_LEFT       = 0x4B;
    public static final int GUIKEY_RIGHT      = 0x4D;
    public static final int GUIKEY_HOME       = 0x47;
    public static final int GUIKEY_END        = 0x4F;
    public static final int GUIKEY_PGUP       = 0x49;
    public static final int GUIKEY_PGDN       = 0x51;
    public static final int GUIKEY_DELETE     = 0x53;
    public static final int GUIKEY_INSERT     = 0x52;
    public static final int GUIKEY_F1         = 0x3B;
    public static final int GUIKEY_F2         = 0x3C;
    public static final int GUIKEY_F3         = 0x3D;
    public static final int GUIKEY_F4         = 0x3E;
    public static final int GUIKEY_F5         = 0x3F;
    public static final int GUIKEY_F6         = 0x40;
    public static final int GUIKEY_F7         = 0x41;
    public static final int GUIKEY_F8         = 0x42;
    public static final int GUIKEY_F9         = 0x43;
    public static final int GUIKEY_F10        = 0x44;
    public static final int GUIKEY_F11        = 0x45;
    public static final int GUIKEY_F12        = 0x46;
    public static final int GUIKEY_ALT        = 0x38;
    public static final int GUIKEY_ALT_GR     = 0xE038;
    public static final int GUIKEY_CTRL       = 0x1D;
    public static final int GUIKEY_SHIFT      = 0x2A;
    public static final int GUIKEY_META       = 0x5B; // Win/Super key
    public static final int GUIKEY_1          = 0x02;
    public static final int GUIKEY_2          = 0x03;
    public static final int GUIKEY_3          = 0x04;
    public static final int GUIKEY_4          = 0x05;
    public static final int GUIKEY_D          = 0x20;

    // Window structure (matches include/rust_gui.h window_t)
    public static class Window {
        public long id;
        public String title;
        public int x, y, w, h;
        public int clientX, clientY, clientW, clientH;
        public int flags;
        public boolean visible;
        public boolean focused;
    }

    // Rectangle structure
    public static class Rect {
        public int x, y, w, h;

        public Rect() {}
        public Rect(int x, int y, int w, int h) {
            this.x = x; this.y = y; this.w = w; this.h = h;
        }
    }

    private NativeGUI() {}

    // ============================================================
    // Native method declarations (implemented in C via syscalls)
    // ============================================================

    /**
     * Initialize the Rust GUI system.
     * @param width Screen width in pixels
     * @param height Screen height in pixels
     * @return 0 on success, negative on error
     */
    public static native int init(int width, int height);

    /**
     * Create a new window.
     * @param title Window title (UTF-8)
     * @param x Initial X position
     * @param y Initial Y position
     * @param w Window width
     * @param h Window height
     * @return Window ID (0 on error)
     */
    public static native long createWindow(String title, int x, int y, int w, int h);

    /**
     * Destroy a window.
     * @param id Window ID to destroy
     */
    public static native void destroyWindow(long id);

    /**
     * Invalidate a window (mark for redraw).
     * @param id Window ID to invalidate
     */
    public static native void invalidateWindow(long id);

    /**
     * Render a frame (compose and display all windows).
     * @return 0 on success, negative on error
     */
    public static native int renderFrame();

    /**
     * Set the framebuffer pointer (called from C kernel).
     * @param fb Pointer to framebuffer memory
     * @param width Framebuffer width
     * @param height Framebuffer height
     * @param pitch Framebuffer pitch (bytes per row)
     */
    public static native void setFramebuffer(long fb, int width, int height, int pitch);

    /**
     * Get the number of active windows.
     * @return Window count
     */
    public static native int windowCount();

    /**
     * Get a window by index.
     * @param index Window index (0-based)
     * @return Window object, or null if invalid
     */
    public static native Window getWindow(int index);

    /**
     * Focus a window.
     * @param id Window ID to focus
     * @return true on success
     */
    public static native boolean focusWindow(long id);

    /**
     * Set window title.
     * @param id Window ID
     * @param title New title
     */
    public static native void setWindowTitle(long id, String title);

    /**
     * Get window frame rectangle.
     * @param id Window ID
     * @param rect Output rectangle
     * @return true on success
     */
    public static native boolean getWindowRect(long id, Rect rect);

    /**
     * Set window frame rectangle (move/resize).
     * @param id Window ID
     * @param rect New rectangle
     * @return true on success
     */
    public static native boolean setWindowRect(long id, Rect rect);

    /**
     * Push a keyboard event to the Rust GUI event queue.
     * @param eventType Event type (EV_KEY_DOWN, EV_KEY_UP, EV_KEY_REPEAT)
     * @param scancode Hardware scancode
     * @param ascii ASCII character (0 if none)
     * @param modifiers Modifier flags (KMOD_SHIFT, KMOD_CTRL, etc.)
     */
    public static native void pushKeyEvent(byte eventType, byte scancode, byte ascii, int modifiers);

    /**
     * Push a mouse event to the Rust GUI event queue.
     * @param eventType Event type (EV_MOUSE_MOVE, EV_MOUSE_DOWN, EV_MOUSE_UP, EV_SCROLL_VERTICAL, EV_SCROLL_HORIZONTAL)
     * @param x Mouse X position
     * @param y Mouse Y position
     * @param button Button number (1=left, 2=middle, 3=right)
     * @param delta Scroll delta (for scroll events)
     */
    public static native void pushMouseEvent(byte eventType, int x, int y, byte button, int delta);

    // ============================================================
    // High-level Java API
    // ============================================================

    private static boolean initialized = false;
    private static int screenWidth = 0;
    private static int screenHeight = 0;

    /**
     * Initialize the GUI system. Safe to call multiple times.
     * @param width Screen width
     * @param height Screen height
     * @return true on success
     */
    public static synchronized boolean initialize(int width, int height) {
        if (initialized && screenWidth == width && screenHeight == height) {
            return true;
        }
        int result = init(width, height);
        if (result == 0) {
            initialized = true;
            screenWidth = width;
            screenHeight = height;
            return true;
        }
        return false;
    }

    /**
     * Create a window with Java-friendly API.
     * @param title Window title
     * @param x X position
     * @param y Y position
     * @param width Window width
     * @param height Window height
     * @return Window object, or null on failure
     */
    public static Window createWindowEx(String title, int x, int y, int width, int height) {
        long id = createWindow(title, x, y, width, height);
        if (id == 0) return null;
        Window w = new Window();
        w.id = id;
        w.title = title;
        w.x = x; w.y = y; w.w = width; w.h = height;
        w.visible = true; w.focused = true;
        return w;
    }

    /**
     * Main render loop - call once per frame.
     */
    public static void render() {
        renderFrame();
    }

    /**
     * Process pending input events. Call regularly.
     */
    public static void processEvents() {
        // Events are processed automatically in renderFrame()
    }

    /**
     * Get screen dimensions.
     * @param out Output array [width, height]
     */
    public static void getScreenSize(int[] out) {
        out[0] = screenWidth;
        out[1] = screenHeight;
    }

    // ============================================================
    // Input event helpers
    // ============================================================

    public static void onKeyDown(int scancode, char ascii, int modifiers) {
        pushKeyEvent(EV_KEY_DOWN, (byte) scancode, (byte) ascii, modifiers);
    }

    public static void onKeyUp(int scancode, int modifiers) {
        pushKeyEvent(EV_KEY_UP, (byte) scancode, (byte) 0, modifiers);
    }

    public static void onMouseMove(int x, int y) {
        pushMouseEvent(EV_MOUSE_MOVE, x, y, (byte) 0, 0);
    }

    public static void onMouseDown(int x, int y, int button) {
        pushMouseEvent(EV_MOUSE_DOWN, x, y, (byte) button, 0);
    }

    public static void onMouseUp(int x, int y, int button) {
        pushMouseEvent(EV_MOUSE_UP, x, y, (byte) button, 0);
    }

    public static void onScroll(int x, int y, int delta) {
        pushMouseEvent(EV_SCROLL_VERTICAL, x, y, (byte) 0, delta);
    }
}