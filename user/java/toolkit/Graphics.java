package toolkit;

/**
 * Graphics class for drawing operations.
 */
public class Graphics {
    protected Color color = Color.BLACK;
    protected Font font = new Font("SansSerif", Font.PLAIN, 12);
    protected Rectangle clipRect = null;
    protected int translateX = 0;
    protected int translateY = 0;
    
    public Graphics() {}
    
    // Color
    public Color getColor() { return color; }
    public void setColor(Color c) { this.color = c; }
    
    // Font
    public Font getFont() { return font; }
    public void setFont(Font font) { this.font = font; }
    public FontMetrics getFontMetrics() { return new FontMetrics(font); }
    public FontMetrics getFontMetrics(Font f) { return new FontMetrics(f); }
    
    // Translation
    public void translate(int x, int y) {
        translateX += x;
        translateY += y;
    }
    
    // Clipping
    public void clipRect(int x, int y, int width, int height) {
        Rectangle r = new Rectangle(x + translateX, y + translateY, width, height);
        if (clipRect == null) {
            clipRect = r;
        } else {
            clipRect = clipRect.intersection(r);
        }
    }
    
    public void setClip(int x, int y, int width, int height) {
        clipRect = new Rectangle(x + translateX, y + translateY, width, height);
    }
    
    public Rectangle getClipBounds() { return clipRect; }
    public Rectangle getClip() { return clipRect; }
    
    // Drawing
    public void drawRect(int x, int y, int width, int height) {
        // Implementation would draw rectangle outline
    }
    
    public void fillRect(int x, int y, int width, int height) {
        // Implementation would fill rectangle
    }
    
    public void drawRoundRect(int x, int y, int width, int height, int arcWidth, int arcHeight) {
        // Implementation would draw rounded rectangle
    }
    
    public void fillRoundRect(int x, int y, int width, int height, int arcWidth, int arcHeight) {
        // Implementation would fill rounded rectangle
    }
    
    public void drawLine(int x1, int y1, int x2, int y2) {
        // Implementation would draw line
    }
    
    public void drawString(String str, int x, int y) {
        // Implementation would draw string
    }
    
    public void drawString(String str, int x, int y, int length) {
        // Implementation would draw string
    }
    
    public void drawChars(char[] data, int offset, int length, int x, int y) {
        // Implementation would draw chars
    }
    
    public void clearRect(int x, int y, int width, int height) {
        // Implementation would clear rectangle
    }
    
    public void copyArea(int x, int y, int width, int height, int dx, int dy) {
        // Implementation would copy area
    }
    
    public Graphics create() {
        Graphics g = new Graphics();
        g.color = this.color;
        g.font = this.font;
        g.clipRect = this.clipRect;
        g.translateX = this.translateX;
        g.translateY = this.translateY;
        return g;
    }
    
    public Graphics create(int x, int y, int width, int height) {
        Graphics g = create();
        g.translate(x, y);
        g.clipRect(x, y, width, height);
        return g;
    }
    
    public void dispose() {
        // Cleanup
    }
}