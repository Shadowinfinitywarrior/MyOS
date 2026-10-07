package toolkit;

import java.awt.*;

/**
 * Interface for layout managers.
 * Defines how components are arranged within a container.
 */
public interface LayoutManager {
    /**
     * Sets the container this layout manager manages.
     */
    void setContainer(Container container);
    
    /**
     * Adds a component to the layout.
     */
    void addLayoutComponent(Widget component);
    
    /**
     * Adds a component with constraints to the layout.
     */
    void addLayoutComponent(Widget component, Object constraints);
    
    /**
     * Removes a component from the layout.
     */
    void removeLayoutComponent(Widget component);
    
    /**
     * Lays out the container's components.
     */
    void layoutContainer(Container container);
    
    /**
     * Returns the preferred size of the container.
     */
    Dimension preferredLayoutSize(Container container);
    
    /**
     * Returns the minimum size of the container.
     */
    Dimension minimumLayoutSize(Container container);
}