package toolkit;

/**
 * Font metrics for text measurement.
 */
public class FontMetrics {
    private Font font;
    private int ascent;
    private int descent;
    private int leading;
    private int height;
    private int maxAdvance;
    
    public FontMetrics(Font font) {
        this.font = font;
        this.ascent = font.getSize();
        this.descent = font.getSize() / 4;
        this.leading = font.getSize() / 8;
        this.height = ascent + descent + leading;
        this.maxAdvance = font.getSize();
    }
    
    public Font getFont() { return font; }
    public int getAscent() { return ascent; }
    public int getDescent() { return descent; }
    public int getLeading() { return leading; }
    public int getHeight() { return height; }
    public int getMaxAdvance() { return maxAdvance; }
    public int stringWidth(String str) {
        // Approximate width
        return str.length() * maxAdvance / 2;
    }
    public int charWidth(char ch) { return maxAdvance / 2; }
    public int charsWidth(char[] data, int off, int len) { return len * maxAdvance / 2; }
}