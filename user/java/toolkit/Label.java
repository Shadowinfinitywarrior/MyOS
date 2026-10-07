package toolkit;

/**
 * A simple text label widget.
 */
public class Label extends Widget {
    protected String text = "";
    protected int alignment = LEFT;
    protected int verticalAlignment = CENTER;
    protected Color textColor = Color.BLACK;
    protected boolean opaque = false;
    
    public static final int LEFT = 0;
    public static final int CENTER = 1;
    public static final int RIGHT = 2;
    public static final int TOP = 0;
    public static final int BOTTOM = 2;
    
    public Label() {
        super();
    }
    
    public Label(String text) {
        this();
        this.text = text;
    }
    
    public Label(String text, int alignment) {
        this(text);
        this.alignment = alignment;
    }
    
    // Text
    public void setText(String text) {
        this.text = text;
        repaint();
    }
    
    public String getText() { return text; }
    
    // Alignment
    public void setHorizontalAlignment(int alignment) {
        this.alignment = alignment;
        repaint();
    }
    
    public int getHorizontalAlignment() { return alignment; }
    
    public void setVerticalAlignment(int alignment) {
        this.verticalAlignment = alignment;
        repaint();
    }
    
    public int getVerticalAlignment() { return verticalAlignment; }
    
    // Colors
    public void setTextColor(Color color) {
        this.textColor = color;
        repaint();
    }
    
    public Color getTextColor() { return textColor; }
    
    // Opaque background
    public void setOpaque(boolean opaque) {
        this.opaque = opaque;
        repaint();
    }
    
    public boolean isOpaque() { return opaque; }
    
    // Preferred size
    public Dimension getPreferredSize() {
        FontMetrics fm = getFontMetrics(getFont());
        return new Dimension(fm.stringWidth(text) + 8, fm.getHeight() + 4);
    }
    
    // Painting
    @Override
    public void paint(Graphics g) {
        if (!visible) return;
        
        // Background if opaque
        if (opaque && background != null) {
            g.setColor(background);
            g.fillRect(0, 0, width, height);
        }
        
        // Text
        g.setColor(enabled ? textColor : textColor.darker());
        g.setFont(font);
        FontMetrics fm = g.getFontMetrics();
        int textWidth = fm.stringWidth(text);
        int textHeight = fm.getAscent();
        
        int textX;
        switch (alignment) {
            case CENTER -> textX = (width - textWidth) / 2;
            case RIGHT -> textX = width - textWidth - 4;
            default -> textX = 4;
        }
        
        int textY;
        switch (verticalAlignment) {
            case TOP -> textY = fm.getAscent() + 2;
            case BOTTOM -> textY = height - fm.getDescent() - 2;
            default -> textY = (height + fm.getAscent() - fm.getDescent()) / 2;
        }
        
        g.drawString(text, textX, textY);
    }
}