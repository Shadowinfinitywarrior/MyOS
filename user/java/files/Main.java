package files;

import toolkit.Button;
import toolkit.Container;
import toolkit.Label;
import toolkit.Window;
import toolkit.FlowLayout;
import toolkit.BorderLayout;
import toolkit.Widget;
import toolkit.Dimension;
import toolkit.Insets;
import toolkit.Graphics;
import toolkit.Color;
import toolkit.Font;
import toolkit.FontMetrics;
import toolkit.Rectangle;
import toolkit.Cursor;
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
import toolkit.event.FocusEvent;
import toolkit.event.FocusListener;
import toolkit.event.FocusAdapter;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.HashMap;
import java.io.File;
import java.io.IOException;
import java.util.Date;

/**
 * MyOS Java File Manager
 * Provides directory tree, file list, path bar, and file operations.
 */
public class Main {
    public static void main(String[] args) {
        // Create and show the file manager window
        FileManagerWindow window = new FileManagerWindow();
        window.setVisible(true);
    }
}

/**
 * Main file manager window with tree view, file list, and path bar.
 */
class FileManagerWindow extends Window {
    private PathBar pathBar;
    private FileTreePanel treePanel;
    private FileListPanel listPanel;
    private ToolBar toolBar;
    private StatusBar statusBar;
    
    private File currentDirectory;
    private FileTreeNode rootNode;
    private List<FileOperationListener> operationListeners = new ArrayList<>();
    
    public FileManagerWindow() {
        super("Files");
        setSize(800, 600);
        setLayout(new BorderLayout());
        
        // Initialize with root directory
        currentDirectory = new File("/");
        rootNode = new FileTreeNode(currentDirectory);
        
        initComponents();
        loadDirectory(currentDirectory);
    }
    
    private void initComponents() {
        // Tool bar
        toolBar = new ToolBar();
        toolBar.addActionListener(this::handleToolBarAction);
        add(toolBar, BorderLayout.NORTH);
        
        // Path bar
        pathBar = new PathBar();
        pathBar.addPathChangeListener(this::onPathChanged);
        add(pathBar, BorderLayout.NORTH); // Will be below toolbar
        
        // Main content area with split pane
        Container contentPanel = new Container();
        contentPanel.setLayout(new BorderLayout());
        
        // Tree panel (left side)
        treePanel = new FileTreePanel(rootNode);
        treePanel.addTreeSelectionListener(this::onTreeSelectionChanged);
        treePanel.setPreferredSize(new Dimension(250, 0));
        contentPanel.add(treePanel, BorderLayout.WEST);
        
        // File list panel (center)
        listPanel = new FileListPanel();
        listPanel.addFileSelectionListener(this::onFileSelectionChanged);
        listPanel.addFileActionListener(this::onFileAction);
        contentPanel.add(listPanel, BorderLayout.CENTER);
        
        add(contentPanel, BorderLayout.CENTER);
        
        // Status bar
        statusBar = new StatusBar();
        add(statusBar, BorderLayout.SOUTH);
        
        centerOnScreen();
    }
    
    private void handleToolBarAction(ActionEvent e) {
        String cmd = e.getActionCommand();
        switch (cmd) {
            case "back" -> navigateBack();
            case "forward" -> navigateForward();
            case "up" -> navigateUp();
            case "refresh" -> refresh();
            case "new_folder" -> createNewFolder();
            case "copy" -> copySelectedFiles();
            case "move" -> moveSelectedFiles();
            case "delete" -> deleteSelectedFiles();
            case "rename" -> renameSelectedFile();
            case "properties" -> showProperties();
        }
    }
    
    private void onPathChanged(PathChangeEvent e) {
        File newPath = e.getPath();
        if (newPath.exists() && newPath.isDirectory()) {
            navigateTo(newPath);
        }
    }
    
    private void onTreeSelectionChanged(TreeSelectionEvent e) {
        FileTreeNode node = e.getSelectedNode();
        if (node != null) {
            navigateTo(node.getFile());
        }
    }
    
