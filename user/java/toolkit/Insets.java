package toolkit;

/**
 * Insets class for borders.
 */
public class Insets {
    public int top;
    public int left;
    public int bottom;
    public int right;
    
    public Insets() { this(0, 0, 0, 0); }
    public Insets(int top, int left, int bottom, int right) {
        this.top = top;
        this.left = left;
        this.bottom = bottom;
        this.right = right;
    }
    
    public int getTop() { return top; }
    public int getLeft() { return left; }
    public int getBottom() { return bottom; }
    public int getRight() { return right; }
    public boolean equals(Object obj) {
        if (obj instanceof Insets) {
            Insets i = (Insets) obj;
            return top == i.top && left == i.left && bottom == i.bottom && right == i.right;
        }
        return false;
    }
    public String toString() { return "Insets[" + top + "," + left + "," + bottom + "," + right + "]"; }
}