#include <Arduino.h>
#include "BluetoothSerial.h"
#include "ELMduino.h"

BluetoothSerial SerialBT;
#define ELM_PORT   SerialBT
#define DEBUG_PORT Serial

ELM327 myELM327;
const char* ELM_NAME = "OBDII"; 

typedef enum { 
  STATE_RPM, STATE_SPEED, STATE_LOAD, STATE_VOLTAGE, 
  STATE_COOLANT, STATE_MAP, STATE_FUEL, STATE_OIL, STATE_THROTTLE 
} obd_pid_states;

obd_pid_states obd_state = STATE_RPM;// NEW: Variable to track the last time we printed our status message
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
        obd_state = STATE_SPEED; 
      } else if (myELM327.nb_rx_state != ELM_GETTING_MSG) {
        obd_state = STATE_SPEED;
      }
      break;
    }
    
    case STATE_SPEED: {
      float tempSpeed = myELM327.mph();
      if (myELM327.nb_rx_state == ELM_SUCCESS) {
        DEBUG_PORT.print("Speed (MPH): ");
        DEBUG_PORT.println((uint32_t)tempSpeed);
        obd_state = STATE_LOAD; 
      } else if (myELM327.nb_rx_state != ELM_GETTING_MSG) {
        obd_state = STATE_LOAD;
      }
      break;
    }

    case STATE_LOAD: {
      float tempLoad = myELM327.engineLoad();
      if (myELM327.nb_rx_state == ELM_SUCCESS) {
        DEBUG_PORT.print("Engine Load (%): ");
        DEBUG_PORT.println(tempLoad);
        obd_state = STATE_VOLTAGE; 
      } else if (myELM327.nb_rx_state != ELM_GETTING_MSG) {
        obd_state = STATE_VOLTAGE;
      }
      break;
    }

    case STATE_VOLTAGE: {
      float tempVoltage = myELM327.batteryVoltage();
      if (myELM327.nb_rx_state == ELM_SUCCESS) {
        DEBUG_PORT.print("Battery (V): ");
        DEBUG_PORT.println(tempVoltage);
        obd_state = STATE_COOLANT; 
      } else if (myELM327.nb_rx_state != ELM_GETTING_MSG) {
        obd_state = STATE_COOLANT;
      }
      break;
    }

    case STATE_COOLANT: {
      float tempCoolant = myELM327.engineCoolantTemp();
      if (myELM327.nb_rx_state == ELM_SUCCESS) {
        DEBUG_PORT.print("Coolant (C): ");
        DEBUG_PORT.println(tempCoolant);
        obd_state = STATE_MAP; 
      } else if (myELM327.nb_rx_state != ELM_GETTING_MSG) {
        obd_state = STATE_MAP;
      }
      break;
    }

    case STATE_MAP: {
      float tempMap = myELM327.manifoldPressure();
      if (myELM327.nb_rx_state == ELM_SUCCESS) {
        DEBUG_PORT.print("MAP (kPa): ");
        DEBUG_PORT.println(tempMap);
        obd_state = STATE_FUEL; 
      } else if (myELM327.nb_rx_state != ELM_GETTING_MSG) {
        obd_state = STATE_FUEL;
      }
      break;
    }

    case STATE_FUEL: {
      float tempFuel = myELM327.fuelLevel();
      if (myELM327.nb_rx_state == ELM_SUCCESS) {
        DEBUG_PORT.print("Fuel (%): ");
        DEBUG_PORT.println(tempFuel);
        obd_state = STATE_OIL; 
      } else if (myELM327.nb_rx_state != ELM_GETTING_MSG) {
        obd_state = STATE_OIL;
      }
      break;
    }

    case STATE_OIL: {
      float tempOil = myELM327.engineOilTemp();
      if (myELM327.nb_rx_state == ELM_SUCCESS) {
        DEBUG_PORT.print("Oil Temp (C): ");
        DEBUG_PORT.println(tempOil);
        obd_state = STATE_THROTTLE; 
      } else if (myELM327.nb_rx_state != ELM_GETTING_MSG) {
        obd_state = STATE_THROTTLE;
      }
      break;
    }

    case STATE_THROTTLE: {
      float tempThrottle = myELM327.throttle();
      if (myELM327.nb_rx_state == ELM_SUCCESS) {
        DEBUG_PORT.print("Throttle (%): ");
        DEBUG_PORT.println(tempThrottle);
        DEBUG_PORT.println("-------------------------");
        obd_state = STATE_RPM; 
      } else if (myELM327.nb_rx_state != ELM_GETTING_MSG) {
        obd_state = STATE_RPM;
      }
      break;
    }
  }
}