# PyQt-like bindings for MyOS
# Maps Qt API to Rust GUI via pymyos syscalls

import pymyos
import sys

# ============================================================================
# Core Types
# ============================================================================

class QRect:
    """Rectangle geometry"""
    def __init__(self, x=0, y=0, w=0, h=0):
        self.x = x
        self.y = y
        self.w = w
        self.h = h
    
    def __repr__(self):
        return f"QRect({self.x}, {self.y}, {self.w}, {self.h})"
    
    def left(self):
        return self.x
    
    def top(self):
        return self.y
    
    def right(self):
        return self.x + self.w
    
    def bottom(self):
        return self.y + self.h
    
    def width(self):
        return self.w
    
    def height(self):
        return self.h
    
    def size(self):
        return QSize(self.w, self.h)
    
    def contains(self, x, y):
        return (self.x <= x < self.x + self.w and 
                self.y <= y < self.y + self.h)
    
    def moveTo(self, x, y):
        self.x = x
        self.y = y
    
    def translate(self, dx, dy):
        self.x += dx
        self.y += dy

class QSize:
    """Size geometry"""
    def __init__(self, w=0, h=0):
        self.w = w
        self.h = h
    
    def __repr__(self):
        return f"QSize({self.w}, {self.h})"
    
    def width(self):
        return self.w
    
    def height(self):
        return self.h
    
    def setWidth(self, w):
        self.w = w
    
    def setHeight(self, h):
        self.h = h

class QPoint:
    """Point geometry"""
    def __init__(self, x=0, y=0):
        self.x = x
        self.y = y
    
    def __repr__(self):
        return f"QPoint({self.x}, {self.y})"

class QColor:
    """Color representation (ARGB)"""
    def __init__(self, r=0, g=0, b=0, a=255):
        self.r = r
        self.g = g
        self.b = b
        self.a = a
    
    def __repr__(self):
        return f"QColor({self.r}, {self.g}, {self.b}, {self.a})"
    
    def rgba(self):
        return (self.a << 24) | (self.r << 16) | (self.g << 8) | self.b
    
    @staticmethod
    def fromRgb(r, g, b, a=255):
        return QColor(r, g, b, a)
    
    @staticmethod
    def fromArgb(a, r, g, b):
        return QColor(r, g, b, a)

# ============================================================================
# Paint System
# ============================================================================

class QPaintDevice:
    """Base class for paint devices"""
    def __init__(self):
        self._surface = None
    
    def paintEngine(self):
        return None

class QPainter:
    """Painting operations"""
    def __init__(self, device=None):
        self._device = device
        self._active = False
        self._pen = QPen()
        self._brush = QBrush()
        self._font = QFont()
    
    def begin(self, device):
        if self._active:
            return False
        self._device = device
        self._active = True
        return True
    
    def end(self):
        self._active = False
        self._device = None
    
    def isActive(self):
        return self._active
    
    def device(self):
        return self._device
    
    def setPen(self, pen):
        self._pen = pen
    
    def pen(self):
        return self._pen
    
    def setBrush(self, brush):
        self._brush = brush
    
    def brush(self):
        return self._brush
    
    def setFont(self, font):
        self._font = font
    
    def font(self):
        return self._font
    
    # Drawing primitives
    def drawRect(self, x, y, w, h):
        if not self._active:
            return
        if isinstance(x, QRect):
            rect = x
        else:
            rect = QRect(x, y, w, h)
        # Call Rust GUI via pymyos syscall
        color = self._pen.color().rgba()
        pymyos._gui_draw_rect(self._device._surface, rect.x, rect.y, rect.w, rect.h, color, self._pen.width())
    
    def fillRect(self, x, y, w, h, color):
        if not self._active:
            return
        if isinstance(x, QRect):
            rect = x
            color = y
        else:
            rect = QRect(x, y, w, h)
        if isinstance(color, QColor):
            color = color.rgba()
        pymyos._gui_fill_rect(self._device._surface, rect.x, rect.y, rect.w, rect.h, color)
    
    def drawLine(self, x1, y1, x2, y2):
        if not self._active:
            return
        if isinstance(x1, QPoint):
            p1, p2 = x1, y1
        else:
            p1, p2 = QPoint(x1, y1), QPoint(x2, y2)
        color = self._pen.color().rgba()
        pymyos._gui_draw_line(self._device._surface, p1.x, p1.y, p2.x, p2.y, color, self._pen.width())
    
    def drawText(self, x, y, text):
        if not self._active:
            return
        if isinstance(x, QPoint):
            pos = x
            text = y
        else:
            pos = QPoint(x, y)
        # Text rendering via Rust GUI
        color = self._pen.color().rgba()
        pymyos._gui_draw_text(self._device._surface, pos.x, pos.y, text, color, self._font.size())
    
    def drawEllipse(self, x, y, w, h):
        if not self._active:
            return
        if isinstance(x, QRect):
            rect = x
        else:
            rect = QRect(x, y, w, h)
        color = self._pen.color().rgba()
        cx = rect.x + rect.w // 2
        cy = rect.y + rect.h // 2
        rx = rect.w // 2
        ry = rect.h // 2
        pymyos._gui_draw_ellipse(self._device._surface, cx, cy, rx, ry, color, self._pen.width())

