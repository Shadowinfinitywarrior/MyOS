package toolkit;

/**
 * Border interface.
 */
public interface Border {
    Insets getBorderInsets(Component c);
    boolean isBorderOpaque();
    void paintBorder(Component c, Graphics g, int x, int y, int width, int height);
}