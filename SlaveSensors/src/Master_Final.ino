/*
 * Project HLM_I2C_MASTER
 * Description: HLM1 Master Module for I2C Slaves
 * Author: ASFM HLM STEM LAB - 2017 Near Space Team
 * License: MIT Open Source
 * Date: May/2018
 */
 
#include <Wire.h>
#include "intersemaBaro.h"

int slaveCount = 0;

// TMP36_Temperature STORAGE
char tempTMP36[10]={}; // empty array for temperature

// DHT22_Humidity STORAGE
char tempDHT22[10]={}; // empty array for temperature
char humDHT22[10]={}; // empty array for humidity

// ALTIMETER MODULE: MS5607
Intersema::BaroPressure_MS5607B baro(true);

// MS5607 STORAGE
float decTempMS5607 = 0.0;
float decPresMS5607 = 0.0;

int valueRequest = 0; // 0: TMP36-TEMPERATURE, 
                      // 1: DHT22-TEMPERATURE,
                      // 2: DHT22-HUMIDITY,
                      // 3: MS5607-TEMPERATURE,
                      // 4: MS5607-PRESSURE
    

int maxValue = 4;

void setup() {
  
  // Start the I2C Bus as Master
  Wire.begin(); 
  Serial.begin(9600);

  setupMS5607();

  Serial.println("SETUP DONE");
}

void loop() {

  Serial.println("CURRENT REQUEST: " + String(valueRequest));
  
  // alternate between sensors
  Wire.beginTransmission(1); // transmit to Teensy on LINE 1
  Wire.write(valueRequest);
  Wire.endTransmission ();

  if (valueRequest == 0) { // TMP36-TEMPERATURE
    
    // request TMP36-TEMPERATURE
    requestTeensy(valueRequest, tempTMP36, "Temperature [ºC]");
    
  } else if (valueRequest == 1) { // DHT22-TEMPERATURE
    
    // request DHT22-TEMPERATURE
    requestTeensy(valueRequest, tempDHT22, "Temperature [ºC]");
    
  } else if (valueRequest == 2) { // DHT22-HUMIDITY

    // request DHT22-HUMIDITY
    requestTeensy(valueRequest, humDHT22, "Relative Humidity [%]");
    
  } else if (valueRequest == 3) { // MS5607-TEMPERATURE
    
    // request MS5607-TEMPERATURE
    decTempMS5607 = readMS5607("t");
      
  } else if (valueRequest == 4) { // MS5607-PRESSURE

    // request MS5607-PRESSURE
    decPresMS5607 = readMS5607("p");
  }

  valueRequest++;

  if (valueRequest == maxValue + 1) {
    
    valueRequest = 0; // if last sensor, reset request

  }
  
  delay(1000); //give some time to relax
  
}

void requestTeensy(int value, char store[10], String unit) {

  Wire.requestFrom(1, 5);    // request 5 bytes from Teensy (standard)

  // gather data coming from Teensy
  int i = 0; // counter for each byte as it arrives
  while (Wire.available()) { 
     store[i] = Wire.read(); // every character that arrives is written to empty array 'store'
     i = i + 1;
  }

  if (value == 0) {
    Serial.println("TMP36");
  } else if (value < 3) {
    Serial.println("DHT22");
  }
    
  // better method for printing with a decimal point after 2 values would be nice
  Serial.print(unit + ": ");
  Serial.println(String(store[0]) + 
                 String(store[1]) + 
                 String(store[2]) + 
                 String(store[3]) + 
                 String(store[4])); // shows the data in the array 'store'
  Serial.println("--------------");

}

float readMS5607(String value) {

  Serial.println("MS5607");

  if (value == "t") {
    decTempMS5607 = baro.getTemperature();
    Serial.print("Temperature [ºC]: ");
    Serial.println(decTempMS5607);
    Serial.println("--------------");
    
  } else {
    decPresMS5607 = baro.getPressurePascals();
    Serial.print("Pressure [Pa]: ");
    Serial.println(decPresMS5607);
    Serial.println("--------------");
  }
}

void setupMS5607() {

  baro.init();

  Serial.println("------------------------------------");

  Serial.println("SETTING UP MS5607");
  Serial.println("."); 
  Serial.println("."); 
  Serial.println(".");

  long alt = baro.getHeightCentiMeters();
  Serial.println("Got altitude");
  
  long pre = baro.getPressurePascals();
  Serial.println("Got pressure");
  
  long temp = baro.getTemperature();
  Serial.println("Got temperature");

  Serial.println("--ALTITUDE--");
  Serial.println("Centimeters:");
  Serial.println((float)(alt), 2);

  Serial.println("--PRESSURE--");
  Serial.println("Pressure (Pa): ");
  Serial.println((float)(pre), 2);

  Serial.println("--TEMPERATURE--");
  Serial.println("Temperature (C):");
  Serial.print((float)(temp / 100), 2);
  
  Serial.println("------------------------------------");
}
