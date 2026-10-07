package toolkit.event;

import java.util.EventListener;

/**
 * Mouse listener interface.
 */
public interface MouseListener extends EventListener {
    void mouseClicked(MouseEvent e);
    void mousePressed(MouseEvent e);
    void mouseReleased(MouseEvent e);
    void mouseEntered(MouseEvent e);
    void mouseExited(MouseEvent e);
}