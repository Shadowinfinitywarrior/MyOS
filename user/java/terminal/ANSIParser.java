package terminal;

import toolkit.Color;
import java.util.ArrayList;
import java.util.List;

/**
 * ANSI escape sequence parser for terminal emulation.
 * Supports common VT100/VT220/VT520 sequences, colors (16/256/true color),
 * cursor movement, scrolling, and other terminal features.
 */
public class ANSIParser {
    // Parser states
    private static final int STATE_GROUND = 0;
    private static final int STATE_ESCAPE = 1;
    private static final int STATE_CSI_ENTRY = 2;
    private static final int STATE_CSI_PARAM = 3;
    private static final int STATE_CSI_INTERMEDIATE = 4;
    private static final int STATE_CSI_IGNORE = 5;
    private static final int STATE_OSC_STRING = 6;
    private static final int STATE_OSC_STRING_END = 7;
    private static final int STATE_DCS_ENTRY = 8;
    private static final int STATE_DCS_PARAM = 9;
    private static final int STATE_DCS_INTERMEDIATE = 10;
    private static final int STATE_DCS_PASSTHROUGH = 11;
    private static final int STATE_DCS_IGNORE = 12;
    private static final int STATE_SOS_PM_APC_STRING = 13;
    
    private Terminal terminal;
    private int state = STATE_GROUND;
    
    // CSI parameters
    private List<Integer> params = new ArrayList<>();
    private int currentParam = 0;
    private boolean paramHasValue = false;
    private List<Character> intermediates = new ArrayList<>();
    private char finalByte = 0;
    
    // OSC/DCS
    private StringBuilder oscBuffer = new StringBuilder();
    private int oscParam = 0;
    
    public ANSIParser(Terminal terminal) {
        this.terminal = terminal;
    }
    
    /**
     * Parse a byte array of terminal output.
     */
    public void parse(byte[] data, int offset, int length) {
        for (int i = 0; i < length; i++) {
            byte b = data[offset + i];
            parseByte(b);
        }
    }
    
    private void parseByte(byte b) {
        char ch = (char) (b & 0xFF);
        
        switch (state) {
            case STATE_GROUND -> handleGround(ch);
            case STATE_ESCAPE -> handleEscape(ch);
            case STATE_CSI_ENTRY -> handleCsiEntry(ch);
            case STATE_CSI_PARAM -> handleCsiParam(ch);
            case STATE_CSI_INTERMEDIATE -> handleCsiIntermediate(ch);
            case STATE_CSI_IGNORE -> handleCsiIgnore(ch);
            case STATE_OSC_STRING -> handleOscString(ch);
            case STATE_OSC_STRING_END -> handleOscStringEnd(ch);
            case STATE_DCS_ENTRY -> handleDcsEntry(ch);
            case STATE_DCS_PARAM -> handleDcsParam(ch);
            case STATE_DCS_INTERMEDIATE -> handleDcsIntermediate(ch);
            case STATE_DCS_PASSTHROUGH -> handleDcsPassthrough(ch);
            case STATE_DCS_IGNORE -> handleDcsIgnore(ch);
            case STATE_SOS_PM_APC_STRING -> handleSosPmApcString(ch);
        }
    }
    
    private void handleGround(char ch) {
        if (ch >= 0x00 && ch <= 0x17) {
            // Control characters (C0)
            executeControl(ch);
        } else if (ch >= 0x18 && ch <= 0x1F) {
            // More control characters
            executeControl(ch);
        } else if (ch == 0x1B) { // ESC
            state = STATE_ESCAPE;
        } else if (ch >= 0x20 && ch <= 0x7E) {
            // Printable ASCII
            terminal.putChar(ch);
        } else if (ch >= 0x80 && ch <= 0x9F) {
            // C1 control characters (8-bit)
            executeC1Control(ch);
        } else {
            // Other bytes (UTF-8 continuation, etc.)
            terminal.putChar(ch);
        }
    }
    
