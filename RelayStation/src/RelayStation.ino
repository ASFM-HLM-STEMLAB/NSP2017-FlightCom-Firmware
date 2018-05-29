
SYSTEM_THREAD(ENABLED);


String radioSerialData;
String computerSerialData;

//====[MACROS]====================================================
#define RADIO Serial1
#define RADIOEvent serialEvent1
#define COMPUTEREvent serialEvent
#define COMPUTER Serial
#define RadioStatusLED D0
#define CloudStatusLED C5

bool success = Particle.function("rdio", groundRequest);
int loopPeriod = 0;
int lastTime = 0;
int seconds = 0;
int halfSeconds = 0;
bool sendingToCloud = false;

String lastMessage = "";


int messageCountBuffer = 0; 


//====[SETUP]====================================================
void setup() {
  Serial.begin();
  RADIO.begin(9600);
  delay(100);
  pinMode(RadioStatusLED, OUTPUT);	
  pinMode(CloudStatusLED, OUTPUT);	
  delay(100);
  digitalWrite(RadioStatusLED, LOW);
  digitalWrite(CloudStatusLED, LOW);
  delay(1000);  
  Serial.println("[Event] HLM1 Relay Station Boot V0.1");
  lastTime = millis();
}

//====[MAIN LOOP]===============================================
void loop() {

	int currentTime = millis();
	loopPeriod = currentTime - lastTime;

	if (loopPeriod > 500) {
		digitalWrite(RadioStatusLED, HIGH);
  		digitalWrite(CloudStatusLED, HIGH);
  		halfSeconds++;
  		lastTime = currentTime;
	}

	if (halfSeconds >= 2) {
		halfSeconds = 0;
		seconds++;
	}

	if (seconds % 12 == 0) {
		seconds++;	
		sendToCloud(lastMessage);
	}

}



//====[EVENTS]================================================
void RADIOEvent()
{	
	if (sendingToCloud == true) { return; }

	if (RADIO.available()) {
		while (RADIO.available()) {			
			digitalWrite(RadioStatusLED, LOW);
			char c = RADIO.read();
			radioSerialData = radioSerialData + c;			
			if (c == '\n') {				
				radioSerialData = radioSerialData.remove(radioSerialData.indexOf('\n'));
				radioSerialData = radioSerialData.remove(radioSerialData.indexOf('\r'));
				String countField = getValue(radioSerialData, '>', 0);
				String cleanField = getValue(radioSerialData, '>', 1);				

				if (countField.toInt() == cleanField.length()) {
					Serial.println(cleanField);
					if (cleanField.indexOf(',') != -1) { //This is a telemetry string. Send it to the cloud. Otherwise only display locally.
						// sendToCloud(cleanField);
						queueToCloud(cleanField);
					} else {						
						Serial.println("<LOCAL> " + radioSerialData);
					}
				} else {
					Serial.println("[Error] Checksum missmatch: \n    " + radioSerialData);
					Serial.println("");
				}


				radioSerialData = "";
			}
		}
	}	
}

void COMPUTEREvent() {
	if (COMPUTER.available()) {
		while (COMPUTER.available()) {			
			char c = COMPUTER.read();
			digitalWrite(CloudStatusLED, LOW);
			computerSerialData = computerSerialData + c;
			if (c == '\n') {				
				computerSerialData = computerSerialData.remove(radioSerialData.indexOf('\n'));
				groundRequest(computerSerialData);
				computerSerialData = "";
			}
		}
	}	
}


//====[HELPERS]===============================================
int groundRequest(String request) {
	digitalWrite(CloudStatusLED, LOW);
	RADIO.println(request);	
	return 1;
}

void sendToCloud(String data) {	
	if (sendingToCloud == false && Particle.connected()) {
		sendingToCloud = true;
		Particle.publish("R", data);			
		sendingToCloud = false;
	}
}

void queueToCloud(String data) {
	lastMessage = data;
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