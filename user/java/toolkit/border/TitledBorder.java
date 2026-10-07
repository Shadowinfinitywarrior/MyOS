package toolkit.border;

import toolkit.Border;
import toolkit.Component;
import toolkit.Graphics;
import toolkit.Color;
import toolkit.Insets;

/**
 * Titled border implementation.
 */
public class TitledBorder implements Border {
    private String title;
    
    public TitledBorder(String title) { this.title = title; }
    
    public Insets getBorderInsets(Component c) { return new Insets(12, 5, 5, 5); }
    public boolean isBorderOpaque() { return false; }
    
    public void paintBorder(Component c, Graphics g, int x, int y, int width, int height) {
        g.setColor(Color.GRAY);
        g.drawRect(x, y + 6, width - 1, height - 7);
        g.setColor(Color.BLACK);
        g.drawString(title, x + 8, y + 12);
    }
}