class QPen:
    """Pen for drawing outlines"""
    def __init__(self, color=None, width=1, style=None):
        self._color = color or QColor(0, 0, 0)
        self._width = width
        self._style = style or "solid"
    
    def color(self):
        return self._color
    
    def setColor(self, color):
        self._color = color
    
    def width(self):
        return self._width
    
    def setWidth(self, width):
        self._width = width

class QBrush:
    """Brush for filling"""
    def __init__(self, color=None, style=None):
        self._color = color or QColor(255, 255, 255)
        self._style = style or "solid"
    
    def color(self):
        return self._color
    
    def setColor(self, color):
        self._color = color

class QFont:
    """Font handling"""
    def __init__(self, family="DejaVu Sans", size=12, weight=50, italic=False):
        self._family = family
        self._size = size
        self._weight = weight
        self._italic = italic
    
    def family(self):
        return self._family
    
    def setFamily(self, family):
        self._family = family
    
    def pointSize(self):
        return self._size
    
    def setPointSize(self, size):
        self._size = size
    
    def size(self):
        return self._size
    
    def bold(self):
        return self._weight >= 75
    
    def setBold(self, bold):
        self._weight = 75 if bold else 50
    
    def italic(self):
        return self._italic
    
    def setItalic(self, italic):
        self._italic = italic

# ============================================================================
# GUI Surface
# ============================================================================

class QSurface(QPaintDevice):
    """Off-screen drawing surface"""
    def __init__(self, width, height):
        super().__init__()
        self._width = width
        self._height = height
        # Create surface via syscall
        self._surface = pymyos.gui_create_surface(width, height)
    
    def width(self):
        return self._width
    
    def height(self):
        return self._height
    
    def size(self):
        return QSize(self._width, self._height)
    
    def surface(self):
        return self._surface
    
    def blit(self, x, y):
        """Blit this surface to screen"""
        return pymyos.gui_blit_surface(self._surface, x, y)

# ============================================================================
# Widget System
# ============================================================================

class QObject:
    """Base class for all Qt objects"""
    def __init__(self, parent=None):
        self._parent = parent
        self._children = []
        self._signals = {}
        if parent:
            parent._children.append(self)
    
    def parent(self):
        return self._parent
    
    def children(self):
        return self._children
    
    def connect(self, signal, slot):
        """Connect signal to slot"""
        if signal not in self._signals:
            self._signals[signal] = []
        self._signals[signal].append(slot)
    
    def disconnect(self, signal, slot):
        """Disconnect signal from slot"""
        if signal in self._signals:
            if slot in self._signals[signal]:
                self._signals[signal].remove(slot)
    
    def emit(self, signal, *args):
        """Emit signal"""
        if signal in self._signals:
            for slot in self._signals[signal]:
                slot(*args)
    
    def deleteLater(self):
        """Schedule for deletion"""
        if self._parent:
            self._parent._children.remove(self)
        self._parent = None

