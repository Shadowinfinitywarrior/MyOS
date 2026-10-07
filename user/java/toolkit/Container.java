package toolkit;

import toolkit.event.*;
import java.util.ArrayList;
import java.util.List;

/**
 * A widget that can contain other widgets.
 * Manages layout and event delegation to children.
 */
public class Container extends Widget {
    protected List<Widget> children = new ArrayList<>();
    protected LayoutManager layoutManager;
    protected Widget focusedWidget;
    protected Insets insets = new Insets(0, 0, 0, 0);
    
    public Container() {
        super();
        setLayout(new FlowLayout());
    }
    
    // Layout management
    public void setLayout(LayoutManager layoutManager) {
        this.layoutManager = layoutManager;
        if (layoutManager != null) {
            layoutManager.setContainer(this);
        }
    }
    
    public LayoutManager getLayout() { return layoutManager; }
    
    // Child management
    public void add(Widget child) {
        children.add(child);
        child.setParent(this);
        if (layoutManager != null) {
            layoutManager.addLayoutComponent(child);
        }
        revalidate();
    }
    
    public void add(Widget child, Object constraints) {
        children.add(child);
        child.setParent(this);
        if (layoutManager != null) {
            layoutManager.addLayoutComponent(child, constraints);
        }
        revalidate();
    }
    
    public void remove(Widget child) {
        children.remove(child);
        child.setParent(null);
        if (layoutManager != null) {
            layoutManager.removeLayoutComponent(child);
        }
        if (focusedWidget == child) {
            focusedWidget = null;
        }
        revalidate();
    }
    
    public void removeAll() {
        for (Widget child : children) {
            child.setParent(null);
        }
        children.clear();
        if (layoutManager != null) {
            // Layout manager will be notified on next layout
        }
        focusedWidget = null;
        revalidate();
    }
    
    public Widget[] getComponents() {
        return children.toArray(new Widget[0]);
    }
    
    public int getComponentCount() {
        return children.size();
    }
    
    public Widget getComponent(int index) {
        return children.get(index);
    }
    
    // Insets
    public void setInsets(Insets insets) {
        this.insets = insets;
        revalidate();
    }
    
    public Insets getInsets() {
        return insets;
    }
    
    // Layout
    public void revalidate() {
        if (layoutManager != null) {
            layoutManager.layoutContainer(this);
        }
        repaint();
    }
    
    public void doLayout() {
        if (layoutManager != null) {
            layoutManager.layoutContainer(this);
        }
    }
    
    // Focus management
    public void setFocusedWidget(Widget widget) {
        if (focusedWidget != null && focusedWidget != widget) {
            focusedWidget.fireFocusEvent(new FocusEvent(focusedWidget, FocusEvent.FOCUS_LOST));
        }
        focusedWidget = widget;
        if (focusedWidget != null) {
            focusedWidget.fireFocusEvent(new FocusEvent(focusedWidget, FocusEvent.FOCUS_GAINED));
        }
    }
    
    public Widget getFocusedWidget() {
        return focusedWidget;
    }
    
    // Event processing - delegate to children
    public void processMouseEvent(MouseEvent e) {
        // Find child at event location
        for (int i = children.size() - 1; i >= 0; i--) {
            Widget child = children.get(i);
            if (child.isVisible() && child.contains(e.getX() - child.getX(), e.getY() - child.getY())) {
                // Translate event coordinates to child's coordinate system
                MouseEvent translated = new MouseEvent(
                    child, e.getID(), e.getWhen(), e.getModifiersEx(),
                    e.getX() - child.getX(), e.getY() - child.getY(),
                    e.getXOnScreen(), e.getYOnScreen(),
                    e.getClickCount(), e.isPopupTrigger(), e.getButton()
                );
                child.fireMouseEvent(translated);
                
                // Also process recursively if child is a container
                if (child instanceof Container) {
                    ((Container) child).processMouseEvent(translated);
                }
                break; // Only topmost child gets the event
            }
        }
        
        // Also fire on this container
        fireMouseEvent(e);
    }
    
    public void processKeyEvent(KeyEvent e) {
        if (focusedWidget != null) {
            focusedWidget.fireKeyEvent(e);
        }
        fireKeyEvent(e);
    }
    
    // Painting
    @Override
    public void paint(Graphics g) {
        // Paint background
        if (background != null) {
            g.setColor(background);
            g.fillRect(0, 0, width, height);
        }
        
        // Paint border if needed
        if (isFocusable() && this == getRootContainer().getFocusedWidget()) {
            g.setColor(Color.BLUE);
            g.drawRect(0, 0, width - 1, height - 1);
        }
        
        // Paint children
        Graphics childGraphics = g.create();
        try {
            for (Widget child : children) {
                if (child.isVisible()) {
                    childGraphics.translate(child.getX(), child.getY());
                    child.paint(childGraphics);
                    childGraphics.translate(-child.getX(), -child.getY());
                }
            }
        } finally {
            childGraphics.dispose();
        }
    }
    
    // Preferred size
    public Dimension getPreferredSize() {
        if (layoutManager != null) {
            return layoutManager.preferredLayoutSize(this);
        }
        // Default: union of children bounds
        int maxX = 0, maxY = 0;
        for (Widget child : children) {
            if (child.isVisible()) {
                maxX = Math.max(maxX, child.getX() + child.getWidth());
                maxY = Math.max(maxY, child.getY() + child.getHeight());
            }
        }
        return new Dimension(maxX + insets.right, maxY + insets.bottom);
    }
    
    public Dimension getMinimumSize() {
        if (layoutManager != null) {
            return layoutManager.minimumLayoutSize(this);
        }
        return getPreferredSize();
    }
    
    // Helper to find root container
    private Container getRootContainer() {
        Container current = this;
        while (current.getParent() instanceof Container) {
            current = (Container) current.getParent();
        }
        return current;
    }
}