    private void executeControl(char ch) {
        switch (ch) {
            case 0x00 -> {} // NUL - ignored
            case 0x07 -> terminal.bell(); // BEL
            case 0x08 -> terminal.cursorBackward(1); // BS
            case 0x09 -> terminal.moveCursorRelative(0, 8 - (terminal.getCursorCol() % 8)); // HT
            case 0x0A -> { // LF
                terminal.moveCursorRelative(1, 0);
                if (terminal.getCursorRow() >= terminal.getRows()) {
                    terminal.scrollUp(1);
                    terminal.moveCursor(terminal.getRows() - 1, terminal.getCursorCol());
                }
            }
            case 0x0B -> {} // VT - treated as LF
            case 0x0C -> {} // FF - treated as LF
            case 0x0D -> terminal.cursorHome(); // CR
            case 0x0E -> {} // SO - Shift Out (ignored)
            case 0x0F -> {} // SI - Shift In (ignored)
            case 0x18, 0x1A -> { // CAN, SUB - cancel sequence
                state = STATE_GROUND;
            }
            case 0x1B -> {} // ESC - handled in ground state
        }
    }
    
    private void executeC1Control(char ch) {
        // 8-bit C1 controls (0x80-0x9F)
        // These are equivalent to ESC + (ch - 0x40)
        char escChar = (char) (ch - 0x40);
        handleEscape(escChar);
    }
    
    private void handleEscape(char ch) {
        if (ch >= 0x30 && ch <= 0x3F) {
            // 0x30-0x3F: ESC + byte (intermediate)
            // For now, ignore
            state = STATE_GROUND;
        } else if (ch >= 0x20 && ch <= 0x2F) {
            // 0x20-0x2F: ESC + intermediate byte
            intermediates.clear();
            intermediates.add(ch);
            state = STATE_CSI_INTERMEDIATE; // Actually ESC intermediate
        } else if (ch >= 0x40 && ch <= 0x5F) {
            // 0x40-0x5F: ESC + final byte (single-byte escape)
            executeEscapeSequence(ch);
            state = STATE_GROUND;
        } else if (ch >= 0x60 && ch <= 0x7E) {
            // 0x60-0x7E: ESC + final byte
            executeEscapeSequence(ch);
            state = STATE_GROUND;
        } else if (ch == '[') {
            // CSI entry
            params.clear();
            currentParam = 0;
            paramHasValue = false;
            intermediates.clear();
            state = STATE_CSI_ENTRY;
        } else if (ch == ']') {
            // OSC entry
            oscBuffer.setLength(0);
            oscParam = 0;
            state = STATE_OSC_STRING;
        } else if (ch == 'P') {
            // DCS entry
            params.clear();
            currentParam = 0;
            paramHasValue = false;
            intermediates.clear();
            state = STATE_DCS_ENTRY;
        } else if (ch == 'X' || ch == '^' || ch == '_') {
            // SOS, PM, APC
            state = STATE_SOS_PM_APC_STRING;
        } else if (ch == 'c') {
            // RIS - Reset to Initial State
            terminal.reset();
            state = STATE_GROUND;
        } else if (ch == '7') {
            // DECSC - Save Cursor
            terminal.saveCursor();
            state = STATE_GROUND;
        } else if (ch == '8') {
            // DECRC - Restore Cursor
            terminal.restoreCursor();
            state = STATE_GROUND;
        } else if (ch == 'D') {
            // IND - Index
            terminal.moveCursorRelative(1, 0);
            state = STATE_GROUND;
        } else if (ch == 'E') {
            // NEL - Next Line
            terminal.moveCursorRelative(1, 0);
            terminal.cursorHome();
            state = STATE_GROUND;
        } else if (ch == 'H') {
            // HTS - Horizontal Tab Set
            state = STATE_GROUND;
        } else if (ch == 'M') {
            // RI - Reverse Index
            terminal.moveCursorRelative(-1, 0);
            state = STATE_GROUND;
        } else if (ch == 'N' || ch == 'O' || ch == '~') {
            // SS2, SS3, LS1R, etc. - ignored
            state = STATE_GROUND;
        } else if (ch == '(' || ch == ')' || ch == '*' || ch == '+') {
            // G0, G1, G2, G3 designation - ignored
            state = STATE_GROUND;
        } else {
            // Unknown escape
            state = STATE_GROUND;
        }
    }
    
