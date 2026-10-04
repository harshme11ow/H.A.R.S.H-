#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "driver/twai.h" // Native ESP32 CAN library

// --- OLED & MULTIPLEXER CONFIG ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
#define TCAADDR 0x70 

// --- CAN TRANSCEIVER PINS ---
#define CAN_TX_PIN 5
#define CAN_RX_PIN 4

// Global data storage (Continuously updated by the CAN bus)
float valRPM = 0, valSpeed = 0, valMap = 0;
float valLoad = 0, valFuel = 0, valVolt = 0;
float valThrottle = 0, valCoolant = 0, valIAT = 0, valOil = 0; // Parsed but hidden for now

unsigned long lastDisplayUpdate = 0;

// Hardware I2C routing function
void tcaselect(uint8_t i) {
  if (i > 7) return;
  Wire.beginTransmission(TCAADDR);
  Wire.write(1 << i);
  Wire.endTransmission();
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000); 
  
  // 1. Boot all 3 screens
  for(int i = 0; i < 3; i++) {
    tcaselect(i);
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
      Serial.print("OLED init failed on channel "); Serial.println(i);
    }
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(2);
    display.setCursor(10, 20);
    display.print("BOOTING...");
    display.display();
  }

  // 2. Configure and Start the CAN Driver (500 kbps to match the dummy/vehicle)
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX_PIN, (gpio_num_t)CAN_RX_PIN, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
    Serial.println("CAN Driver Installed.");
  } else {
    Serial.println("CAN Driver Install Failed.");
    while(1);
  }

  if (twai_start() == ESP_OK) {
    Serial.println("CAN Sniffing Started!");
  }
}

void updateScreens() {
  // --- SCREEN 1 (Channel 0): Main Dash ---
  tcaselect(0);
  display.clearDisplay();
  
  display.setCursor(0, 5);
  display.setTextSize(3); 
  display.print((uint32_t)valSpeed); 
  display.setTextSize(1);
  display.println(" MPH");
  
  display.setCursor(0, 40);
  display.setTextSize(2);
  display.print((uint32_t)valRPM); 
  display.println(" RPM");
  display.display();

  // --- SCREEN 2 (Channel 1): The Segmented Boost Gauge ---
  tcaselect(1);
  display.clearDisplay();
  
  float boostPSI = (valMap - 101.325) * 0.145038;

  // 1. Draw the segmented radial arc (-10 to 20 PSI)
  int num_segments = 20; 
  int active_segments = map(boostPSI, -10, 20, 0, num_segments);
  active_segments = constrain(active_segments, 0, num_segments);

  for (int i = 0; i < num_segments; i++) {
    float angle = 3.14159 - (i * (3.14159 / (num_segments - 1)));
    
    int x_in = 64 + 42 * cos(angle);
    int y_in = 58 - 42 * sin(angle);
    int x_out = 64 + 56 * cos(angle);
    int y_out = 58 - 56 * sin(angle);

    if (i < active_segments) {
      for(int w = -1; w <= 1; w++) {
        display.drawLine(x_in + w, y_in, x_out + w, y_out, WHITE);
        display.drawLine(x_in, y_in + w, x_out, y_out + w, WHITE);
      }
    } else {
      display.drawLine(x_in, y_in, x_out, y_out, WHITE);
    }
  }

  // 2. Draw the large centered digital readout
  display.setTextSize(3);
  if (boostPSI <= -10 || boostPSI >= 10) { display.setCursor(16, 20); } 
  else if (boostPSI < 0) { display.setCursor(25, 20); }
  else { display.setCursor(34, 20); }
  display.print(boostPSI, 1);
  
  // 3. Draw the PSI label
  display.setTextSize(1);
  display.setCursor(55, 48);
  display.print("PSI");
  
  display.display();

  // --- SCREEN 3 (Channel 2): Refined Telemetry ---
  tcaselect(2);
  display.clearDisplay();
  display.setTextSize(2);
  
  display.setCursor(0, 0);  display.print("LOD:"); display.print((int)valLoad); display.println("%");
  display.setCursor(0, 22); display.print("FUL:"); display.print((int)valFuel); display.println("%");
  display.setCursor(0, 44); display.print("BAT:"); display.print(valVolt, 1); display.println("V");
  display.display();
}

void loop() {
  // Update screens exactly 5 times a second
  if (millis() - lastDisplayUpdate >= 200) {
    updateScreens();
    lastDisplayUpdate = millis();
  }

  // --- THE NEW NON-BLOCKING CAN SNIFFER ---
  twai_message_t message;
  
  // Check the CAN buffer as fast as the ESP32 loop can run
  while (twai_receive(&message, 0) == ESP_OK) {
    // If it's a standard OBD-II engine response (0x7E8) and Mode is 41 (Success)
    if (message.identifier == 0x7E8 && message.data[1] == 0x41) {
      uint8_t pid = message.data[2];
      uint8_t A = message.data[3];
      uint8_t B = message.data[4];

      // Route the raw data bytes into standard car metrics
      switch (pid) {
        case 0x0C: valRPM = ((A * 256.0) + B) / 4.0; break;
        case 0x0D: valSpeed = A * 0.621371; break; // Raw CAN is km/h, convert to MPH
        case 0x0B: valMap = A; break;
        case 0x04: valLoad = (A * 100.0) / 255.0; break;
        case 0x2F: valFuel = (A * 100.0) / 255.0; break;
        case 0x42: valVolt = ((A * 256.0) + B) / 1000.0; break;
        case 0x11: valThrottle = (A * 100.0) / 255.0; break;
        case 0x05: valCoolant = A - 40; break;
        case 0x0F: valIAT = A - 40; break;
        case 0x5C: valOil = A - 40; break;
      }
    }
  }
}