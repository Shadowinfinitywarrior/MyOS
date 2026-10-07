package toolkit;

import toolkit.event.*;
import java.util.ArrayList;
import java.util.List;

/**
 * Top-level window with title bar, borders, and window controls.
 * Can be shown as a modal or non-modal dialog.
 */
public class Window extends Container {
    protected String title = "";
    protected boolean resizable = true;
    protected boolean closable = true;
    protected boolean minimizable = true;
    protected boolean maximizable = true;
    protected int titleBarHeight = 28;
    protected int borderWidth = 1;
    protected Color titleBarColor = new Color(60, 60, 70);
    protected Color titleBarTextColor = Color.WHITE;
    protected Color borderColor = new Color(100, 100, 100);
    
    // Window state
    protected boolean visible = false;
    protected boolean focused = false;
    protected boolean minimized = false;
    protected boolean maximized = false;
    protected Rectangle restoreBounds;
    
    // Window controls
    protected Button closeButton;
    protected Button minimizeButton;
    protected Button maximizeButton;
    
    // Dragging state
    protected boolean dragging = false;
    protected int dragStartX, dragStartY;
    protected int dragOffsetX, dragOffsetY;
    
    // Window listeners
    protected List<WindowListener> windowListeners = new ArrayList<>();
    
    // Native window handle (for GraalVM native integration)
    protected long nativeHandle = 0;
    
    public Window() {
        this("");
    }
    
    public Window(String title) {
        super();
        this.title = title;
        setLayout(new BorderLayout());
        initWindowControls();
    }
    
    private void initWindowControls() {
        // Create window control buttons
        closeButton = new Button("×");
        closeButton.setBounds(0, 0, 24, titleBarHeight);
        closeButton.setBackground(new Color(200, 60, 60));
        closeButton.setForeground(Color.WHITE);
        closeButton.addMouseListener(new MouseAdapter() {
            @Override
            public void mouseClicked(MouseEvent e) {
                dispose();
            }
        });
        
        minimizeButton = new Button("−");
        minimizeButton.setBounds(0, 0, 24, titleBarHeight);
        minimizeButton.addMouseListener(new MouseAdapter() {
            @Override
            public void mouseClicked(MouseEvent e) {
                setMinimized(true);
            }
        });
        
        maximizeButton = new Button("□");
        maximizeButton.setBounds(0, 0, 24, titleBarHeight);
        maximizeButton.addMouseListener(new MouseAdapter() {
            @Override
            public void mouseClicked(MouseEvent e) {
                setMaximized(!maximized);
            }
        });
    }
    
    // Title
    public void setTitle(String title) {
        this.title = title;
        repaint();
    }
    
    public String getTitle() { return title; }
    
    // Window properties
    public void setResizable(boolean resizable) { this.resizable = resizable; }
    public boolean isResizable() { return resizable; }
    
    public void setClosable(boolean closable) { this.closable = closable; }
    public boolean isClosable() { return closable; }
    
    public void setMinimizable(boolean minimizable) { this.minimizable = minimizable; }
    public boolean isMinimizable() { return minimizable; }
    
    public void setMaximizable(boolean maximizable) { this.maximizable = maximizable; }
    public boolean isMaximizable() { return maximizable; }
    
    // Window state
    public void setVisible(boolean visible) {
        if (this.visible != visible) {
            this.visible = visible;
            if (visible) {
                fireWindowEvent(WindowEvent.WINDOW_OPENED);
                // Create native window
                createNativeWindow();
            } else {
                fireWindowEvent(WindowEvent.WINDOW_CLOSED);
                // Destroy native window
                destroyNativeWindow();
            }
            repaint();
        }
    }
    
    @Override
    public boolean isVisible() { return visible; }
    
    public void setFocused(boolean focused) {
        this.focused = focused;
        repaint();
    }
    
    public boolean isFocused() { return focused; }
    
    public void setMinimized(boolean minimized) {
        if (this.minimized != minimized) {
            this.minimized = minimized;
            if (minimized) {
                restoreBounds = getBounds();
                setSize(200, titleBarHeight);
                fireWindowEvent(WindowEvent.WINDOW_ICONIFIED);
            } else if (restoreBounds != null) {
                setBounds(restoreBounds.x, restoreBounds.y, restoreBounds.width, restoreBounds.height);
                fireWindowEvent(WindowEvent.WINDOW_DEICONIFIED);
            }
            repaint();
        }
    }
    
