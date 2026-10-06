#include <Arduino.h>
#include "driver/twai.h"

#define CAN_TX_PIN 5
#define CAN_RX_PIN 4

//this is the code for the dummy ECU CAN broadcaster. It will broadcast a set of OBD-II frames with simulated data to mimic a real ECU. The data is generated using sine waves to create smooth transitions for gauges like RPM, speed, and temperature.

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Dummy ECU CAN Broadcaster...");

  // Configure TWAI (CAN) Driver at 500 kbps
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
    Serial.println("CAN Broadcasting Started!");
  }
}

// Function to package data into standard OBD-II format
void sendOBDFrame(uint8_t pid, uint8_t dataA, uint8_t dataB) {
  twai_message_t message;
  message.identifier = 0x7E8; // 0x7E8 is the standard ID for Engine ECU responses
  message.extd = 0;           
  message.rtr = 0;
  message.data_length_code = 8;
  
  message.data[0] = 4;        // Byte 0: Data length (4 bytes follow)
  message.data[1] = 0x41;     // Byte 1: Mode 41 (Standard Success Response)
  message.data[2] = pid;      // Byte 2: The requested PID
  message.data[3] = dataA;    // Byte 3: Data A
  message.data[4] = dataB;    // Byte 4: Data B
  message.data[5] = 0x00;     // Padding
  message.data[6] = 0x00;
  message.data[7] = 0x00;

  twai_transmit(&message, pdMS_TO_TICKS(10));
}

void loop() {
  // Use millis() to generate smooth sine waves for realistic sweeping gauges
  float time = millis() / 1000.0;
  
  // 1. RPM (0x0C): Formula = ((A * 256) + B) / 4
  int simRPM = 3650 + 2850 * sin(time); // Sweeps 800 to 6500
  int rpmA = (simRPM * 4) / 256;
  int rpmB = (simRPM * 4) % 256;
  
  // 2. Speed (0x0D): Formula = A (in km/h)
  int simSpeedKPH = 64 + 64 * sin(time * 0.5); // Sweeps 0 to 128 km/h (~80 MPH)
  int speedA = simSpeedKPH;

  // 3. MAP/Boost (0x0B): Formula = A (in kPa)
  int simMAP = 115 + 85 * sin(time * 1.5); // Sweeps 30 to 200 kPa
  int mapA = simMAP;
  
  // 4. Engine Load (0x04): Formula = A * 100 / 255
  int simLoad = 50 + 50 * sin(time * 0.8); // Sweeps 0 to 100%
  int loadA = (simLoad * 255) / 100;

  // 5. Throttle Position (0x11): Formula = A * 100 / 255
  int simThrottle = 50 + 50 * sin(time * 0.8); // Sweeps 0 to 100%
  int throttleA = (simThrottle * 255) / 100;

  // 6. Coolant Temp (0x05): Formula = A - 40 (in Celsius)
  int simCoolant = 90 + 10 * sin(time * 0.2); // Sweeps 80C to 100C
  int coolantA = simCoolant + 40;

  // 7. Intake Air Temp (0x0F): Formula = A - 40 (in Celsius)
  int simIAT = 40 + 10 * sin(time * 0.3); // Sweeps 30C to 50C
  int iatA = simIAT + 40;

  // 8. Oil Temp (0x5C): Formula = A - 40 (in Celsius)
  int simOil = 100 + 10 * sin(time * 0.2); // Sweeps 90C to 110C
  int oilA = simOil + 40;

  // 9. Fuel Level (0x2F): Formula = A * 100 / 255
  int simFuel = 50 + 50 * sin(time * 0.05); // Very slow sweep 0 to 100%
  int fuelA = (simFuel * 255) / 100;

  // 10. Control Module Voltage (0x42): Formula = ((A * 256) + B) / 1000
  float simVolt = 13.9 + 0.9 * sin(time * 0.1); // Sweeps 13.0V to 14.8V
  int voltVal = (int)(simVolt * 1000);
  int voltA = voltVal / 256;
  int voltB = voltVal % 256;

  // --- BROADCAST ALL FRAMES ---
  sendOBDFrame(0x0C, rpmA, rpmB); 
  delay(5);
  sendOBDFrame(0x0D, speedA, 0);  
  delay(5);
  sendOBDFrame(0x0B, mapA, 0);    
  delay(5);
  sendOBDFrame(0x04, loadA, 0);   
  delay(5);
  sendOBDFrame(0x11, throttleA, 0);   
  delay(5);
  sendOBDFrame(0x05, coolantA, 0);   
  delay(5);
  sendOBDFrame(0x0F, iatA, 0);   
  delay(5);
  sendOBDFrame(0x5C, oilA, 0);   
  delay(5);
  sendOBDFrame(0x2F, fuelA, 0);   
  delay(5);
  sendOBDFrame(0x42, voltA, voltB);   
  delay(5);
  
  // Entire loop takes ~50ms. We are flooding the bus at roughly 20Hz, 
  // mirroring the constant chatter of a high-speed automotive CAN bus.
}