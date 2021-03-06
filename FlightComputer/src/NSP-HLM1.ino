/*
 * Project HLM_FLIGHT_COMPUTER
 * Description: HLM1 High Altitude Weather Balloon Flight Computer
 * Author: ASFM HLM STEM LAB - 2017 Near Space Team
 * License: MIT Open Source
 * Date: May/2018
 */

#include "TinyGPS++.h"
#include "Serial5/Serial5.h"
#include "Serial4/Serial4.h"
#include "ParticleSoftSerial.h"
#include "Wire.h"
#include "IntersemaBaro.h"

//====[SETTINGS]=================================================
bool cellModemEnabled = true;  //NO CONST!
bool satModemEnabled = true;  //NO CONST!
bool cellMuteEnabled = true;   //NO CONST!
bool sdMuteEnabled = false;   //NO CONST!
bool satMuteEnabled = true;   //NO CONST!

const float altitudeGainClimbTrigger = 200; //FT Minimum alt gain after startup to detect climb.
const float altitudePerMinuteGainClimbTrigger = 200; //ft per minute to detect a climb
const float altitudeLossPerMinuteForDescentDetection = -400;
const float iterationsInLowDescentToTriggerRecovery = 80;
const float minimumAltitudeToTriggerRecovery = 10000; //If above this level we will not trigger recovery (Should we remove this??)
const float minimumSonarDistanceToConfirmRecovery = 1; //Meters
const uint periodBetweenCellularReports = 23; //Seconds
const uint periodBetweenSatelliteReports = 33; //Seconds
// const uint periodBetweenRadioReports = 2; //Seconds
const float minimumBatteryForLEDUse = 70;
const int anticollisionFlashInterval = 18; //External LED flash ever x seconds (see altitude condition too)
const float maximumAltitudeForAntiCollision = 35000; // In Feets..
// const uint periodBetweenSDWriteReports = 5; //Seconds
// const char *SDFileName = "NSP2017Log.txt";
bool debugMode = true;  //NO CONST!

//ADC PARAMS FOR SENSOR READINGS
const int ADC_OVERSAMPLE = 10; //Number of samples to take before making an accurate reading (average)


//SIMULATION SETTINGS >>>>>>
bool simulationMode = false;
bool gpsDebugDump = false;
bool satDebugDump = false;
bool sdDebugDump = false;
float simulatedApogeeAltitude = 200;
//<<<<<<

//EEPROM PARAMETERS FOR REBOOT PERSISTENT ADDRESS >>>>>>
int eeprom_rebootRecoveryModeAddress = 1;
uint16_t rebootRecoveryMode = 0; 
struct RebootRecoveryData {
	uint8_t version; //eeprom version check
 	uint8_t mode;
 	uint8_t groundRecovery;
 	float initialGPSAltitude;    
};

//PARTICLE SYSTEM PARAMS
SYSTEM_MODE(SEMI_AUTOMATIC);
SYSTEM_THREAD(ENABLED);



//====[MACROS]====================================================
#define GPS Serial1
#define COMPUTER Serial
#define SDCARD Serial4
#define GPSEvent serialEvent1
#define computerEvent serialEvent
#define SDCardEvent serialEvent4
#define SATCOM Serial5
#define SATCOMEvent serialEvent5
#define RADIO SoftSer
#define SATCOMEnablePin D2
#define EXTSTATUSLEDPin A5
#define BUZZERPin D4
#define SONARPin A1
#define TEMPSENSORPin A0
#define RADIO_RXPIN A2
#define RADIO_TXPIN A3
#define SW0 A5
#define ANTICOL D1
#define SENSORCOMP Wire1


//====[VARIABLES]=================================================
//## DO NOT MODIFY
//##
uint celTelemetryTurn = 0;
uint satTelemetryTurn = 0;
uint elapsedSeconds = 0;
uint lastCycleTime = 0;
uint recoveryDetectionIterations = 0;
float initialGPSAltitude = -1.0;
float lastPositiveGPSAltitude = 0.0;
float lastGPSAltitude = -1.0;
float altitudePerMinute = -1.0;
float altitudeGain = -1.0;
float altitudeOfApogee = -1.0;
float sonarDistance = -1.0; //In Meters
float internalTempC = -1.0; //In C
float ext_TempAnalogC = -1.0; //In C (From remote unit)
float ext_TempDigitalC = -1.0; //In C (From remote unit)
float ext_HumidityDigital = -1.0; //In C (From remote unit)
float ext_Pressure = -1.0; //
int gpsFixValue = 0;
float simulatedAltitude = 0.0; //For simulation only.


//States
bool simulatedClimbEnded = false;

FuelGauge fuel;
float batteryLevel = -1.0;

int satcomSignal = -1;
bool satcomAlive = false;

int cellSignalRSSI = -1;
int cellSignalQuality = -1;

bool transmittingToCloud = false;

// ALTIMETER MODULE: MS5607
Intersema::BaroPressure_MS5607B baro(true);

String computerSerialData;
String radioSerialData;
String SDCardSerialData;
String satSerialData;
String sensorCompData;
String lastSatModemRequest = "";


enum MissionStage { 
	ground,
	climb,	
	descent,
	recovery,
	recovery_confirmed
};

enum GPSState { 
	unknown,
	noFix,
	Fix  
};


TinyGPSPlus gpsParser;
MissionStage missionStage = ground;
GPSState gpsState = unknown;

TinyGPSCustom gpsFixType(gpsParser, "GPGGA", 6); // $GPGGA fixType decoding from GPS

ParticleSoftSerial SoftSer(RADIO_RXPIN, RADIO_TXPIN); 

//====[PARTICLE CLOUD FNs]=================================
bool success = Particle.function("c", computerRequest);

//##########.##########.##########.##########.##########.#########.#########.#########.#########.#########.#########.#########.#########

