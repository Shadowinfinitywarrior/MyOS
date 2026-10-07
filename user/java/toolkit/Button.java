package toolkit;

import toolkit.event.*;

/**
 * A clickable button widget.
 */
public class Button extends Widget {
    protected String text = "";
    protected int alignment = CENTER;
    protected boolean pressed = false;
    protected boolean rollover = false;
    protected Color pressedColor = new Color(80, 80, 90);
    protected Color rolloverColor = new Color(100, 100, 110);
    protected Color defaultColor = new Color(70, 70, 80);
    protected Color borderColor = new Color(120, 120, 130);
    protected Color textColor = Color.WHITE;
    protected int borderWidth = 1;
    protected int arcWidth = 4;
    protected int arcHeight = 4;
    
    // Action listeners
    protected java.util.List<ActionListener> actionListeners = new java.util.ArrayList<>();
    
    public static final int LEFT = 0;
    public static final int CENTER = 1;
    public static final int RIGHT = 2;
    
    public Button() {
        super();
    }
    
    public Button(String text) {
        this();
        this.text = text;
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
    
    // Colors
    public void setPressedColor(Color c) { pressedColor = c; repaint(); }
    public void setRolloverColor(Color c) { rolloverColor = c; repaint(); }
    public void setDefaultColor(Color c) { defaultColor = c; repaint(); }
    public void setBorderColor(Color c) { borderColor = c; repaint(); }
    public void setTextColor(Color c) { textColor = c; repaint(); }
    
    // Action listeners
    public void addActionListener(ActionListener listener) {
        actionListeners.add(listener);
    }
    
    public void removeActionListener(ActionListener listener) {
        actionListeners.remove(listener);
    }
    
    protected void fireActionEvent() {
        ActionEvent e = new ActionEvent(this, ActionEvent.ACTION_PERFORMED, text);
        for (ActionListener listener : actionListeners) {
            listener.actionPerformed(e);
        }
    }
    
    // Preferred size
    public Dimension getPreferredSize() {
        FontMetrics fm = getFontMetrics(getFont());
        int textWidth = fm.stringWidth(text);
        return new Dimension(textWidth + 24, fm.getHeight() + 12);
    }
    
    // Mouse event handling
    @Override
    public void fireMouseEvent(MouseEvent e) {
        switch (e.getID()) {
            case MouseEvent.MOUSE_ENTERED -> {
                rollover = true;
                repaint();
            }
            case MouseEvent.MOUSE_EXITED -> {
                rollover = false;
                pressed = false;
                repaint();
            }
            case MouseEvent.MOUSE_PRESSED -> {
                if (e.getButton() == MouseEvent.BUTTON1 && contains(e.getX(), e.getY())) {
                    pressed = true;
                    repaint();
                }
            }
            case MouseEvent.MOUSE_RELEASED -> {
                if (pressed && contains(e.getX(), e.getY())) {
                    fireActionEvent();
                }
                pressed = false;
                repaint();
            }
        }
        super.fireMouseEvent(e);
    }
    
    // Keyboard handling
    @Override
    public void fireKeyEvent(KeyEvent e) {
        if (e.getID() == KeyEvent.KEY_PRESSED) {
            if (e.getKeyCode() == KeyEvent.VK_SPACE || e.getKeyCode() == KeyEvent.VK_ENTER) {
                pressed = true;
                repaint();
            }
        } else if (e.getID() == KeyEvent.KEY_RELEASED) {
            if (e.getKeyCode() == KeyEvent.VK_SPACE || e.getKeyCode() == KeyEvent.VK_ENTER) {
                if (pressed) {
                    fireActionEvent();
                }
                pressed = false;
                repaint();
            }
        }
        super.fireKeyEvent(e);
    }
    
    // Process mouse event (for direct event dispatching)
    public void processMouseEvent(MouseEvent e) {
        fireMouseEvent(e);
    }
    
    // Painting
    @Override
    public void paint(Graphics g) {
        if (!visible) return;
        
        // Background
        Color bg;
        if (!enabled) {
            bg = defaultColor.darker();
        } else if (pressed) {
            bg = pressedColor;
        } else if (rollover) {
            bg = rolloverColor;
        } else {
            bg = background != null ? background : defaultColor;
        }
        
        g.setColor(bg);
        g.fillRoundRect(0, 0, width, height, arcWidth, arcHeight);
        
        // Border
        g.setColor(enabled ? borderColor : borderColor.darker());
        // Draw border using multiple rectangles
        for (int i = 0; i < borderWidth; i++) {
            g.drawRoundRect(i, i, width - 1 - 2*i, height - 1 - 2*i, arcWidth, arcHeight);
        }
        
        // Text
        g.setColor(enabled ? textColor : textColor.darker());
        g.setFont(font);
        FontMetrics fm = g.getFontMetrics();
        int textWidth = fm.stringWidth(text);
        int textHeight = fm.getAscent();
        
        int textX;
        switch (alignment) {
            case LEFT -> textX = 8;
            case RIGHT -> textX = width - textWidth - 8;
            default -> textX = (width - textWidth) / 2;
        }
        int textY = (height + textHeight) / 2 - 2;
        
        g.drawString(text, textX, textY);
        
        // Focus indicator
        if (focused && enabled) {
            g.setColor(Color.BLUE);
            g.drawRoundRect(2, 2, width - 5, height - 5, arcWidth, arcHeight);
        }
    }
}