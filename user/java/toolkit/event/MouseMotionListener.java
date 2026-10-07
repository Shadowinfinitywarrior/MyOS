package toolkit.event;

import toolkit.Widget;

/**
 * Mouse motion listener interface.
 */
public interface MouseMotionListener extends java.util.EventListener {
    void mouseDragged(MouseEvent e);
    void mouseMoved(MouseEvent e);
}