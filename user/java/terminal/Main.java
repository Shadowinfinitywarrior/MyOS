package terminal;

import toolkit.Button;
import toolkit.Container;
import toolkit.Label;
import toolkit.Window;
import toolkit.FlowLayout;
import toolkit.BorderLayout;
import toolkit.Dimension;
import toolkit.Insets;
import toolkit.Graphics;
import toolkit.Color;
import toolkit.Font;
import toolkit.Rectangle;
import toolkit.BorderFactory;
import toolkit.border.MatteBorder;
import toolkit.Border;
import toolkit.Component;
import toolkit.event.ActionEvent;
import toolkit.event.ActionListener;
import toolkit.event.MouseEvent;
import toolkit.event.MouseListener;
import toolkit.event.MouseAdapter;
import toolkit.event.KeyEvent;
import toolkit.event.KeyListener;
import toolkit.event.KeyAdapter;
import java.util.ArrayList;
import java.util.List;

/**
 * MyOS Java Terminal Emulator
 * Provides PTY-based terminal with ANSI escape sequence support,
 * color (16/256/true color), scrollback buffer, and tab support.
 */
public class Main {
    public static void main(String[] args) {
        // Create and show the terminal window
        TerminalWindow window = new TerminalWindow();
        window.setVisible(true);
    }
}

/**
 * Main terminal window with tab support.
 */
class TerminalWindow extends Window {
    private TabBar tabBar;
    private Container tabContentPanel;
    private List<Terminal> tabs = new ArrayList<>();
    private int activeTabIndex = -1;
    
    // Toolbar
    private ToolBar toolBar;
    private StatusBar statusBar;
    
    public TerminalWindow() {
        super("Terminal");
        setSize(800, 600);
        setLayout(new BorderLayout());
        
        initComponents();
        addTab(); // Create initial tab
    }
    
    private void initComponents() {
        // Tool bar
        toolBar = new ToolBar();
        toolBar.addActionListener(this::handleToolBarAction);
        add(toolBar, BorderLayout.NORTH);
        
        // Tab bar
        tabBar = new TabBar();
        tabBar.addTabChangeListener(this::onTabChanged);
        tabBar.addTabCloseListener(this::onTabClose);
        tabBar.addNewTabListener(this::onNewTab);
        add(tabBar, BorderLayout.NORTH);
        
        // Tab content area
        tabContentPanel = new Container();
        tabContentPanel.setLayout(new BorderLayout());
        add(tabContentPanel, BorderLayout.CENTER);
        
        // Status bar
        statusBar = new StatusBar();
        add(statusBar, BorderLayout.SOUTH);
        
        centerOnScreen();
    }
    
    private void handleToolBarAction(ActionEvent e) {
        String cmd = e.getActionCommand();
        Terminal currentTab = getActiveTab();
        if (currentTab == null) return;
        
        switch (cmd) {
            case "new_tab" -> addTab();
            case "close_tab" -> closeTab(activeTabIndex);
            case "copy" -> currentTab.copySelection();
            case "paste" -> currentTab.paste();
            case "clear" -> currentTab.clear();
            case "reset" -> currentTab.reset();
            case "zoom_in" -> currentTab.zoomIn();
            case "zoom_out" -> currentTab.zoomOut();
            case "zoom_reset" -> currentTab.zoomReset();
            case "find" -> currentTab.showFindDialog();
            case "preferences" -> showPreferences();
        }
    }
    
    private void onTabChanged(TabChangeEvent e) {
        int newIndex = e.getNewIndex();
        if (newIndex >= 0 && newIndex < tabs.size()) {
            switchToTab(newIndex);
        }
    }
    
    private void onTabClose(TabCloseEvent e) {
        closeTab(e.getIndex());
    }
    
    private void onNewTab() {
        addTab();
    }
    
    private void addTab() {
        Terminal tab = new Terminal();
        tab.start();
        tabs.add(tab);
        int index = tabs.size() - 1;
        
        tabBar.addTab("Terminal " + (index + 1));
        
        if (activeTabIndex == -1) {
            switchToTab(index);
        }
    }
    