    private void onFileSelectionChanged(FileSelectionEvent e) {
        List<File> selected = e.getSelectedFiles();
        statusBar.setSelectedCount(selected.size());
        
        // Calculate total size
        long totalSize = selected.stream()
            .mapToLong(f -> f.isFile() ? f.length() : 0)
            .sum();
        statusBar.setTotalSize(totalSize);
    }
    
    private void onFileAction(FileActionEvent e) {
        String action = e.getAction();
        File file = e.getFile();
        
        switch (action) {
            case "open" -> {
                if (file.isDirectory()) {
                    navigateTo(file);
                } else {
                    openFile(file);
                }
            }
            case "copy" -> copyFile(file);
            case "move" -> moveFile(file);
            case "delete" -> deleteFile(file);
            case "rename" -> renameFile(file);
            case "properties" -> showProperties(file);
        }
    }
    
    // Navigation methods
    private void navigateTo(File dir) {
        if (!dir.exists() || !dir.isDirectory()) return;
        
        currentDirectory = dir;
        pathBar.setPath(dir);
        loadDirectory(dir);
        
        // Update tree selection
        treePanel.selectNodeForFile(dir);
    }
    
    private void loadDirectory(File dir) {
        listPanel.clear();
        
        File[] files = dir.listFiles();
        if (files != null) {
            java.util.Arrays.sort(files, (a, b) -> {
                // Directories first, then alphabetical
                if (a.isDirectory() != b.isDirectory()) {
                    return a.isDirectory() ? -1 : 1;
                }
                return a.getName().compareToIgnoreCase(b.getName());
            });
            
            for (File file : files) {
                listPanel.addFile(file);
            }
        }
        
        statusBar.setPath(dir.getAbsolutePath());
        statusBar.setItemCount(files != null ? files.length : 0);
    }
    
    private void navigateBack() {
        // Implementation would use a history stack
    }
    
    private void navigateForward() {
        // Implementation would use a history stack
    }
    
    private void navigateUp() {
        File parent = currentDirectory.getParentFile();
        if (parent != null) {
            navigateTo(parent);
        }
    }
    
    private void refresh() {
        loadDirectory(currentDirectory);
        treePanel.refresh();
    }
    
    // File operations
    private void createNewFolder() {
        String name = JOptionPane.showInputDialog(this, "Folder name:", "New Folder");
        if (name != null && !name.trim().isEmpty()) {
            File newDir = new File(currentDirectory, name.trim());
            if (newDir.mkdir()) {
                refresh();
            } else {
                JOptionPane.showMessageDialog(this, "Failed to create folder", "Error", JOptionPane.ERROR_MESSAGE);
            }
        }
    }
    
    private void copySelectedFiles() {
        List<File> selected = listPanel.getSelectedFiles();
        if (selected.isEmpty()) return;
        
        // Store for paste operation
        ClipboardManager.setFiles(selected, ClipboardManager.Operation.COPY);
        statusBar.setMessage("Copied " + selected.size() + " item(s)");
    }
    
    private void moveSelectedFiles() {
        List<File> selected = listPanel.getSelectedFiles();
        if (selected.isEmpty()) return;
        
        ClipboardManager.setFiles(selected, ClipboardManager.Operation.MOVE);
        statusBar.setMessage("Cut " + selected.size() + " item(s)");
    }
    
    private void deleteSelectedFiles() {
        List<File> selected = listPanel.getSelectedFiles();
        if (selected.isEmpty()) return;
        
        int confirm = JOptionPane.showConfirmDialog(this,
            "Delete " + selected.size() + " item(s)?",
            "Confirm Delete", JOptionPane.YES_NO_OPTION);
        
        if (confirm == JOptionPane.YES_OPTION) {
            for (File file : selected) {
                deleteFile(file);
            }
            refresh();
        }
    }
    
    private void copyFile(File file) {
        ClipboardManager.setFiles(List.of(file), ClipboardManager.Operation.COPY);
    }
    
    private void moveFile(File file) {
        ClipboardManager.setFiles(List.of(file), ClipboardManager.Operation.MOVE);
    }
    