    private void executeEscapeSequence(char finalByte) {
        switch (finalByte) {
            case '7' -> terminal.saveCursor(); // DECSC
            case '8' -> terminal.restoreCursor(); // DECRC
            case 'D' -> terminal.moveCursorRelative(1, 0); // IND
            case 'E' -> { terminal.moveCursorRelative(1, 0); terminal.cursorHome(); } // NEL
            case 'H' -> {} // HTS
            case 'M' -> terminal.moveCursorRelative(-1, 0); // RI
            case 'c' -> terminal.reset(); // RIS
            case 'n' -> {} // LS2
            case 'o' -> {} // LS3
            case '|' -> {} // LS3R
            case '}' -> {} // LS2R
            case '~' -> {} // LS1R
        }
    }
    
    private void handleCsiEntry(char ch) {
        if (ch >= 0x30 && ch <= 0x39) { // 0-9
            currentParam = currentParam * 10 + (ch - '0');
            paramHasValue = true;
            state = STATE_CSI_PARAM;
        } else if (ch == ';') {
            if (paramHasValue) {
                params.add(currentParam);
            } else {
                params.add(0); // Default parameter
            }
            currentParam = 0;
            paramHasValue = false;
        } else if (ch == ':') {
            // Sub-parameters (ignored for now)
        } else if (ch >= 0x3A && ch <= 0x3F) { // : ; < = > ?
            intermediates.add(ch);
            state = STATE_CSI_INTERMEDIATE;
        } else if (ch >= 0x40 && ch <= 0x7E) { // @ A-Z [ \ ] ^ _ ` a-z { | } ~
            if (paramHasValue) {
                params.add(currentParam);
            }
            executeCsiSequence(intermediates, finalByte = ch);
            state = STATE_GROUND;
        } else {
            state = STATE_CSI_IGNORE;
        }
    }
    
    private void handleCsiParam(char ch) {
        if (ch >= 0x30 && ch <= 0x39) {
            currentParam = currentParam * 10 + (ch - '0');
            paramHasValue = true;
        } else if (ch == ';') {
            params.add(currentParam);
            currentParam = 0;
            paramHasValue = false;
        } else if (ch == ':') {
            // Sub-parameters
        } else if (ch >= 0x3A && ch <= 0x3F) {
            intermediates.add(ch);
            state = STATE_CSI_INTERMEDIATE;
        } else if (ch >= 0x40 && ch <= 0x7E) {
            if (paramHasValue) {
                params.add(currentParam);
            }
            executeCsiSequence(intermediates, ch);
            state = STATE_GROUND;
        } else {
            state = STATE_CSI_IGNORE;
        }
    }
    
    private void handleCsiIntermediate(char ch) {
        if (ch >= 0x3A && ch <= 0x3F) {
            intermediates.add(ch);
        } else if (ch >= 0x40 && ch <= 0x7E) {
            if (paramHasValue) {
                params.add(currentParam);
            }
            executeCsiSequence(intermediates, ch);
            state = STATE_GROUND;
        } else {
            state = STATE_CSI_IGNORE;
        }
    }
    
    private void handleCsiIgnore(char ch) {
        if (ch >= 0x40 && ch <= 0x7E) {
            state = STATE_GROUND;
        }
    }
    
