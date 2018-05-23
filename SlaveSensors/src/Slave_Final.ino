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

// TEMPERATURE MODULE: TMP36
int tmpPin = 0; //the analog pin the TMP36's Vout (sense) pin is connected to
                   //the resolution is 10 mV / degree centigrade with a
                   //500 mV offset to allow for negative temperatures

// HUMIDITY MODULE: DHT22
#define DHTPIN 22
#define DHTTYPE DHT22
DHT_Unified dht(DHTPIN, DHTTYPE);
uint32_t delayMS;

int led = 13;

// TMP36_Temperature STORAGE
char tempTMP36[10]={}; // empty array for temperature
float decTempTMP36 = 0.0;

// DHT22 STORAGE
char humDHT22[10]={}; // empty array for humidity
char tempDHT22[10]={}; // empty array for temperature
float decHumDHT22 = 0.0;
float decTempDHT22 = 0.0;

int valueRequest = 0; // 0: TMP36-TEMPERATURE, 
                      // 1: DHT22-TEMPERATURE,
                      // 2: DHT22-HUMIDITY

int maxValue = 4;

void setup() {
  
  Serial.begin(9600);

  Serial.print("JOINING I2C BUS AS SLAVE...");
  // Join the I2C Bus as Slave on address 1
  Wire.begin(1);
  Wire.onRequest(requestEvent); // Function to run when master asking for data
  Wire.onReceive(receiveEvent); // Function to run when master sending data
  Serial.println("DONE");

  // Blinking led signals POWER
  pinMode(led, OUTPUT);

  Serial.println("SETTING UP SENSORS...");
  // Setup sensors for data retrieval
  setupDHT22();
  Serial.println("DONE");  
  Serial.println("SETUP DONE");
}

void loop() {

  digitalWrite(led, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(200);               // wait for half a second
  digitalWrite(led, LOW);    // turn the LED off by making the voltage LOW
  delay(200);               // wait for half a second
  
  decTempTMP36 = readTMP36("t");
  Serial.println("TMP36 READ: " + String(decTempTMP36));
  
  decTempDHT22 = readDHT22("t");
  Serial.println("DHT22-TEMP READ: " + String(decTempDHT22));
 
  decHumDHT22 = readDHT22("h");
  Serial.println("DHT22-HUM READ: " + String(decHumDHT22));

  // dtostrf store in char arrays
  // dtostrf converts the float variables to a char array (String) for I2C. (floatVar, minStringWidthIncDecimalPoint, numVarsAfterDecimal, empty array);
  dtostrf(decTempTMP36, 3, 2, tempTMP36);
  dtostrf(decTempDHT22, 3, 2, tempDHT22);
  dtostrf(decHumDHT22, 3, 2, humDHT22);
  
  // Delay between measurements.
  delay(delayMS);
}

// function that executes whenever data is sent to master
void requestEvent() {
  
  if (valueRequest == 0) { // TMP36-TEMPERATURE
    
    // write TMP36-TEMPERATURE
    Wire.write(tempTMP36);
  
  } else if (valueRequest == 1) { // DHT22-TEMPERATURE
    
    // write DHT22-TEMPERATURE
    Wire.write(tempDHT22);
    
  } else if (valueRequest == 2) { // DHT22-HUMIDITY

    // write DHT22-HUMIDITY
    Wire.write(humDHT22);
    
  }
  
}

// function that executes whenever data is received from master
void receiveEvent(int howMany) {
  
  valueRequest = Wire.read(); // receive byte (sensor request) as an integer - see list.
  
}

void setupDHT22() {
  
  // Initialize device.
  dht.begin();

  Serial.println("------------------------------------");

  Serial.println("SETTING UP DHT22");
  Serial.println("."); 
  Serial.println("."); 
  Serial.println("."); 
  
  // Print temperature sensor details.
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
  
  // Set delay between sensor readings based on sensor details.
  delayMS = sensor.min_delay / 1000;
}


float readDHT22(String value) {

  Serial.print("Reading DHT22...");
  Serial.println(value);
  
    sensors_event_t event;
    
    if (value == "t") { // read temperature
      
      // Get temperature event and print its value.
      sensors_event_t event;  
      dht.temperature().getEvent(&event);
      if (isnan(event.temperature)) {
        Serial.println("Error reading temperature!");
      } else {
        Serial.print("Temperature: ");
        Serial.print(event.temperature);
        Serial.println(" *C");
        Serial.println();

        return event.temperature;
      }
      
    } else if (value == "h") { // read humidity
      
      // Get humidity event and print its value.
      dht.humidity().getEvent(&event);
      if (isnan(event.relative_humidity)) {
        Serial.println("Error reading humidity!");
      } else {
        Serial.print("Humidity: ");
        Serial.print(event.relative_humidity);
        Serial.println("%");
        Serial.println();

        return event.relative_humidity;
      }
    }
}

float readTMP36(String value) {

  Serial.print("Reading TMP36...");
  Serial.println(value);

  //getting the voltage reading from the temperature sensor
  int reading = analogRead(tmpPin);  
 
  // converting that reading to voltage, for 3.3v arduino use 3.3
  float voltage = reading * 5.0;
  voltage /= 1024.0; 
  
  // Print out the temperature
  float temperature = (voltage - 0.5) * 100 ;  //converting from 10 mv per degree wit 500 mV offset
                                               //to degrees ((voltage - 500mV) times 100)
  float fakeTemp = 28.35;
  Serial.print("Temperature: ");
  Serial.print(String(fakeTemp));
  Serial.println(" *C");
  Serial.println();
  
  return fakeTemp;
}
