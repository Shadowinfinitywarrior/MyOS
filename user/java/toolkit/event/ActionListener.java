package toolkit.event;

import java.util.EventListener;

/**
 * Action listener interface for button clicks and menu selections.
 */
public interface ActionListener extends EventListener {
    void actionPerformed(ActionEvent e);
}