    public boolean isMinimized() { return minimized; }
    
    public void setMaximized(boolean maximized) {
        if (this.maximized != maximized) {
            this.maximized = maximized;
            if (maximized) {
                restoreBounds = getBounds();
                // Would maximize to screen size in real implementation
                fireWindowEvent(WindowEvent.WINDOW_MAXIMIZED);
            } else if (restoreBounds != null) {
                setBounds(restoreBounds.x, restoreBounds.y, restoreBounds.width, restoreBounds.height);
            }
            repaint();
        }
    }
    
    public boolean isMaximized() { return maximized; }
    
    // Native window integration (for GraalVM native-image)
    private native void createNativeWindow();
    private native void destroyNativeWindow();
    private native void updateNativeWindow();
    
    // Window listeners
    public void addWindowListener(WindowListener listener) {
        windowListeners.add(listener);
    }
    
    public void removeWindowListener(WindowListener listener) {
        windowListeners.remove(listener);
    }
    
    protected void fireWindowEvent(int eventType) {
        WindowEvent e = new WindowEvent(this, eventType);
        for (WindowListener listener : windowListeners) {
            switch (eventType) {
                case WindowEvent.WINDOW_OPENED -> listener.windowOpened(e);
                case WindowEvent.WINDOW_CLOSING -> listener.windowClosing(e);
                case WindowEvent.WINDOW_CLOSED -> listener.windowClosed(e);
                case WindowEvent.WINDOW_ICONIFIED -> listener.windowIconified(e);
                case WindowEvent.WINDOW_DEICONIFIED -> listener.windowDeiconified(e);
                case WindowEvent.WINDOW_ACTIVATED -> listener.windowActivated(e);
                case WindowEvent.WINDOW_DEACTIVATED -> listener.windowDeactivated(e);
                case WindowEvent.WINDOW_MAXIMIZED -> listener.windowMaximized(e);
                case WindowEvent.WINDOW_MINIMIZED -> listener.windowMinimized(e);
                case WindowEvent.WINDOW_RESTORED -> listener.windowRestored(e);
            }
        }
    }
    
    // Close/dispose
    public void dispose() {
        fireWindowEvent(WindowEvent.WINDOW_CLOSING);
        setVisible(false);
        fireWindowEvent(WindowEvent.WINDOW_CLOSED);
        // Remove from parent if any
        if (parent != null) {
            parent.remove(this);
        }
    }
    
    // Pack window to preferred size
    public void pack() {
        Dimension pref = getPreferredSize();
        // Add title bar height and borders
        pref.height += titleBarHeight + borderWidth * 2;
        pref.width += borderWidth * 2;
        setSize(pref.width, pref.height);
    }
    
    // Center on screen
    public void centerOnScreen() {
        // Would get screen size from native in real implementation
        int screenWidth = 1024;
        int screenHeight = 768;
        setLocation((screenWidth - width) / 2, (screenHeight - height) / 2);
    }
    
    // Show as modal dialog
    public void showDialog() {
        setVisible(true);
        // In a real implementation, this would block until window is closed
    }
    
    // Painting
    @Override
    public void paint(Graphics g) {
        if (!visible) return;
        
        // Draw window frame
        drawFrame(g);
        
        // Draw title bar
        drawTitleBar(g);
        
        // Draw client area background
        g.setColor(background != null ? background : Color.WHITE);
        g.fillRect(borderWidth, titleBarHeight + borderWidth,
                  width - borderWidth * 2, height - titleBarHeight - borderWidth * 2);
        
        // Paint children (client area)
        Graphics clientGraphics = g.create();
        try {
            clientGraphics.translate(borderWidth, titleBarHeight + borderWidth);
            clientGraphics.clipRect(0, 0, width - borderWidth * 2, height - titleBarHeight - borderWidth * 2);
            for (Widget child : children) {
                if (child.isVisible()) {
                    clientGraphics.translate(child.getX(), child.getY());
                    child.paint(clientGraphics);
                    clientGraphics.translate(-child.getX(), -child.getY());
                }
            }
        } finally {
            clientGraphics.dispose();
        }
        
        // Draw window control buttons
        drawWindowControls(g);
    }
    
