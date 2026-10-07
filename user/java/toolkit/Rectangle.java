package toolkit;

/**
 * Rectangle class.
 */
public class Rectangle {
    public int x, y, width, height;
    
    public Rectangle() { this(0, 0, 0, 0); }
    public Rectangle(int x, int y, int width, int height) {
        this.x = x;
        this.y = y;
        this.width = width;
        this.height = height;
    }
    public Rectangle(Rectangle r) { this(r.x, r.y, r.width, r.height); }
    
    public int getX() { return x; }
    public int getY() { return y; }
    public int getWidth() { return width; }
    public int getHeight() { return height; }
    public void setBounds(int x, int y, int width, int height) { this.x = x; this.y = y; this.width = width; this.height = height; }
    public void setLocation(int x, int y) { this.x = x; this.y = y; }
    public void setSize(int width, int height) { this.width = width; this.height = height; }
    public void setSize(Dimension d) { this.width = d.width; this.height = d.height; }
    public void translate(int dx, int dy) { x += dx; y += dy; }
    public boolean contains(int x, int y) { return x >= this.x && x < this.x + width && y >= this.y && y < this.y + height; }
    public boolean contains(Point p) { return contains(p.x, p.y); }
    public boolean contains(Rectangle r) { return contains(r.x, r.y) && contains(r.x + r.width, r.y + r.height); }
    public boolean intersects(Rectangle r) {
        return x < r.x + r.width && x + width > r.x && y < r.y + r.height && y + height > r.y;
    }
    public Rectangle intersection(Rectangle r) {
        int x1 = Math.max(x, r.x);
        int y1 = Math.max(y, r.y);
        int x2 = Math.min(x + width, r.x + r.width);
        int y2 = Math.min(y + height, r.y + r.height);
        if (x2 > x1 && y2 > y1) return new Rectangle(x1, y1, x2 - x1, y2 - y1);
        return new Rectangle(0, 0, 0, 0);
    }
    public Rectangle union(Rectangle r) {
        int x1 = Math.min(x, r.x);
        int y1 = Math.min(y, r.y);
        int x2 = Math.max(x + width, r.x + r.width);
        int y2 = Math.max(y + height, r.y + r.height);
        return new Rectangle(x1, y1, x2 - x1, y2 - y1);
    }
    public boolean equals(Object obj) {
        if (obj instanceof Rectangle) {
            Rectangle r = (Rectangle) obj;
            return x == r.x && y == r.y && width == r.width && height == r.height;
        }
        return false;
    }
    public String toString() { return "Rectangle[x=" + x + ",y=" + y + ",w=" + width + ",h=" + height + "]"; }
}

class Point {
    public int x, y;
    public Point() { this(0, 0); }
    public Point(int x, int y) { this.x = x; this.y = y; }
}