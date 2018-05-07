
/*
 * Project HLM_I2C_MASTER
 * Description: HLM1 Master Module for I2C Slaves
 * Author: ASFM HLM STEM LAB - 2017 Near Space Team
 * License: MIT Open Source
 * Date: May/2018
 */
 
#include <Wire.h>

int slaveCount = 0;

// TMP36_Temperature STORAGE
char tempTMP36[10]={}; // empty array for temperature

// MS5607 STORAGE
char tempMS5607[10]={}; // empty array for temperature
char presMS5607[10]={}; // empty array for pressure

// DHT22_Humidity STORAGE
char tempDHT22[10]={}; // empty array for temperature
char humDHT22[10]={}; // empty array for humidity

int valueRequest = 0; // 0: TMP36-TEMPERATURE, 
                      // 1: MS5607-TEMPERATURE,
                      // 2: MS5607-PRESSURE,
                      // 3: DHT22-TEMPERATURE,
                      // 4: DHT22-HUMIDITY

int maxValue = 4;

void setup() {
  
  // Start the I2C Bus as Master
  Wire.begin(); 
  Serial.begin(9600);
  
  Serial.println("\nI2C Scanner");
  while (slaveCount < 1) {
    scanI2C();
  }
  
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
    
  } else if (valueRequest == 1) { // MS5607-TEMPERATURE
    
    // request MS5607-TEMPERATURE
    requestTeensy(valueRequest, tempMS5607, "Temperature [ºC]");
    
  } else if (valueRequest == 2) { // MS5607-PRESSURE

    // request MS5607-PRESSURE
    requestTeensy(valueRequest, presMS5607, "Pressure [Pa]");
    
  } else if (valueRequest == 3) { // DHT22-TEMPERATURE
    
    // request DHT22-TEMPERATURE
    requestTeensy(valueRequest, tempDHT22, "Temperature [ºC]");
    
  } else if (valueRequest == 4) { // DHT22-HUMIDITY

    // request DHT22-HUMIDITY
    requestTeensy(valueRequest, humDHT22, "Relative Humidity [%]");
    
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
    
  // better method for printing with a decimal point after 2 values would be nice
  Serial.print(unit + ": ");
  Serial.println(String(store[0]) + 
                 String(store[1]) + 
                 String(store[2]) + 
                 String(store[3]) + 
                 String(store[4])); // shows the data in the array 'store'
  Serial.println("--------------");

}

void scanI2C() {
  
  byte error, address;
 
  Serial.println("Scanning...");
 
  for(address = 1; address < 127; address++ ) {
    
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
 
    if (error == 0) {
      
      Serial.print("I2C device found at address 0x");
      
      if (address < 16) {
        Serial.print("0");
      }
      
      Serial.print(address,HEX);
      Serial.println("  !");
 
      slaveCount++;
      
    } else if (error == 4) {
      
      Serial.print("Unknown error at address 0x");
      
      if (address < 16) {
        Serial.print("0");
        Serial.println(address,HEX);
      }
    }    
  }
  
  if (slaveCount == 0) {
    Serial.println("No I2C devices found\n");
  } else {
    Serial.println("done\n");
  }

  delay(5000);           // wait 5 seconds for next scan
}

