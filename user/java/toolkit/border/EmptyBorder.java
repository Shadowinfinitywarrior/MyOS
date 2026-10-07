package toolkit.border;

import toolkit.Border;
import toolkit.Component;
import toolkit.Graphics;
import toolkit.Insets;

/**
 * Empty border implementation.
 */
public class EmptyBorder implements Border {
    private int top, left, bottom, right;
    
    public EmptyBorder(int top, int left, int bottom, int right) {
        this.top = top; this.left = left; this.bottom = bottom; this.right = right;
    }
    
    public Insets getBorderInsets(Component c) {
        return new Insets(top, left, bottom, right);
    }
    
    public boolean isBorderOpaque() { return false; }
    public void paintBorder(Component c, Graphics g, int x, int y, int width, int height) {}
}