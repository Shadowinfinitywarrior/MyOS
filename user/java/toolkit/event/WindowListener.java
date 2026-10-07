package toolkit.event;

import java.util.EventListener;

/**
 * Window listener interface.
 */
public interface WindowListener extends EventListener {
    void windowOpened(WindowEvent e);
    void windowClosing(WindowEvent e);
    void windowClosed(WindowEvent e);
    void windowIconified(WindowEvent e);
    void windowDeiconified(WindowEvent e);
    void windowActivated(WindowEvent e);
    void windowDeactivated(WindowEvent e);
    void windowMaximized(WindowEvent e);
    void windowMinimized(WindowEvent e);
    void windowRestored(WindowEvent e);
}