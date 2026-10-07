package toolkit.event;

import java.util.EventListener;

/**
 * Focus listener interface.
 */
public interface FocusListener extends EventListener {
    void focusGained(FocusEvent e);
    void focusLost(FocusEvent e);
}