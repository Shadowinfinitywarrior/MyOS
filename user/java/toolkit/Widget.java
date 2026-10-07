package toolkit;

import toolkit.event.*;
import java.util.ArrayList;
import java.util.List;

/**
 * Base class for all UI widgets.
 * Provides common functionality like bounds, visibility, event handling.
 */
public abstract class Widget {
    protected int x, y, width, height;
    protected boolean visible = true;
    protected boolean enabled = true;
    protected Color background;
    protected Color foreground;
    protected Font font;
    protected Container parent;
    protected List<MouseListener> mouseListeners = new ArrayList<>();
    protected List<KeyListener> keyListeners = new ArrayList<>();
    protected List<FocusListener> focusListeners = new ArrayList<>();
    protected Cursor cursor = Cursor.getDefaultCursor();
    protected boolean focused = false;
    
    public Widget() {
        this.font = new Font("SansSerif", Font.PLAIN, 14);
    }
    
    // Bounds management
    public void setBounds(int x, int y, int width, int height) {
        this.x = x;
        this.y = y;
        this.width = width;
        this.height = height;
    }
    
    public void setLocation(int x, int y) {
        this.x = x;
        this.y = y;
    }
    
    public void setSize(int width, int height) {
        this.width = width;
        this.height = height;
    }
    
    public Rectangle getBounds() {
        return new Rectangle(x, y, width, height);
    }
    
    public int getX() { return x; }
    public int getY() { return y; }
    public int getWidth() { return width; }
    public int getHeight() { return height; }
    
    // Visibility
    public void setVisible(boolean visible) {
        this.visible = visible;
        repaint();
    }
    
    public boolean isVisible() { return visible; }
    
    // Enabled state
    public void setEnabled(boolean enabled) {
        this.enabled = enabled;
        repaint();
    }
    
    public boolean isEnabled() { return enabled; }
    
    // Colors
    public void setBackground(Color bg) {
        this.background = bg;
        repaint();
    }
    
    public Color getBackground() { return background; }
    
    public void setForeground(Color fg) {
        this.foreground = fg;
        repaint();
    }
    
    public Color getForeground() { return foreground; }
    
    // Font
    public void setFont(Font font) {
        this.font = font;
        repaint();
    }
    
    public Font getFont() { return font; }
    
    public FontMetrics getFontMetrics(Font f) {
        return new FontMetrics(f);
    }
    
    public FontMetrics getFontMetrics() {
        return new FontMetrics(font);
    }
    
    // Focus
    public boolean isFocused() { return focused; }
    public void setFocused(boolean focused) { this.focused = focused; }
    
    // Action command (for buttons)
    protected String actionCommand;
    
    public void setActionCommand(String cmd) {
        this.actionCommand = cmd;
    }
    
    public String getActionCommand() {
        return actionCommand;
    }
    
    // Tool tip text
    protected String toolTipText;
    
    public void setToolTipText(String text) {
        this.toolTipText = text;
    }
    
    public String getToolTipText() {
        return toolTipText;
    }
    
    // Border width (for buttons)
    protected int borderWidth = 1;
    
    public void setBorderWidth(int width) {
        this.borderWidth = width;
        repaint();
    }
    
    public int getBorderWidth() {
        return borderWidth;
    }
    
    // Parent container
    public void setParent(Container parent) {
        this.parent = parent;
    }
    
    public Container getParent() { return parent; }
    
    // Cursor
    public void setCursor(Cursor cursor) {
        this.cursor = cursor;
    }
    
    public Cursor getCursor() { return cursor; }
    
    // Event listeners
    public void addMouseListener(MouseListener listener) {
        mouseListeners.add(listener);
    }
    
    public void removeMouseListener(MouseListener listener) {
        mouseListeners.remove(listener);
    }
    
    public void addKeyListener(KeyListener listener) {
        keyListeners.add(listener);
    }
    
    public void removeKeyListener(KeyListener listener) {
        keyListeners.remove(listener);
    }
    
    public void addFocusListener(FocusListener listener) {
        focusListeners.add(listener);
    }
    
    public void removeFocusListener(FocusListener listener) {
        focusListeners.remove(listener);
    }
    
    // Event firing
    protected void fireMouseEvent(MouseEvent e) {
        for (MouseListener listener : mouseListeners) {
            switch (e.getID()) {
                case MouseEvent.MOUSE_CLICKED -> listener.mouseClicked(e);
                case MouseEvent.MOUSE_PRESSED -> listener.mousePressed(e);
                case MouseEvent.MOUSE_RELEASED -> listener.mouseReleased(e);
                case MouseEvent.MOUSE_ENTERED -> listener.mouseEntered(e);
                case MouseEvent.MOUSE_EXITED -> listener.mouseExited(e);
            }
        }
    }
    
    protected void fireKeyEvent(KeyEvent e) {
        for (KeyListener listener : keyListeners) {
            switch (e.getID()) {
                case KeyEvent.KEY_TYPED -> listener.keyTyped(e);
                case KeyEvent.KEY_PRESSED -> listener.keyPressed(e);
                case KeyEvent.KEY_RELEASED -> listener.keyReleased(e);
            }
        }
    }
    
    protected void fireFocusEvent(FocusEvent e) {
        for (FocusListener listener : focusListeners) {
            if (e.getID() == FocusEvent.FOCUS_GAINED) {
                listener.focusGained(e);
            } else if (e.getID() == FocusEvent.FOCUS_LOST) {
                listener.focusLost(e);
            }
        }
    }
    
    // Painting
    public abstract void paint(Graphics g);
    
    public void repaint() {
        if (parent != null) {
            parent.repaint();
        }
    }
    
    // Hit testing
    public boolean contains(int x, int y) {
        return x >= this.x && x < this.x + width && y >= this.y && y < this.y + height;
    }
    
    // Focus
    public void requestFocus() {
        if (parent != null) {
            parent.setFocusedWidget(this);
        }
    }
    
    // Preferred size
    public Dimension getPreferredSize() {
        return new Dimension(width, height);
    }
    
    public void setPreferredSize(Dimension d) {
        setSize(d.width, d.height);
    }
    
    public Dimension getMinimumSize() {
        return getPreferredSize();
    }
    
    public Dimension getMaximumSize() {
        return getPreferredSize();
    }
    
    // Border support
    protected Border border;
    
    public void setBorder(Border border) {
        this.border = border;
        repaint();
    }
    
    public Border getBorder() { return border; }
    
    // Focusable
    protected boolean focusable = true;
    
    public void setFocusable(boolean focusable) {
        this.focusable = focusable;
    }
    
    public boolean isFocusable() {
        return focusable && enabled && visible;
    }
    
    // Dispose
    public void dispose() {
        if (parent != null) {
            parent.remove(this);
        }
    }
}