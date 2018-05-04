
String radioSerialData;

//====[MACROS]====================================================
#define RADIO Serial1
#define RADIOEvent serialEvent1

//PARTICLE SYSTEM PARAMS
// SYSTEM_MODE(SEMI_AUTOMATIC);
SYSTEM_THREAD(ENABLED);

void setup() {
  Serial.begin();
  RADIO.begin(9600);
  delay(1000);
  Serial.println("[Event] HLM1 Relay Station Boot V0.1");
}


void loop() {

}



void RADIOEvent()
{	
	if (RADIO.available()) {
		while (RADIO.available()) {			
			char c = RADIO.read();
			radioSerialData = radioSerialData + c;
			if (c == '\n') {
				radioSerialData = radioSerialData.remove(radioSerialData.indexOf('\n'));
				Serial.println(radioSerialData);
				sendToCloud(radioSerialData);
				RADIO.println("OK");
				radioSerialData = "";
			}
		}
	}	
}


void sendToCloud(String data) {
	Particle.publish("R", data);
}