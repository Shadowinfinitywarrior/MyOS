#!/usr/bin/env python3
"""
MyOS Python Calculator Application
Demonstrates PyQt-like bindings for MyOS
"""

import sys
sys.path.insert(0, '/user/python')

from qt import *

class Calculator(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Calculator")
        self.setGeometry(100, 100, 300, 400)
        self._expression = ""
        self._display = None
        self._init_ui()
    
    def _init_ui(self):
        layout = QVBoxLayout(self)
        
        # Display
        self._display = QLineEdit()
        self._display.setFixedSize(280, 50)
        font = QFont("DejaVu Sans Mono", 24)
        self._display.setFont(font)
        layout.addWidget(self._display)
        
        # Button grid
        grid = QGridLayout()
        buttons = [
            ('7', 0, 0), ('8', 0, 1), ('9', 0, 2), ('/', 0, 3),
            ('4', 1, 0), ('5', 1, 1), ('6', 1, 2), ('*', 1, 3),
            ('1', 2, 0), ('2', 2, 1), ('3', 2, 2), ('-', 2, 3),
            ('0', 3, 0), ('.', 3, 1), ('=', 3, 2), ('+', 3, 3),
            ('C', 4, 0), ('CE', 4, 1), ('<-', 4, 2), ('%', 4, 3),
        ]
        
        for text, row, col in buttons:
            btn = QPushButton(text)
            btn.setFixedSize(60, 60)
            btn.clicked.connect(lambda checked, t=text: self._on_button(t))
            grid.addWidget(btn, row, col)
        
        layout.addLayout(grid)
        self.setLayout(layout)
    
    def _on_button(self, text):
        if text == 'C':
            self._expression = ""
            self._display.setText("")
        elif text == 'CE':
            self._expression = ""
            self._display.setText("")
        elif text == '<-':
            self._expression = self._expression[:-1]
            self._display.setText(self._expression)
        elif text == '=':
            try:
                result = eval(self._expression)
                self._display.setText(str(result))
                self._expression = str(result)
            except:
                self._display.setText("Error")
                self._expression = ""
        else:
            self._expression += text
            self._display.setText(self._expression)

def main():
    app = QApplication(sys.argv)
    
    calc = Calculator()
    calc.show()
    
    return app.exec()

if __name__ == "__main__":
    main()