class QWidget(QObject):
    """Base widget class"""
    def __init__(self, parent=None, flags=0):
        super().__init__(parent)
        self._geometry = QRect()
        self._visible = True
        self._enabled = True
        self._focus = False
        self._window_id = 0
        self._surface = None
        self._layout = None
        self._style_sheet = ""
        self._title = ""
        self._flags = flags
    
    def setGeometry(self, x, y, w, h):
        self._geometry = QRect(x, y, w, h)
        if self._window_id:
            # Update window via syscall
            pass
    
    def geometry(self):
        return self._geometry
    
    def rect(self):
        return QRect(0, 0, self._geometry.w, self._geometry.h)
    
    def setFixedSize(self, w, h):
        self._geometry.w = w
        self._geometry.h = h
    
    def resize(self, w, h):
        self._geometry.w = w
        self._geometry.h = h
        self.resizeEvent(QResizeEvent(QSize(w, h)))
    
    def move(self, x, y):
        self._geometry.x = x
        self._geometry.y = y
    
    def show(self):
        self._visible = True
        if not self._window_id and not self._parent:
            # Create top-level window
            self._window_id = pymyos.rust_gui_create_window(
                self._title, self._geometry.x, self._geometry.y, 
                self._geometry.w, self._geometry.h
            )
            # Create surface for this window
            self._surface = pymyos.gui_create_surface(self._geometry.w, self._geometry.h)
    
    def hide(self):
        self._visible = False
    
    def isVisible(self):
        return self._visible
    
    def setEnabled(self, enabled):
        self._enabled = enabled
    
    def isEnabled(self):
        return self._enabled
    
    def setFocus(self):
        self._focus = True
        if self._window_id:
            pymyos._gui_set_focus(self._window_id)
    
    def hasFocus(self):
        return self._focus
    
    def setWindowTitle(self, title):
        self._title = title
        if self._window_id:
            # Update window title
            pass
    
    def windowTitle(self):
        return self._title
    
    def setLayout(self, layout):
        self._layout = layout
        layout._parent = self
        # Layout children
        layout.activate()
    
    def layout(self):
        return self._layout
    
    def update(self):
        """Request repaint"""
        if self._window_id:
            pymyos.gui_invalidate(self._window_id, 0, 0, self._geometry.w, self._geometry.h)
    
    def paintEvent(self, event):
        """Override for custom painting"""
        pass
    
    def resizeEvent(self, event):
        """Override for resize handling"""
        pass
    
    def mousePressEvent(self, event):
        """Override for mouse press"""
        pass
    
    def mouseReleaseEvent(self, event):
        """Override for mouse release"""
        pass
    
    def mouseMoveEvent(self, event):
        """Override for mouse move"""
        pass
    
    def keyPressEvent(self, event):
        """Override for key press"""
        pass
    
    def keyReleaseEvent(self, event):
        """Override for key release"""
        pass
    
    def closeEvent(self, event):
        """Override for close handling"""
        pass

# ============================================================================
# Layout System
# ============================================================================

class QLayout(QObject):
    """Base layout class"""
    def __init__(self, parent=None):
        super().__init__(parent)
        self._items = []
        self._spacing = 4
        self._margin = 4
        self._parent_widget = parent
    
    def addWidget(self, widget):
        self._items.append(('widget', widget))
        widget._parent = self._parent_widget
    
    def addLayout(self, layout):
        self._items.append(('layout', layout))
        layout._parent_widget = self._parent_widget
    
    def addSpacing(self, size):
        self._items.append(('spacing', size))
    
    def addStretch(self, stretch=1):
        self._items.append(('stretch', stretch))
    
    def setSpacing(self, spacing):
        self._spacing = spacing
    
    def setContentsMargins(self, left, top, right, bottom):
        self._margin = (left, top, right, bottom)
    
    def activate(self):
        """Layout all items"""
        pass
    
    def count(self):
        return len(self._items)
    
    def itemAt(self, index):
        if 0 <= index < len(self._items):
            return self._items[index]
        return None

