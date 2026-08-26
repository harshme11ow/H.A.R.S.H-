#include <Arduino.h>
#include "BluetoothSerial.h"
#include "ELMduino.h"

BluetoothSerial SerialBT;
#define ELM_PORT   SerialBT
#define DEBUG_PORT Serial

ELM327 myELM327;
const char* ELM_NAME = "OBDII"; 

typedef enum { STATE_RPM, STATE_COOLANT, STATE_SPEED } obd_pid_states;
obd_pid_states obd_state = STATE_RPM;

// NEW: Variable to track the last time we printed our status message
unsigned long lastStatusTime = 0;

void setup() {
  DEBUG_PORT.begin(115200);
  ELM_PORT.begin("ESP32_OBD_Client", true); 
  
  DEBUG_PORT.println("Attempting to connect to ELM327...");

  if (!ELM_PORT.connect(ELM_NAME)) {
    DEBUG_PORT.println("Couldn't connect to OBD scanner. Check power and name.");
    while(1);
  }
  
  DEBUG_PORT.println("Bluetooth connected! Initializing ELMduino...");

  if (!myELM327.begin(ELM_PORT, false, 2000)) {
    DEBUG_PORT.println("Couldn't initialize ELM327 protocol.");
    while (1);
  }

  DEBUG_PORT.println("Connected and initialized successfully!");
}

void loop() {
  // NEW: Non-blocking 5-second timer
  if (millis() - lastStatusTime >= 5000) {
    DEBUG_PORT.println("hi there - ESP32 is running!");
    lastStatusTime = millis(); // Reset the timer
  }

  // The rest of your state machine stays exactly the same
  switch (obd_state) {
    
    case STATE_RPM: {
      float tempRPM = myELM327.rpm();
      
      if (myELM327.nb_rx_state == ELM_SUCCESS) {
        DEBUG_PORT.print("Engine RPM: ");
        DEBUG_PORT.println((uint32_t)tempRPM);
        obd_state = STATE_COOLANT; 
      } 
      else if (myELM327.nb_rx_state != ELM_GETTING_MSG) {
        myELM327.printError();
        obd_state = STATE_COOLANT;
      }
      break;
    }
    
    case STATE_COOLANT: {
      float tempCoolant = myELM327.throttle(); // Returns a percentage from 0-100%
      
      if (myELM327.nb_rx_state == ELM_SUCCESS) {
        DEBUG_PORT.print("Coolant Temp (C): ");
        DEBUG_PORT.println(tempCoolant);
        obd_state = STATE_SPEED; 
      } 
      else if (myELM327.nb_rx_state != ELM_GETTING_MSG) {
        myELM327.printError();
        obd_state = STATE_SPEED;
      }
      break;
    }
    
    case STATE_SPEED: {
      float tempSpeed = myELM327.kph();
      
      if (myELM327.nb_rx_state == ELM_SUCCESS) {
        DEBUG_PORT.print("Speed (KPH): ");
        DEBUG_PORT.println((uint32_t)tempSpeed);
        DEBUG_PORT.println("-------------------------");
        obd_state = STATE_RPM; 
      } 
      else if (myELM327.nb_rx_state != ELM_GETTING_MSG) {
        myELM327.printError();
        obd_state = STATE_RPM;
      }
      break;
    }
  }
}