//====[MAIN PGM]==========================================
// ##
// ##
// ##
void setup() {	
	//TAKE CONTROL OF THE RGB LED ONBOARD
	RGB.control(true); 
	//Setup SATCOM
	pinMode(SATCOMEnablePin, OUTPUT);	
	pinMode(BUZZERPin, OUTPUT);	
	pinMode(EXTSTATUSLEDPin, OUTPUT);
	pinMode(SW0, INPUT_PULLDOWN);
	pinMode(ANTICOL, OUTPUT);
	// pinMode(SONARPin, INPUT);
	digitalWrite(SATCOMEnablePin, HIGH);
	digitalWrite(BUZZERPin, LOW);
	digitalWrite(EXTSTATUSLEDPin, LOW);
	
	//JOIN  WIRE NETWORK AS MASTER
	SENSORCOMP.begin();
	//CONNECT TO GPS
	GPS.begin(57600);
	//CONNECT TO COMPUTER VIA USB
	COMPUTER.begin(57600);	
	//CONNECT TO SDCARD LOGGER
	SDCARD.begin(115200); //MAKE SURE YOU SET THIS UP IN THE CONFIG.TXT (115200,26,3,0,1,1,0)
	//CONNECT TO XBEE
	RADIO.begin(9600);
	baro.init();
	delay(100);
	

	//INIT SDCARD
	bootSDCard();
	//SETUP BATTERY CHARGER 
	setupBatteryCharger();	
	//CHANGE LED COLOR
	RGB.color(0, 0, 255);
	RGB.brightness(100);	
	//Set Cell Modem depending on the initial setup configuration (See CellModemEnable var at start of code)
	setCellModem(cellModemEnabled);
	//WAIT 2 seconds to start
	delay(2000);
	RGB.color(255, 255, 255);
	RGB.brightness(0);
	//BEGIN SatCom Comunications (delay further to allow super cap to charge);
	SATCOM.begin(19200, SERIAL_8N1);
	setSatModem(satModemEnabled);
	//Start the basic timed events
	// masterTimer.start();	
	//READ INITIAL POWER LEFT In Battery
	updateLocalSensors();	
	batteryLevel = fuel.getSoC();
	
	delay(300);
	initialGPSAltitude = -1;
	sendToComputer("[Stage] Ground ");
}

void reqSesnorCompUpdate() {
		SENSORCOMP.requestFrom(1, 17); //Request data from external sensor slave unit.		 
}

void sensorCompEvent() {
		while(SENSORCOMP.available()){   
    		char c = SENSORCOMP.read();
    		sensorCompData = sensorCompData + c;    		
  		} 
  		decodeRemoteSensorData();
}

void decodeRemoteSensorData() {
		// Serial.println("[Event] i2C Event : " + sensorCompData);

		 String dTemp = getValue(sensorCompData, ',', 0);
		 String dHum = getValue(sensorCompData, ',', 1);
		 String aTemp = getValue(sensorCompData, ',', 2);

		 ext_TempDigitalC = dTemp.toFloat();
		 ext_HumidityDigital = dHum.toFloat();
		 ext_TempAnalogC = aTemp.toFloat();		 
		
  		sensorCompData = "";
}