    private void deleteFile(File file) {
        try {
            if (file.isDirectory()) {
                deleteDirectory(file);
            } else {
                file.delete();
            }
        } catch (IOException ex) {
            JOptionPane.showMessageDialog(this, "Failed to delete: " + ex.getMessage(), "Error", JOptionPane.ERROR_MESSAGE);
        }
    }
    
    private void deleteDirectory(File dir) throws IOException {
        File[] files = dir.listFiles();
        if (files != null) {
            for (File file : files) {
                if (file.isDirectory()) {
                    deleteDirectory(file);
                } else {
                    file.delete();
                }
            }
        }
        dir.delete();
    }
    
    private void renameSelectedFile() {
        List<File> selected = listPanel.getSelectedFiles();
        if (selected.size() == 1) {
            renameFile(selected.get(0));
        }
    }
    
    private void renameFile(File file) {
        String newName = JOptionPane.showInputDialog(this, "New name:", file.getName());
        if (newName != null && !newName.trim().isEmpty() && !newName.equals(file.getName())) {
            File newFile = new File(file.getParentFile(), newName.trim());
            if (file.renameTo(newFile)) {
                refresh();
            } else {
                JOptionPane.showMessageDialog(this, "Failed to rename", "Error", JOptionPane.ERROR_MESSAGE);
            }
        }
    }
    
    private void showProperties() {
        List<File> selected = listPanel.getSelectedFiles();
        if (selected.size() == 1) {
            showProperties(selected.get(0));
        }
    }
    
    private void showProperties(File file) {
        StringBuilder sb = new StringBuilder();
        sb.append("Name: ").append(file.getName()).append("\n");
        sb.append("Path: ").append(file.getAbsolutePath()).append("\n");
        sb.append("Type: ").append(file.isDirectory() ? "Directory" : "File").append("\n");
        sb.append("Size: ").append(formatSize(file.length())).append("\n");
        sb.append("Last Modified: ").append(new Date(file.lastModified())).append("\n");
        sb.append("Readable: ").append(file.canRead()).append("\n");
        sb.append("Writable: ").append(file.canWrite()).append("\n");
        sb.append("Executable: ").append(file.canExecute()).append("\n");
        
        JOptionPane.showMessageDialog(this, sb.toString(), "Properties", JOptionPane.INFORMATION_MESSAGE);
    }
    
    private void openFile(File file) {
        // In a real implementation, this would launch the appropriate application
        statusBar.setMessage("Opening: " + file.getName());
    }
    
    private String formatSize(long bytes) {
        if (bytes < 1024) return bytes + " B";
        if (bytes < 1024 * 1024) return String.format("%.1f KB", bytes / 1024.0);
        if (bytes < 1024 * 1024 * 1024) return String.format("%.1f MB", bytes / (1024.0 * 1024));
        return String.format("%.1f GB", bytes / (1024.0 * 1024 * 1024));
    }
}

/**
 * Path bar component showing current directory path with clickable segments.
 */
class PathBar extends Container {
    private List<PathChangeListener> pathListeners = new ArrayList<>();
    private File currentPath;
    private List<Button> pathButtons = new ArrayList<>();
    
    public PathBar() {
        setLayout(new FlowLayout(FlowLayout.LEFT, 2, 2));
        setPreferredSize(new Dimension(0, 32));
        setBackground(new Color(240, 240, 240));
        setBorder(BorderFactory.createMatteBorder(0, 0, 1, 0, Color.GRAY));
    }
    
    public void setPath(File path) {
        currentPath = path;
        rebuildPathBar();
    }
    
    public File getPath() {
        return currentPath;
    }
    