class QVBoxLayout(QLayout):
    """Vertical box layout"""
    def __init__(self, parent=None):
        super().__init__(parent)
    
    def activate(self):
        if not self._parent_widget:
            return
        
        rect = self._parent_widget.rect()
        x = rect.x() + self._margin[0] if isinstance(self._margin, tuple) else rect.x() + self._margin
        y = rect.y() + self._margin[1] if isinstance(self._margin, tuple) else rect.y() + self._margin
        w = rect.width() - (self._margin[0] + self._margin[2]) if isinstance(self._margin, tuple) else rect.width() - 2 * self._margin
        
        # Calculate total fixed height and stretch factors
        total_fixed = 0
        total_stretch = 0
        for item_type, item in self._items:
            if item_type == 'widget':
                total_fixed += item.geometry().height()
            elif item_type == 'layout':
                # Layout height calculation would go here
                pass
            elif item_type == 'spacing':
                total_fixed += item
            elif item_type == 'stretch':
                total_stretch += item
        
        available = rect.height() - total_fixed
        per_stretch = available / total_stretch if total_stretch > 0 else 0
        
        for item_type, item in self._items:
            if item_type == 'widget':
                item.setGeometry(x, y, w, item.geometry().height())
                y += item.geometry().height() + self._spacing
            elif item_type == 'spacing':
                y += item + self._spacing
            elif item_type == 'stretch':
                y += int(item * per_stretch) + self._spacing

class QHBoxLayout(QLayout):
    """Horizontal box layout"""
    def __init__(self, parent=None):
        super().__init__(parent)
    
    def activate(self):
        if not self._parent_widget:
            return
        
        rect = self._parent_widget.rect()
        x = rect.x() + self._margin[0] if isinstance(self._margin, tuple) else rect.x() + self._margin
        y = rect.y() + self._margin[1] if isinstance(self._margin, tuple) else rect.y() + self._margin
        h = rect.height() - (self._margin[1] + self._margin[3]) if isinstance(self._margin, tuple) else rect.height() - 2 * self._margin
        
        total_fixed = 0
        total_stretch = 0
        for item_type, item in self._items:
            if item_type == 'widget':
                total_fixed += item.geometry().width()
            elif item_type == 'spacing':
                total_fixed += item
            elif item_type == 'stretch':
                total_stretch += item
        
        available = rect.width() - total_fixed
        per_stretch = available / total_stretch if total_stretch > 0 else 0
        
        for item_type, item in self._items:
            if item_type == 'widget':
                item.setGeometry(x, y, item.geometry().width(), h)
                x += item.geometry().width() + self._spacing
            elif item_type == 'spacing':
                x += item + self._spacing
            elif item_type == 'stretch':
                x += int(item * per_stretch) + self._spacing

class QGridLayout(QLayout):
    """Grid layout"""
    def __init__(self, parent=None):
        super().__init__(parent)
        self._grid = {}  # (row, col) -> (item, rowspan, colspan)
        self._row_stretch = {}
        self._col_stretch = {}
    
    def addWidget(self, widget, row, col, rowspan=1, colspan=1):
        self._grid[(row, col)] = ('widget', widget, rowspan, colspan)
        widget._parent = self._parent_widget
    
    def addLayout(self, layout, row, col, rowspan=1, colspan=1):
        self._grid[(row, col)] = ('layout', layout, rowspan, colspan)
        layout._parent_widget = self._parent_widget
    
    def setRowStretch(self, row, stretch):
        self._row_stretch[row] = stretch
    
    def setColumnStretch(self, col, stretch):
        self._col_stretch[col] = stretch
    
    def activate(self):
        if not self._parent_widget:
            return
        # Grid layout implementation would go here
        pass

# ============================================================================
# Core Widgets
# ============================================================================

class QLabel(QWidget):
    """Text label widget"""
    def __init__(self, text="", parent=None):
        super().__init__(parent)
        self._text = text
        self._alignment = 0  # Qt.AlignLeft | Qt.AlignVCenter
        self._word_wrap = False
    
    def setText(self, text):
        self._text = text
        self.update()
    
    def text(self):
        return self._text
    
    def setAlignment(self, alignment):
        self._alignment = alignment
    
    def alignment(self):
        return self._alignment
    
    def setWordWrap(self, wrap):
        self._word_wrap = wrap
    
    def paintEvent(self, event):
        painter = QPainter(self)
        painter.begin(self)
        painter.setFont(self.font())
        painter.setPen(QPen(QColor(255, 255, 255)))
        painter.drawText(5, 5 + self.font().size(), self._text)
        painter.end()

