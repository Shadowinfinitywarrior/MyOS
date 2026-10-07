package toolkit;

import toolkit.border.*;

/**
 * Factory for creating borders.
 */
public class BorderFactory {
    public static Border createLineBorder(Color color) {
        return new LineBorder(color);
    }
    
    public static Border createLineBorder(Color color, int thickness) {
        return new LineBorder(color, thickness);
    }
    
    public static Border createMatteBorder(int top, int left, int bottom, int right, Color color) {
        return new MatteBorder(top, left, bottom, right, color);
    }
    
    public static Border createEmptyBorder(int top, int left, int bottom, int right) {
        return new EmptyBorder(top, left, bottom, right);
    }
    
    public static Border createEtchedBorder() {
        return new EtchedBorder();
    }
    
    public static Border createTitledBorder(String title) {
        return new TitledBorder(title);
    }
}