    private void rebuildPathBar() {
        removeAll();
        pathButtons.clear();
        
        if (currentPath == null) return;
        
        // Build path components
        List<File> components = new ArrayList<>();
        File f = currentPath;
        while (f != null) {
            components.add(0, f);
            f = f.getParentFile();
        }
        
        // Create buttons for each component
        for (int i = 0; i < components.size(); i++) {
            File comp = components.get(i);
            String name = comp.getName();
            if (name.isEmpty()) name = "/"; // Root
            
            Button btn = new Button(name);
            btn.setBorderWidth(0);
            btn.setBackground(null);
            btn.setFont(new Font("SansSerif", Font.PLAIN, 12));
            
            final File path = comp;
            btn.addActionListener(e -> firePathChange(path));
            
            // Hover effect
            btn.addMouseListener(new MouseAdapter() {
                @Override
                public void mouseEntered(MouseEvent e) {
                    btn.setBackground(new Color(200, 220, 255));
                }
                @Override
                public void mouseExited(MouseEvent e) {
                    btn.setBackground(null);
                }
            });
            
            add(btn);
            pathButtons.add(btn);
        }
        
        revalidate();
    }
    
    public void addPathChangeListener(PathChangeListener listener) {
        pathListeners.add(listener);
    }
    
    public void removePathChangeListener(PathChangeListener listener) {
        pathListeners.remove(listener);
    }
    
    private void firePathChange(File path) {
        PathChangeEvent e = new PathChangeEvent(this, path);
        for (PathChangeListener listener : pathListeners) {
            listener.pathChanged(e);
        }
    }
}

/**
 * Tool bar with common file operations.
 */
class ToolBar extends Container {
    private List<ActionListener> actionListeners = new ArrayList<>();
    
    public ToolBar() {
        setLayout(new FlowLayout(FlowLayout.LEFT, 2, 2));
        setPreferredSize(new Dimension(0, 36));
        setBackground(new Color(230, 230, 230));
        setBorder(BorderFactory.createMatteBorder(0, 0, 1, 0, Color.GRAY));
        
        addButton("←", "back", "Go Back");
        addButton("→", "forward", "Go Forward");
        addButton("↑", "up", "Go Up");
        addButton("↻", "refresh", "Refresh");
        addSeparator();
        addButton("📁", "new_folder", "New Folder");
        addSeparator();
        addButton("📋", "copy", "Copy");
        addButton("✂", "move", "Cut/Move");
        addButton("🗑", "delete", "Delete");
        addSeparator();
        addButton("✏", "rename", "Rename");
        addButton("ℹ", "properties", "Properties");
    }
    
    private void addButton(String text, String actionCommand, String tooltip) {
        Button btn = new Button(text);
        btn.setPreferredSize(new Dimension(32, 28));
        btn.setActionCommand(actionCommand);
        btn.setToolTipText(tooltip);
        btn.addActionListener(e -> fireAction(e));
        add(btn);
    }
    
    private void addSeparator() {
        Label sep = new Label("|");
        sep.setPreferredSize(new Dimension(10, 28));
        sep.setHorizontalAlignment(Label.CENTER);
        sep.setTextColor(Color.GRAY);
        add(sep);
    }
    
    public void addActionListener(ActionListener listener) {
        actionListeners.add(listener);
    }
    
    public void removeActionListener(ActionListener listener) {
        actionListeners.remove(listener);
    }
    
    private void fireAction(ActionEvent e) {
        for (ActionListener listener : actionListeners) {
            listener.actionPerformed(e);
        }
    }
}

/**
 * File tree panel showing directory hierarchy.
 */
class FileTreePanel extends Container {
    private FileTreeNode rootNode;
    private FileTreeNode selectedNode;
    private List<TreeSelectionListener> selectionListeners = new ArrayList<>();
    private Map<File, FileTreeNode> nodeCache = new HashMap<>();
    
    public FileTreePanel(FileTreeNode rootNode) {
        this.rootNode = rootNode;
        setLayout(new BorderLayout());
        setBackground(Color.WHITE);
        setBorder(BorderFactory.createMatteBorder(0, 0, 0, 1, Color.GRAY));
        
        buildTree(rootNode);
    }
    
    private void buildTree(FileTreeNode node) {
        nodeCache.put(node.getFile(), node);
        // In a real implementation, this would render a tree view
        // For now, we just store the structure
    }
    