    private void switchToTab(int index) {
        // Remove current tab from content panel
        if (activeTabIndex >= 0 && activeTabIndex < tabs.size()) {
            tabContentPanel.remove(tabs.get(activeTabIndex));
        }
        
        // Add new tab
        activeTabIndex = index;
        Terminal tab = tabs.get(index);
        tabContentPanel.add(tab, BorderLayout.CENTER);
        tabContentPanel.revalidate();
        tab.requestFocus();
        
        tabBar.setSelectedIndex(index);
        updateStatusBar();
    }
    
    private void closeTab(int index) {
        if (tabs.size() <= 1) return; // Don't close last tab
        
        Terminal tab = tabs.get(index);
        tab.dispose();
        tabs.remove(index);
        tabBar.removeTab(index);
        
        if (index == activeTabIndex) {
            // Switch to adjacent tab
            int newIndex = Math.min(index, tabs.size() - 1);
            switchToTab(newIndex);
        } else if (index < activeTabIndex) {
            activeTabIndex--;
            tabBar.setSelectedIndex(activeTabIndex);
        }
    }
    
    private Terminal getActiveTab() {
        if (activeTabIndex >= 0 && activeTabIndex < tabs.size()) {
            return tabs.get(activeTabIndex);
        }
        return null;
    }
    
    private void updateStatusBar() {
        Terminal tab = getActiveTab();
        if (tab != null) {
            statusBar.setCursorPosition(tab.getCursorRow() + 1, tab.getCursorCol() + 1);
            statusBar.setEncoding("UTF-8");
        }
    }
    
    private void showPreferences() {
        // Would show preferences dialog
        statusBar.setMessage("Preferences not yet implemented");
    }
    
    @Override
    public void dispose() {
        for (Terminal tab : tabs) {
            tab.dispose();
        }
        super.dispose();
    }
}

/**
 * Tab bar component.
 */
class TabBar extends Container {
    private List<TabButton> tabButtons = new ArrayList<>();
    private Button newTabButton;
    private int selectedIndex = -1;
    private List<TabChangeListener> changeListeners = new ArrayList<>();
    private List<TabCloseListener> closeListeners = new ArrayList<>();
    private List<Runnable> newTabListeners = new ArrayList<>();
    
    public TabBar() {
        setLayout(new FlowLayout(FlowLayout.LEFT, 0, 0));
        setPreferredSize(new Dimension(0, 28));
        setBackground(new Color(40, 40, 45));
        setBorder(BorderFactory.createMatteBorder(0, 0, 1, 0, new Color(60, 60, 65)));
        
        // New tab button
        newTabButton = new Button("+");
        newTabButton.setPreferredSize(new Dimension(28, 28));
        newTabButton.setBorderWidth(0);
        newTabButton.setBackground(null);
        newTabButton.addActionListener(e -> fireNewTab());
        add(newTabButton);
    }
    
    public void addTab(String title) {
        TabButton btn = new TabButton(title);
        int index = tabButtons.size();
        btn.setTabIndex(index);
        btn.addActionListener(e -> fireTabChange(index));
        btn.addCloseListener(() -> fireTabClose(index));
        
        tabButtons.add(btn);
        add(btn, tabButtons.size() - 1); // Insert before new tab button
        
        if (selectedIndex == -1) {
            setSelectedIndex(0);
        }
        revalidate();
    }
    
    public void removeTab(int index) {
        if (index >= 0 && index < tabButtons.size()) {
            TabButton btn = tabButtons.remove(index);
            remove(btn);
            
            // Update indices
            for (int i = 0; i < tabButtons.size(); i++) {
                tabButtons.get(i).setTabIndex(i);
            }
            
            if (selectedIndex >= tabButtons.size()) {
                selectedIndex = tabButtons.size() - 1;
            }
            
            if (selectedIndex >= 0) {
                tabButtons.get(selectedIndex).setSelected(true);
            }
            
            revalidate();
        }
    }
    
    public void setSelectedIndex(int index) {
        if (index >= 0 && index < tabButtons.size()) {
            if (selectedIndex >= 0 && selectedIndex < tabButtons.size()) {
                tabButtons.get(selectedIndex).setSelected(false);
            }
            selectedIndex = index;
            tabButtons.get(index).setSelected(true);
        }
    }
    
    public int getSelectedIndex() { return selectedIndex; }
    
