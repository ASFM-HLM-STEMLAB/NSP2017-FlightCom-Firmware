/*
 * Project HLM_I2C_SLAVE
 * Description: HLM1 Master Module for I2C Slaves
 * Author: ASFM HLM STEM LAB - 2017 Near Space Team
 * License: MIT Open Source
 * Date: May/2018
 */
 
#include <Wire.h>

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

// HUMIDITY MODULE: DHT22
#define DHTPIN 22
#define DHTTYPE DHT22
DHT_Unified dht(DHTPIN, DHTTYPE);
uint32_t delayMS;
int led = 13;

// TMP36_Temperature STORAGE
float tempTMP36 = 0.0;
float humDHT22 = 0.0;
float tempDHT22 = 0.0;

int elapsedTime = 0.0;
int lastPeriod = 0.0;
bool ledValue = LOW;
String dataString = "";



void setup() {

  analogReadResolution(12);
  Serial.begin(9600);
  
  pinMode(led, OUTPUT);
  digitalWrite(led, HIGH);
  lastPeriod = millis();
  delay(1000);
  digitalWrite(led, LOW);
  setupDHT22();
  
  Wire.begin(1);
  Wire.onRequest(requestEvent); // Function to run when master asking for data
  Wire.onReceive(receiveEvent); // Function to run when master sending data

  
  
}


void loop() {
  elapsedTime = millis() - lastPeriod;  
  if (elapsedTime >= 1000) {    
    ledValue = !ledValue;      //Flash LED
//    digitalWrite(led, ledValue);
    //Read Sensors
    readDHT(); 
    readTMP36();
    //Form DataString to report to Master
    dataString = String(tempDHT22) + "," + String(humDHT22) + "," + String(tempTMP36);
    //Send report to serial Port
    Serial.println(dataString);
    //Reset value to trigger period event next time.
    lastPeriod = millis(); 
  }
}

void readDHT() {
      sensors_event_t event;    
      dht.temperature().getEvent(&event);
      
      if (isnan(event.temperature)) {
        Serial.println("Error reading temperature!");
      } else {
        tempDHT22 = event.temperature;
      }

       dht.humidity().getEvent(&event);
      if (isnan(event.relative_humidity)) {
        Serial.println("Error reading humidity!");
      } else {
        humDHT22 = event.relative_humidity;
      }
}

void readTMP36() {
  int tempReading = 0;
  
  
    for (int i=0; i <= 1000; i++){
      tempReading = tempReading + analogRead(A0);
    }

    tempReading /= 1000;
  
  float voltage = tempReading * 3.3;
  voltage /= 4095;
  float tempC = (voltage - 0.5) * 100;
  tempTMP36 = tempC;
}

// function that executes whenever data is sent to master
void requestEvent() {
  int len = dataString.length() + 1;
  char txBuffer[len];
  dataString.toCharArray(txBuffer, len);
  Wire.write(txBuffer);
  Serial.println(txBuffer);
  Serial.println("[Event] Request from Master");
  
}

// function that executes whenever data is received from master
void receiveEvent(int rxBytes) {
   Serial.print("[Event] i2C RXD: " + String(rxBytes) + " bytes");  
}

void setupDHT22() {
  
  dht.begin();

  Serial.println("----------------[DHT 22 INIT]--------------------");
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  Serial.println("--TEMPERATURE--");
  Serial.print  ("Sensor:       "); Serial.println(sensor.name);
  Serial.print  ("Driver Ver:   "); Serial.println(sensor.version);
  Serial.print  ("Unique ID:    "); Serial.println(sensor.sensor_id);
  Serial.print  ("Max Value:    "); Serial.print(sensor.max_value); Serial.println(" *C");
  Serial.print  ("Min Value:    "); Serial.print(sensor.min_value); Serial.println(" *C");
  Serial.print  ("Resolution:   "); Serial.print(sensor.resolution); Serial.println(" *C");  

  // Print humidity sensor details.
  dht.humidity().getSensor(&sensor);
  Serial.println("--HUMIDITY--");
  Serial.print  ("Sensor:       "); Serial.println(sensor.name);
  Serial.print  ("Driver Ver:   "); Serial.println(sensor.version);
  Serial.print  ("Unique ID:    "); Serial.println(sensor.sensor_id);
  Serial.print  ("Max Value:    "); Serial.print(sensor.max_value); Serial.println("%");
  Serial.print  ("Min Value:    "); Serial.print(sensor.min_value); Serial.println("%");
  Serial.print  ("Resolution:   "); Serial.print(sensor.resolution); Serial.println("%"); 
  
  Serial.println("------------------------------------");
 
  delayMS = sensor.min_delay / 1000;
}


