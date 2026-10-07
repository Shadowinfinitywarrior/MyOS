package terminal;

import toolkit.Color;

/**
 * Terminal color management supporting 16-color, 256-color, and 24-bit true color modes.
 */
public class TerminalColors {
    // Standard 16 colors (VGA palette)
    private static final int[][] VGA_COLORS = {
        {0x00, 0x00, 0x00}, // 0: Black
        {0xAA, 0x00, 0x00}, // 1: Red
        {0x00, 0xAA, 0x00}, // 2: Green
        {0xAA, 0x55, 0x00}, // 3: Yellow/Brown
        {0x00, 0x00, 0xAA}, // 4: Blue
        {0xAA, 0x00, 0xAA}, // 5: Magenta
        {0x00, 0xAA, 0xAA}, // 6: Cyan
        {0xAA, 0xAA, 0xAA}, // 7: White
        {0x55, 0x55, 0x55}, // 8: Bright Black (Gray)
        {0xFF, 0x55, 0x55}, // 9: Bright Red
        {0x55, 0xFF, 0x55}, // 10: Bright Green
        {0xFF, 0xFF, 0x55}, // 11: Bright Yellow
        {0x55, 0x55, 0xFF}, // 12: Bright Blue
        {0xFF, 0x55, 0xFF}, // 13: Bright Magenta
        {0x55, 0xFF, 0xFF}, // 14: Bright Cyan
        {0xFF, 0xFF, 0xFF}, // 15: Bright White
    };
    
    // 256-color palette (6x6x6 color cube + grayscale)
    private static final int[][] PALETTE_256 = new int[256][3];
    
    static {
        // Colors 0-15: VGA colors
        for (int i = 0; i < 16; i++) {
            PALETTE_256[i] = VGA_COLORS[i].clone();
        }
        
        // Colors 16-231: 6x6x6 color cube
        int idx = 16;
        for (int r = 0; r < 6; r++) {
            for (int g = 0; g < 6; g++) {
                for (int b = 0; b < 6; b++) {
                    int rv = r == 0 ? 0 : 0x33 + r * 0x33;
                    int gv = g == 0 ? 0 : 0x33 + g * 0x33;
                    int bv = b == 0 ? 0 : 0x33 + b * 0x33;
                    PALETTE_256[idx++] = new int[]{rv, gv, bv};
                }
            }
        }
        
        // Colors 232-255: Grayscale ramp
        for (int i = 0; i < 24; i++) {
            int v = 0x08 + i * 10;
            PALETTE_256[idx++] = new int[]{v, v, v};
        }
    }
    
    // Current state
    private int currentForeground = 7; // Default white
    private int currentBackground = 0; // Default black
    private int currentAttributes = 0;
    
    // True color support
    private boolean trueColorForeground = false;
    private boolean trueColorBackground = false;
    private int trueColorFgR = 255, trueColorFgG = 255, trueColorFgB = 255;
    private int trueColorBgR = 0, trueColorBgG = 0, trueColorBgB = 0;
    
    // Saved cursor state (for DECSC/DECRC)
    private int savedForeground = 7;
    private int savedBackground = 0;
    private int savedAttributes = 0;
    private boolean savedTrueColorFg = false;
    private boolean savedTrueColorBg = false;
    private int savedTrueColorFgR = 255, savedTrueColorFgG = 255, savedTrueColorFgB = 255;
    private int savedTrueColorBgR = 0, savedTrueColorBgG = 0, savedTrueColorBgB = 0;
    
    public TerminalColors() {
        reset();
    }
    
    public void reset() {
        currentForeground = 7;
        currentBackground = 0;
        currentAttributes = 0;
        trueColorForeground = false;
        trueColorBackground = false;
    }
    
    public void setForeground(int colorIndex) {
        currentForeground = Math.max(0, Math.min(255, colorIndex));
        trueColorForeground = false;
    }
    
    public void setBackground(int colorIndex) {
        currentBackground = Math.max(0, Math.min(255, colorIndex));
        trueColorBackground = false;
    }
    
    public void setTrueColorForeground(int r, int g, int b) {
        trueColorFgR = Math.max(0, Math.min(255, r));
        trueColorFgG = Math.max(0, Math.min(255, g));
        trueColorFgB = Math.max(0, Math.min(255, b));
        trueColorForeground = true;
    }
    
    public void setTrueColorBackground(int r, int g, int b) {
        trueColorBgR = Math.max(0, Math.min(255, r));
        trueColorBgG = Math.max(0, Math.min(255, g));
        trueColorBgB = Math.max(0, Math.min(255, b));
        trueColorBackground = true;
    }
    
    public void setAttribute(int attr) {
        currentAttributes |= attr;
    }
    
    public void clearAttribute(int attr) {
        currentAttributes &= ~attr;
    }
    
    public int getCurrentForeground() { return currentForeground; }
    public int getCurrentBackground() { return currentBackground; }
    public int getCurrentAttributes() { return currentAttributes; }
    
    public boolean isTrueColorForeground() { return trueColorForeground; }
    public boolean isTrueColorBackground() { return trueColorBackground; }
    
    public int getTrueColorFgR() { return trueColorFgR; }
    public int getTrueColorFgG() { return trueColorFgG; }
    public int getTrueColorFgB() { return trueColorFgB; }
    
    public int getTrueColorBgR() { return trueColorBgR; }
    public int getTrueColorBgG() { return trueColorBgG; }
    public int getTrueColorBgB() { return trueColorBgB; }
    