    public void selectNodeForFile(File file) {
        FileTreeNode node = nodeCache.get(file);
        if (node != null) {
            setSelectedNode(node);
        }
    }
    
    public void setSelectedNode(FileTreeNode node) {
        if (selectedNode != null) {
            selectedNode.setSelected(false);
        }
        selectedNode = node;
        if (selectedNode != null) {
            selectedNode.setSelected(true);
        }
        fireSelectionChanged();
    }
    
    public FileTreeNode getSelectedNode() {
        return selectedNode;
    }
    
    public void refresh() {
        nodeCache.clear();
        buildTree(rootNode);
        revalidate();
    }
    
    public void addTreeSelectionListener(TreeSelectionListener listener) {
        selectionListeners.add(listener);
    }
    
    public void removeTreeSelectionListener(TreeSelectionListener listener) {
        selectionListeners.remove(listener);
    }
    
    private void fireSelectionChanged() {
        TreeSelectionEvent e = new TreeSelectionEvent(this, selectedNode);
        for (TreeSelectionListener listener : selectionListeners) {
            listener.valueChanged(e);
        }
    }
}

/**
 * File list panel showing files in current directory.
 */
class FileListPanel extends Container {
    private List<File> files = new ArrayList<>();
    private List<File> selectedFiles = new ArrayList<>();
    private List<FileSelectionListener> selectionListeners = new ArrayList<>();
    private List<FileActionListener> actionListeners = new ArrayList<>();
    
    public FileListPanel() {
        setLayout(new FlowLayout(FlowLayout.LEFT, 5, 5));
        setBackground(Color.WHITE);
    }
    
    public void addFile(File file) {
        files.add(file);
        FileListItem item = new FileListItem(file);
        item.addFileActionListener(this::fireFileAction);
        add(item);
    }
    
    public void clear() {
        removeAll();
        files.clear();
        selectedFiles.clear();
    }
    
    public void addFileSelectionListener(FileSelectionListener listener) {
        selectionListeners.add(listener);
    }
    
    public void removeFileSelectionListener(FileSelectionListener listener) {
        selectionListeners.remove(listener);
    }
    
    public void addFileActionListener(FileActionListener listener) {
        actionListeners.add(listener);
    }
    
    public void removeFileActionListener(FileActionListener listener) {
        actionListeners.remove(listener);
    }
    
    public List<File> getSelectedFiles() {
        return new ArrayList<>(selectedFiles);
    }
    
    public void setSelectedFiles(List<File> files) {
        selectedFiles.clear();
        selectedFiles.addAll(files);
        fireSelectionChanged();
    }
    
    private void fireSelectionChanged() {
        FileSelectionEvent e = new FileSelectionEvent(this, new ArrayList<>(selectedFiles));
        for (FileSelectionListener listener : selectionListeners) {
            listener.selectionChanged(e);
        }
    }
    
    private void fireFileAction(FileActionEvent e) {
        for (FileActionListener listener : actionListeners) {
            listener.fileActionPerformed(e);
        }
    }
}

/**
 * Status bar showing info about current directory and selection.
 */
class StatusBar extends Container {
    private Label pathLabel = new Label();
    private Label countLabel = new Label();
    private Label sizeLabel = new Label();
    private Label messageLabel = new Label();
    
    public StatusBar() {
        setLayout(new FlowLayout(FlowLayout.LEFT, 5, 2));
        setPreferredSize(new Dimension(0, 24));
        setBackground(new Color(230, 230, 230));
        setBorder(BorderFactory.createMatteBorder(1, 0, 0, 0, Color.GRAY));
        
        pathLabel.setPreferredSize(new Dimension(400, 20));
        countLabel.setPreferredSize(new Dimension(150, 20));
        sizeLabel.setPreferredSize(new Dimension(150, 20));
        messageLabel.setPreferredSize(new Dimension(200, 20));
        
        add(pathLabel);
        add(countLabel);
        add(sizeLabel);
        add(messageLabel);
    }
    
    public void setPath(String path) {
        pathLabel.setText("Path: " + path);
    }
    
