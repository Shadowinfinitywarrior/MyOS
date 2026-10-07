package toolkit.border;

import toolkit.Border;
import toolkit.Component;
import toolkit.Graphics;
import toolkit.Color;
import toolkit.Insets;

/**
 * Etched border implementation.
 */
public class EtchedBorder implements Border {
    public Insets getBorderInsets(Component c) { return new Insets(2, 2, 2, 2); }
    public boolean isBorderOpaque() { return true; }
    
    public void paintBorder(Component c, Graphics g, int x, int y, int width, int height) {
        g.setColor(c.getBackground().brighter());
        g.drawLine(x, y + height - 1, x + width - 1, y + height - 1);
        g.drawLine(x + width - 1, y, x + width - 1, y + height - 1);
        g.setColor(c.getBackground().darker());
        g.drawLine(x, y, x + width - 1, y);
        g.drawLine(x, y, x, y + height - 1);
    }
}