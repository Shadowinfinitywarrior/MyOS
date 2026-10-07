package toolkit.event;

import toolkit.Widget;

/**
 * Action event for button clicks and menu selections.
 */
public class ActionEvent extends java.util.EventObject {
    public static final int ACTION_PERFORMED = 1;
    
    private String actionCommand;
    private int modifiers;
    private long when;
    
    public ActionEvent(Object source, int id, String command) {
        this(source, id, command, 0, System.currentTimeMillis());
    }
    
    public ActionEvent(Object source, int id, String command, int modifiers, long when) {
        super(source);
        this.actionCommand = command;
        this.modifiers = modifiers;
        this.when = when;
    }
    
    public String getActionCommand() { return actionCommand; }
    public int getModifiers() { return modifiers; }
    public long getWhen() { return when; }
    public int getID() { return ACTION_PERFORMED; }
    public String paramString() { return "cmd=" + actionCommand; }
}