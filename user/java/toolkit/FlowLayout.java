package toolkit;

import java.awt.*;
import java.util.ArrayList;
import java.util.List;

/**
 * FlowLayout arranges components in a flow, wrapping to the next row when needed.
 */
public class FlowLayout implements LayoutManager {
    public static final int LEFT = 0;
    public static final int CENTER = 1;
    public static final int RIGHT = 2;
    public static final int LEADING = 3;
    public static final int TRAILING = 4;
    
    protected int alignment = CENTER;
    protected int hgap = 5;
    protected int vgap = 5;
    protected Container container;
    
    public FlowLayout() {
        this(CENTER, 5, 5);
    }
    
    public FlowLayout(int alignment) {
        this(alignment, 5, 5);
    }
    
    public FlowLayout(int alignment, int hgap, int vgap) {
        this.alignment = alignment;
        this.hgap = hgap;
        this.vgap = vgap;
    }
    
    @Override
    public void setContainer(Container container) {
        this.container = container;
    }
    
    @Override
    public void addLayoutComponent(Widget component) {
        // No constraints needed for FlowLayout
    }
    
    @Override
    public void addLayoutComponent(Widget component, Object constraints) {
        addLayoutComponent(component);
    }
    
    @Override
    public void removeLayoutComponent(Widget component) {
        // Nothing to do
    }
    
    @Override
    public void layoutContainer(Container container) {
        if (container == null) return;
        
        Insets insets = container.getInsets();
        int x = insets.left;
        int y = insets.top;
        int rowHeight = 0;
        int rowStart = insets.left;
        
        List<Widget> rowComponents = new ArrayList<>();
        int maxWidth = container.getWidth() - insets.left - insets.right;
        
        for (Widget comp : container.getComponents()) {
            if (!comp.isVisible()) continue;
            
            Dimension pref = comp.getPreferredSize();
            
            // Check if we need to wrap to next row
            if (x + pref.width > maxWidth && !rowComponents.isEmpty()) {
                // Layout the current row
                layoutRow(rowComponents, rowStart, y, rowHeight, maxWidth);
                
                // Move to next row
                y += rowHeight + vgap;
                x = insets.left;
                rowHeight = 0;
                rowStart = insets.left;
                rowComponents.clear();
            }
            
            rowComponents.add(comp);
            x += pref.width + hgap;
            rowHeight = Math.max(rowHeight, pref.height);
        }
        
        // Layout the last row
        if (!rowComponents.isEmpty()) {
            layoutRow(rowComponents, rowStart, y, rowHeight, maxWidth);
        }
    }
    
    private void layoutRow(List<Widget> rowComponents, int rowStart, int y, int rowHeight, int maxWidth) {
        if (rowComponents.isEmpty()) return;
        
        // Calculate total width of components
        int totalWidth = 0;
        for (Widget comp : rowComponents) {
            totalWidth += comp.getPreferredSize().width;
        }
        totalWidth += hgap * (rowComponents.size() - 1);
        
        // Calculate starting x based on alignment
        int startX;
        switch (alignment) {
            case LEFT, LEADING -> startX = rowStart;
            case RIGHT, TRAILING -> startX = rowStart + maxWidth - totalWidth;
            default -> startX = rowStart + (maxWidth - totalWidth) / 2;
        }
        
        // Position components
        int x = startX;
        for (Widget comp : rowComponents) {
            Dimension pref = comp.getPreferredSize();
            int compY = y + (rowHeight - pref.height) / 2;
            comp.setBounds(x, compY, pref.width, pref.height);
            x += pref.width + hgap;
        }
    }
    
    @Override
    public Dimension preferredLayoutSize(Container container) {
        if (container == null) return new Dimension(0, 0);
        
        Insets insets = container.getInsets();
        int width = 0;
        int height = 0;
        int rowWidth = 0;
        int rowHeight = 0;
        
        for (Widget comp : container.getComponents()) {
            if (!comp.isVisible()) continue;
            
            Dimension pref = comp.getPreferredSize();
            
            if (rowWidth + pref.width > container.getWidth() - insets.left - insets.right && rowWidth > 0) {
                // Wrap to next row
                width = Math.max(width, rowWidth);
                height += rowHeight + vgap;
                rowWidth = 0;
                rowHeight = 0;
            }
            
            rowWidth += pref.width + (rowWidth > 0 ? hgap : 0);
            rowHeight = Math.max(rowHeight, pref.height);
        }
        
        width = Math.max(width, rowWidth);
        height += rowHeight;
        
        return new Dimension(width + insets.left + insets.right, height + insets.top + insets.bottom);
    }
    
    @Override
    public Dimension minimumLayoutSize(Container container) {
        return preferredLayoutSize(container);
    }
    
    // Getters/setters
    public int getAlignment() { return alignment; }
    public void setAlignment(int alignment) { this.alignment = alignment; }
    public int getHgap() { return hgap; }
    public void setHgap(int hgap) { this.hgap = hgap; }
    public int getVgap() { return vgap; }
    public void setVgap(int vgap) { this.vgap = vgap; }
}