    public void addTabChangeListener(TabChangeListener listener) {
        changeListeners.add(listener);
    }
    
    public void removeTabChangeListener(TabChangeListener listener) {
        changeListeners.remove(listener);
    }
    
    public void addTabCloseListener(TabCloseListener listener) {
        closeListeners.add(listener);
    }
    
    public void removeTabCloseListener(TabCloseListener listener) {
        closeListeners.remove(listener);
    }
    
    public void addNewTabListener(Runnable listener) {
        newTabListeners.add(listener);
    }
    
    private void fireTabChange(int index) {
        int oldIndex = selectedIndex;
        selectedIndex = index;
        for (TabButton btn : tabButtons) {
            btn.setSelected(btn.getTabIndex() == index);
        }
        TabChangeEvent e = new TabChangeEvent(this, oldIndex, index);
        for (TabChangeListener listener : changeListeners) {
            listener.tabChanged(e);
        }
    }
    
    private void fireTabClose(int index) {
        TabCloseEvent e = new TabCloseEvent(this, index);
        for (TabCloseListener listener : closeListeners) {
            listener.tabClosed(e);
        }
    }
    
    private void fireNewTab() {
        for (Runnable listener : newTabListeners) {
            listener.run();
        }
    }
}

/**
 * Individual tab button with close button.
 */
class TabButton extends Container {
    private Label titleLabel;
    private Button closeButton;
    private String title;
    private int tabIndex = -1;
    private boolean selected = false;
    private List<ActionListener> actionListeners = new ArrayList<>();
    private List<Runnable> closeListeners = new ArrayList<>();
    
    public TabButton(String title) {
        this.title = title;
        setLayout(new FlowLayout(FlowLayout.LEFT, 2, 2));
        setPreferredSize(new Dimension(120, 28));
        
        titleLabel = new Label(title);
        titleLabel.setPreferredSize(new Dimension(80, 24));
        titleLabel.setTextColor(Color.WHITE);
        titleLabel.setFont(new Font("SansSerif", Font.PLAIN, 11));
        
        closeButton = new Button("×");
        closeButton.setPreferredSize(new Dimension(16, 16));
        closeButton.setBorderWidth(0);
        closeButton.setBackground(null);
        closeButton.setFont(new Font("SansSerif", Font.BOLD, 10));
        closeButton.addActionListener(e -> fireClose());
        
        add(titleLabel);
        add(closeButton);
        
        addMouseListener(new MouseAdapter() {
            @Override
            public void mouseClicked(MouseEvent e) {
                if (e.getClickCount() == 1 && !isOverCloseButton(e.getX(), e.getY())) {
                    fireAction();
                }
            }
        });
    }
    
    private boolean isOverCloseButton(int x, int y) {
        // Simple check - close button is on the right
        return x > getWidth() - 20;
    }
    
    public void setTabIndex(int index) {
        tabIndex = index;
    }
    
    public int getTabIndex() { return tabIndex; }
    
    public void setSelected(boolean selected) {
        this.selected = selected;
        if (selected) {
            setBackground(new Color(60, 60, 70));
            titleLabel.setTextColor(Color.WHITE);
        } else {
            setBackground(new Color(40, 40, 45));
            titleLabel.setTextColor(new Color(180, 180, 180));
        }
        repaint();
    }
    
    public boolean isSelected() { return selected; }
    
    public void setTitle(String title) {
        this.title = title;
        titleLabel.setText(title);
    }
    
    public void addActionListener(ActionListener listener) {
        actionListeners.add(listener);
    }
    
    public void addCloseListener(Runnable listener) {
        closeListeners.add(listener);
    }
    
    private void fireAction() {
        ActionEvent e = new ActionEvent(this, ActionEvent.ACTION_PERFORMED, title);
        for (ActionListener listener : actionListeners) {
            listener.actionPerformed(e);
        }
    }
    
    private void fireClose() {
        for (Runnable listener : closeListeners) {
            listener.run();
        }
    }
}

/**
 * Tool bar for terminal window.
 */
class ToolBar extends Container {
    private List<ActionListener> actionListeners = new ArrayList<>();
    
