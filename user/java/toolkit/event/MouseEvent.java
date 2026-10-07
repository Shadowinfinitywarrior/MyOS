package toolkit.event;

import toolkit.Widget;

/**
 * Mouse event for mouse interactions.
 */
public class MouseEvent extends java.util.EventObject {
    public static final int MOUSE_CLICKED = 1;
    public static final int MOUSE_PRESSED = 2;
    public static final int MOUSE_RELEASED = 3;
    public static final int MOUSE_ENTERED = 4;
    public static final int MOUSE_EXITED = 5;
    public static final int MOUSE_DRAGGED = 6;
    public static final int MOUSE_MOVED = 7;
    
    public static final int BUTTON1 = 1;
    public static final int BUTTON2 = 2;
    public static final int BUTTON3 = 3;
    
    private int x, y;
    private int xOnScreen, yOnScreen;
    private int clickCount;
    private boolean popupTrigger;
    private int button;
    private int modifiersEx;
    private long when;
    private int id;
    
    public MouseEvent(Widget source, int id, long when, int modifiersEx,
                      int x, int y, int xOnScreen, int yOnScreen,
                      int clickCount, boolean popupTrigger, int button) {
        super(source);
        this.id = id;
        this.when = when;
        this.modifiersEx = modifiersEx;
        this.x = x;
        this.y = y;
        this.xOnScreen = xOnScreen;
        this.yOnScreen = yOnScreen;
        this.clickCount = clickCount;
        this.popupTrigger = popupTrigger;
        this.button = button;
    }
    
    public int getX() { return x; }
    public int getY() { return y; }
    public int getXOnScreen() { return xOnScreen; }
    public int getYOnScreen() { return yOnScreen; }
    public int getClickCount() { return clickCount; }
    public boolean isPopupTrigger() { return popupTrigger; }
    public int getButton() { return button; }
    public int getModifiersEx() { return modifiersEx; }
    public long getWhen() { return when; }
    public int getID() { return id; }
}