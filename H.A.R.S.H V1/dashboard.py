import sys
import serial
from PySide6.QtWidgets import (QApplication, QMainWindow, QLabel, QVBoxLayout, 
                               QHBoxLayout, QWidget, QProgressBar)
from PySide6.QtCore import QTimer, Qt
from PySide6.QtGui import QFont

COM_PORT = 'COM8'
BAUD_RATE = 115200

class OBDDashboard(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Honda Accord Telemetry")
        self.resize(550, 650) # Taller window for 7 bars total
        
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
        main_layout.setSpacing(10)
        
        # --- TEXT GAUGES ---
        self.rpm_label = QLabel("RPM: ----")
        self.rpm_label.setFont(QFont("Consolas", 48, QFont.Bold))
        self.rpm_label.setAlignment(Qt.AlignCenter)
        main_layout.addWidget(self.rpm_label)
        
        self.speed_label = QLabel("SPEED: -- MPH")
        self.speed_label.setFont(QFont("Consolas", 32, QFont.Bold))
        self.speed_label.setAlignment(Qt.AlignCenter)
        main_layout.addWidget(self.speed_label)
        
        # --- BAR BUILDER HELPER ---
        def create_bar_row(label_text, min_val, max_val):
            layout = QHBoxLayout()
            text_label = QLabel(label_text)
            text_label.setFont(QFont("Consolas", 14, QFont.Bold))
            text_label.setFixedWidth(200)
            
            bar = QProgressBar()
            bar.setRange(min_val, max_val)
            bar.setValue(min_val)
            bar.setFixedHeight(22)
            bar.setFormat("") 
            
            layout.addWidget(text_label)
            layout.addWidget(bar)
            main_layout.addLayout(layout)
            return text_label, bar

        # --- PROGRESS BARS ---
        self.load_text, self.load_bar       = create_bar_row("LOAD: --.- %", 0, 100)
        self.volt_text, self.volt_bar       = create_bar_row("BATT: --.- V", 100, 150)
        self.coolant_text, self.coolant_bar = create_bar_row("COOLANT: --- °F", 100, 250)
        self.boost_text, self.boost_bar     = create_bar_row("BOOST: --.- PSI", -15, 25)
        
        # NEW BARS
        self.fuel_text, self.fuel_bar       = create_bar_row("FUEL: --.- %", 0, 100)
        self.oil_text, self.oil_bar         = create_bar_row("OIL TMP: --- °F", 100, 300)
        self.throttle_text, self.throttle_bar = create_bar_row("THROTTLE: --.- %", 0, 100)
        
        # --- STATUS INDICATOR ---
        self.status_label = QLabel("⚪ Waiting for ESP32...")
        self.status_label.setFont(QFont("Consolas", 12, QFont.Bold))
        self.status_label.setAlignment(Qt.AlignRight)
        self.status_label.setStyleSheet("color: #888888;")
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
            
        self.timer = QTimer()
        self.timer.timeout.connect(self.update_data)
        self.timer.start(50) 
        
    def update_data(self):
        if not self.ser:
            return
            
        try:
            while self.ser.in_waiting:
                line = self.ser.readline().decode('utf-8').strip()
                
                if "Couldn't connect to OBD scanner" in line or "Couldn't initialize" in line:
                    self.status_label.setText("🔴 ELM327 BT Connection Failed")
                    self.status_label.setStyleSheet("color: #FF3333;")
                elif line and not line.startswith("Attempting"):
                    self.status_label.setText("🟢 ELM327 Connected")
                    self.status_label.setStyleSheet("color: #00E676;")

                # Parse data strings
                if "Engine RPM:" in line:
                    val = line.split(":")[1].strip()
                    self.rpm_label.setText(f"RPM: {val}")
                    
                elif "Speed (MPH):" in line:
                    val = line.split(":")[1].strip()
                    self.speed_label.setText(f"SPEED: {val} MPH")
                    
                elif "Engine Load (%):" in line:
                    val_float = float(line.split(":")[1].strip())
                    self.load_text.setText(f"LOAD: {val_float:.1f} %")
                    self.load_bar.setValue(int(val_float))
                    
                elif "Battery (V):" in line:
                    val_float = float(line.split(":")[1].strip())
                    self.volt_text.setText(f"BATT: {val_float:.1f} V")
                    self.volt_bar.setValue(int(val_float * 10))

                elif "Coolant (C):" in line:
                    celsius = float(line.split(":")[1].strip())
                    fahrenheit = (celsius * 9/5) + 32
                    self.coolant_text.setText(f"COOLANT: {int(fahrenheit)} °F")
                    self.coolant_bar.setValue(int(fahrenheit))

                elif "MAP (kPa):" in line:
                    map_kpa = float(line.split(":")[1].strip())
                    boost_psi = (map_kpa - 101.325) * 0.145038
                    self.boost_text.setText(f"BOOST: {boost_psi:.1f} PSI")
                    self.boost_bar.setValue(int(boost_psi))
                    
                # Parse NEW strings
                elif "Fuel (%):" in line:
                    val_float = float(line.split(":")[1].strip())
                    self.fuel_text.setText(f"FUEL: {val_float:.1f} %")
                    self.fuel_bar.setValue(int(val_float))
                    
                elif "Oil Temp (C):" in line:
                    celsius = float(line.split(":")[1].strip())
                    fahrenheit = (celsius * 9/5) + 32
                    self.oil_text.setText(f"OIL TMP: {int(fahrenheit)} °F")
                    self.oil_bar.setValue(int(fahrenheit))
                    
                elif "Throttle (%):" in line:
                    val_float = float(line.split(":")[1].strip())
                    self.throttle_text.setText(f"THROTTLE: {val_float:.1f} %")
                    self.throttle_bar.setValue(int(val_float))
                    
        except Exception as e:
            pass

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = OBDDashboard()
    window.show()
    sys.exit(app.exec())