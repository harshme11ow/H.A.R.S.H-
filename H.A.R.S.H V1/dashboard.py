import sys
import serial
from PySide6.QtWidgets import QApplication, QMainWindow, QLabel, QVBoxLayout, QWidget
from PySide6.QtCore import QTimer, Qt
from PySide6.QtGui import QFont

# CHANGE THIS to your ESP32's COM port
COM_PORT = 'COM10'
BAUD_RATE = 115200

class OBDDashboard(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Honda Accord Telemetry")
        self.resize(500, 300)
        
        # Dark mode styling for a dashboard look
        self.setStyleSheet("background-color: #121212; color: #00E676;")
        
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        layout = QVBoxLayout(central_widget)
        
        # RPM Gauge Label
        self.rpm_label = QLabel("RPM: ----")
        self.rpm_label.setFont(QFont("Consolas", 48, QFont.Bold))
        self.rpm_label.setAlignment(Qt.AlignCenter)
        layout.addWidget(self.rpm_label)
        
        # Speed Gauge Label
        self.speed_label = QLabel("SPEED: -- KPH")
        self.speed_label.setFont(QFont("Consolas", 40, QFont.Bold))
        self.speed_label.setAlignment(Qt.AlignCenter)
        layout.addWidget(self.speed_label)
        
        # Initialize Serial Connection
        try:
            # timeout=0 makes it non-blocking
            self.ser = serial.Serial(COM_PORT, BAUD_RATE, timeout=0) 
        except Exception as e:
            self.rpm_label.setText("PORT ERROR")
            self.speed_label.setText(f"Check {COM_PORT}")
            self.setStyleSheet("background-color: #121212; color: #FF3333;")
            self.ser = None
            
        # Set up a timer to constantly check the serial port without freezing the GUI
        self.timer = QTimer()
        self.timer.timeout.connect(self.update_data)
        self.timer.start(50) # Polls every 50 milliseconds
        
    def update_data(self):
        if not self.ser:
            return
            
        try:
            # Read all available lines in the buffer
            while self.ser.in_waiting:
                line = self.ser.readline().decode('utf-8').strip()
                
                # Parse the strings being sent from your ESP32
                if "Engine RPM:" in line:
                    val = line.split(":")[1].strip()
                    self.rpm_label.setText(f"RPM: {val}")
                elif "Speed (KPH):" in line:
                    val = line.split(":")[1].strip()
                    self.speed_label.setText(f"SPEED: {val} KPH")
        except Exception as e:
            pass

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = OBDDashboard()
    window.show()
    sys.exit(app.exec())