String getValue(String data, char separator, int index)
{
    int found = 0;
    int strIndex[] = { 0, -1 };
    int maxIndex = data.length() - 1;

    for (int i = 0; i <= maxIndex && found <= index; i++) {
        if (data.charAt(i) == separator || i == maxIndex) {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i+1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void loop() {
	uint currentTime = millis(); //Get the time since boot.
	uint currentPeriod = currentTime - lastCycleTime; //Calculate elapsed time since last loop.	


	triggerAntiCollisionLight(currentPeriod);
	

	if (currentPeriod > 500) { //Every half a second
		reqSesnorCompUpdate();

		if (RADIO.available()) {
			radioEvent();
		}

		if (SENSORCOMP.available()) {
			sensorCompEvent();
		}
	
		signalFlareCheck();				
	}

	if (currentPeriod >= 1000) { //Every Second
		elapsedSeconds++;		
		setMissionIndicators();
		logDataToSDCard(); //HERE OR IN THE UPPER check?		
		satcomKeepAlive();
		sendDataToCloud();			
		updateStage();
		updateLocalSensors();
		doDebugToComputer();
		sendDataToRadio();
		getBaroData();
		lastCycleTime = currentTime; //Reset lastCycleTime for performing the next cycle calculation.				 
	}





	if (elapsedSeconds >= 240) { elapsedSeconds = 0; } //Prevent overflow
	
	
} 

void getBaroData() {	
	ext_Pressure = baro.getPressurePascals();
}
// ==========================================================
// PERIPHERALS
// ==========================================================
void bootSDCard() {
	delay(100);
	writeLineToSDCard("[EVENT] System Boot");

	if (Time.isValid() == true) {
		time_t time = Time.now();  	
		writeLineToSDCard("TimeStamp: " + Time.format(time, TIME_FORMAT_ISO8601_FULL));
	} else {
		writeLineToSDCard("[EVENT] System Boot");
		writeLineToSDCard("TimeStamp: Not provided by Kernel");
	}

	delay(200);
	sendToComputer("[SDCARD] Initialized to append writing");

	checkRebootRecoveryMode();
}

void checkRebootRecoveryMode() {
	RebootRecoveryData rebootRecoveryData;
	EEPROM.get(eeprom_rebootRecoveryModeAddress, rebootRecoveryData);

	if (rebootRecoveryData.version == 0) {		
		rebootRecoveryMode = rebootRecoveryData.mode;
	}


	if (rebootRecoveryMode == 1) {		
		initialGPSAltitude = rebootRecoveryData.initialGPSAltitude;
		cellMuteEnabled = false;
		satMuteEnabled = false;
		sdMuteEnabled = false;
		sendToComputer("[EVENT] >RECOVERY M ODE DETECTED<");
		sendToComputer("[EVENT] RECOVERY ALT: " + String(initialGPSAltitude));
		writeLineToSDCard("[EVENT] >RECOVERY MODE DETECTED<");
		writeLineToSDCard("[EVENT] RECOVERY ALT: " + String(initialGPSAltitude));

		if (rebootRecoveryData.groundRecovery == 1) {
			digitalWrite(BUZZERPin, HIGH);
			writeLineToSDCard("[EVENT] >GROUND RECOVERY STAGE DETECTED<");
			sendToComputer("[EVENT] >GROUND RECOVERY STAGE DETECTED<");
		}
	}

}

// ==========================================================
// PERIODIC TASKS
// ==========================================================
void setMissionIndicators() {

		if (missionStage == ground && Cellular.ready() == false) { RGB.color(255,165,0); } //Orange
		if (missionStage == ground && Cellular.connecting() == true) { RGB.color(50,50,255); } //Whitish
		if (missionStage == ground && Cellular.ready() == true) { RGB.color(0,255,0); } //Green
		if (missionStage == climb) { RGB.color(0,0,255); } //Blue
		if (missionStage == descent) { RGB.color(255,215,0); } //Yellow
		if (missionStage == recovery) { RGB.color(255,0,255); } //Magenta

		bool missionWarningLED = false;
		if (missionStage == ground) {
			if (performPreflightCheck() != 1 && batteryLevel >= minimumBatteryForLEDUse) {
				missionWarningLED = true;
			}
		}

		if (elapsedSeconds % 2 == 0 ) { //LEDS
			RGB.brightness(0);	
			digitalWrite(EXTSTATUSLEDPin, HIGH);
			if (missionWarningLED==true) { digitalWrite(EXTSTATUSLEDPin, HIGH); }
		} else {				
			if (debugMode == true && batteryLevel >= minimumBatteryForLEDUse) {
				RGB.brightness(100);				
			}		

			if (missionWarningLED==true) { digitalWrite(EXTSTATUSLEDPin, LOW); } 
		}

	

}

void satcomKeepAlive() {
	if (elapsedSeconds % 9 == 0) {		
			if (satModemEnabled == true && satcomAlive == false) {			
				SatPing();
			}

			if (satModemEnabled == true && satcomAlive == true) {			
				getSatSignal();	
			}			
		}
}

void sendDataToCloud() {
	if (elapsedSeconds % periodBetweenCellularReports == 0) { //CELLULAR
		sendStatusToCell(); //Send STAT STRING to cloud via CELL if connected..			

	}

	if (elapsedSeconds % periodBetweenSatelliteReports == 0) {	//SATELLITE
		sendStatusToSat();
	}
}

void sendDataToRadio() {
	if (transmittingToCloud == false) {	//SATELLITE
		sendToRadio(telemetryString());		
	}
}

void logDataToSDCard() { //TODO [Set a period for storage]
	if (sdMuteEnabled == false) {
		logStatusToSDCard();		
	}
}

void updateLocalSensors() {
	sonarDistance = readSonarDistance();
	batteryLevel = fuel.getSoC();
	internalTempC = readInternalTemp();	
}

void doDebugToComputer() {		 	
 	if (debugMode == true && gpsDebugDump == false && satDebugDump == false && simulationMode == false) {
		sendToComputer(telemetryString());
	}
}

void signalFlareCheck() {
	if (missionStage == recovery_confirmed) {
		if (elapsedSeconds % 2 == 0) {			
				digitalWrite(BUZZERPin, HIGH);										
			} else {			
				digitalWrite(BUZZERPin, LOW);						
		}
	}
}

void triggerAntiCollisionLight(uint currentPeriod) {
	if (currentPeriod > 1000 && (elapsedSeconds % anticollisionFlashInterval == 0) && (lastGPSAltitude < maximumAltitudeForAntiCollision)) {		
		digitalWrite(ANTICOL, HIGH);		
	} else {
		digitalWrite(ANTICOL, LOW);
	}
}


// ==========================================================
// STATE MACHINE 
// ==========================================================
void updateStage() {

	float gpsAltitude = gpsParser.altitude.feet() + simulatedAltitude;	
	altitudeGain = (gpsAltitude - initialGPSAltitude);
	float altitudeTrend = gpsAltitude - lastGPSAltitude;	
	altitudePerMinute = altitudeTrend * 60;
		

	
	//SIMULATOR>>>////////////
	if (simulationMode) {
		if (simulatedClimbEnded == false && (missionStage == ground || missionStage == climb)) {
			simulatedAltitude+=random(5, 8); //Between 300/480ft per minute
			if (altitudeGain >= simulatedApogeeAltitude) {
				simulatedClimbEnded = true;	
			}
		} else if (simulatedClimbEnded == true) {
			simulatedAltitude-=random(5, 17); //Between -600/-1000ft per minute
			if (gpsAltitude <= initialGPSAltitude) {
				simulatedClimbEnded = false;
			}
		} 

		// if (debugMode == true) {
			COMPUTER.print("[SIMULATOR] Altitude: " + String(gpsAltitude));
			COMPUTER.print("ft | VSI: " + String(altitudePerMinute));			
			COMPUTER.print("ftxmin | TREND: " + String(altitudeTrend));
			COMPUTER.println(" | GAIN: " + String(altitudeGain) + " ft");

			// if (missionStage == recovery) {
			// 	simulationMode = false;
			// 	sendToComputer("[SIMULATOR] Simulation Ended");
			// }
		// }
	} 
	//<<<<SIMULATOR////////////

	//State Machine
	if (gpsState == Fix && initialGPSAltitude > 0 && missionStage == ground) {
		if ((altitudeGain > altitudeGainClimbTrigger) && (altitudePerMinute > altitudePerMinuteGainClimbTrigger)) {
			missionStage = climb;
			lastPositiveGPSAltitude = gpsAltitude;		
			sendToComputer("[Stage] Climb Detected at: " + String(gpsAltitude));		
			writeLineToSDCard("[Stage] Climb Detected at: " + String(gpsAltitude));
		}
	}

		if ((missionStage == climb) && (gpsAltitude < lastGPSAltitude) && (gpsAltitude > altitudeOfApogee) && (lastGPSAltitude != -1))  {
			sendToComputer("[Event] Apogee Reached at " + String(gpsAltitude));		
			writeLineToSDCard("[Event] Apogee Reached at " + String(gpsAltitude));		
			altitudeOfApogee = gpsAltitude;
		}
			
		if (missionStage == climb && altitudeTrend < 10 && altitudePerMinute <= altitudeLossPerMinuteForDescentDetection) {
			missionStage = descent;		
			sendToComputer("[Stage] Descent Detected at " + String(gpsAltitude));
			writeLineToSDCard("[Stage] Descent Detected at " + String(gpsAltitude));

		}

		if ((missionStage == descent && altitudePerMinute < 2) && (gpsAltitude <= minimumAltitudeToTriggerRecovery)) {
			recoveryDetectionIterations++;
			if (recoveryDetectionIterations >= iterationsInLowDescentToTriggerRecovery) {			
				missionStage = recovery;	
				digitalWrite(BUZZERPin, HIGH);		
				sendToComputer("[Stage] Recovery Detected at " + String(gpsAltitude));		
				writeLineToSDCard("[Stage] Recovery Detected at " + String(gpsAltitude));		
					RebootRecoveryData rebootRecoveryData;
					rebootRecoveryData.mode = rebootRecoveryMode;
					rebootRecoveryData.initialGPSAltitude = initialGPSAltitude;		
					rebootRecoveryData.version = 0;
					rebootRecoveryData.groundRecovery = 1;
					EEPROM.put(eeprom_rebootRecoveryModeAddress, rebootRecoveryData);
			}
		}
	

	if (missionStage == recovery)  {
		//TODO: Add bypass to signalflare
		if (sonarDistance <= minimumSonarDistanceToConfirmRecovery) {
			missionStage = recovery_confirmed;
			sendToComputer("[Stage] Recovery Confirmed at " + String(sonarDistance));
			writeLineToSDCard("[Stage] Recovery Confirmed at " + String(sonarDistance));
		}
	}

	lastGPSAltitude = gpsAltitude;	
}


// ==========================================================
// LOCAL SENSORS &  PERIPHERALS
// ==========================================================
float readSonarDistance() {	
	float rawAnalog = 0;
	
	for (int i = 0; i < ADC_OVERSAMPLE; i++) { //Do several samples and then average them.
	 rawAnalog += analogRead(SONARPin);	
	}

	rawAnalog = rawAnalog / ADC_OVERSAMPLE;

	float Vm = (rawAnalog * 0.8); //Measured Voltage (MILI VOLTS) [0.000805] [0.8]
	float Vi =  6.44; //0.00322 //3.222
	float m = ((Vm/Vi) * 2.54) / 100;    //Inches to cm || to meters

	return m;
}

float readInternalTemp() {	
	float rawAnalog = 0;
	
	for (int i = 0; i < ADC_OVERSAMPLE; i++) { //Do several samples and then average them.
	 rawAnalog += analogRead(TEMPSENSORPin);	
	}

	rawAnalog = rawAnalog / ADC_OVERSAMPLE;
 
	float tempC = (((rawAnalog * 3.3)/4095) - 0.5) * 100;

	return tempC;
}

void setupBatteryCharger() {
	PMIC pmic; //Initalize the PMIC class so you can call the Power Management functions below. 	
	pmic.setChargeCurrent(0,0,1,0,0,0);
	pmic.setInputCurrentLimit(1500);
	pmic.setChargeVoltage(4208);
}

// ==========================================================
// HELPER FUNCTIONS
// ==========================================================
String gpsTimeFormatted() {
	//Format: HHMMSS
	String hour = String(gpsParser.time.hour());
	String minute = String(gpsParser.time.minute());
	String second = String(gpsParser.time.second());

	if (hour.length() == 1) {
		hour = "0" + String(gpsParser.time.hour());
	} 

	if (minute.length() == 1) {
		minute = "0" + String(gpsParser.time.minute());
	} 

	if (second.length() == 1) {
		second = "0" + String(gpsParser.time.second());
	} 	

	return hour + minute + second;
}

String gpsDateFormatted() {
	//Format: DDMM
	String day = String(gpsParser.date.day());
	String month = String(gpsParser.date.month());
	
	if (day.length() == 1) {
		day = "0" + String(gpsParser.date.day());
	} 

	if (month.length() == 1) {
		month = "0" + String(gpsParser.date.month());
	} 

	return day + month;
}

String gpsTimeStamp() {			
	return gpsDateFormatted() + gpsTimeFormatted();
}

String missionStageShortString() {
	if (missionStage == ground)  { return "G"; }
	if (missionStage == climb)  { return "C"; }	
	if (missionStage == descent)  { return "D"; }
	if (missionStage == recovery)  { return "R"; }
}

void updateGPSFixType() {
	String inValue =  gpsFixType.value();
	gpsFixValue = inValue.toInt();

	switch(gpsFixValue) {
		case 0:
		gpsState = noFix; 
		break; 
		case 1:
		gpsState = Fix; 
		break; 
		case 2:
		gpsState = Fix; 			
		break;
		default :
		gpsState = noFix; 
		break;
	}
}

void sendToComputer(String text) {
	TRY_LOCK(COMPUTER) {	
		COMPUTER.println(text);
	}		
}

void sendToRadio(String text) {	
	int xmlength = text.length();
	String textOutput = String(xmlength) + ">" + text;
	RADIO.println(textOutput);
}

void sendToComputerAndRadio(String text) {
	sendToComputer(text);
	sendToRadio(text);
}

void sendToSDCard(String text) {	
		SDCARD.println(text);	
}

void writeLineToSDCard(String line) {	
  	SDCARD.println(line);
}


// ==========================================================
// INTERRUPT BASED SERIAL COM (CALLBACKS)
// ==========================================================
void SATCOMEvent() {	
		while (SATCOM.available()) {			
			char c = SATCOM.read();	
			satSerialData = satSerialData + c;								
			if (satDebugDump==true && gpsDebugDump==false) {
				TRY_LOCK(COMPUTER) {
					COMPUTER.write(c);
				}
				RADIO.write(c);
			}
			if (c == '\r') {				
				satSerialData = satSerialData.trim();									
				if (satSerialData.substring(0,5) == "+CSQ:") {
					String signal = satSerialData.substring(5);
					satcomSignal = signal.toInt();										
				}

				if (satSerialData.substring(0,2) == "OK") {
					if (satcomAlive == false && satModemEnabled == true && lastSatModemRequest == "AT") {
						satcomAlive = true;	
						sendToComputer("[Event] SatCom Alive");	
						writeLineToSDCard("[Event] SatCom Alive");
						getSatSignal();
					}
					
					if (lastSatModemRequest == "AT+SBDWT=") {
						SATCOM.println("AT+SBDIX");	 //Do transmit session!					
					} 

					if (lastSatModemRequest == "AT&K0") {
						// SATCOM.println("ATE0"); //Echo Off
						// lastSatModemRequest == "ATE0";
					}

				  lastSatModemRequest = "";						
				}					
				
				satSerialData = ""; //Empty Serial Buffer
			}
		}	
}

void SW0Event() 
{
	COMPUTER.write("C");
}

void GPSEvent()
{	
	
		while (GPS.available()) {					
			char c = GPS.read();			
			gpsParser.encode(c);
			if (gpsDebugDump==true && satDebugDump==false) {
				TRY_LOCK(COMPUTER) {
					COMPUTER.print(c);  
				}
			}			
		}
		

	updateGPSFixType();

	if (gpsState == Fix && initialGPSAltitude==-1 && gpsParser.altitude.feet() > 0) {
		initialGPSAltitude = gpsParser.altitude.feet();					
		sendToComputer("[Event] Initial Altitude Set to: " + String(initialGPSAltitude,0));
		writeLineToSDCard("[Event] Initial Altitude Set to: " + String(initialGPSAltitude,0));
	}
}



void computerEvent() 
{
	if (COMPUTER.available()) {
		while (COMPUTER.available()) {			
			char c = COMPUTER.read();
			computerSerialData = computerSerialData + c;
			if (c == '\n') {
				computerSerialData = computerSerialData.remove(computerSerialData.indexOf('\n'));
				computerRequest(computerSerialData);
				computerSerialData = "";
			}
		}
	}	
}

void radioEvent() {	
	if (RADIO.available()) {
		while (RADIO.available()) {			
			char c = RADIO.read();
			radioSerialData = radioSerialData + c;
			if (c == '\n') {
				radioSerialData = radioSerialData.remove(radioSerialData.indexOf('\n'));
				computerRequest(radioSerialData);
				radioSerialData = "";
			}
		}
	}	
}

void SDCardEvent()
{
	if (SDCARD.available()) {
		while (SDCARD.available()) {			
			char c = SDCARD.read();
			SDCardSerialData = SDCardSerialData + c;
			if (c == '\r') {
				if (sdDebugDump == true) {
					sendToComputer(SDCardSerialData);
				}
				
				SDCardSerialData = SDCardSerialData.remove(SDCardSerialData.indexOf('\n'));				
				SDCardSerialData = "";
			}
		}
	}	
}



// ==========================================================
// REMOTE CONTROL
// ==========================================================
int computerRequest(String param) {	
	
	if  (param == "deboff") {
		debugMode = false;
		sendToComputerAndRadio("OK");		
		return 1;
	}
	if  (param == "debon") {
		debugMode = true;
		sendToComputerAndRadio("OK");
		return 1;
	}
	if  (param == "simon") {
		simulationMode = true;		
		sendToComputerAndRadio("OK");
		return 1;
	}
	if  (param == "simoff") {
		simulationMode = false;
		sendToComputerAndRadio("OK");
		return 1;
	}
	if  (param == "reset") {
		simulationMode = false;
		missionStage = ground;
		initialGPSAltitude = -1.0;
		lastPositiveGPSAltitude = 0.0;
		altitudeGain = -1.0;
		lastGPSAltitude = -1.0;
		altitudePerMinute = -1.0;
		altitudeOfApogee = -1.0;
		simulatedAltitude = 0.0; //For simulation only.
		sendToComputerAndRadio("OK");
		return 1;
	}
	if (param == "reboot") {
		sendToComputerAndRadio("OK");
		System.reset();		
	}
	if (param == "cellon") {				
		setCellModem(true);		
		return 1;
	}
	if (param == "celloff") {		
		setCellModem(false);
		return 1;
	}
	if (param == "cellmute") {
		cellMuteEnabled = !cellMuteEnabled;
		if (cellMuteEnabled == true) {
			sendToComputerAndRadio("[Event] CellMute Enabled");
			return 1;
		} else {
			sendToComputerAndRadio("[Event] CellMute Disabled");
			return 0;
		}
	}
	if (param == "satmute") {
		satMuteEnabled = !satMuteEnabled;
		if (satMuteEnabled == true) {
			sendToComputerAndRadio("[Event] SatMute Enabled");
			return 1;
		} else {
			sendToComputerAndRadio("[Event] SatMute Disabled");
			return 0;
		}
	}

	if (param == "sdmute") {
		sdMuteEnabled = !sdMuteEnabled;
		if (sdMuteEnabled == true) {
			sendToComputerAndRadio("[Event] SDMute Enabled");			
			return 1;
		} else {
			sendToComputerAndRadio("[Event] SDMute Disabled");
			return 0;
		}
	}

	if (param == "flymode") {
		cellMuteEnabled = false;
		satMuteEnabled = false;
		sdMuteEnabled = false;
		rebootRecoveryMode = 1;

		RebootRecoveryData rebootRecoveryData;
		rebootRecoveryData.mode = rebootRecoveryMode;
		rebootRecoveryData.initialGPSAltitude = initialGPSAltitude;		
		rebootRecoveryData.version = 0;
		rebootRecoveryData.groundRecovery = 0;
		EEPROM.put(eeprom_rebootRecoveryModeAddress, rebootRecoveryData);

		// EEPROM.put(eeprom_rebootRecoveryModeAddress, rebootRecoveryMode);
		writeLineToSDCard("[EVENT] Fly Mode Armed");
		sendToComputerAndRadio("[EVENT] Fly Mode Armed");
		
		return 1;
	}

	if (param == "rigmode") {
		cellMuteEnabled = true;
		satMuteEnabled = true;		
		rebootRecoveryMode = 0;		
		
		RebootRecoveryData rebootRecoveryData;
		rebootRecoveryData.mode = rebootRecoveryMode;
		rebootRecoveryData.initialGPSAltitude = initialGPSAltitude;
		rebootRecoveryData.version = 0;
		rebootRecoveryData.groundRecovery = 0;
		EEPROM.put(eeprom_rebootRecoveryModeAddress, rebootRecoveryData);
		writeLineToSDCard("[EVENT] Rig Mode Enabled");
		sendToComputerAndRadio("[EVENT] Rig Mode Enabled");
		return 1;
	}



	if (param == "saton") {				
		setSatModem(true);		
		return 1;
	}
	if (param == "satoff") {		
		setSatModem(false);
		return 1;
	}
	if (param == "comon") {				
		setSatModem(true);	
		setCellModem(true);	
		return 1;
	}
	if (param == "comoff") {		
		setSatModem(false);
		setCellModem(false);
		return 1;
	}
	if (param == "gpsdump") {		
		gpsDebugDump = !gpsDebugDump;
		return 1;
	}
	if (param == "satdump") {		
		satDebugDump = !satDebugDump;
		return 1;
	}
	if (param == "sddump") {
		sdDebugDump = !sdDebugDump;		
		return 1;
	}
	if (param == "querysatsignal") {		
		sendToComputer("OK");
		getSatSignal();
		return 1;
	}	
	if (param == "querycellsignal") {		
		sendToComputer("OK");
		getCellSignal();
		return 1;
	}
	if (param == "buzzeron") {		
		digitalWrite(BUZZERPin, HIGH);		
		return 1;
	}
	if (param == "buzzeroff") {		
		digitalWrite(BUZZERPin, LOW);		
		return 1;
	}	
	if (param == "buzzerchirp") {				
		digitalWrite(BUZZERPin, HIGH);
		delay(1000);
		digitalWrite(BUZZERPin, LOW);		
		return 1;
	}

	if (param == "resetinitialaltitude") {				
		initialGPSAltitude = gpsParser.altitude.feet();
		return initialGPSAltitude;
	}

	if (param == "preflight?") {		
		return performPreflightCheck();
	}

	if (param == "initialaltitude?") {		
		sendToComputerAndRadio(String(initialGPSAltitude));
		return initialGPSAltitude;
	}		
	if  (param == "vsi?") {
		sendToComputerAndRadio(String(altitudePerMinute));	
		return (int)altitudePerMinute;		
	}
	if  (param == "alt?") {
		sendToComputerAndRadio(String(lastGPSAltitude));
		return (int)lastGPSAltitude;	
	}
	if  (param == "apogee?") {
		sendToComputerAndRadio(String(altitudeOfApogee));
		return (int)altitudeOfApogee;
	}
	if  (param == "stage?") {
		sendToComputerAndRadio(missionStageShortString());
		return missionStage;
	}	
	if  (param == "cell?") {
		if (Cellular.ready() == true) { 
			sendToComputerAndRadio("YES");
		} else {
			sendToComputerAndRadio("NO");
		}
	}	
	if  (param == "cellconnecting?") {
		if (Cellular.connecting() == true) { 
			sendToComputerAndRadio("YES");
		} else {
			sendToComputerAndRadio("NO");
		}
	}
	if  (param == "cellsignal?") {		
		CellularSignal sig = Cellular.RSSI();		
		sendToComputerAndRadio(sig);
		return sig.rssi;		
	}	

	if  (param == "cloud?") {
		if (Particle.connected() == true) { 
			sendToComputerAndRadio("YES");			
		} else {
			sendToComputerAndRadio("NO");
		}
	}	
	
	if  (param == "satsignal?") {
		sendToComputerAndRadio(String(satcomSignal));
		return satcomSignal;
	}	

	if  (param == "satenabled?") {
		if (satModemEnabled) {
			sendToComputerAndRadio("YES");
			return 1;
		}
		sendToComputerAndRadio("NO");
		return 0;
	}

	if (param == "flymode?") {	
		if (rebootRecoveryMode == 1) {
			sendToComputerAndRadio("Fly Mode Armed");
		} else {
			sendToComputerAndRadio("Rig Mode Enabled");
		}

		return rebootRecoveryMode;
	}

	if (param == "fwversion?") {
		TRY_LOCK(COMPUTER) {
			COMPUTER.printlnf("Firmware version: %s", System.version().c_str());
		}
		return 1;
	}

	if (param == "bat?") {
		sendToComputerAndRadio(String(batteryLevel));		
		return batteryLevel;
	}

	if (param == "gpsfix?") {
		sendToComputerAndRadio(String(gpsFixValue));
		return gpsFixValue;
	}

	if (param == "sonar?") {
		sendToComputerAndRadio(String(sonarDistance) + " meters");
		return int(sonarDistance*100); //meters to centimeters and int
	}

	if (param == "temp?") {
		sendToComputerAndRadio(String(internalTempC) + " C");
		return int(internalTempC); //meters to centimeters and int
	}

	if (param == "$") {
		sendToComputerAndRadio(SDLogString());
		return 1;		
	}


	if (param == "$$") {	
		sendToComputerAndRadio("OK");	
		sendStatusToCell();		
		return 1;		
	}	

	// if (param == "x$$") {	
	// 	sendToComputer("OK");	
	// 	sendExtendedDataToCell();
	// 	return 1;		
	// }	

	if (param == "$$$") {	
		sendToComputerAndRadio("OK");
		sendStatusToSat();
		sendStatusToCell();
		return 1;
	}


	if (param == "$$$$") {	
		sendToComputerAndRadio("OK");
		sendStatusToCell();
		//sendExtendedDataToCell();
		sendStatusToSat();
		//sendExtendedDataToSat();
		return 1;
	}

	// if (param == ">ls") {
	// 	sendToSDCard("new NSP2017Log.txt");
	// 	sendToComputer("OK");
	// 	return 1;
	// }

	if (param == ">cmd") {
		SDCARD.write(26);
		SDCARD.write(26);
		SDCARD.write(26); 
		delay(100);
		sendToComputerAndRadio("OK");
	}

	if (param == ">init") {
  		sendToSDCard("init");
  		sendToComputerAndRadio("OK");
	}

	if (param == ">set4") {
  		sendToSDCard("set");
  		sendToComputerAndRadio("OK");
	}

	

	if (param == "?") {
		TRY_LOCK(COMPUTER) {
		COMPUTER.println("-------------------------.--------------------------.--------------------");		
		COMPUTER.println("Status Sentence (1hz):");
		COMPUTER.println("TimeStamp,Lat,Lon,Alt,Speed,HDG,GPS_SATS,GPS_PRECISION,BATTLVL,IRIDIUM_SATS,INT_TEMP,STAGE");
		COMPUTER.println("-------------------------.--------------------------.--------------------");		
		COMPUTER.println("deboff = Debug Off");
		COMPUTER.println("debon = Debug On");
		COMPUTER.println("simon = Start Simulation");
		COMPUTER.println("simoff = Stop Simulation");
		COMPUTER.println("reset = Set mission to ground mode");
		COMPUTER.println("reboot = Reboot Flight Computer");
		COMPUTER.println("simon = Start Simulation");
		COMPUTER.println("cellon = Cell Modem On");
		COMPUTER.println("celloff = Cell Modem Off");
		COMPUTER.println("cellmute = Toggle Cell Reporting");
		COMPUTER.println("satmute = Toggle Sat Reporting");
		COMPUTER.println("flymode = Set system to fly mode [needed for mission]");
		COMPUTER.println("rigmode = Toggle Sat Reporting [disbales fly mode]");
		COMPUTER.println("saton = SAT Modem ON");
		COMPUTER.println("satoff = SAT Modem Off");
		COMPUTER.println("comoff = All Comunication systems OFF [cell + sat]");
		COMPUTER.println("comon = All Comunication systems ON [cell + sat]");
		COMPUTER.println("gpsdump = GPS Serial Dump to computer toggle");
		COMPUTER.println("satdump = SATCOM Serial Dump to computer toggle");
		COMPUTER.println("sddump = SDCARD Serial Dump to computer toggle");
		COMPUTER.println("querysatsignal = Send a request to the satelite modem to get sat signal");
		COMPUTER.println("querycellsignal = Send a request to the cellular modem to get RSSI signal");
		COMPUTER.println("buzzeron = Turn Buzzer ON");
		COMPUTER.println("buzzeroff = Turn Buzzer ON");
		COMPUTER.println("buzzerchirp = Chirp the buzzer");
		COMPUTER.println("resetinitialaltitude = Set the initial altitude to current altitude");
		COMPUTER.println("preflight? = Go no Go for launch");
		COMPUTER.println("initialaltitude? = Get the initial altitude set uppon gps fix");
		COMPUTER.println("vsi? = Vertical Speed?");
		COMPUTER.println("alt? = Altitude in feet?");
		COMPUTER.println("cell? = Cell Status?");		
		COMPUTER.println("cellconnecting? = Cell Modem attempting to connect?");
		COMPUTER.println("cellsignal? = Cell Signal Strength [RSSI,QUAL] ?");
		COMPUTER.println("cloud? = Is cloud available?");
		COMPUTER.println("satsignal? = 0-5 Satcom signal strength?");		
		COMPUTER.println("satenabled? = Is the sat modem enabled?");		
		COMPUTER.println("flymode? = Is the system in fly mode?");		
		COMPUTER.println("bat? = Get battery level?");	
		COMPUTER.println("gpsfix? = Get GpsFix ValueType? (0=NoFix,1=Fix,2=DGPSFix)");
		COMPUTER.println("sonar? = Get the sonar distance in meters. (cm for cell)");
		COMPUTER.println("temp? = Get the internal (onboard) temperature in C");
		COMPUTER.println("fwversion? = OS Firmware Version?");		
		COMPUTER.println(">cmd = Set SD to Command Mode");		
		COMPUTER.println(">init = Force Initialize");			
		COMPUTER.println(">set = Enter set Menu");
		COMPUTER.println("$ = Print status string");		
		COMPUTER.println("$$ = Print and send to CELL cloud status string");		
		COMPUTER.println("$$$ = Print and send to SAT cloud status string");		
		COMPUTER.println("-------------------------.--------------------------");
		}
		//Todo (Eliminate duplicated code)
		RADIO.println("-------------------------.--------------------------.--------------------");		
		RADIO.println("Status Sentence (1hz):");
		RADIO.println("TimeStamp,Lat,Lon,Alt,Speed,HDG,GPS_SATS,GPS_PRECISION,BATTLVL,IRIDIUM_SATS,INT_TEMP,STAGE");
		RADIO.println("-------------------------.--------------------------.--------------------");		
		RADIO.println("deboff = Debug Off");
		RADIO.println("debon = Debug On");
		RADIO.println("simon = Start Simulation");
		RADIO.println("simoff = Stop Simulation");
		RADIO.println("reset = Set mission to ground mode");
		RADIO.println("reboot = Reboot Flight Computer");
		RADIO.println("simon = Start Simulation");
		RADIO.println("cellon = Cell Modem On");
		RADIO.println("celloff = Cell Modem Off");
		RADIO.println("cellmute = Toggle Cell Reporting");
		RADIO.println("satmute = Toggle Sat Reporting");
		RADIO.println("flymode = Set system to fly mode [needed for mission]");
		RADIO.println("rigmode = Toggle Sat Reporting [disbales fly mode]");
		RADIO.println("saton = SAT Modem ON");
		RADIO.println("satoff = SAT Modem Off");
		RADIO.println("comoff = All Comunication systems OFF [cell + sat]");
		RADIO.println("comon = All Comunication systems ON [cell + sat]");
		RADIO.println("gpsdump = GPS Serial Dump to computer toggle");
		RADIO.println("satdump = SATCOM Serial Dump to computer toggle");
		RADIO.println("sddump = SDCARD Serial Dump to computer toggle");
		RADIO.println("querysatsignal = Send a request to the satelite modem to get sat signal");
		RADIO.println("querycellsignal = Send a request to the cellular modem to get RSSI signal");
		RADIO.println("buzzeron = Turn Buzzer ON");
		RADIO.println("buzzeroff = Turn Buzzer ON");
		RADIO.println("buzzerchirp = Chirp the buzzer");
		RADIO.println("resetinitialaltitude = Set the initial altitude to current altitude");
		RADIO.println("preflight? = Go no Go for launch");
		RADIO.println("initialaltitude? = Get the initial altitude set uppon gps fix");
		RADIO.println("vsi? = Vertical Speed?");
		RADIO.println("alt? = Altitude in feet?");
		RADIO.println("cell? = Cell Status?");		
		RADIO.println("cellconnecting? = Cell Modem attempting to connect?");
		RADIO.println("cellsignal? = Cell Signal Strength [RSSI,QUAL] ?");
		RADIO.println("cloud? = Is cloud available?");
		RADIO.println("satsignal? = 0-5 Satcom signal strength?");		
		RADIO.println("satenabled? = Is the sat modem enabled?");		
		RADIO.println("flymode? = Is the system in fly mode?");		
		RADIO.println("bat? = Get battery level?");	
		RADIO.println("gpsfix? = Get GpsFix ValueType? (0=NoFix,1=Fix,2=DGPSFix)");
		RADIO.println("sonar? = Get the sonar distance in meters. (cm for cell)");
		RADIO.println("temp? = Get the internal (onboard) temperature in C");
		RADIO.println("fwversion? = OS Firmware Version?");		
		RADIO.println(">cmd = Set SD to Command Mode");		
		RADIO.println(">init = Force Initialize");			
		RADIO.println(">set = Enter set Menu");
		RADIO.println("$ = Print status string");		
		RADIO.println("$$ = Print and send to CELL cloud status string");		
		RADIO.println("$$$ = Print and send to SAT cloud status string");		
		RADIO.println("-------------------------.--------------------------");
	}

	return -99;
}

int performPreflightCheck() {
		if (initialGPSAltitude == -1) {
			// sendToComputer("NO GO - MISSING INITIAL ALTITUDE");
			return -1;
		}
		
		if (missionStage != ground) {
			// sendToComputer("NO GO - STAGE NOT IN GOUND MODE");
			return -2;
		}

		if (gpsParser.hdop.value() > 300) {
			// sendToComputer("NO GO - GPS PRECISION OUT OF RANGE");
			return -3;
		}

		if (gpsState != Fix) {
			// sendToComputer("NO GO - GPS DOES NOT HAVE FIX");
			return -4;
		}


		if (batteryLevel < 80) {
			// sendToComputer("NO GO - LOW BATTERY FOR LAUNCH");
			return -5;	
		}

		if (sonarDistance > 10) {
			// sendToComputer("NO GO - SONAR TEST FAILED");
			return -6;	
		}	


		if (satModemEnabled == false) {
			// sendToComputer("NO GO - SAT MODEM IS OFF");
			return -7;	
		}

		if (cellModemEnabled == false) {
			// sendToComputer("NO GO - CELL MODEM IS OFF");
			return -8;	
		}

		if (satcomSignal < 2) {
			// sendToComputer("NO GO - NOT ENOUGH SATCOM SATS FOR LAUNCH");
			return -9;	
		}

		getCellSignal();
		if (cellSignalRSSI <= 0 || cellSignalQuality <= 0) {
			// sendToComputer("NO GO - NOT ENOUGH CELL SIGNAL FOR LAUNCH");
			return -10;	
		}

		// sendToComputer("[System] GO FOR LAUNCH");
		// writeLineToSDCard("[System] GO FOR LAUNCH");
		return 1;
}

// ==========================================================
// TELEMETRY - MODEMS - CONTROL
// ==========================================================
void sendStatusToCell() {
	if (Particle.connected() == true && cellMuteEnabled == false && transmittingToCloud == false) {
		transmittingToCloud = true;
		Particle.publish("S",telemetryString());		
		transmittingToCloud = false;
	}
}

void sendStatusToSat() {
	if (satMuteEnabled == false) {	
		sendTextToSat(telemetryString());
	}
}

void logStatusToSDCard() {
	writeLineToSDCard(SDLogString());
}

String telemetryString() {	 //THIS IS ONE OF THE STRINGS THAT WILL BE SENT FOR TELEMETRY	
	String value = gpsTimeStamp() + "," + 
	String(gpsParser.location.lat(), 6) + "," + 
	String(gpsParser.location.lng(), 6) + "," + 
	String(gpsParser.altitude.feet(),0) + "," + 
	String(gpsParser.speed.knots(),0) + "," +
	String(gpsParser.course.deg(),0) + "," + 
	String(gpsParser.satellites.value()) + "," + 
	String(gpsParser.hdop.value()) +  "," +    
	String(batteryLevel,0) +  "," + 
	String(satcomSignal) +  "," + 
	String(internalTempC,0) +  "," + 
	String(ext_TempDigitalC,1) +  "," + 
	String(ext_HumidityDigital,0) +  "," +
	String(ext_TempAnalogC,1) +  "," + 
	String(ext_Pressure,0) +  "," + 
	missionStageShortString();

    return value;
}


String SDLogString() {
	String value =  gpsTimeStamp() + "," + 
	String(gpsParser.location.lat(), 6) + "," + 
	String(gpsParser.location.lng(), 6) + "," + 
	String(gpsParser.altitude.feet(),0) + "," + 
	String(gpsParser.speed.knots(),0) + "," +
	String(gpsParser.course.deg(),0) + "," + 
	String(gpsParser.satellites.value()) + "," + 
	String(gpsParser.hdop.value()) +  "," +    
	String(batteryLevel,0) +  "," + 
	String(satcomSignal) +  "," + 
	String(internalTempC,2) +  "," +
	String(ext_TempDigitalC,2) +  "," + 
	String(ext_HumidityDigital,2) +  "," + 
	String(ext_TempAnalogC,2) +  "," + 	 
	String(ext_Pressure,2) +  "," + 
	String(sonarDistance,0) +  "," + 
	String(altitudePerMinute,0) +  "," + 
	String(altitudeGain,0) +  "," + 
	missionStageShortString();

	return value;
}

void setCellModem(bool value) {		
	cellModemEnabled = value;
	if ((cellModemEnabled == true) && (Cellular.ready() == false) && (Cellular.connecting() == false)) {		
			Cellular.on();
			Cellular.connect();
			Particle.connect();					
		sendToComputer("[Event] Cell Modem ON");
		getCellSignal();		
	} else if (cellModemEnabled == false) {
		Cellular.off();
		cellSignalRSSI = -1;
		cellSignalQuality = -1;
		sendToComputer("[Event] Cell Modem OFF");
	}
}

void getCellSignal() {
	if (cellModemEnabled == false) { return; }		
		CellularSignal cellSignal = Cellular.RSSI();
		cellSignalRSSI = cellSignal.rssi;
		cellSignalQuality = cellSignal.qual;
}

void setSatModem(bool value) {	
	if (satModemEnabled == false && value == true) {  sendToComputer("[Event] SAT Modem ON"); }
	satModemEnabled = value;	
	if (satModemEnabled == false) {   satcomAlive = false; satcomSignal = -1; sendToComputer("[Event] SAT Modem OFF"); return; }	
	SATCOM.println("AT&K0");		
	lastSatModemRequest = "AT&K0";	

	getSatSignal();	
}

void getSatSignal() {
	if (satModemEnabled == false) { return; }				
		SATCOM.println("AT+CSQ");
		lastSatModemRequest = "AT+CSQ";	

}

void sendTextToSat(String text) {
	if (satModemEnabled == false) { return; }			
		SATCOM.println("AT+SBDWT=" + text + "\r");
		lastSatModemRequest = "AT+SBDWT=";			
}

void SatPing() {	
		SATCOM.println("AT");
		lastSatModemRequest = "AT";
}