    private void executeCsiSequence(List<Character> intermediates, char finalByte) {
        // Get parameters with defaults
        int p1 = params.isEmpty() ? 1 : params.get(0);
        int p2 = params.size() < 2 ? 1 : params.get(1);
        int p3 = params.size() < 3 ? 0 : params.get(2);
        
        // Handle intermediate bytes
        boolean hasQuestionMark = intermediates.contains('?');
        boolean hasGreaterThan = intermediates.contains('>');
        boolean hasExclamation = intermediates.contains('!');
        boolean hasQuote = intermediates.contains('"');
        boolean hasSpace = intermediates.contains(' ');
        
        switch (finalByte) {
            // Cursor movement
            case 'A' -> terminal.cursorUp(p1); // CUU
            case 'B' -> terminal.cursorDown(p1); // CUD
            case 'C' -> terminal.cursorForward(p1); // CUF
            case 'D' -> terminal.cursorBackward(p1); // CUB
            case 'E' -> terminal.cursorNextLine(p1); // CNL
            case 'F' -> terminal.cursorPrevLine(p1); // CPL
            case 'G' -> terminal.moveCursor(terminal.getCursorRow(), p1 - 1); // CHA
            case 'H' -> { // CUP - Cursor Position
                int row = params.isEmpty() ? 1 : params.get(0);
                int col = params.size() < 2 ? 1 : params.get(1);
                terminal.moveCursor(row - 1, col - 1);
            }
            case 'f' -> { // HVP - Horizontal and Vertical Position (same as CUP)
                int row = params.isEmpty() ? 1 : params.get(0);
                int col = params.size() < 2 ? 1 : params.get(1);
                terminal.moveCursor(row - 1, col - 1);
            }
            
            // Screen/line editing
            case 'J' -> terminal.clearScreen(p1); // ED
            case 'K' -> terminal.clearLine(p1); // EL
            case 'L' -> terminal.insertLines(p1); // IL
            case 'M' -> terminal.deleteLines(p1); // DL
            case '@' -> terminal.insertChars(p1); // ICH
            case 'P' -> terminal.deleteChars(p1); // DCH
            case 'X' -> terminal.eraseChars(p1); // ECH
            
            // Scrolling
            case 'S' -> terminal.scrollUp(p1); // SU
            case 'T' -> terminal.scrollDown(p1); // SD
            
            // Scroll region
            case 'r' -> { // DECSTBM - Set Scroll Region
                int top = params.isEmpty() ? 1 : params.get(0);
                int bottom = params.size() < 2 ? terminal.getRows() : params.get(1);
                terminal.setScrollRegion(top - 1, bottom - 1);
            }
            
            // Cursor save/restore
            case 's' -> terminal.saveCursor(); // SCP (or DECSC)
            case 'u' -> terminal.restoreCursor(); // RCP (or DECRC)
            
            // Mode setting (DEC private modes)
            case 'h' -> setMode(intermediates, true); // SM
            case 'l' -> setMode(intermediates, false); // RM
            
            // Device status report
            case 'n' -> handleDSR(p1); // DSR
            
            // Cursor style
            case ' ' -> {
                if (intermediates.contains('q')) {
                    // DECSCUSR - Set cursor style
                    // Ignored for now
                }
            }
            
            // SGR - Select Graphic Rendition (colors, attributes)
            case 'm' -> handleSGR();
            
            // Window title (OSC-like)
            case 't' -> {} // DECSLPP, etc.
            
            // Report cursor position
            case 'R' -> {} // CPR - Cursor Position Report (response)
        }
    }
    
    private void setMode(List<Character> intermediates, boolean set) {
        if (intermediates.contains('?')) {
            // DEC private modes
            for (int param : params) {
                switch (param) {
                    case 1 -> {} // DECCKM - Cursor keys mode
                    case 3 -> {} // DECCOLM - 80/132 columns
                    case 5 -> {} // DECSCNM - Reverse video
                    case 6 -> {} // DECOM - Origin mode
                    case 7 -> {} // DECAWM - Auto wrap
                    case 9 -> {} // X10 mouse
                    case 25 -> {} // DECTCEM - Text cursor enable
                    case 47 -> {} // Alternate screen buffer (old)
                    case 1047 -> {} // Alternate screen buffer (new)
                    case 1049 -> {} // Alternate screen buffer with clear
                    case 1000 -> {} // VT200 mouse
                    case 1002 -> {} // Button event mouse
                    case 1003 -> {} // Any event mouse
                    case 1004 -> {} // Focus reporting
                    case 1005 -> {} // UTF-8 mouse
                    case 1006 -> {} // SGR mouse
                    case 1015 -> {} // URXVT mouse
                    case 2004 -> {} // Bracketed paste
                }
            }
        } else {
            // Standard modes
            for (int param : params) {
                switch (param) {
                    case 4 -> {} // IRM - Insert mode
                    case 20 -> {} // LNM - Line feed/new line mode
                }
            }
        }
    }
    
