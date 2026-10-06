#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "BluetoothSerial.h"
#include "ELMduino.h"

// --- OLED & MULTIPLEXER CONFIG ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
#define TCAADDR 0x70 

// --- OBD CONFIG ---
BluetoothSerial SerialBT;
#define ELM_PORT SerialBT
ELM327 myELM327;
const char* ELM_NAME = "OBDII"; 

// Asymmetric state machine
typedef enum { STATE_RPM, STATE_SPEED, STATE_SLOW_POLL } obd_pid_states;
obd_pid_states obd_state = STATE_RPM;
int slow_metric_step = 0; 

// Global data storage
float valRPM = 0, valSpeed = 0, valLoad = 0, valVolt = 0;
float valCoolant = 0, valMap = 0, valFuel = 0, valOil = 0, valThrottle = 0;

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
  
  // Boot all 3 screens
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

  // Connect to ELM327
  ELM_PORT.begin("ESP32_OBD_Client", true); 
  if (!ELM_PORT.connect(ELM_NAME)) {
    Serial.println("BT Connection Failed!");
    while(1);
  }
  
  // Using auto-protocol negotiation so the Accord ECU doesn't panic
  if (!myELM327.begin(ELM_PORT, true, 2000)) {
    Serial.println("ELM Initialization Failed!");
    while (1);
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

  // --- SCREEN 2 (Channel 1): Vitals ---
  tcaselect(1);
  display.clearDisplay();
  display.setTextSize(1);
  
  float boostPSI = (valMap - 101.325) * 0.145038;
  int coolantF = (valCoolant * 9/5) + 32;
  int oilF = (valOil * 9/5) + 32;

  display.setCursor(0, 5);  display.print("BOOST:   "); display.print(boostPSI, 1); display.println(" PSI");
  display.setCursor(0, 25); display.print("COOLANT: "); display.print(coolantF); display.println(" F");
  display.setCursor(0, 45); display.print("OIL:     "); display.print(oilF); display.println(" F");
  display.display();

  // --- SCREEN 3 (Channel 2): Telemetry ---
  tcaselect(2);
  display.clearDisplay();
  display.setCursor(0, 0);  display.print("LOAD: "); display.print(valLoad, 1); display.println("%");
  display.setCursor(0, 16); display.print("THR:  "); display.print(valThrottle, 1); display.println("%");
  display.setCursor(0, 32); display.print("FUEL: "); display.print(valFuel, 1); display.println("%");
  display.setCursor(0, 48); display.print("BATT: "); display.print(valVolt, 1); display.println("V");
  display.display();
}

void loop() {
  // Refresh all screens at 5 FPS
  if (millis() - lastDisplayUpdate >= 200) {
    updateScreens();
    lastDisplayUpdate = millis();
  }

  // --- ASYMMETRIC OBD STATE MACHINE ---
  switch (obd_state) {
    case STATE_RPM: {
      float temp = myELM327.rpm(); 
      if (myELM327.nb_rx_state == ELM_SUCCESS) {
        valRPM = temp;
        Serial.print("Engine RPM: "); Serial.println(valRPM);
        obd_state = STATE_SPEED; 
      } else if (myELM327.nb_rx_state != ELM_GETTING_MSG) obd_state = STATE_SPEED;
      break;
    }
      
    case STATE_SPEED: {
      float temp = myELM327.mph();
      if (myELM327.nb_rx_state == ELM_SUCCESS) {
        valSpeed = temp;
        Serial.print("Speed (MPH): "); Serial.println(valSpeed);
        obd_state = STATE_SLOW_POLL; 
      } else if (myELM327.nb_rx_state != ELM_GETTING_MSG) obd_state = STATE_SLOW_POLL;
      break;
    }

    case STATE_SLOW_POLL: {
      // Cycle through one background metric, then instantly bounce back to RPM
      switch (slow_metric_step) {
        case 0: {
          float temp = myELM327.engineLoad();
          if (myELM327.nb_rx_state == ELM_SUCCESS) { valLoad = temp; Serial.print("Engine Load (%): "); Serial.println(valLoad); slow_metric_step = 1; obd_state = STATE_RPM; } 
          else if (myELM327.nb_rx_state != ELM_GETTING_MSG) { slow_metric_step = 1; obd_state = STATE_RPM; }
          break;
        }
        case 1: {
          float temp = myELM327.batteryVoltage();
          if (myELM327.nb_rx_state == ELM_SUCCESS) { valVolt = temp; Serial.print("Battery (V): "); Serial.println(valVolt); slow_metric_step = 2; obd_state = STATE_RPM; } 
          else if (myELM327.nb_rx_state != ELM_GETTING_MSG) { slow_metric_step = 2; obd_state = STATE_RPM; }
          break;
        }
        case 2: {
          float temp = myELM327.engineCoolantTemp();
          if (myELM327.nb_rx_state == ELM_SUCCESS) { valCoolant = temp; Serial.print("Coolant (C): "); Serial.println(valCoolant); slow_metric_step = 3; obd_state = STATE_RPM; } 
          else if (myELM327.nb_rx_state != ELM_GETTING_MSG) { slow_metric_step = 3; obd_state = STATE_RPM; }
          break;
        }
        case 3: {
          float temp = myELM327.manifoldPressure();
          if (myELM327.nb_rx_state == ELM_SUCCESS) { valMap = temp; Serial.print("MAP (kPa): "); Serial.println(valMap); slow_metric_step = 4; obd_state = STATE_RPM; } 
          else if (myELM327.nb_rx_state != ELM_GETTING_MSG) { slow_metric_step = 4; obd_state = STATE_RPM; }
          break;
        }
        case 4: {
          float temp = myELM327.fuelLevel();
          if (myELM327.nb_rx_state == ELM_SUCCESS) { valFuel = temp; Serial.print("Fuel (%): "); Serial.println(valFuel); slow_metric_step = 5; obd_state = STATE_RPM; } 
          else if (myELM327.nb_rx_state != ELM_GETTING_MSG) { slow_metric_step = 5; obd_state = STATE_RPM; }
          break;
        }
        case 5: {
          float temp = myELM327.oilTemp();
          if (myELM327.nb_rx_state == ELM_SUCCESS) { valOil = temp; Serial.print("Oil Temp (C): "); Serial.println(valOil); slow_metric_step = 6; obd_state = STATE_RPM; } 
          else if (myELM327.nb_rx_state != ELM_GETTING_MSG) { slow_metric_step = 6; obd_state = STATE_RPM; }
          break;
        }
        case 6: {
          float temp = myELM327.throttle();
          if (myELM327.nb_rx_state == ELM_SUCCESS) { valThrottle = temp; Serial.print("Throttle (%): "); Serial.println(valThrottle); slow_metric_step = 0; obd_state = STATE_RPM; } 
          else if (myELM327.nb_rx_state != ELM_GETTING_MSG) { slow_metric_step = 0; obd_state = STATE_RPM; }
          break;
        }
      }
      break;
    }
  }
}