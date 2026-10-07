package toolkit.event;

import java.util.EventListener;

/**
 * Key listener interface.
 */
public interface KeyListener extends EventListener {
    void keyTyped(KeyEvent e);
    void keyPressed(KeyEvent e);
    void keyReleased(KeyEvent e);
}