    private void handleDSR(int param) {
        // Device Status Report - we'd respond via PTY
        // For now, ignored
    }
    
    private void handleSGR() {
        // Select Graphic Rendition - colors and attributes
        for (int i = 0; i < params.size(); i++) {
            int param = params.get(i);
            
            switch (param) {
                case 0 -> terminal.resetAttributes(); // Reset
                case 1 -> terminal.setAttribute(1); // Bold
                case 2 -> terminal.setAttribute(2); // Dim
                case 3 -> terminal.setAttribute(4); // Italic
                case 4 -> terminal.setAttribute(8); // Underline
                case 5 -> terminal.setAttribute(16); // Blink slow
                case 6 -> terminal.setAttribute(32); // Blink rapid
                case 7 -> terminal.setAttribute(64); // Reverse
                case 8 -> terminal.setAttribute(128); // Conceal
                case 9 -> terminal.setAttribute(256); // Crossed out
                
                case 21, 22 -> terminal.setAttribute(0); // Bold off, normal intensity
                case 23 -> terminal.setAttribute(0); // Italic off
                case 24 -> terminal.setAttribute(0); // Underline off
                case 25 -> terminal.setAttribute(0); // Blink off
                case 27 -> terminal.setAttribute(0); // Reverse off
                case 28 -> terminal.setAttribute(0); // Conceal off
                case 29 -> terminal.setAttribute(0); // Crossed out off
                
                // Foreground colors (30-37, 90-97)
                case 30, 31, 32, 33, 34, 35, 36, 37 ->
                    terminal.setForeground(param - 30);
                case 38 -> handleExtendedForeground(i);
                case 39 -> terminal.setForeground(7); // Default foreground
                
                // Background colors (40-47, 100-107)
                case 40, 41, 42, 43, 44, 45, 46, 47 ->
                    terminal.setBackground(param - 40);
                case 48 -> handleExtendedBackground(i);
                case 49 -> terminal.setBackground(0); // Default background
                
                // Bright foreground (90-97)
                case 90, 91, 92, 93, 94, 95, 96, 97 ->
                    terminal.setForeground(param - 90 + 8);
                
                // Bright background (100-107)
                case 100, 101, 102, 103, 104, 105, 106, 107 ->
                    terminal.setBackground(param - 100 + 8);
            }
        }
    }
    
    private void handleExtendedForeground(int paramIndex) {
        if (paramIndex + 1 >= params.size()) return;
        
        int type = params.get(paramIndex + 1);
        
        switch (type) {
            case 2 -> { // 24-bit RGB
                if (paramIndex + 4 < params.size()) {
                    int r = params.get(paramIndex + 2);
                    int g = params.get(paramIndex + 3);
                    int b = params.get(paramIndex + 4);
                    terminal.setTrueColorForeground(r, g, b);
                }
            }
            case 5 -> { // 256-color
                if (paramIndex + 2 < params.size()) {
                    int colorIndex = params.get(paramIndex + 2);
                    terminal.setForeground(colorIndex);
                }
            }
        }
    }
    