    public void setItemCount(int count) {
        countLabel.setText(count + " items");
    }
    
    public void setSelectedCount(int count) {
        if (count > 0) {
            countLabel.setText(count + " selected of " + countLabel.getText());
        }
    }
    
    public void setTotalSize(long bytes) {
        sizeLabel.setText(formatSize(bytes));
    }
    
    public void setMessage(String message) {
        messageLabel.setText(message);
    }
    
    private String formatSize(long bytes) {
        if (bytes < 1024) return bytes + " B";
        if (bytes < 1024 * 1024) return String.format("%.1f KB", bytes / 1024.0);
        if (bytes < 1024 * 1024 * 1024) return String.format("%.1f MB", bytes / (1024.0 * 1024));
        return String.format("%.1f GB", bytes / (1024.0 * 1024 * 1024));
    }
}

/**
 * Individual file item in the file list.
 */
class FileListItem extends Container {
    private File file;
    private Label iconLabel = new Label();
    private Label nameLabel = new Label();
    private Label sizeLabel = new Label();
    private List<FileActionListener> actionListeners = new ArrayList<>();
    private boolean selected = false;
    
    public FileListItem(File file) {
        this.file = file;
        setLayout(new FlowLayout(FlowLayout.LEFT, 5, 2));
        setPreferredSize(new Dimension(300, 24));
        
        // Icon
        iconLabel.setText(file.isDirectory() ? "📁" : "📄");
        iconLabel.setPreferredSize(new Dimension(24, 24));
        
        // Name
        nameLabel.setText(file.getName());
        nameLabel.setPreferredSize(new Dimension(200, 24));
        
        // Size
        if (file.isFile()) {
            sizeLabel.setText(formatSize(file.length()));
        }
        sizeLabel.setPreferredSize(new Dimension(80, 24));
        sizeLabel.setHorizontalAlignment(Label.RIGHT);
        
        add(iconLabel);
        add(nameLabel);
        add(sizeLabel);
        
        // Click handling
        addMouseListener(new MouseAdapter() {
            @Override
            public void mouseClicked(MouseEvent e) {
                if (e.getClickCount() == 2) {
                    fireAction("open");
                } else {
                    setSelected(!selected);
                }
            }
            
            @Override
            public void mousePressed(MouseEvent e) {
                if (e.isPopupTrigger()) showContextMenu(e);
            }
            
            @Override
            public void mouseReleased(MouseEvent e) {
                if (e.isPopupTrigger()) showContextMenu(e);
            }
        });
    }
    
    private void showContextMenu(MouseEvent e) {
        // Would show context menu in real implementation
    }
    
    public void setSelected(boolean selected) {
        this.selected = selected;
        setBackground(selected ? new Color(200, 220, 255) : null);
        repaint();
    }
    
    public boolean isSelected() { return selected; }
    
    public File getFile() { return file; }
    
    public void addFileActionListener(FileActionListener listener) {
        actionListeners.add(listener);
    }
    
    private void fireAction(String action) {
        FileActionEvent e = new FileActionEvent(this, action, file);
        for (FileActionListener listener : actionListeners) {
            listener.fileActionPerformed(e);
        }
    }
    
    private String formatSize(long bytes) {
        if (bytes < 1024) return bytes + " B";
        if (bytes < 1024 * 1024) return String.format("%.1f KB", bytes / 1024.0);
        if (bytes < 1024 * 1024 * 1024) return String.format("%.1f MB", bytes / (1024.0 * 1024));
        return String.format("%.1f GB", bytes / (1024.0 * 1024 * 1024));
    }
}

/**
 * Tree node for file tree.
 */
class FileTreeNode {
    private File file;
    private List<FileTreeNode> children = new ArrayList<>();
    private boolean expanded = false;
    private boolean selected = false;
    private FileTreeNode parent;
    
    public FileTreeNode(File file) {
        this.file = file;
        if (file.isDirectory()) {
            loadChildren();
        }
    }
    
