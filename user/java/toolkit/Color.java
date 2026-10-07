package toolkit;

/**
 * Color class for RGB colors.
 */
public class Color {
    public final int r, g, b, a;
    
    // Predefined colors
    public static final Color WHITE = new Color(255, 255, 255);
    public static final Color BLACK = new Color(0, 0, 0);
    public static final Color RED = new Color(255, 0, 0);
    public static final Color GREEN = new Color(0, 255, 0);
    public static final Color BLUE = new Color(0, 0, 255);
    public static final Color GRAY = new Color(128, 128, 128);
    public static final Color LIGHT_GRAY = new Color(192, 192, 192);
    public static final Color DARK_GRAY = new Color(64, 64, 64);
    public static final Color YELLOW = new Color(255, 255, 0);
    public static final Color CYAN = new Color(0, 255, 255);
    public static final Color MAGENTA = new Color(255, 0, 255);
    public static final Color ORANGE = new Color(255, 165, 0);
    public static final Color PINK = new Color(255, 192, 203);
    
    public Color(int r, int g, int b) { this(r, g, b, 255); }
    public Color(int r, int g, int b, int a) {
        this.r = clamp(r);
        this.g = clamp(g);
        this.b = clamp(b);
        this.a = clamp(a);
    }
    
    public Color(int rgb) {
        this((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF, (rgb >> 24) & 0xFF);
    }
    
    private static int clamp(int v) { return Math.max(0, Math.min(255, v)); }
    
    public int getRed() { return r; }
    public int getGreen() { return g; }
    public int getBlue() { return b; }
    public int getAlpha() { return a; }
    public int getRGB() { return (a << 24) | (r << 16) | (g << 8) | b; }
    
    public Color brighter() {
        return new Color(Math.min(255, (int)(r * 1.2)), Math.min(255, (int)(g * 1.2)), Math.min(255, (int)(b * 1.2)), a);
    }
    
    public Color darker() {
        return new Color(Math.max(0, (int)(r * 0.7)), Math.max(0, (int)(g * 0.7)), Math.max(0, (int)(b * 0.7)), a);
    }
    
    public boolean equals(Object obj) {
        if (obj instanceof Color) {
            Color c = (Color) obj;
            return r == c.r && g == c.g && b == c.b && a == c.a;
        }
        return false;
    }
    
    public String toString() { return "Color[r=" + r + ",g=" + g + ",b=" + b + ",a=" + a + "]"; }
}