    private void handleExtendedBackground(int paramIndex) {
        if (paramIndex + 1 >= params.size()) return;
        
        int type = params.get(paramIndex + 1);
        
        switch (type) {
            case 2 -> { // 24-bit RGB
                if (paramIndex + 4 < params.size()) {
                    int r = params.get(paramIndex + 2);
                    int g = params.get(paramIndex + 3);
                    int b = params.get(paramIndex + 4);
                    terminal.setTrueColorBackground(r, g, b);
                }
            }
            case 5 -> { // 256-color
                if (paramIndex + 2 < params.size()) {
                    int colorIndex = params.get(paramIndex + 2);
                    terminal.setBackground(colorIndex);
                }
            }
        }
    }
    
    private void handleOscString(char ch) {
        if (ch == 0x1B) { // ESC
            state = STATE_OSC_STRING_END;
        } else if (ch == 0x07) { // BEL
            executeOsc();
            state = STATE_GROUND;
        } else if (ch >= 0x20 && ch <= 0x7E) {
            if (oscParam == 0 && ch >= '0' && ch <= '9') {
                oscParam = oscParam * 10 + (ch - '0');
            } else if (ch == ';') {
                // Parameter separator
            } else {
                oscBuffer.append(ch);
            }
        }
    }
    
    private void handleOscStringEnd(char ch) {
        if (ch == '\\') { // ST (String Terminator)
            executeOsc();
            state = STATE_GROUND;
        } else {
            state = STATE_OSC_STRING;
        }
    }
    
    private void executeOsc() {
        switch (oscParam) {
            case 0, 2 -> { // Set window title/icon
                // terminal.setTitle(oscBuffer.toString());
            }
            case 4 -> { // Set color palette
                // Parse color change
            }
            case 10, 11, 12, 13, 14, 15, 16, 17, 18, 19 -> {
                // Set foreground/background/cursor colors
            }
        }
    }
    
    private void handleDcsEntry(char ch) {
        // Device Control String - similar to CSI entry
        if (ch >= 0x30 && ch <= 0x39) {
            currentParam = currentParam * 10 + (ch - '0');
            paramHasValue = true;
            state = STATE_DCS_PARAM;
        } else if (ch == ';') {
            if (paramHasValue) params.add(currentParam);
            else params.add(0);
            currentParam = 0;
            paramHasValue = false;
        } else if (ch >= 0x3A && ch <= 0x3F) {
            intermediates.add(ch);
            state = STATE_DCS_INTERMEDIATE;
        } else {
            state = STATE_DCS_IGNORE;
        }
    }
    
    private void handleDcsParam(char ch) {
        if (ch >= 0x30 && ch <= 0x39) {
            currentParam = currentParam * 10 + (ch - '0');
            paramHasValue = true;
        } else if (ch == ';') {
            params.add(currentParam);
            currentParam = 0;
            paramHasValue = false;
        } else if (ch >= 0x3A && ch <= 0x3F) {
            intermediates.add(ch);
            state = STATE_DCS_INTERMEDIATE;
        } else if (ch >= 0x40 && ch <= 0x7E) {
            if (paramHasValue) params.add(currentParam);
            // DCS with final byte
            state = STATE_DCS_PASSTHROUGH;
        } else {
            state = STATE_DCS_IGNORE;
        }
    }
    
    private void handleDcsIntermediate(char ch) {
        if (ch >= 0x40 && ch <= 0x7E) {
            if (paramHasValue) params.add(currentParam);
            state = STATE_DCS_PASSTHROUGH;
        } else {
            state = STATE_DCS_IGNORE;
        }
    }
    
    private void handleDcsPassthrough(char ch) {
        if (ch == 0x1B) { // ESC
            state = STATE_DCS_IGNORE; // Wait for ST
        }
        // Passthrough data ignored for now
    }
    
    private void handleDcsIgnore(char ch) {
        if (ch == 0x1B) {
            // Check for ST
        } else if (ch == 0x07 || ch == '\\') {
            state = STATE_GROUND;
        }
    }
    
    private void handleSosPmApcString(char ch) {
        if (ch == 0x1B || ch == 0x07 || ch == '\\') {
            state = STATE_GROUND;
        }
    }
}