package toolkit;

/**
 * Font class.
 */
public class Font {
    public static final int PLAIN = 0;
    public static final int BOLD = 1;
    public static final int ITALIC = 2;
    public static final int BOLD_ITALIC = 3;
    
    private String name;
    private int style;
    private int size;
    
    public Font(String name, int style, int size) {
        this.name = name;
        this.style = style;
        this.size = size;
    }
    
    public String getName() { return name; }
    public int getStyle() { return style; }
    public int getSize() { return size; }
    public boolean isBold() { return (style & BOLD) != 0; }
    public boolean isItalic() { return (style & ITALIC) != 0; }
    public boolean isPlain() { return style == PLAIN; }
    public Font deriveFont(int style) { return new Font(name, style, size); }
    public Font deriveFont(float size) { return new Font(name, style, (int)size); }
    public Font deriveFont(int style, float size) { return new Font(name, style, (int)size); }
    public String toString() { return "Font[" + name + "," + style + "," + size + "]"; }
}