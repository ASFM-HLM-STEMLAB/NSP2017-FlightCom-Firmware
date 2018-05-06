
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
  		lastTime = currentTime;
	}

}



//====[EVENTS]================================================
void RADIOEvent()
{	
	if (RADIO.available()) {
		while (RADIO.available()) {			
			digitalWrite(RadioStatusLED, LOW);
			char c = RADIO.read();
			radioSerialData = radioSerialData + c;			
			if (c == '\n') {				
				radioSerialData = radioSerialData.remove(radioSerialData.indexOf('\n'));
				Serial.println(radioSerialData);
				sendToCloud(radioSerialData);				
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
	Particle.publish("R", data);
}