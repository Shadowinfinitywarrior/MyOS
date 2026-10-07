#!/usr/bin/env python3
"""
MyOS Python Text Editor Application
Demonstrates PyQt-like bindings for MyOS
"""

import sys
sys.path.insert(0, '/user/python')

from qt import *

class TextEditor(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Text Editor")
        self.setGeometry(100, 100, 600, 400)
        self._text = ""
        self._cursor_pos = 0
        self._init_ui()
    
    def _init_ui(self):
        layout = QVBoxLayout(self)
        
        # Toolbar
        toolbar = QHBoxLayout()
        
        new_btn = QPushButton("New")
        new_btn.clicked.connect(lambda: self._new_file())
        toolbar.addWidget(new_btn)
        
        open_btn = QPushButton("Open")
        open_btn.clicked.connect(lambda: self._open_file())
        toolbar.addWidget(open_btn)
        
        save_btn = QPushButton("Save")
        save_btn.clicked.connect(lambda: self._save_file())
        toolbar.addWidget(save_btn)
        
        toolbar.addStretch()
        
        layout.addLayout(toolbar)
        
        # Text area (simplified - just a label for now)
        self._text_label = QLabel()
        self._text_label.setFont(QFont("DejaVu Sans Mono", 12))
        layout.addWidget(self._text_label)
        
        # Input line for typing
        self._input = QLineEdit()
        self._input.setFixedSize(580, 30)
        font = QFont("DejaVu Sans Mono", 14)
        self._input.setFont(font)
        self._input.keyPressEvent = self._on_key
        layout.addWidget(self._input)
        
        self.setLayout(layout)
    
    def _new_file(self):
        self._text = ""
        self._cursor_pos = 0
        self._update_display()
    
    def _open_file(self):
        # Placeholder - would use file dialog
        pass
    
    def _save_file(self):
        # Placeholder - would use file dialog
        pass
    
    def _on_key(self, event):
        if event.key() == Qt.Key_Return:
            self._text += "\n"
            self._cursor_pos = len(self._text)
            self._input.setText("")
            self._update_display()
        elif event.key() == Qt.Key_Backspace:
            if self._cursor_pos > 0:
                self._text = self._text[:self._cursor_pos-1] + self._text[self._cursor_pos:]
                self._cursor_pos -= 1
                self._update_display()
        elif event.key() == Qt.Key_Left:
            if self._cursor_pos > 0:
                self._cursor_pos -= 1
        elif event.key() == Qt.Key_Right:
            if self._cursor_pos < len(self._text):
                self._cursor_pos += 1
        elif 32 <= event.key() <= 126:
            self._text = self._text[:self._cursor_pos] + chr(event.key()) + self._text[self._cursor_pos:]
            self._cursor_pos += 1
            self._update_display()
    
    def _update_display(self):
        # Show last 20 lines
        lines = self._text.split('\n')
        display_text = '\n'.join(lines[-20:])
        self._text_label.setText(display_text)

def main():
    app = QApplication(sys.argv)
    
    editor = TextEditor()
    editor.show()
    
    return app.exec()

if __name__ == "__main__":
    main()