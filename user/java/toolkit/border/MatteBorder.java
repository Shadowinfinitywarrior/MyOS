package toolkit.border;

import toolkit.Border;
import toolkit.Component;
import toolkit.Graphics;
import toolkit.Color;
import toolkit.Insets;

/**
 * Matte border implementation.
 */
public class MatteBorder implements Border {
    private int top, left, bottom, right;
    private Color color;
    
    public MatteBorder(int top, int left, int bottom, int right, Color color) {
        this.top = top;
        this.left = left;
        this.bottom = bottom;
        this.right = right;
        this.color = color;
    }
    
    public Insets getBorderInsets(Component c) {
        return new Insets(top, left, bottom, right);
    }
    
    public boolean isBorderOpaque() { return true; }
    
    public void paintBorder(Component c, Graphics g, int x, int y, int width, int height) {
        g.setColor(color);
        if (top > 0) g.fillRect(x, y, width, top);
        if (left > 0) g.fillRect(x, y, left, height);
        if (bottom > 0) g.fillRect(x, y + height - bottom, width, bottom);
        if (right > 0) g.fillRect(x + width - right, y, right, height);
    }
}