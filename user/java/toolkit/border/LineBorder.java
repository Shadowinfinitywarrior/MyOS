package toolkit.border;

import toolkit.Border;
import toolkit.Component;
import toolkit.Graphics;
import toolkit.Color;
import toolkit.Insets;

/**
 * Line border implementation.
 */
public class LineBorder implements Border {
    private Color color;
    private int thickness;
    
    public LineBorder(Color color) { this(color, 1); }
    public LineBorder(Color color, int thickness) {
        this.color = color;
        this.thickness = thickness;
    }
    
    public Insets getBorderInsets(Component c) {
        return new Insets(thickness, thickness, thickness, thickness);
    }
    
    public boolean isBorderOpaque() { return true; }
    
    public void paintBorder(Component c, Graphics g, int x, int y, int width, int height) {
        g.setColor(color);
        for (int i = 0; i < thickness; i++) {
            g.drawRect(x + i, y + i, width - 1 - 2*i, height - 1 - 2*i);
        }
    }
}