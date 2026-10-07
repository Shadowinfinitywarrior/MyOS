package toolkit.event;

import toolkit.Widget;

/**
 * Key event for keyboard interactions.
 */
public class KeyEvent extends java.util.EventObject {
    public static final int KEY_TYPED = 1;
    public static final int KEY_PRESSED = 2;
    public static final int KEY_RELEASED = 3;
    
    // Common key codes
    public static final int VK_ENTER = 10;
    public static final int VK_ESCAPE = 27;
    public static final int VK_SPACE = 32;
    public static final int VK_LEFT = 37;
    public static final int VK_UP = 38;
    public static final int VK_RIGHT = 39;
    public static final int VK_DOWN = 40;
    public static final int VK_TAB = 9;
    public static final int VK_BACKSPACE = 8;
    public static final int VK_DELETE = 127;
    public static final int VK_INSERT = 126;
    public static final int VK_HOME = 100;
    public static final int VK_END = 101;
    public static final int VK_PAGE_UP = 102;
    public static final int VK_PAGE_DOWN = 103;
    public static final int VK_F1 = 112;
    public static final int VK_F2 = 113;
    public static final int VK_F3 = 114;
    public static final int VK_F4 = 115;
    public static final int VK_F5 = 116;
    public static final int VK_F6 = 117;
    public static final int VK_F7 = 118;
    public static final int VK_F8 = 119;
    public static final int VK_F9 = 120;
    public static final int VK_F10 = 121;
    public static final int VK_F11 = 122;
    public static final int VK_F12 = 123;
    
    public static final char CHAR_UNDEFINED = (char) 0xFFFF;
    
    private int keyCode;
    private char keyChar;
    private int modifiersEx;
    private long when;
    private int id;
    
    public KeyEvent(Widget source, int id, long when, int modifiersEx, int keyCode, char keyChar) {
        super(source);
        this.id = id;
        this.when = when;
        this.modifiersEx = modifiersEx;
        this.keyCode = keyCode;
        this.keyChar = keyChar;
    }
    
    public int getKeyCode() { return keyCode; }
    public char getKeyChar() { return keyChar; }
    public int getModifiersEx() { return modifiersEx; }
    public long getWhen() { return when; }
    public int getID() { return id; }
    public boolean isActionKey() { return false; }
}