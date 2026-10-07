package toolkit;

import java.awt.*;

/**
 * BorderLayout arranges components in five regions: NORTH, SOUTH, EAST, WEST, CENTER.
 */
public class BorderLayout implements LayoutManager {
    public static final String NORTH = "North";
    public static final String SOUTH = "South";
    public static final String EAST = "East";
    public static final String WEST = "West";
    public static final String CENTER = "Center";
    
    protected int hgap = 0;
    protected int vgap = 0;
    protected Container container;
    protected Widget north, south, east, west, center;
    
    public BorderLayout() {
        this(0, 0);
    }
    
    public BorderLayout(int hgap, int vgap) {
        this.hgap = hgap;
        this.vgap = vgap;
    }
    
    @Override
    public void setContainer(Container container) {
        this.container = container;
    }
    
    @Override
    public void addLayoutComponent(Widget component) {
        addLayoutComponent(component, CENTER);
    }
    
    @Override
    public void addLayoutComponent(Widget component, Object constraints) {
        if (constraints == null) constraints = CENTER;
        
        String region = constraints.toString();
        switch (region) {
            case NORTH -> north = component;
            case SOUTH -> south = component;
            case EAST -> east = component;
            case WEST -> west = component;
            case CENTER -> center = component;
            default -> center = component;
        }
    }
    
    @Override
    public void removeLayoutComponent(Widget component) {
        if (north == component) north = null;
        else if (south == component) south = null;
        else if (east == component) east = null;
        else if (west == component) west = null;
        else if (center == component) center = null;
    }
    
    @Override
    public void layoutContainer(Container container) {
        if (container == null) return;
        
        Insets insets = container.getInsets();
        int x = insets.left;
        int y = insets.top;
        int width = container.getWidth() - insets.left - insets.right;
        int height = container.getHeight() - insets.top - insets.bottom;
        
        // Calculate sizes for NORTH and SOUTH
        int northHeight = 0;
        if (north != null && north.isVisible()) {
            Dimension pref = north.getPreferredSize();
            northHeight = pref.height;
        }
        
        int southHeight = 0;
        if (south != null && south.isVisible()) {
            Dimension pref = south.getPreferredSize();
            southHeight = pref.height;
        }
        
        // Calculate sizes for WEST and EAST
        int westWidth = 0;
        if (west != null && west.isVisible()) {
            Dimension pref = west.getPreferredSize();
            westWidth = pref.width;
        }
        
        int eastWidth = 0;
        if (east != null && east.isVisible()) {
            Dimension pref = east.getPreferredSize();
            eastWidth = pref.width;
        }
        
        // Layout NORTH
        if (north != null && north.isVisible()) {
            north.setBounds(x + hgap, y + vgap, width - 2 * hgap, northHeight);
            y += northHeight + vgap;
            height -= northHeight + vgap;
        }
        
        // Layout SOUTH
        if (south != null && south.isVisible()) {
            south.setBounds(x + hgap, y + height - southHeight, width - 2 * hgap, southHeight);
            height -= southHeight + vgap;
        }
        
        // Layout WEST
        if (west != null && west.isVisible()) {
            west.setBounds(x + hgap, y, westWidth, height);
            x += westWidth + hgap;
            width -= westWidth + hgap;
        }
        
        // Layout EAST
        if (east != null && east.isVisible()) {
            east.setBounds(x + width - eastWidth, y, eastWidth, height);
            width -= eastWidth + hgap;
        }
        
        // Layout CENTER (takes remaining space)
        if (center != null && center.isVisible()) {
            center.setBounds(x, y, width, height);
        }
    }
    
    @Override
    public Dimension preferredLayoutSize(Container container) {
        if (container == null) return new Dimension(0, 0);
        
        Insets insets = container.getInsets();
        int width = 0;
        int height = 0;
        
        // NORTH and SOUTH contribute to height
        if (north != null && north.isVisible()) {
            Dimension pref = north.getPreferredSize();
            width = Math.max(width, pref.width);
            height += pref.height + vgap;
        }
        
        if (south != null && south.isVisible()) {
            Dimension pref = south.getPreferredSize();
            width = Math.max(width, pref.width);
            height += pref.height + vgap;
        }
        
        // CENTER contributes to both
        int centerWidth = 0;
        int centerHeight = 0;
        if (center != null && center.isVisible()) {
            Dimension pref = center.getPreferredSize();
            centerWidth = pref.width;
            centerHeight = pref.height;
        }
        
        // WEST and EAST contribute to width
        if (west != null && west.isVisible()) {
            Dimension pref = west.getPreferredSize();
            centerWidth += pref.width + hgap;
            centerHeight = Math.max(centerHeight, pref.height);
        }
        
        if (east != null && east.isVisible()) {
            Dimension pref = east.getPreferredSize();
            centerWidth += pref.width + hgap;
            centerHeight = Math.max(centerHeight, pref.height);
        }
        
        width = Math.max(width, centerWidth);
        height += centerHeight;
        
        return new Dimension(width + insets.left + insets.right + 2 * hgap,
                           height + insets.top + insets.bottom + 2 * vgap);
    }
    
    @Override
    public Dimension minimumLayoutSize(Container container) {
        return preferredLayoutSize(container);
    }
    
    // Getters/setters
    public int getHgap() { return hgap; }
    public void setHgap(int hgap) { this.hgap = hgap; }
    public int getVgap() { return vgap; }
    public void setVgap(int vgap) { this.vgap = vgap; }
    
    public Widget getComponent(String region) {
        return switch (region) {
            case NORTH -> north;
            case SOUTH -> south;
            case EAST -> east;
            case WEST -> west;
            case CENTER -> center;
            default -> null;
        };
    }
    
    public void setComponent(String region, Widget component) {
        removeLayoutComponent(component);
        addLayoutComponent(component, region);
    }
}