class QPushButton(QWidget):
    """Push button widget"""
    clicked = "clicked"
    
    def __init__(self, text="", parent=None):
        super().__init__(parent)
        self._text = text
        self._pressed = False
        self._default = False
        self.setFixedSize(100, 30)
    
    def setText(self, text):
        self._text = text
        self.update()
    
    def text(self):
        return self._text
    
    def setDefault(self, default):
        self._default = default
    
    def isDefault(self):
        return self._default
    
    def mousePressEvent(self, event):
        if event.button() == 1:  # Left button
            self._pressed = True
            self.update()
    
    def mouseReleaseEvent(self, event):
        if self._pressed and event.button() == 1:
            self._pressed = False
            self.update()
            # Emit clicked signal
            self.emit(self.clicked)
    
    def paintEvent(self, event):
        painter = QPainter(self)
        painter.begin(self)
        
        rect = self.rect()
        
        # Button background
        if self._pressed:
            painter.fillRect(rect, QColor(100, 100, 150))
        elif self._default:
            painter.fillRect(rect, QColor(70, 70, 120))
        else:
            painter.fillRect(rect, QColor(60, 60, 80))
        
        # Border
        painter.setPen(QPen(QColor(150, 150, 150), 1))
        painter.drawRect(0, 0, rect.w - 1, rect.h - 1)
        
        # Text
        painter.setPen(QPen(QColor(255, 255, 255)))
        font = self.font()
        painter.setFont(font)
        text_w = len(self._text) * font.size() * 0.6
        text_x = (rect.w - text_w) // 2
        text_y = (rect.h + font.size()) // 2
        painter.drawText(text_x, text_y, self._text)
        
        painter.end()

