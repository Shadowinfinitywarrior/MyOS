package toolkit.event;

import toolkit.Window;

/**
 * Window event for window state changes.
 */
public class WindowEvent extends java.util.EventObject {
    public static final int WINDOW_OPENED = 1;
    public static final int WINDOW_CLOSING = 2;
    public static final int WINDOW_CLOSED = 3;
    public static final int WINDOW_ICONIFIED = 4;
    public static final int WINDOW_DEICONIFIED = 5;
    public static final int WINDOW_ACTIVATED = 6;
    public static final int WINDOW_DEACTIVATED = 7;
    public static final int WINDOW_MAXIMIZED = 8;
    public static final int WINDOW_MINIMIZED = 9;
    public static final int WINDOW_RESTORED = 10;
    
    private int id;
    
    public WindowEvent(Window source, int id) {
        super(source);
        this.id = id;
    }
    
    public int getID() { return id; }
    public Window getWindow() { return (Window) getSource(); }
}