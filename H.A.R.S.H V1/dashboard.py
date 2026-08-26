import sys
import serial
from PySide6.QtWidgets import (QApplication, QMainWindow, QLabel, QVBoxLayout, 
                               QHBoxLayout, QWidget, QProgressBar)
from PySide6.QtCore import QTimer, Qt
from PySide6.QtGui import QFont

# CHANGE THIS if your COM port changed!
COM_PORT = 'COM10'
BAUD_RATE = 115200

class OBDDashboard(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Honda Accord Telemetry")
        self.resize(550, 420) # Made the window slightly taller for the status bar
        
        # Dark mode styling 
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
        self.volt_bar.setRange(100, 150) 
        self.volt_bar.setValue(100)
        self.volt_bar.setFixedHeight(30)
        self.volt_bar.setFormat("%v V") 
        
        volt_layout.addWidget(self.volt_text)
        volt_layout.addWidget(self.volt_bar)
        main_layout.addLayout(volt_layout)
        
        # --- STATUS INDICATOR ---
        self.status_label = QLabel("⚪ Waiting for ESP32...")
        self.status_label.setFont(QFont("Consolas", 12, QFont.Bold))
        self.status_label.setAlignment(Qt.AlignRight)
        self.status_label.setStyleSheet("color: #888888;") # Default gray text
        main_layout.addWidget(self.status_label)

        # --- SERIAL CONNECTION ---
        try:
            self.ser = serial.Serial(COM_PORT, BAUD_RATE, timeout=0) 
        except Exception as e:
            self.rpm_label.setText("PORT ERROR")
            self.speed_label.setText(f"Check {COM_PORT}")
            self.status_label.setText("🔴 Serial Port Disconnected")
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
                
                # Check for explicit failure strings during the boot process
                if "Couldn't connect to OBD scanner" in line or "Couldn't initialize" in line:
                    self.status_label.setText("🔴 ELM327 BT Connection Failed")
                    self.status_label.setStyleSheet("color: #FF3333;")
                
                # If we get ANY normal looping strings, the ESP32 is successfully linked to the ELM
                elif line and not line.startswith("Attempting"):
                    self.status_label.setText("🟢 ELM327 Connected")
                    self.status_label.setStyleSheet("color: #00E676;")

                # Parse specific data strings
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
                    self.load_bar.setFormat("") 
                    
                elif "Battery (V):" in line:
                    val_float = float(line.split(":")[1].strip())
                    self.volt_text.setText(f"BATT: {val_float:.1f} V")
                    self.volt_bar.setValue(int(val_float * 10))
                    self.volt_bar.setFormat("")
                    
        except Exception as e:
            pass

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = OBDDashboard()
    window.show()
    sys.exit(app.exec())