class QLineEdit(QWidget):
    """Single-line text editor"""
    def __init__(self, text="", parent=None):
        super().__init__(parent)
        self._text = text
        self._cursor_pos = len(text)
        self._focus = False
        self.setFixedSize(200, 25)
    
    def setText(self, text):
        self._text = text
        self._cursor_pos = len(text)
        self.update()
    
    def text(self):
        return self._text
    
    def setFocus(self):
        super().setFocus()
        self._focus = True
    
    def keyPressEvent(self, event):
        if event.key() == 16777219:  # Backspace
            if self._cursor_pos > 0:
                self._text = self._text[:self._cursor_pos-1] + self._text[self._cursor_pos:]
                self._cursor_pos -= 1
                self.update()
        elif event.key() == 16777223:  # Delete
            if self._cursor_pos < len(self._text):
                self._text = self._text[:self._cursor_pos] + self._text[self._cursor_pos+1:]
                self.update()
        elif event.key() == 16777217:  # Left
            if self._cursor_pos > 0:
                self._cursor_pos -= 1
                self.update()
        elif event.key() == 16777218:  # Right
            if self._cursor_pos < len(self._text):
                self._cursor_pos += 1
                self.update()
        elif event.key() == 16777220:  # Home
            self._cursor_pos = 0
            self.update()
        elif event.key() == 16777221:  # End
            self._cursor_pos = len(self._text)
            self.update()
        elif 32 <= event.key() <= 126:  # Printable characters
            self._text = self._text[:self._cursor_pos] + chr(event.key()) + self._text[self._cursor_pos:]
            self._cursor_pos += 1
            self.update()
    
    def paintEvent(self, event):
        painter = QPainter(self)
        painter.begin(self)
        
        rect = self.rect()
        
        # Background
        if self._focus:
            painter.fillRect(rect, QColor(50, 50, 60))
        else:
            painter.fillRect(rect, QColor(40, 40, 50))
        
        # Border
        painter.setPen(QPen(QColor(100, 100, 100) if not self._focus else QColor(0, 120, 215), 1))
        painter.drawRect(0, 0, rect.w - 1, rect.h - 1)
        
        # Text
        painter.setPen(QPen(QColor(255, 255, 255)))
        painter.setFont(self.font())
        painter.drawText(4, (rect.h + self.font().size()) // 2, self._text)
        
        # Cursor
        if self._focus:
            cursor_x = 4 + int(self._cursor_pos * self.font().size() * 0.6)
            painter.setPen(QPen(QColor(255, 255, 255), 1))
            painter.drawLine(cursor_x, 3, cursor_x, rect.h - 4)
        
        painter.end()

# ============================================================================
# Application
# ============================================================================

class QCoreApplication(QObject):
    """Core application"""
    _instance = None
    
    def __new__(cls, *args, **kwargs):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
        return cls._instance
    
    def __init__(self, args=None):
        if hasattr(self, '_initialized'):
            return
        super().__init__()
        self._initialized = True
        self._args = args or sys.argv
        self._running = False
        self._widgets = []
    
    @staticmethod
    def instance():
        return QCoreApplication._instance
    
    def exec(self):
        """Main event loop"""
        self._running = True
        
        # Initialize Rust GUI
        fb_info = pymyos.gui_get_fb_info()
        width, height, pitch = fb_info
        pymyos.rust_gui_init(width, height)
        pymyos.rust_gui_init_desktop(width, height)
        
        # Initialize framebuffer
        # Note: In real implementation, we'd get the framebuffer from kernel
        
        # Event loop
        while self._running:
            # Process events
            pymyos.rust_gui_render_frame()
            pymyos.sleep(16)  # ~60 FPS
            
            # Check for exit condition
            # In real implementation, we'd check for quit signal
        
        return 0
    
    def quit(self):
        self._running = False
    
    def processEvents(self):
        """Process pending events"""
        pymyos.rust_gui_render_frame()
    
    def addWidget(self, widget):
        self._widgets.append(widget)
    
    def removeWidget(self, widget):
        if widget in self._widgets:
            self._widgets.remove(widget)

class QApplication(QCoreApplication):
    """GUI application"""
    def __init__(self, args=None):
        super().__init__(args)
        self._style = "fusion"
    
    def setStyle(self, style):
        self._style = style
    
    def style(self):
        return self._style
    
    def setFont(self, font):
        # Set application-wide font
        pass
    
    def font(self):
        return QFont()

# ============================================================================
# Events
# ============================================================================

class QEvent:
    """Base event class"""
    def __init__(self, type):
        self._type = type
        self._accepted = True
    
    def type(self):
        return self._type
    
    def accept(self):
        self._accepted = True
    
    def ignore(self):
        self._accepted = False
    
    def isAccepted(self):
        return self._accepted

class QResizeEvent(QEvent):
    def __init__(self, size):
        super().__init__("resize")
        self._size = size
    
    def size(self):
        return self._size

class QMouseEvent(QEvent):
    def __init__(self, type, pos, button=0, buttons=0, modifiers=0):
        super().__init__(type)
        self._pos = pos
        self._button = button
        self._buttons = buttons
        self._modifiers = modifiers
    
    def pos(self):
        return self._pos
    
    def button(self):
        return self._button
    
    def buttons(self):
        return self._buttons
    
    def modifiers(self):
        return self._modifiers

class QKeyEvent(QEvent):
    def __init__(self, type, key, text="", modifiers=0):
        super().__init__(type)
        self._key = key
        self._text = text
        self._modifiers = modifiers
    
    def key(self):
        return self._key
    
    def text(self):
        return self._text
    
    def modifiers(self):
        return self._modifiers

class QPaintEvent(QEvent):
    def __init__(self, rect):
        super().__init__("paint")
        self._rect = rect
    
    def rect(self):
        return self._rect

# ============================================================================
# Qt namespace constants
# ============================================================================

class Qt:
    """Qt namespace constants"""
    # Alignment
    AlignLeft = 0x0001
    AlignRight = 0x0002
    AlignHCenter = 0x0004
    AlignTop = 0x0020
    AlignBottom = 0x0040
    AlignVCenter = 0x0080
    AlignCenter = AlignHCenter | AlignVCenter
    
    # Window flags
    Widget = 0x00000000
    Window = 0x00000001
    Dialog = 0x00000002 | Window
    Sheet = 0x00000004 | Window
    Drawer = 0x00000006 | Window
    Popup = 0x00000008 | Window
    Tool = 0x0000000a | Window
    ToolTip = 0x0000000c | Window
    SplashScreen = 0x0000000e | Window
    
    # Window states
    WindowNoState = 0x00000000
    WindowMinimized = 0x00000001
    WindowMaximized = 0x00000002
    WindowFullScreen = 0x00000004
    WindowActive = 0x00000008
    
    # Key codes
    Key_Escape = 16777216
    Key_Tab = 16777217
    Key_Backtab = 16777218
    Key_Backspace = 16777219
    Key_Return = 16777220
    Key_Enter = 16777221
    Key_Insert = 16777222
    Key_Delete = 16777223
    Key_Pause = 16777224
    Key_Print = 16777225
    Key_SysReq = 16777226
    Key_Clear = 16777227
    Key_Home = 16777228
    Key_End = 16777229
    Key_Left = 16777230
    Key_Up = 16777231
    Key_Right = 16777232
    Key_Down = 16777233
    Key_PageUp = 16777234
    Key_PageDown = 16777235
    Key_Shift = 16777236
    Key_Control = 16777237
    Key_Meta = 16777238
    Key_Alt = 16777239
    Key_CapsLock = 16777240
    Key_NumLock = 16777241
    Key_ScrollLock = 16777242
    Key_F1 = 16777243
    Key_F2 = 16777244
    Key_F3 = 16777245
    Key_F4 = 16777246
    Key_F5 = 16777247
    Key_F6 = 16777248
    Key_F7 = 16777249
    Key_F8 = 16777250
    Key_F9 = 16777251
    Key_F10 = 16777252
    Key_F11 = 16777253
    Key_F12 = 16777254
    
    # Mouse buttons
    NoButton = 0x00000000
    LeftButton = 0x00000001
    RightButton = 0x00000002
    MiddleButton = 0x00000004
    
    # Orientations
    Horizontal = 0x1
    Vertical = 0x2

# ============================================================================
# Signal/Slot mechanism (simplified)
# ============================================================================

def connect(sender, signal, receiver, slot):
    """Connect signal to slot"""
    sender.connect(signal, slot)

def disconnect(sender, signal, receiver, slot):
    """Disconnect signal from slot"""
    sender.disconnect(signal, slot)

# ============================================================================
# Internal pymyos GUI functions (to be implemented in C)
# ============================================================================

# These will be added to the pymyos C module
# For now, we'll define stubs that will be replaced by C implementations

def _gui_draw_rect(surface, x, y, w, h, color, width):
    """Draw rectangle on surface"""
    pass

def _gui_fill_rect(surface, x, y, w, h, color):
    """Fill rectangle on surface"""
    pass

def _gui_draw_line(surface, x1, y1, x2, y2, color, width):
    """Draw line on surface"""
    pass

def _gui_draw_text(surface, x, y, text, color, font_size):
    """Draw text on surface"""
    pass

def _gui_draw_ellipse(surface, cx, cy, rx, ry, color, width):
    """Draw ellipse on surface"""
    pass

def _gui_set_focus(window_id):
    """Set window focus"""
    pass

# Attach internal functions to pymyos module
pymyos._gui_draw_rect = _gui_draw_rect
pymyos._gui_fill_rect = _gui_fill_rect
pymyos._gui_draw_line = _gui_draw_line
pymyos._gui_draw_text = _gui_draw_text
pymyos._gui_draw_ellipse = _gui_draw_ellipse
pymyos._gui_set_focus = _gui_set_focus

# ============================================================================
# Public API
# ============================================================================

__all__ = [
    # Core
    'QRect', 'QSize', 'QPoint', 'QColor',
    # Paint
    'QPaintDevice', 'QPainter', 'QPen', 'QBrush', 'QFont',
    # Surfaces
    'QSurface',
    # Widgets
    'QObject', 'QWidget', 'QLabel', 'QPushButton', 'QLineEdit',
    # Layouts
    'QLayout', 'QVBoxLayout', 'QHBoxLayout', 'QGridLayout',
    # Application
    'QCoreApplication', 'QApplication',
    # Events
    'QEvent', 'QResizeEvent', 'QMouseEvent', 'QKeyEvent', 'QPaintEvent',
    # Constants
    'Qt',
    # Signal/Slot
    'connect', 'disconnect',
]