    protected void drawFrame(Graphics g) {
        g.setColor(borderColor);
        g.drawRect(0, 0, width - 1, height - 1);
        if (borderWidth > 1) {
            g.drawRect(1, 1, width - 3, height - 3);
        }
    }
    
    protected void drawTitleBar(Graphics g) {
        // Title bar background
        g.setColor(focused ? titleBarColor : titleBarColor.darker());
        g.fillRect(borderWidth, borderWidth, width - borderWidth * 2, titleBarHeight);
        
        // Title text
        g.setColor(titleBarTextColor);
        g.setFont(font != null ? font : new Font("SansSerif", Font.BOLD, 12));
        FontMetrics fm = g.getFontMetrics();
        int textX = 10;
        int textY = borderWidth + (titleBarHeight + fm.getAscent()) / 2 - 2;
        g.drawString(title, textX, textY);
    }
    
    protected void drawWindowControls(Graphics g) {
        int buttonX = width - borderWidth - 24;
        int buttonY = borderWidth;
        
        // Close button
        if (closable) {
            closeButton.setBounds(buttonX, buttonY, 24, titleBarHeight);
            closeButton.paint(g);
            buttonX -= 24;
        }
        
        // Maximize button
        if (maximizable) {
            maximizeButton.setBounds(buttonX, buttonY, 24, titleBarHeight);
            maximizeButton.paint(g);
            buttonX -= 24;
        }
        
        // Minimize button
        if (minimizable) {
            minimizeButton.setBounds(buttonX, buttonY, 24, titleBarHeight);
            minimizeButton.paint(g);
        }
    }
    
    // Process mouse events for window dragging and controls
    @Override
    public void processMouseEvent(MouseEvent e) {
        // Check if clicking on title bar for dragging
        if (e.getID() == MouseEvent.MOUSE_PRESSED) {
            if (e.getY() >= borderWidth && e.getY() < borderWidth + titleBarHeight) {
                // Check if clicking on window controls
                int buttonX = width - borderWidth - 24;
                if (closable && e.getX() >= buttonX && e.getX() < buttonX + 24) {
                    closeButton.processMouseEvent(e);
                    return;
                }
                buttonX -= 24;
                if (maximizable && e.getX() >= buttonX && e.getX() < buttonX + 24) {
                    maximizeButton.processMouseEvent(e);
                    return;
                }
                buttonX -= 24;
                if (minimizable && e.getX() >= buttonX && e.getX() < buttonX + 24) {
                    minimizeButton.processMouseEvent(e);
                    return;
                }
                
                // Start dragging
                dragging = true;
                dragStartX = e.getXOnScreen();
                dragStartY = e.getYOnScreen();
                dragOffsetX = x;
                dragOffsetY = y;
                return;
            }
        }
        
        if (e.getID() == MouseEvent.MOUSE_DRAGGED && dragging) {
            int newX = dragOffsetX + (e.getXOnScreen() - dragStartX);
            int newY = dragOffsetY + (e.getYOnScreen() - dragStartY);
            setLocation(newX, newY);
            return;
        }
        
        if (e.getID() == MouseEvent.MOUSE_RELEASED) {
            dragging = false;
        }
        
        // Delegate to children for client area clicks
        if (e.getY() >= borderWidth + titleBarHeight) {
            MouseEvent clientEvent = new MouseEvent(
                this, e.getID(), e.getWhen(), e.getModifiersEx(),
                e.getX() - borderWidth, e.getY() - borderWidth - titleBarHeight,
                e.getXOnScreen(), e.getYOnScreen(),
                e.getClickCount(), e.isPopupTrigger(), e.getButton()
            );
            super.processMouseEvent(clientEvent);
        }
    }
    
    // Native methods for GraalVM integration
    static {
        // Native library would be loaded here for GraalVM native-image
        // System.loadLibrary("myos_gui");
    }
}