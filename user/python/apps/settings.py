#!/usr/bin/env python3
"""
MyOS Python Settings Application
Demonstrates PyQt-like bindings for MyOS
"""

import sys
sys.path.insert(0, '/user/python')

from qt import *

class SettingsApp(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Settings")
        self.setGeometry(100, 100, 500, 400)
        self._init_ui()
    
    def _init_ui(self):
        layout = QVBoxLayout(self)
        
        # Title
        title = QLabel("MyOS Settings")
        title.setFont(QFont("DejaVu Sans", 18))
        layout.addWidget(title)
        
        # Settings sections
        sections = [
            ("Display", [
                ("Resolution", "1920x1080"),
                ("Refresh Rate", "60 Hz"),
                ("Scale", "100%"),
            ]),
            ("Audio", [
                ("Volume", "50%"),
                ("Output Device", "Speakers"),
                ("Input Device", "Microphone"),
            ]),
            ("Network", [
                ("Interface", "eth0"),
                ("DHCP", "Enabled"),
                ("IP Address", "192.168.1.100"),
            ]),
            ("System", [
                ("Language", "English"),
                ("Timezone", "UTC"),
                ("Auto-update", "Enabled"),
            ]),
        ]
        
        for section_name, items in sections:
            # Section header
            header = QLabel(section_name)
            header.setFont(QFont("DejaVu Sans", 14))
            layout.addWidget(header)
            
            # Settings items
            for name, value in items:
                item_layout = QHBoxLayout()
                
                label = QLabel(name)
                label.setFixedWidth(150)
                item_layout.addWidget(label)
                
                value_label = QLabel(value)
                item_layout.addWidget(value_label)
                
                item_layout.addStretch()
                
                edit_btn = QPushButton("Edit")
                edit_btn.setFixedSize(60, 25)
                item_layout.addWidget(edit_btn)
                
                layout.addLayout(item_layout)
            
            # Separator
            sep = QLabel("─" * 50)
            layout.addWidget(sep)
        
        # Buttons
        btn_layout = QHBoxLayout()
        btn_layout.addStretch()
        
        apply_btn = QPushButton("Apply")
        apply_btn.clicked.connect(lambda: self._apply())
        btn_layout.addWidget(apply_btn)
        
        cancel_btn = QPushButton("Cancel")
        cancel_btn.clicked.connect(lambda: self._cancel())
        btn_layout.addWidget(cancel_btn)
        
        ok_btn = QPushButton("OK")
        ok_btn.setDefault(True)
        ok_btn.clicked.connect(lambda: self._ok())
        btn_layout.addWidget(ok_btn)
        
        layout.addLayout(btn_layout)
        self.setLayout(layout)
    
    def _apply(self):
        print("Settings applied")
    
    def _cancel(self):
        print("Settings cancelled")
    
    def _ok(self):
        print("Settings OK")
        # In real app, would close window

def main():
    app = QApplication(sys.argv)
    
    settings = SettingsApp()
    settings.show()
    
    return app.exec()

if __name__ == "__main__":
    main()