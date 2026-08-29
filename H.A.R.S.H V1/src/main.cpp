#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BluetoothSerial.h>
#include "ELMduino.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

BluetoothSerial SerialBT;
#define ELM_PORT SerialBT
ELM327 myELM327;
const char* ELM_NAME = "OBDII"; 

typedef enum { 
  STATE_RPM, STATE_SPEED, STATE_LOAD, STATE_VOLTAGE, 
  STATE_COOLANT, STATE_MAP, STATE_FUEL, STATE_OIL, STATE_THROTTLE 
} obd_pid_states;

obd_pid_states obd_state = STATE_RPM;

// Global variables to hold the latest data
float valRPM = 0, valSpeed = 0, valLoad = 0, valVolt = 0;
float valCoolant = 0, valMap = 0, valFuel = 0, valOil = 0, valThrottle = 0;

unsigned long lastDisplayUpdate = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin(); 
  Wire.setClock(400000); // 400kHz I2C for fast screen refreshing
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    while(1);
  }
  
  // Show booting status on the screen
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setCursor(10, 20);
  display.print("BOOTING...");
  display.display();

  ELM_PORT.begin("ESP32_OBD_Client", true); 
  if (!ELM_PORT.connect(ELM_NAME)) {
    display.clearDisplay();
    display.setCursor(0, 20);
    display.print("BT FAULT");
    display.display();
    while(1);
  }
  
  if (!myELM327.begin(ELM_PORT, false, 2000, '6')) {
    display.clearDisplay();
    display.setCursor(0, 20);
    display.print("OBD FAULT");
    display.display();
    while (1);
  }
}

void updateDisplay() {
  display.clearDisplay();
  
  // --- TOP HALF: Driving Data (Large Text) ---
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print((uint32_t)valSpeed); display.print(" MPH");
  
  display.setCursor(0, 20);
  display.print((uint32_t)valRPM); display.print(" RPM");
  
  // --- BOTTOM HALF: Vitals (Small Text) ---
  display.setTextSize(1);
  
  // Perform conversions
  float boostPSI = (valMap - 101.325) * 0.145038;
  int coolantF = (valCoolant * 9/5) + 32;

  // Row 1: Boost and Temp
  display.setCursor(0, 42);
  display.print("BST:"); display.print(boostPSI, 1); 
  display.setCursor(64, 42);
  display.print("TMP:"); display.print(coolantF);

  // Row 2: Load and Voltage
  display.setCursor(0, 54);
  display.print("LOD:"); display.print((int)valLoad); display.print("%");
  display.setCursor(64, 54);
  display.print("VLT:"); display.print(valVolt, 1);
  
  display.display();
}

void loop() {
  // Refresh screen independently of the ELM327 speed
  if (millis() - lastDisplayUpdate >= 200) {
    updateDisplay();
    lastDisplayUpdate = millis();
  }

  // OBD State Machine (runs constantly to keep global variables fresh)
  switch (obd_state) {
    case STATE_RPM:
      if (myELM327.nb_rx_state == ELM_SUCCESS) {
        valRPM = myELM327.rpm();
        obd_state = STATE_SPEED; 
      } else if (myELM327.nb_rx_state != ELM_GETTING_MSG) obd_state = STATE_SPEED;
      break;
      
    case STATE_SPEED:
      if (myELM327.nb_rx_state == ELM_SUCCESS) {
        valSpeed = myELM327.mph();
        obd_state = STATE_LOAD; 
      } else if (myELM327.nb_rx_state != ELM_GETTING_MSG) obd_state = STATE_LOAD;
      break;

    case STATE_LOAD:
      if (myELM327.nb_rx_state == ELM_SUCCESS) {
        valLoad = myELM327.engineLoad();
        obd_state = STATE_VOLTAGE; 
      } else if (myELM327.nb_rx_state != ELM_GETTING_MSG) obd_state = STATE_VOLTAGE;
      break;

    case STATE_VOLTAGE:
      if (myELM327.nb_rx_state == ELM_SUCCESS) {
        valVolt = myELM327.batteryVoltage();
        obd_state = STATE_COOLANT; 
      } else if (myELM327.nb_rx_state != ELM_GETTING_MSG) obd_state = STATE_COOLANT;
      break;

    case STATE_COOLANT:
      if (myELM327.nb_rx_state == ELM_SUCCESS) {
        valCoolant = myELM327.engineCoolantTemp();
        obd_state = STATE_MAP; 
      } else if (myELM327.nb_rx_state != ELM_GETTING_MSG) obd_state = STATE_MAP;
      break;

    case STATE_MAP:
      if (myELM327.nb_rx_state == ELM_SUCCESS) {
        valMap = myELM327.manifoldPressure();
        obd_state = STATE_FUEL; 
      } else if (myELM327.nb_rx_state != ELM_GETTING_MSG) obd_state = STATE_FUEL;
      break;

    case STATE_FUEL:
      if (myELM327.nb_rx_state == ELM_SUCCESS) {
        valFuel = myELM327.fuelLevel();
        obd_state = STATE_OIL; 
      } else if (myELM327.nb_rx_state != ELM_GETTING_MSG) obd_state = STATE_OIL;
      break;

    case STATE_OIL:
      if (myELM327.nb_rx_state == ELM_SUCCESS) {
        valOil = myELM327.engineOilTemp();
        obd_state = STATE_THROTTLE; 
      } else if (myELM327.nb_rx_state != ELM_GETTING_MSG) obd_state = STATE_THROTTLE;
      break;

    case STATE_THROTTLE:
      if (myELM327.nb_rx_state == ELM_SUCCESS) {
        valThrottle = myELM327.throttle();
        obd_state = STATE_RPM; 
      } else if (myELM327.nb_rx_state != ELM_GETTING_MSG) obd_state = STATE_RPM;
      break;
  }
}