    private void loadChildren() {
        File[] files = file.listFiles();
        if (files != null) {
            for (File child : files) {
                if (child.isDirectory()) {
                    FileTreeNode node = new FileTreeNode(child);
                    node.parent = this;
                    children.add(node);
                }
            }
        }
    }
    
    public File getFile() { return file; }
    public List<FileTreeNode> getChildren() { return children; }
    public boolean isExpanded() { return expanded; }
    public void setExpanded(boolean expanded) { this.expanded = expanded; }
    public boolean isSelected() { return selected; }
    public void setSelected(boolean selected) { this.selected = selected; }
    public FileTreeNode getParent() { return parent; }
}

/**
 * Clipboard manager for copy/move operations.
 */
class ClipboardManager {
    public enum Operation { COPY, MOVE }
    
    private static List<File> clipboardFiles = new ArrayList<>();
    private static Operation clipboardOp = Operation.COPY;
    
    public static void setFiles(List<File> files, Operation op) {
        clipboardFiles = new ArrayList<>(files);
        clipboardOp = op;
    }
    
    public static List<File> getFiles() {
        return new ArrayList<>(clipboardFiles);
    }
    
    public static Operation getOperation() {
        return clipboardOp;
    }
    
    public static void clear() {
        clipboardFiles.clear();
    }
    
    public static boolean hasContent() {
        return !clipboardFiles.isEmpty();
    }
}

/**
 * Custom event classes for file manager.
 */

class PathChangeEvent extends java.util.EventObject {
    private File path;
    public PathChangeEvent(Object source, File path) { super(source); this.path = path; }
    public File getPath() { return path; }
}

interface PathChangeListener extends java.util.EventListener {
    void pathChanged(PathChangeEvent e);
}

class TreeSelectionEvent extends java.util.EventObject {
    private FileTreeNode selectedNode;
    public TreeSelectionEvent(Object source, FileTreeNode node) { super(source); this.selectedNode = node; }
    public FileTreeNode getSelectedNode() { return selectedNode; }
}

interface TreeSelectionListener extends java.util.EventListener {
    void valueChanged(TreeSelectionEvent e);
}

class FileSelectionEvent extends java.util.EventObject {
    private List<File> selectedFiles;
    public FileSelectionEvent(Object source, List<File> files) { super(source); this.selectedFiles = files; }
    public List<File> getSelectedFiles() { return selectedFiles; }
}

interface FileSelectionListener extends java.util.EventListener {
    void selectionChanged(FileSelectionEvent e);
}

class FileActionEvent extends java.util.EventObject {
    private String action;
    private File file;
    public FileActionEvent(Object source, String action, File file) { super(source); this.action = action; this.file = file; }
    public String getAction() { return action; }
    public File getFile() { return file; }
}

interface FileActionListener extends java.util.EventListener {
    void fileActionPerformed(FileActionEvent e);
}

interface FileOperationListener extends java.util.EventListener {
    void operationCompleted(FileOperationEvent e);
}

class FileOperationEvent extends java.util.EventObject {
    private String operation;
    private File source;
    private File dest;
    public FileOperationEvent(Object source, String operation, File src, File dest) { super(source); this.operation = operation; this.source = src; this.dest = dest; }
    public String getOperation() { return operation; }
    public File getSource() { return source; }
    public File getDestination() { return dest; }
}

/**
 * Simple JOptionPane-like dialogs for the toolkit.
 */
class JOptionPane {
    public static final int YES_NO_OPTION = 0;
    public static final int YES_OPTION = 0;
    public static final int NO_OPTION = 1;
    public static final int ERROR_MESSAGE = 0;
    public static final int INFORMATION_MESSAGE = 1;
    
    public static String showInputDialog(Widget parent, String message, String title) {
        // In a real implementation, this would show a dialog
        // For now, return a default
        return null;
    }
    
    public static int showConfirmDialog(Widget parent, String message, String title, int optionType) {
        // In a real implementation, this would show a dialog
        return 0; // YES
    }
    
    public static void showMessageDialog(Widget parent, String message, String title, int messageType) {
        // In a real implementation, this would show a dialog
        System.out.println(title + ": " + message);
    }
}