    /**
     * Get a Color object for the given color index.
     * Handles 16-color, 256-color, and true color modes.
     */
    public Color getColor(int index) {
        if (index < 0) index = 0;
        if (index > 255) index = 255;
        
        int[] rgb = PALETTE_256[index];
        return new Color(rgb[0], rgb[1], rgb[2]);
    }
    
    /**
     * Get the current foreground color.
     */
    public Color getCurrentForegroundColor() {
        if (trueColorForeground) {
            return new Color(trueColorFgR, trueColorFgG, trueColorFgB);
        }
        return getColor(currentForeground);
    }
    
    /**
     * Get the current background color.
     */
    public Color getCurrentBackgroundColor() {
        if (trueColorBackground) {
            return new Color(trueColorBgR, trueColorBgG, trueColorBgB);
        }
        return getColor(currentBackground);
    }
    
    /**
     * Save cursor state (for DECSC).
     */
    public void saveCursor() {
        savedForeground = currentForeground;
        savedBackground = currentBackground;
        savedAttributes = currentAttributes;
        savedTrueColorFg = trueColorForeground;
        savedTrueColorBg = trueColorBackground;
        savedTrueColorFgR = trueColorFgR;
        savedTrueColorFgG = trueColorFgG;
        savedTrueColorFgB = trueColorFgB;
        savedTrueColorBgR = trueColorBgR;
        savedTrueColorBgG = trueColorBgG;
        savedTrueColorBgB = trueColorBgB;
    }
    
    /**
     * Restore cursor state (for DECRC).
     */
    public void restoreCursor() {
        currentForeground = savedForeground;
        currentBackground = savedBackground;
        currentAttributes = savedAttributes;
        trueColorForeground = savedTrueColorFg;
        trueColorBackground = savedTrueColorBg;
        trueColorFgR = savedTrueColorFgR;
        trueColorFgG = savedTrueColorFgG;
        trueColorFgB = savedTrueColorFgB;
        trueColorBgR = savedTrueColorBgR;
        trueColorBgG = savedTrueColorBgG;
        trueColorBgB = savedTrueColorBgB;
    }
    
    /**
     * Get a color from the 256-color palette.
     */
    public static Color getPaletteColor(int index) {
        if (index < 0 || index >= 256) index = 0;
        int[] rgb = PALETTE_256[index];
        return new Color(rgb[0], rgb[1], rgb[2]);
    }
    
    /**
     * Get a color from the 16-color VGA palette.
     */
    public static Color getVGAColor(int index) {
        if (index < 0 || index >= 16) index = 0;
        int[] rgb = VGA_COLORS[index];
        return new Color(rgb[0], rgb[1], rgb[2]);
    }
    
    /**
     * Convert 24-bit RGB to closest 256-color palette index.
     */
    public static int rgbToPaletteIndex(int r, int g, int b) {
        // Check if it's a grayscale value
        if (r == g && g == b) {
            // Map to grayscale ramp (232-255)
            int gray = r;
            int idx = Math.min(23, Math.max(0, (gray - 8) / 10));
            return 232 + idx;
        }
        
        // Map to 6x6x6 color cube (16-231)
        // Each component: 0, 95, 135, 175, 215, 255
        int ri = (r < 75) ? 0 : (r < 115) ? 1 : (r < 155) ? 2 : (r < 195) ? 3 : (r < 235) ? 4 : 5;
        int gi = (g < 75) ? 0 : (g < 115) ? 1 : (g < 155) ? 2 : (g < 195) ? 3 : (g < 235) ? 4 : 5;
        int bi = (b < 75) ? 0 : (b < 115) ? 1 : (b < 155) ? 2 : (b < 195) ? 3 : (b < 235) ? 4 : 5;
        
        return 16 + (ri * 36) + (gi * 6) + bi;
    }
    
    /**
     * Get the default foreground color (white).
     */
    public static Color getDefaultForeground() {
        return new Color(0xAA, 0xAA, 0xAA);
    }
    
    /**
     * Get the default background color (black).
     */
    public static Color getDefaultBackground() {
        return new Color(0x00, 0x00, 0x00);
    }
    
    // Attribute constants
    public static final int ATTR_BOLD = 1;
    public static final int ATTR_DIM = 2;
    public static final int ATTR_ITALIC = 4;
    public static final int ATTR_UNDERLINE = 8;
    public static final int ATTR_BLINK_SLOW = 16;
    public static final int ATTR_BLINK_RAPID = 32;
    public static final int ATTR_REVERSE = 64;
    public static final int ATTR_CONCEAL = 128;
    public static final int ATTR_CROSSED_OUT = 256;
    
    public boolean isBold() { return (currentAttributes & ATTR_BOLD) != 0; }
    public boolean isDim() { return (currentAttributes & ATTR_DIM) != 0; }
    public boolean isItalic() { return (currentAttributes & ATTR_ITALIC) != 0; }
    public boolean isUnderline() { return (currentAttributes & ATTR_UNDERLINE) != 0; }
    public boolean isBlink() { return (currentAttributes & (ATTR_BLINK_SLOW | ATTR_BLINK_RAPID)) != 0; }
    public boolean isReverse() { return (currentAttributes & ATTR_REVERSE) != 0; }
    public boolean isConceal() { return (currentAttributes & ATTR_CONCEAL) != 0; }
    public boolean isCrossedOut() { return (currentAttributes & ATTR_CROSSED_OUT) != 0; }
}