package toolkit.event;

/**
 * Adapter class for WindowListener.
 */
public class WindowAdapter implements WindowListener {
    public void windowOpened(WindowEvent e) {}
    public void windowClosing(WindowEvent e) {}
    public void windowClosed(WindowEvent e) {}
    public void windowIconified(WindowEvent e) {}
    public void windowDeiconified(WindowEvent e) {}
    public void windowActivated(WindowEvent e) {}
    public void windowDeactivated(WindowEvent e) {}
    public void windowMaximized(WindowEvent e) {}
    public void windowMinimized(WindowEvent e) {}
    public void windowRestored(WindowEvent e) {}
}