import sys
import serial
from PySide6.QtWidgets import (QApplication, QMainWindow, QLabel, QVBoxLayout, 
                               QHBoxLayout, QWidget, QProgressBar)
from PySide6.QtCore import QTimer, Qt
from PySide6.QtGui import QFont

# CHANGE THIS if your COM port changed!
COM_PORT = 'COM8'
BAUD_RATE = 115200

class OBDDashboard(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Honda Accord Telemetry")
        self.resize(550, 400)
        
        # Dark mode styling for the window and the progress bars
        self.setStyleSheet("""
            QMainWindow { background-color: #121212; color: #00E676; }
            QLabel { color: #00E676; }
            QProgressBar {
                border: 2px solid #333333;
                border-radius: 5px;
                text-align: center;
                color: white;
                background-color: #1e1e1e;
                font-weight: bold;
            }
            QProgressBar::chunk {
                background-color: #00E676;
                width: 10px;
            }
        """)
        
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout(central_widget)
        main_layout.setSpacing(20)
        
        # --- TOP SECTION: TEXT GAUGES ---
        self.rpm_label = QLabel("RPM: ----")
        self.rpm_label.setFont(QFont("Consolas", 48, QFont.Bold))
        self.rpm_label.setAlignment(Qt.AlignCenter)
        main_layout.addWidget(self.rpm_label)
        
        self.speed_label = QLabel("SPEED: -- KPH")
        self.speed_label.setFont(QFont("Consolas", 36, QFont.Bold))
        self.speed_label.setAlignment(Qt.AlignCenter)
        main_layout.addWidget(self.speed_label)
        
        # --- MIDDLE SECTION: ENGINE LOAD BAR ---
        load_layout = QHBoxLayout()
        self.load_text = QLabel("LOAD: --.- %")
        self.load_text.setFont(QFont("Consolas", 18, QFont.Bold))
        self.load_text.setFixedWidth(180)
        
        self.load_bar = QProgressBar()
        self.load_bar.setRange(0, 100)
        self.load_bar.setValue(0)
        self.load_bar.setFixedHeight(30)
        
        load_layout.addWidget(self.load_text)
        load_layout.addWidget(self.load_bar)
        main_layout.addLayout(load_layout)
        
        # --- BOTTOM SECTION: BATTERY VOLTAGE BAR ---
        volt_layout = QHBoxLayout()
        self.volt_text = QLabel("BATT: --.- V")
        self.volt_text.setFont(QFont("Consolas", 18, QFont.Bold))
        self.volt_text.setFixedWidth(180)
        
        self.volt_bar = QProgressBar()
        # Scale 10.0V - 15.0V to integers 100 - 150 for smooth rendering
        self.volt_bar.setRange(100, 150) 
        self.volt_bar.setValue(100)
        self.volt_bar.setFixedHeight(30)
        self.volt_bar.setFormat("%v V") # We will override this text manually
        
        volt_layout.addWidget(self.volt_text)
        volt_layout.addWidget(self.volt_bar)
        main_layout.addLayout(volt_layout)
        
        # --- SERIAL CONNECTION ---
        try:
            self.ser = serial.Serial(COM_PORT, BAUD_RATE, timeout=0) 
        except Exception as e:
            self.rpm_label.setText("PORT ERROR")
            self.speed_label.setText(f"Check {COM_PORT}")
            self.setStyleSheet("QMainWindow { background-color: #121212; } QLabel { color: #FF3333; }")
            self.ser = None
            
        # Data polling loop
        self.timer = QTimer()
        self.timer.timeout.connect(self.update_data)
        self.timer.start(50) 
        
    def update_data(self):
        if not self.ser:
            return
            
        try:
            while self.ser.in_waiting:
                line = self.ser.readline().decode('utf-8').strip()
                
                if "Engine RPM:" in line:
                    val = line.split(":")[1].strip()
                    self.rpm_label.setText(f"RPM: {val}")
                    
                elif "Speed (KPH):" in line:
                    val = line.split(":")[1].strip()
                    self.speed_label.setText(f"SPEED: {val} KPH")
                    
                elif "Engine Load (%):" in line:
                    val_float = float(line.split(":")[1].strip())
                    self.load_text.setText(f"LOAD: {val_float:.1f} %")
                    self.load_bar.setValue(int(val_float))
                    # Hide the default progress bar percentage text
                    self.load_bar.setFormat("") 
                    
                elif "Battery (V):" in line:
                    val_float = float(line.split(":")[1].strip())
                    self.volt_text.setText(f"BATT: {val_float:.1f} V")
                    # Multiply by 10 to fit our 100-150 integer scale
                    self.volt_bar.setValue(int(val_float * 10))
                    self.volt_bar.setFormat("")
                    
        except Exception as e:
            pass

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = OBDDashboard()
    window.show()
    sys.exit(app.exec())