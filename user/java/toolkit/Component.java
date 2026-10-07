package toolkit;

import toolkit.Graphics;

/**
 * Base class for all UI components.
 */
public class Component extends Widget {
    public Component() { super(); }
    
    @Override
    public void paint(Graphics g) {
        // Default: paint children if container
    }
}