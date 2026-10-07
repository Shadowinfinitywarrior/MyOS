package toolkit.event;

import toolkit.Widget;

/**
 * Focus event for focus changes.
 */
public class FocusEvent extends java.util.EventObject {
    public static final int FOCUS_GAINED = 1;
    public static final int FOCUS_LOST = 2;
    
    private boolean temporary;
    private int id;
    
    public FocusEvent(Widget source, int id) {
        this(source, id, false);
    }
    
    public FocusEvent(Widget source, int id, boolean temporary) {
        super(source);
        this.id = id;
        this.temporary = temporary;
    }
    
    public boolean isTemporary() { return temporary; }
    public int getID() { return id; }
}