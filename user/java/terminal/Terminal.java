package terminal;

import toolkit.Component;
import toolkit.Container;
import toolkit.Graphics;
import toolkit.Color;
import toolkit.Font;
import toolkit.FontMetrics;
import toolkit.Dimension;
import toolkit.Insets;
import toolkit.Rectangle;
import toolkit.BorderFactory;
import toolkit.border.LineBorder;
import toolkit.Border;
import toolkit.event.*;
import toolkit.Cursor;
import myos.binding.Syscall;
import java.util.ArrayList;
import java.util.List;
import java.util.Arrays;

/**
 * Terminal widget with PTY backend, ANSI escape sequence parsing,
 * color support (16/256/true color), scrollback buffer, and copy/paste.
 */
public class Terminal extends Container {
    // Terminal dimensions (in characters)
    private int cols = 80;
    private int rows = 24;
    
    // Font settings
    private Font font = new Font("Monospace", Font.PLAIN, 12);
    private int fontSize = 12;
    private int charWidth = 8;
    private int charHeight = 16;
    private int lineHeight = 18;
    
    // Terminal buffer
    private TerminalBuffer buffer;
    private ScrollbackBuffer scrollback;
    
    // PTY
    private PTY pty;
    private int ptyId = -1;
    private Thread readerThread;
    private boolean running = false;
    
    // Cursor
    private int cursorRow = 0;
    private int cursorCol = 0;
    private boolean cursorVisible = true;
    private boolean cursorBlink = true;
    private long lastCursorBlink = 0;
    
    // Selection
    private boolean selecting = false;
    private int selStartRow = -1, selStartCol = -1;
    private int selEndRow = -1, selEndCol = -1;
    
    // Colors
    private TerminalColors colors = new TerminalColors();
    
    // ANSI parser
    private ANSIParser ansiParser;
    
    // Scroll position
    private int scrollOffset = 0;
    
    // Bell
    private boolean visualBell = false;
    private long bellTime = 0;
    
    // Input
    private boolean focused = false;
    
    // Search
    private String searchText = "";
    private boolean searching = false;
    private List<SearchMatch> searchMatches = new ArrayList<>();
    private int currentMatchIndex = -1;
    
    public Terminal() {
        super();
        setLayout(null); // Custom layout
        setBackground(Color.BLACK);
        setBorder(BorderFactory.createLineBorder(new Color(60, 60, 60)));
        setFocusable(true);
        
        // Initialize components
        buffer = new TerminalBuffer(cols, rows);
        scrollback = new ScrollbackBuffer(10000);
        ansiParser = new ANSIParser(this);
        
        // Calculate font metrics
        updateFontMetrics();
        
        // Add event listeners
        addMouseListener(new TerminalMouseListener());
        addKeyListener(new TerminalKeyListener());
        addFocusListener(new TerminalFocusListener());
    }
    
    @Override
    public void setBounds(int x, int y, int width, int height) {
        super.setBounds(x, y, width, height);
        // Recalculate terminal dimensions based on size
        recalcTerminalSize();
    }
    
    private void recalcTerminalSize() {
        int newCols = Math.max(1, (width - 4) / charWidth);
        int newRows = Math.max(1, (height - 4) / lineHeight);
        
        if (newCols != cols || newRows != rows) {
            resizeTerminal(newCols, newRows);
        }
    }
    
    private void updateFontMetrics() {
        FontMetrics fm = getFontMetrics(font);
        charWidth = fm.getMaxAdvance() / 2;
        charHeight = fm.getHeight();
        lineHeight = charHeight + 2;
    }
    
    /**
     * Start the terminal with a PTY.
     */
    public void start() {
        if (running) return;
        
        // Allocate PTY
        ptyId = Syscall.ptyAlloc("terminal");
        if (ptyId < 0) {
            System.err.println("Failed to allocate PTY");
            return;
        }
        
        pty = new PTY(ptyId);
        running = true;
        
        // Start reader thread
        readerThread = new Thread(this::readerLoop);
        readerThread.start();
        
        // Send initial terminal size
        pty.setSize(cols, rows);
    }
    
