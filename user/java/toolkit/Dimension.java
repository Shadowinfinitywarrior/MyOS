package toolkit;

import java.awt.Graphics2D;

/**
 * Dimension class for width/height.
 */
public class Dimension {
    public int width;
    public int height;
    
    public Dimension() { this(0, 0); }
    public Dimension(int width, int height) {
        this.width = width;
        this.height = height;
    }
    
    public int getWidth() { return width; }
    public int getHeight() { return height; }
    public void setSize(int width, int height) { this.width = width; this.height = height; }
    public void setSize(Dimension d) { this.width = d.width; this.height = d.height; }
    public boolean equals(Object obj) {
        if (obj instanceof Dimension) {
            Dimension d = (Dimension) obj;
            return width == d.width && height == d.height;
        }
        return false;
    }
    public String toString() { return "Dimension[" + width + "," + height + "]"; }
}