    public ToolBar() {
        setLayout(new FlowLayout(FlowLayout.LEFT, 2, 2));
        setPreferredSize(new Dimension(0, 32));
        setBackground(new Color(50, 50, 55));
        setBorder(BorderFactory.createMatteBorder(0, 0, 1, 0, new Color(60, 60, 65)));
        
        addButton("➕", "new_tab", "New Tab");
        addButton("✕", "close_tab", "Close Tab");
        addSeparator();
        addButton("📋", "copy", "Copy");
        addButton("📄", "paste", "Paste");
        addSeparator();
        addButton("🗑", "clear", "Clear");
        addButton("🔄", "reset", "Reset");
        addSeparator();
        addButton("🔍", "zoom_in", "Zoom In");
        addButton("🔎", "zoom_out", "Zoom Out");
        addButton("📐", "zoom_reset", "Reset Zoom");
        addSeparator();
        addButton("🔎", "find", "Find");
        addButton("⚙", "preferences", "Preferences");
    }
    
    private void addButton(String text, String actionCommand, String tooltip) {
        Button btn = new Button(text);
        btn.setPreferredSize(new Dimension(28, 26));
        btn.setActionCommand(actionCommand);
        btn.setToolTipText(tooltip);
        btn.setBorderWidth(0);
        btn.setBackground(null);
        btn.addActionListener(e -> fireAction(e));
        add(btn);
    }
    
    private void addSeparator() {
        Label sep = new Label("|");
        sep.setPreferredSize(new Dimension(10, 26));
        sep.setHorizontalAlignment(Label.CENTER);
        sep.setTextColor(new Color(100, 100, 100));
        add(sep);
    }
    
    public void addActionListener(ActionListener listener) {
        actionListeners.add(listener);
    }
    
    private void fireAction(ActionEvent e) {
        for (ActionListener listener : actionListeners) {
            listener.actionPerformed(e);
        }
    }
}

/**
 * Status bar for terminal window.
 */
class StatusBar extends Container {
    private Label cursorLabel = new Label();
    private Label encodingLabel = new Label();
    private Label messageLabel = new Label();
    
    public StatusBar() {
        setLayout(new FlowLayout(FlowLayout.LEFT, 10, 2));
        setPreferredSize(new Dimension(0, 24));
        setBackground(new Color(30, 30, 35));
        setBorder(BorderFactory.createMatteBorder(1, 0, 0, 0, new Color(60, 60, 65)));
        
        cursorLabel.setPreferredSize(new Dimension(100, 20));
        cursorLabel.setTextColor(new Color(180, 180, 180));
        cursorLabel.setFont(new Font("SansSerif", Font.PLAIN, 10));
        
        encodingLabel.setPreferredSize(new Dimension(80, 20));
        encodingLabel.setTextColor(new Color(180, 180, 180));
        encodingLabel.setFont(new Font("SansSerif", Font.PLAIN, 10));
        
        messageLabel.setPreferredSize(new Dimension(300, 20));
        messageLabel.setTextColor(new Color(150, 150, 150));
        messageLabel.setFont(new Font("SansSerif", Font.PLAIN, 10));
        
        add(cursorLabel);
        add(encodingLabel);
        add(messageLabel);
    }
    
    public void setCursorPosition(int row, int col) {
        cursorLabel.setText("Ln " + row + ", Col " + col);
    }
    
    public void setEncoding(String encoding) {
        encodingLabel.setText(encoding);
    }
    
    public void setMessage(String message) {
        messageLabel.setText(message);
    }
}

/**
 * Events for tab bar.
 */
class TabChangeEvent extends java.util.EventObject {
    private int oldIndex;
    private int newIndex;
    public TabChangeEvent(Object source, int oldIndex, int newIndex) {
        super(source);
        this.oldIndex = oldIndex;
        this.newIndex = newIndex;
    }
    public int getOldIndex() { return oldIndex; }
    public int getNewIndex() { return newIndex; }
}

interface TabChangeListener extends java.util.EventListener {
    void tabChanged(TabChangeEvent e);
}

class TabCloseEvent extends java.util.EventObject {
    private int index;
    public TabCloseEvent(Object source, int index) {
        super(source);
        this.index = index;
    }
    public int getIndex() { return index; }
}

interface TabCloseListener extends java.util.EventListener {
    void tabClosed(TabCloseEvent e);
}