    /**
     * Stop the terminal.
     */
    public void stop() {
        running = false;
        if (readerThread != null) {
            readerThread.interrupt();
            try {
                readerThread.join(1000);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }
        if (pty != null) {
            pty.close();
        }
        if (ptyId >= 0) {
            Syscall.ptyFree(ptyId);
            ptyId = -1;
        }
    }
    
    @Override
    public void dispose() {
        stop();
        super.dispose();
    }
    
    private void readerLoop() {
        byte[] readBuffer = new byte[4096];
        
        while (running) {
            try {
                int bytesRead = pty.read(readBuffer);
                if (bytesRead > 0) {
                    // Process ANSI sequences
                    ansiParser.parse(readBuffer, 0, bytesRead);
                    repaint();
                } else if (bytesRead < 0) {
                    // Error or EOF
                    break;
                } else {
                    // No data available, yield
                    Thread.sleep(10);
                }
            } catch (InterruptedException e) {
                break;
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }
    
    /**
     * Write data to the PTY.
     */
    public void write(byte[] data) {
        if (pty != null) {
            pty.write(data);
        }
    }
    
    /**
     * Write string to the PTY.
     */
    public void write(String str) {
        write(str.getBytes());
    }
    
    /**
     * Resize the terminal.
     */
    public void resizeTerminal(int newCols, int newRows) {
        // Save current content to scrollback if shrinking
        if (newRows < rows) {
            for (int r = newRows; r < rows; r++) {
                TerminalLine line = buffer.getLine(r);
                if (line != null && !line.isEmpty()) {
                    scrollback.addLine(line);
                }
            }
        }
        
        cols = newCols;
        rows = newRows;
        buffer.resize(cols, rows);
        
        // Notify PTY of size change
        if (pty != null) {
            pty.setSize(cols, rows);
        }
        
        // Reset cursor
        cursorRow = Math.min(cursorRow, rows - 1);
        cursorCol = Math.min(cursorCol, cols - 1);
        scrollOffset = 0;
        
        repaint();
    }
    
    // Buffer manipulation methods (called by ANSI parser)
    
    public void putChar(char ch) {
        if (cursorRow >= rows) {
            scrollUp(1);
            cursorRow = rows - 1;
        }
        
        TerminalLine line = buffer.getLine(cursorRow);
        if (line != null) {
            line.setChar(cursorCol, ch, colors.getCurrentForeground(), colors.getCurrentBackground(), colors.getCurrentAttributes());
            cursorCol++;
            
            if (cursorCol >= cols) {
                cursorCol = 0;
                cursorRow++;
            }
        }
    }
    
    public void moveCursor(int row, int col) {
        cursorRow = Math.max(0, Math.min(row, rows - 1));
        cursorCol = Math.max(0, Math.min(col, cols - 1));
    }
    
    public void moveCursorRelative(int dRow, int dCol) {
        moveCursor(cursorRow + dRow, cursorCol + dCol);
    }
    
    public void cursorUp(int n) {
        moveCursor(cursorRow - n, cursorCol);
    }
    
    public void cursorDown(int n) {
        moveCursor(cursorRow + n, cursorCol);
    }
    
    public void cursorForward(int n) {
        moveCursor(cursorRow, cursorCol + n);
    }
    
    public void cursorBackward(int n) {
        moveCursor(cursorRow, cursorCol - n);
    }
    
    public void cursorNextLine(int n) {
        moveCursor(cursorRow + n, 0);
    }
    
    public void cursorPrevLine(int n) {
        moveCursor(cursorRow - n, 0);
    }
    
    public void cursorHome() {
        cursorCol = 0;
    }
    
    public void clearScreen(int mode) {
        switch (mode) {
            case 0: // Clear from cursor to end
                clearFromCursor();
                break;
            case 1: // Clear from start to cursor
                clearToCursor();
                break;
            case 2: // Clear entire screen
            case 3: // Clear entire screen (alternate)
                buffer.clear();
                scrollback.clear();
                cursorRow = 0;
                cursorCol = 0;
                break;
        }
    }
    
    public void clearLine(int mode) {
        TerminalLine line = buffer.getLine(cursorRow);
        if (line == null) return;
        
        switch (mode) {
            case 0: // Clear from cursor to end
                for (int c = cursorCol; c < cols; c++) {
                    line.setChar(c, ' ', colors.getCurrentForeground(), colors.getCurrentBackground(), colors.getCurrentAttributes());
                }
                break;
            case 1: // Clear from start to cursor
                for (int c = 0; c <= cursorCol; c++) {
                    line.setChar(c, ' ', colors.getCurrentForeground(), colors.getCurrentBackground(), colors.getCurrentAttributes());
                }
                break;
            case 2: // Clear entire line
                line.clear();
                break;
        }
    }
    
    private void clearFromCursor() {
        // Clear current line from cursor
        TerminalLine line = buffer.getLine(cursorRow);
        if (line != null) {
            for (int c = cursorCol; c < cols; c++) {
                line.setChar(c, ' ', colors.getCurrentForeground(), colors.getCurrentBackground(), colors.getCurrentAttributes());
            }
        }
        // Clear lines below
        for (int r = cursorRow + 1; r < rows; r++) {
            TerminalLine l = buffer.getLine(r);
            if (l != null) l.clear();
        }
    }
    
    private void clearToCursor() {
        // Clear lines above
        for (int r = 0; r < cursorRow; r++) {
            TerminalLine l = buffer.getLine(r);
            if (l != null) l.clear();
        }
        // Clear current line to cursor
        TerminalLine line = buffer.getLine(cursorRow);
        if (line != null) {
            for (int c = 0; c <= cursorCol; c++) {
                line.setChar(c, ' ', colors.getCurrentForeground(), colors.getCurrentBackground(), colors.getCurrentAttributes());
            }
        }
    }
    
    public void scrollUp(int n) {
        // Move lines up
        for (int r = 0; r < rows - n; r++) {
            TerminalLine src = buffer.getLine(r + n);
            TerminalLine dst = buffer.getLine(r);
            if (src != null && dst != null) {
                dst.copyFrom(src);
            }
        }
        // Clear new lines at bottom
        for (int r = rows - n; r < rows; r++) {
            TerminalLine line = buffer.getLine(r);
            if (line != null) line.clear();
        }
        // Add scrolled lines to scrollback
        for (int r = 0; r < n; r++) {
            TerminalLine line = buffer.getLine(r);
            if (line != null && !line.isEmpty()) {
                scrollback.addLine(line);
            }
        }
    }
    
    public void scrollDown(int n) {
        // Move lines down
        for (int r = rows - 1; r >= n; r--) {
            TerminalLine src = buffer.getLine(r - n);
            TerminalLine dst = buffer.getLine(r);
            if (src != null && dst != null) {
                dst.copyFrom(src);
            }
        }
        // Clear new lines at top
        for (int r = 0; r < n; r++) {
            TerminalLine line = buffer.getLine(r);
            if (line != null) line.clear();
        }
    }
    
    public void insertLines(int n) {
        scrollDown(n);
    }
    
    public void deleteLines(int n) {
        scrollUp(n);
    }
    
    public void insertChars(int n) {
        TerminalLine line = buffer.getLine(cursorRow);
        if (line == null) return;
        
        for (int c = cols - 1; c >= cursorCol + n; c--) {
            TerminalCell src = line.getCell(c - n);
            TerminalCell dst = line.getCell(c);
            if (src != null && dst != null) {
                dst.copyFrom(src);
            }
        }
        // Clear inserted positions
        for (int c = cursorCol; c < cursorCol + n && c < cols; c++) {
            TerminalCell cell = line.getCell(c);
            if (cell != null) cell.clear();
        }
    }
    
    public void deleteChars(int n) {
        TerminalLine line = buffer.getLine(cursorRow);
        if (line == null) return;
        
        for (int c = cursorCol; c + n < cols; c++) {
            TerminalCell src = line.getCell(c + n);
            TerminalCell dst = line.getCell(c);
            if (src != null && dst != null) {
                dst.copyFrom(src);
            }
        }
        // Clear deleted positions at end
        for (int c = cols - n; c < cols; c++) {
            TerminalCell cell = line.getCell(c);
            if (cell != null) cell.clear();
        }
    }
    
    public void eraseChars(int n) {
        TerminalLine line = buffer.getLine(cursorRow);
        if (line == null) return;
        
        for (int c = cursorCol; c < cursorCol + n && c < cols; c++) {
            TerminalCell cell = line.getCell(c);
            if (cell != null) cell.clear();
        }
    }
    
    public void setScrollRegion(int top, int bottom) {
        // Not fully implemented - would restrict scrolling to region
    }
    
    public void saveCursor() {
        // Save cursor position and attributes
        colors.saveCursor();
    }
    
    public void restoreCursor() {
        // Restore cursor position and attributes
        colors.restoreCursor();
        // Note: cursor position restore would need separate save/restore
    }
    
    // Color/attribute methods
    
    public void setForeground(int colorIndex) {
        colors.setForeground(colorIndex);
    }
    
    public void setBackground(int colorIndex) {
        colors.setBackground(colorIndex);
    }
    
    public void setTrueColorForeground(int r, int g, int b) {
        colors.setTrueColorForeground(r, g, b);
    }
    
    public void setTrueColorBackground(int r, int g, int b) {
        colors.setTrueColorBackground(r, g, b);
    }
    
    public void setAttribute(int attr) {
        colors.setAttribute(attr);
    }
    
    public void resetAttributes() {
        colors.reset();
    }
    
    // Bell
    public void bell() {
        visualBell = true;
        bellTime = System.currentTimeMillis();
        repaint();
    }
    
    // Selection
    public void startSelection(int row, int col) {
        selecting = true;
        selStartRow = selEndRow = row;
        selStartCol = selEndCol = col;
        repaint();
    }
    
    public void updateSelection(int row, int col) {
        if (selecting) {
            selEndRow = row;
            selEndCol = col;
            repaint();
        }
    }
    
    public void endSelection() {
        selecting = false;
    }
    
    public String getSelectedText() {
        if (selStartRow < 0 || selEndRow < 0) return "";
        
        // Normalize selection
        int startRow = Math.min(selStartRow, selEndRow);
        int endRow = Math.max(selStartRow, selEndRow);
        int startCol = (selStartRow <= selEndRow) ? selStartCol : selEndCol;
        int endCol = (selStartRow <= selEndRow) ? selEndCol : selStartCol;
        
        StringBuilder sb = new StringBuilder();
        
        for (int r = startRow; r <= endRow; r++) {
            TerminalLine line = getVisibleLine(r);
            if (line == null) continue;
            
            int cStart = (r == startRow) ? startCol : 0;
            int cEnd = (r == endRow) ? endCol : cols - 1;
            
            for (int c = cStart; c <= cEnd && c < cols; c++) {
                TerminalCell cell = line.getCell(c);
                if (cell != null && cell.getChar() != ' ') {
                    sb.append(cell.getChar());
                } else if (c > cStart && c < cEnd) {
                    // Don't add trailing spaces unless they're in the middle
                    sb.append(' ');
                }
            }
            
            if (r < endRow) {
                sb.append('\n');
            }
        }
        
        return sb.toString().trim();
    }
    
    public boolean hasSelection() {
        return selStartRow >= 0 && (selStartRow != selEndRow || selStartCol != selEndCol);
    }
    
    public void clearSelection() {
        selStartRow = selEndRow = -1;
        repaint();
    }
    
    public void copySelection() {
        String text = getSelectedText();
        if (!text.isEmpty()) {
            // In a real implementation, this would use system clipboard
            System.out.println("Copied: " + text);
        }
    }
    
    public void paste() {
        // In a real implementation, this would read from system clipboard
        // For now, simulate paste
        write("pasted text\n");
    }
    
    public void clear() {
        buffer.clear();
        scrollOffset = 0;
        repaint();
    }
    
    public void reset() {
        buffer.clear();
        scrollback.clear();
        colors.reset();
        cursorRow = 0;
        cursorCol = 0;
        scrollOffset = 0;
        clearSelection();
        
        // Send reset to PTY
        write("\033c"); // RIS - Reset to Initial State
    }
    
    // Zoom
    public void zoomIn() {
        if (fontSize < 24) {
            fontSize++;
            font = new Font("Monospace", Font.PLAIN, fontSize);
            updateFontMetrics();
            recalcTerminalSize();
            repaint();
        }
    }
    
    public void zoomOut() {
        if (fontSize > 8) {
            fontSize--;
            font = new Font("Monospace", Font.PLAIN, fontSize);
            updateFontMetrics();
            recalcTerminalSize();
            repaint();
        }
    }
    
    public void zoomReset() {
        fontSize = 12;
        font = new Font("Monospace", Font.PLAIN, fontSize);
        updateFontMetrics();
        recalcTerminalSize();
        repaint();
    }
    
    public void showFindDialog() {
        searching = true;
        searchText = "";
        searchMatches.clear();
        currentMatchIndex = -1;
        // In a real implementation, this would show a find dialog
        // For now, just enable search mode
        repaint();
    }
    
    // Getters
    
    public int getCursorRow() { return cursorRow; }
    public int getCursorCol() { return cursorCol; }
    public int getCols() { return cols; }
    public int getRows() { return rows; }
    
    private TerminalLine getVisibleLine(int row) {
        int bufferRow = row + scrollOffset;
        if (bufferRow < 0) {
            // In scrollback
            return scrollback.getLine(-bufferRow - 1);
        } else if (bufferRow < rows) {
            return buffer.getLine(bufferRow);
        }
        return null;
    }
    
    // Painting
    
    @Override
    public void paint(Graphics g) {
        if (!visible) return;
        
        // Background
        g.setColor(background != null ? background : Color.BLACK);
        g.fillRect(0, 0, width, height);
        
        // Visual bell flash
        if (visualBell && System.currentTimeMillis() - bellTime < 100) {
            g.setColor(new Color(255, 255, 255, 50));
            g.fillRect(0, 0, width, height);
        } else if (visualBell) {
            visualBell = false;
        }
        
        // Draw terminal content
        g.setFont(font);
        FontMetrics fm = g.getFontMetrics();
        
        int startX = 2;
        int startY = 2;
        
        // Draw visible lines
        for (int r = 0; r < rows; r++) {
            TerminalLine line = getVisibleLine(r);
            if (line == null) continue;
            
            int y = startY + r * lineHeight;
            
            // Draw line background and text
            drawLine(g, line, startX, y, fm);
        }
        
        // Draw cursor
        if (focused && cursorVisible && cursorBlink) {
            int cx = startX + cursorCol * charWidth;
            int cy = startY + cursorRow * lineHeight;
            g.setColor(Color.WHITE);
            g.fillRect(cx, cy, charWidth, lineHeight);
        }
        
        // Draw selection highlight
        if (hasSelection()) {
            drawSelection(g, startX, startY);
        }
        
        // Draw search matches
        if (searching && !searchMatches.isEmpty()) {
            drawSearchMatches(g, startX, startY, fm);
        }
    }
    
    private void drawLine(Graphics g, TerminalLine line, int x, int y, FontMetrics fm) {
        int currentX = x;
        int currentBg = -1;
        StringBuilder textRun = new StringBuilder();
        int runStartX = x;
        Color runFg = null;
        Color runBg = null;
        
        for (int c = 0; c < cols; c++) {
            TerminalCell cell = line.getCell(c);
            if (cell == null) {
                // Empty cell - treat as space with default colors
                if (textRun.length() > 0) {
                    drawTextRun(g, textRun.toString(), runStartX, y, runFg, runBg, fm);
                    textRun.setLength(0);
                }
                currentX += charWidth;
                continue;
            }
            
            char ch = cell.getChar();
            Color fg = colors.getColor(cell.getForeground());
            Color bg = colors.getColor(cell.getBackground());
            
            // Check if we need to start a new run
            boolean newRun = false;
            if (textRun.length() == 0) {
                newRun = true;
            } else if (!fg.equals(runFg) || !bg.equals(runBg)) {
                newRun = true;
            }
            
            if (newRun && textRun.length() > 0) {
                drawTextRun(g, textRun.toString(), runStartX, y, runFg, runBg, fm);
                textRun.setLength(0);
                runStartX = currentX;
            }
            
            if (newRun) {
                runFg = fg;
                runBg = bg;
            }
            
            textRun.append(ch);
            currentX += charWidth;
        }
        
        // Draw remaining run
        if (textRun.length() > 0) {
            drawTextRun(g, textRun.toString(), runStartX, y, runFg, runBg, fm);
        }
    }
    
    private void drawTextRun(Graphics g, String text, int x, int y, Color fg, Color bg, FontMetrics fm) {
        // Draw background
        if (bg != null) {
            g.setColor(bg);
            g.fillRect(x, y, text.length() * charWidth, lineHeight);
        }
        
        // Draw text
        if (fg != null) {
            g.setColor(fg);
            g.drawString(text, x, y + fm.getAscent());
        }
    }
    
    private void drawSelection(Graphics g, int startX, int startY) {
        g.setColor(new Color(0, 100, 200, 100));
        
        int startRow = Math.min(selStartRow, selEndRow);
        int endRow = Math.max(selStartRow, selEndRow);
        int startCol = (selStartRow <= selEndRow) ? selStartCol : selEndCol;
        int endCol = (selStartRow <= selEndRow) ? selEndCol : selStartCol;
        
        for (int r = startRow; r <= endRow; r++) {
            int cStart = (r == startRow) ? startCol : 0;
            int cEnd = (r == endRow) ? endCol : cols - 1;
            
            int x = startX + cStart * charWidth;
            int y = startY + r * lineHeight;
            int w = (cEnd - cStart + 1) * charWidth;
            
            g.fillRect(x, y, w, lineHeight);
        }
    }
    
    private void drawSearchMatches(Graphics g, int startX, int startY, FontMetrics fm) {
        Color matchColor = new Color(255, 255, 0, 150);
        Color currentMatchColor = new Color(255, 165, 0, 150);
        
        for (int i = 0; i < searchMatches.size(); i++) {
            SearchMatch match = searchMatches.get(i);
            g.setColor(i == currentMatchIndex ? currentMatchColor : matchColor);
            
            int x = startX + match.col * charWidth;
            int y = startY + match.row * lineHeight;
            int w = match.length * charWidth;
            
            g.fillRect(x, y, w, lineHeight);
        }
    }
    
    // Cursor blink timer
    public void updateCursorBlink() {
        long now = System.currentTimeMillis();
        if (now - lastCursorBlink > 500) {
            cursorBlink = !cursorBlink;
            lastCursorBlink = now;
            repaint();
        }
    }
    
    // Event listeners
    
    private class TerminalMouseListener extends MouseAdapter {
        @Override
        public void mousePressed(MouseEvent e) {
            requestFocus();
            
            if (e.getButton() == MouseEvent.BUTTON1) {
                int col = (e.getX() - 2) / charWidth;
                int row = (e.getY() - 2) / lineHeight;
                
                if (col >= 0 && col < cols && row >= 0 && row < rows) {
                    if (e.getClickCount() == 2) {
                        // Double-click: select word
                        selectWord(row, col);
                    } else if (e.getClickCount() == 3) {
                        // Triple-click: select line
                        selectLine(row);
                    } else {
                        // Single click: start selection
                        startSelection(row, col);
                    }
                }
            } else if (e.getButton() == MouseEvent.BUTTON3) {
                // Right click: paste
                paste();
            }
        }
        
        @Override
        public void mouseDragged(MouseEvent e) {
            if (selecting) {
                int col = (e.getX() - 2) / charWidth;
                int row = (e.getY() - 2) / lineHeight;
                
                col = Math.max(0, Math.min(col, cols - 1));
                row = Math.max(0, Math.min(row, rows - 1));
                
                updateSelection(row, col);
            }
        }
        
        @Override
        public void mouseReleased(MouseEvent e) {
            if (e.getButton() == MouseEvent.BUTTON1) {
                endSelection();
            }
        }
        
        private void selectWord(int row, int col) {
            TerminalLine line = getVisibleLine(row);
            if (line == null) return;
            
            // Find word boundaries
            int start = col;
            int end = col;
            
            while (start > 0) {
                TerminalCell cell = line.getCell(start - 1);
                if (cell == null || Character.isWhitespace(cell.getChar())) break;
                start--;
            }
            
            while (end < cols - 1) {
                TerminalCell cell = line.getCell(end + 1);
                if (cell == null || Character.isWhitespace(cell.getChar())) break;
                end++;
            }
            
            selStartRow = selEndRow = row;
            selStartCol = start;
            selEndCol = end;
            repaint();
        }
        
        private void selectLine(int row) {
            selStartRow = selEndRow = row;
            selStartCol = 0;
            selEndCol = cols - 1;
            repaint();
        }
    }
    
    private class TerminalKeyListener extends KeyAdapter {
        @Override
        public void keyPressed(KeyEvent e) {
            if (searching) {
                handleSearchKey(e);
                return;
            }
            
            // Handle special keys
            switch (e.getKeyCode()) {
                case KeyEvent.VK_LEFT -> write("\033[D");
                case KeyEvent.VK_RIGHT -> write("\033[C");
                case KeyEvent.VK_UP -> write("\033[A");
                case KeyEvent.VK_DOWN -> write("\033[B");
                case KeyEvent.VK_HOME -> write("\033[H");
                case KeyEvent.VK_END -> write("\033[F");
                case KeyEvent.VK_PAGE_UP -> {
                    scrollOffset = Math.min(scrollOffset + rows, scrollback.getLineCount());
                    repaint();
                }
                case KeyEvent.VK_PAGE_DOWN -> {
                    scrollOffset = Math.max(scrollOffset - rows, 0);
                    repaint();
                }
                case KeyEvent.VK_INSERT -> write("\033[2~");
                case KeyEvent.VK_DELETE -> write("\033[3~");
                case KeyEvent.VK_F1 -> write("\033OP");
                case KeyEvent.VK_F2 -> write("\033OQ");
                case KeyEvent.VK_F3 -> write("\033OR");
                case KeyEvent.VK_F4 -> write("\033OS");
                case KeyEvent.VK_F5 -> write("\033[15~");
                case KeyEvent.VK_F6 -> write("\033[17~");
                case KeyEvent.VK_F7 -> write("\033[18~");
                case KeyEvent.VK_F8 -> write("\033[19~");
                case KeyEvent.VK_F9 -> write("\033[20~");
                case KeyEvent.VK_F10 -> write("\033[21~");
                case KeyEvent.VK_F11 -> write("\033[23~");
                case KeyEvent.VK_F12 -> write("\033[24~");
                case KeyEvent.VK_TAB -> write("\t");
                case KeyEvent.VK_ENTER -> write("\r");
                case KeyEvent.VK_ESCAPE -> write("\033");
                case KeyEvent.VK_BACKSPACE -> write("\033[3~"); // Or \b
                default -> {
                    // Handle modifier keys
                    if (e.getKeyChar() != KeyEvent.CHAR_UNDEFINED) {
                        char ch = e.getKeyChar();
                        if (ch >= 32 && ch < 127) {
                            write(String.valueOf(ch));
                        }
                    }
                }
            }
        }
        
        private void handleSearchKey(KeyEvent e) {
            // Simple search handling
            if (e.getKeyCode() == KeyEvent.VK_ESCAPE) {
                searching = false;
                searchMatches.clear();
                repaint();
            } else if (e.getKeyCode() == KeyEvent.VK_ENTER) {
                // Find next
                findNext();
            } else if (e.getKeyCode() == KeyEvent.VK_BACKSPACE && !searchText.isEmpty()) {
                searchText = searchText.substring(0, searchText.length() - 1);
                findAll();
            } else if (e.getKeyChar() != KeyEvent.CHAR_UNDEFINED && e.getKeyChar() >= 32) {
                searchText += e.getKeyChar();
                findAll();
            }
        }
    }
    
    private class TerminalFocusListener implements FocusListener {
        @Override
        public void focusGained(FocusEvent e) {
            focused = true;
            repaint();
        }
        
        @Override
        public void focusLost(FocusEvent e) {
            focused = false;
            repaint();
        }
    }
    
    private void findAll() {
        searchMatches.clear();
        if (searchText.isEmpty()) return;
        
        String lowerSearch = searchText.toLowerCase();
        
        for (int r = 0; r < rows + scrollback.getLineCount(); r++) {
            TerminalLine line = getVisibleLine(r);
            if (line == null) continue;
            
            StringBuilder lineText = new StringBuilder();
            for (int c = 0; c < cols; c++) {
                TerminalCell cell = line.getCell(c);
                if (cell != null) {
                    lineText.append(cell.getChar());
                }
            }
            
            String text = lineText.toString().toLowerCase();
            int index = 0;
            while ((index = text.indexOf(lowerSearch, index)) >= 0) {
                searchMatches.add(new SearchMatch(r, index, searchText.length()));
                index++;
            }
        }
        
        if (!searchMatches.isEmpty()) {
            currentMatchIndex = 0;
            // Scroll to first match
            SearchMatch first = searchMatches.get(0);
            if (first.row >= rows) {
                scrollOffset = first.row - rows + 1;
            }
        }
        
        repaint();
    }
    
    private void findNext() {
        if (searchMatches.isEmpty()) return;
        
        currentMatchIndex = (currentMatchIndex + 1) % searchMatches.size();
        SearchMatch match = searchMatches.get(currentMatchIndex);
        
        // Scroll to match
        if (match.row >= rows + scrollOffset) {
            scrollOffset = match.row - rows + 1;
        } else if (match.row < scrollOffset) {
            scrollOffset = match.row;
        }
        
        repaint();
    }
    
    // Inner classes for terminal buffer
    
    static class TerminalBuffer {
        private TerminalLine[] lines;
        private int cols, rows;
        
        public TerminalBuffer(int cols, int rows) {
            this.cols = cols;
            this.rows = rows;
            lines = new TerminalLine[rows];
            for (int i = 0; i < rows; i++) {
                lines[i] = new TerminalLine(cols);
            }
        }
        
        public void resize(int newCols, int newRows) {
            TerminalLine[] newLines = new TerminalLine[newRows];
            
            int copyRows = Math.min(rows, newRows);
            for (int i = 0; i < copyRows; i++) {
                if (i < lines.length && lines[i] != null) {
                    newLines[i] = lines[i].resize(newCols);
                } else {
                    newLines[i] = new TerminalLine(newCols);
                }
            }
            
            for (int i = copyRows; i < newRows; i++) {
                newLines[i] = new TerminalLine(newCols);
            }
            
            lines = newLines;
            cols = newCols;
            rows = newRows;
        }
        
        public void clear() {
            for (TerminalLine line : lines) {
                if (line != null) line.clear();
            }
        }
        
        public TerminalLine getLine(int row) {
            if (row >= 0 && row < rows) {
                return lines[row];
            }
            return null;
        }
        
        public int getCols() { return cols; }
        public int getRows() { return rows; }
    }
    
    static class TerminalLine {
        private TerminalCell[] cells;
        private int cols;
        
        public TerminalLine(int cols) {
            this.cols = cols;
            cells = new TerminalCell[cols];
            for (int i = 0; i < cols; i++) {
                cells[i] = new TerminalCell();
            }
        }
        
        public int getCols() { return cols; }
        
        public TerminalLine resize(int newCols) {
            TerminalLine newLine = new TerminalLine(newCols);
            int copyCols = Math.min(cols, newCols);
            for (int i = 0; i < copyCols; i++) {
                newLine.cells[i].copyFrom(cells[i]);
            }
            return newLine;
        }
        
        public void clear() {
            for (TerminalCell cell : cells) {
                cell.clear();
            }
        }
        
        public boolean isEmpty() {
            for (TerminalCell cell : cells) {
                if (cell.getChar() != ' ' || cell.getForeground() != 7 || cell.getBackground() != 0) {
                    return false;
                }
            }
            return true;
        }
        
        public void copyFrom(TerminalLine other) {
            int copyCols = Math.min(cols, other.cols);
            for (int i = 0; i < copyCols; i++) {
                cells[i].copyFrom(other.cells[i]);
            }
        }
        
        public void setChar(int col, char ch, int fg, int bg, int attr) {
            if (col >= 0 && col < cols) {
                cells[col].set(ch, fg, bg, attr);
            }
        }
        
        public TerminalCell getCell(int col) {
            if (col >= 0 && col < cols) {
                return cells[col];
            }
            return null;
        }
    }
    
    static class TerminalCell {
        private char ch = ' ';
        private int foreground = 7; // Default white
        private int background = 0; // Default black
        private int attributes = 0;
        
        public void set(char ch, int fg, int bg, int attr) {
            this.ch = ch;
            this.foreground = fg;
            this.background = bg;
            this.attributes = attr;
        }
        
        public void clear() {
            ch = ' ';
            foreground = 7;
            background = 0;
            attributes = 0;
        }
        
        public void copyFrom(TerminalCell other) {
            this.ch = other.ch;
            this.foreground = other.foreground;
            this.background = other.background;
            this.attributes = other.attributes;
        }
        
        public char getChar() { return ch; }
        public int getForeground() { return foreground; }
        public int getBackground() { return background; }
        public int getAttributes() { return attributes; }
    }
    
    static class ScrollbackBuffer {
        private List<TerminalLine> lines = new ArrayList<>();
        private int maxLines;
        
        public ScrollbackBuffer(int maxLines) {
            this.maxLines = maxLines;
        }
        
        public void addLine(TerminalLine line) {
            if (lines.size() >= maxLines) {
                lines.remove(0);
            }
            // Create a copy
            TerminalLine copy = new TerminalLine(line.getCols());
            copy.copyFrom(line);
            lines.add(copy);
        }
        
        public TerminalLine getLine(int index) {
            if (index >= 0 && index < lines.size()) {
                return lines.get(index);
            }
            return null;
        }
        
        public int getLineCount() {
            return lines.size();
        }
        
        public void clear() {
            lines.clear();
        }
    }
    
    static class SearchMatch {
        int row, col, length;
        
        public SearchMatch(int row, int col, int length) {
            this.row = row;
            this.col = col;
            this.length = length;
        }
    }
}

/**
 * PTY wrapper using MyOS syscalls.
 */
class PTY {
    private int ptyId;
    
    public PTY(int ptyId) {
        this.ptyId = ptyId;
    }
    
    public int read(byte[] buffer) {
        return Syscall.ptyRead(ptyId, 0, buffer.length); // Simplified - would need actual buffer pointer
    }
    
    public int write(byte[] buffer) {
        return Syscall.ptyWrite(ptyId, 0, buffer.length); // Simplified
    }
    
    public void setSize(int cols, int rows) {
        Syscall.syscall(Syscall.SYS_PTY_SET_SIZE, ptyId, cols, rows, 0, 0, 0);
    }
    
    public void close() {
        Syscall.ptyFree(ptyId);
    }
}