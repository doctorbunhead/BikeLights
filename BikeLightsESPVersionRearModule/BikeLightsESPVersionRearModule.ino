//#include <I2Cdev.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include "BluetoothSerial.h"
#include <WiFi.h>
#include <AsyncUDP.h>

BluetoothSerial SerialBT;

const char* ssid = "KdBikeLights";
const char* password = "1234567890";

AsyncUDP udp;

unsigned long microsOld = 0;
unsigned long microsBrake = 0;
unsigned long microsBrakeOn = 0;
bool brakeOn = false;
bool logging = false;
bool btConnected = false;
bool brakeDetect = false;
bool brakeTrigOnSent = false;
bool brakeTrigOffSent = false;
bool brakeOnSent = false;
bool brakeOffSent = false;
//bool ledState = false;
bool blink = false;

unsigned long microsBlink = 0;

#define LEFT_REAR GPIO_NUM_15
#define RIGHT_REAR GPIO_NUM_2
#define POWER_BUTTON GPIO_NUM_4
#define BRAKE_POWER GPIO_NUM_16
#define LED_POWER GPIO_NUM_17
#define BRAKE_IN GPIO_NUM_18

Adafruit_NeoPixel leftRear = Adafruit_NeoPixel(36, LEFT_REAR, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel rightRear = Adafruit_NeoPixel(36, RIGHT_REAR, NEO_GRB + NEO_KHZ800);
unsigned long microsIndicate = 0;
bool indicating = false;
String UNIQUE_KEY = "01ad64hg";
String strStatus = "z";
String strInput = "";

String inputString = "";

const float GRAVITY = 9.80665f;

void setup() {
	Serial.begin(921600);
	pinMode(BRAKE_POWER, OUTPUT);
	pinMode(LED_POWER, OUTPUT);
	pinMode(BRAKE_IN, INPUT_PULLDOWN);
	digitalWrite(BRAKE_POWER, HIGH);
	digitalWrite(LED_POWER, LOW);

	SerialBT.begin("RearMod");
	tcpip_adapter_init();

	WiFi.softAP(ssid, password);

	IPAddress IP = WiFi.softAPIP();

	Serial.print("wifi IP ");
	Serial.println(IP);

	if (udp.listen(4909)) {
		Serial.println("listening...");
		udp.onPacket([](AsyncUDPPacket packet) {
			Serial.println("GotPacket");
			String statString = (const char*)packet.data();
			SetStatus(statString);
			});
	}

	inputString.reserve(200);
		
	pinMode(POWER_BUTTON, INPUT_PULLDOWN);
	esp_sleep_enable_ext0_wakeup(GPIO_NUM_4, 1);
	delay(500);

	WakeUp();
}

void loop()
{
	int power;
	power = digitalRead(POWER_BUTTON);
	if (power == 1) GoToSleep(); 

	if (SerialBT.available() > 0) BluetoothRoutine();

	BrakeDetection();
	UpdatePixels();
}

void BrakeDetection() {  //detect braking
	int brakesIn = 0;
	brakesIn = digitalRead(BRAKE_IN);
	if (accelNorm > thresholdValue && !brakeDetect) {
		microsBrake = micros();
		brakeDetect = true;
		if (btConnected && !brakeTrigOnSent) {
			SerialBT.println("Brake Trigger On\t\t");
			brakeTrigOnSent = true;
			brakeTrigOffSent = false;
		}
	}
	if (accelNorm < thresholdValue) {
		brakeDetect = false;
		if (btConnected && !brakeTrigOffSent) {
			SerialBT.println("Brake Trigger Off\t\t");
			brakeTrigOnSent = false;
			brakeTrigOffSent = true;
		}
	}
	if (brakeDetect)
	{
		unsigned long btime = microsBrake + brakeTime;

		//check for clock rollover and correct will cause a slight glitch but should be unoticeable
		if (microsBrake > micros()) microsBrake = micros();
		if (micros() > btime)
		{
			brakeOn = true;
			microsBrakeOn = micros();
			if (btConnected && !brakeOnSent) {
				SerialBT.println("Brake On\t\t");
				brakeOnSent = true;
				brakeOffSent = false;
			}
		}
	}
	if (brakeOn)
	{
		if (microsBrakeOn > micros()) microsBrakeOn = micros(); //detect and correct for clock rollover
		if (microsBrakeOn + 2000000 < micros()) { //keep brake on for 2 seconds after braking
			brakeOn = false;
			if (btConnected && !brakeOffSent) {
				SerialBT.print("Brake Off\t\t");
				brakeOnSent = false;
				brakeOffSent = true;
			}
		}
	}
}

void WakeUp() {
	leftRear.begin();
	leftRear.show(); // Initialize all pixels to 'off'
	rightRear.begin();
	rightRear.show(); // Initialize all pixels to 'off'

	int led = 0;
	for (int j = 0; j < 36; j++) {
		for (int i = 0; i < 36; i++) {
			if (i == led) {
				leftRear.setPixelColor(i, 255, 0, 0);
				rightRear.setPixelColor(i, 255, 0, 0);
			}
			else
			{
				leftRear.setPixelColor(i, 0, 0, 0);
				rightRear.setPixelColor(i, 0, 0, 0);
			}
		}
		leftRear.show();
		rightRear.show();
		delay(10);
		led++;
	}
	for (int j = 0; j < 36; j++) {
		for (int i = 35; i > -1; i--) {
			if (i == led) {
				leftRear.setPixelColor(i, 255, 0, 0);
				rightRear.setPixelColor(i, 255, 0, 0);
			}
			else
			{
				leftRear.setPixelColor(i, 0, 0, 0);
				rightRear.setPixelColor(i, 0, 0, 0);
			}
		}
		leftRear.show();
		rightRear.show();
		delay(10);
		led--;
	}
	SerialBT.println("Wagons roll");
}


void UpdatePixels() {
	if (strStatus == "q" && !brakeOn) { //indicate left on
		if (indicating == false) {
			//colorWipe(leftRear.Color(255, 60, 0), 10, leftRear);
			for (uint8_t i = 0; i < 36; i++) {
				leftRear.setPixelColor(i, leftRear.Color(255, 60, 0));
				leftRear.show();
				delay(10);
			}
			//colorWipe(rightRear.Color(64, 0, 0), 0, rightRear);
			for (uint8_t i = 0; i < 36; i++) {
				rightRear.setPixelColor(i, rightRear.Color(64, 0, 0));
			}
			rightRear.show();
			microsIndicate = millis();
			indicating = true;
		}
		if (millis() > microsIndicate + 400) {
			//colorWipe(leftRear.Color(64, 0, 0), 0, leftRear);
			for (uint8_t i = 0; i < 36; i++) {
				leftRear.setPixelColor(i, leftRear.Color(64, 0, 0));
			}
			leftRear.show();
			//colorWipe(rightRear.Color(64, 0, 0), 0, rightRear);
			for (uint8_t i = 0; i < 36; i++) {
				rightRear.setPixelColor(i, rightRear.Color(64, 0, 0));
			}
			rightRear.show();
		}
		if (millis() > microsIndicate + 600) {
			indicating = false;
		}
	}
	if (strStatus == "q" && brakeOn) { //indicate left on , brake on
		if (indicating == false) {
			//colorWipe(leftRear.Color(255, 60, 0), 10, leftRear);
			for (uint8_t i = 0; i < 36; i++) {
				leftRear.setPixelColor(i, leftRear.Color(255, 60, 0));
				leftRear.show();
				delay(10);
			}
			//colorWipe(rightRear.Color(255, 0, 0), 0, rightRear);
			for (uint8_t i = 0; i < 36; i++) {
				rightRear.setPixelColor(i, rightRear.Color(255, 0, 0));
			}
			rightRear.show();
			microsIndicate = millis();
			indicating = true;
		}
		if (millis() > microsIndicate + 400) {
			//colorWipe(rightRear.Color(255, 0, 0), 0, rightRear);
			for (uint8_t i = 0; i < 36; i++) {
				rightRear.setPixelColor(i, rightRear.Color(255, 0, 0));
			}
			rightRear.show();
			//colorWipe(leftRear.Color(255, 0, 0), 0, leftRear);
			for (uint8_t i = 0; i < 36; i++) {
				leftRear.setPixelColor(i, leftRear.Color(255, 0, 0));
			}
			leftRear.show();
		}
		if (millis() > microsIndicate + 600) {
			indicating = false;
		}
	}
	if (strStatus == "a" && !brakeOn) { //indicate right on
		if (indicating == false) {
			//colorWipe(leftRear.Color(64, 0, 0), 0, leftRear);
			for (uint8_t i = 0; i < 36; i++) {
				leftRear.setPixelColor(i, leftRear.Color(64, 0, 0));
			}
			leftRear.show();
			//colorWipe(rightRear.Color(255, 60, 0), 10, rightRear);
			for (uint8_t i = 0; i < 36; i++) {
				rightRear.setPixelColor(i, rightRear.Color(255, 60, 0));
				rightRear.show();
				delay(10);
			}
			microsIndicate = millis();
			indicating = true;
		}
		if (millis() > microsIndicate + 400) {
			//colorWipe(leftRear.Color(64, 0, 0), 0, leftRear);
			for (uint8_t i = 0; i < 36; i++) {
				leftRear.setPixelColor(i, leftRear.Color(64, 0, 0));
			}
			leftRear.show();
			//colorWipe(rightRear.Color(64, 0, 0), 0, rightRear);
			for (uint8_t i = 0; i < 36; i++) {
				rightRear.setPixelColor(i, rightRear.Color(64, 0, 0));
			}
			rightRear.show();
		}
		if (millis() > microsIndicate + 600) {
			indicating = false;
		}
	}
	if (strStatus == "a" && brakeOn) { //indicate right on, brake on
		if (indicating == false) {
			//colorWipe(leftRear.Color(255, 0, 0), 0, leftRear);
			for (uint8_t i = 0; i < 36; i++) {
				leftRear.setPixelColor(i, leftRear.Color(255, 0, 0));
			}
			leftRear.show();
			//colorWipe(rightRear.Color(255, 60, 0), 10, rightRear);
			for (uint8_t i = 0; i < 36; i++) {
				rightRear.setPixelColor(i, rightRear.Color(255, 60, 0));
				rightRear.show();
				delay(10);
			}
			microsIndicate = millis();
			indicating = true;
		}
		if (millis() > microsIndicate + 400) {
			//colorWipe(leftRear.Color(255, 0, 0), 0, leftRear);
			for (uint8_t i = 0; i < 36; i++) {
				leftRear.setPixelColor(i, leftRear.Color(255, 0, 0));
			}
			leftRear.show();
			//colorWipe(rightRear.Color(255, 0, 0), 0, rightRear);
			for (uint8_t i = 0; i < 36; i++) {
				rightRear.setPixelColor(i, rightRear.Color(255, 0, 0));
			}
			rightRear.show();
		}
		if (millis() > microsIndicate + 600) {
			indicating = false;
		}
	}

	if (strStatus == "z" && (!brakeOn)) { //normal
		if ((millis() > microsBlink + 100 && millis() < microsBlink + 150) || millis() > microsBlink + 300) {
			blink = true;
		}
		if ((millis() > microsBlink + 150 && millis() < microsBlink + 200) || millis() > microsBlink + 350) {
			blink = false;
		}
		if (millis() > microsBlink + 1000) microsBlink = millis();

		//detect timer rollover and correct. Will cause a minor blip in operation
		if (microsOld > millis()) microsOld = millis();

		if (millis() > (microsOld + 5000))
		{
			microsOld = millis();
		}

		for (int j = 0; j < 36; j++)
		{
			leftRear.setPixelColor(j, 100, 0, 0);
			rightRear.setPixelColor(j, 100, 0, 0);

			if (blink) {
				if (j % 5 == 0) {
					if (j != 0 && j != 35) {
						leftRear.setPixelColor(j, 192, 0, 0);
						rightRear.setPixelColor(j, 192, 0, 0);
					}
				}
			}
		}

		leftRear.show();
		rightRear.show();
	}
	if (strStatus == "z" && brakeOn) {
		//colorWipe(leftRear.Color(255, 0, 0), 0, leftRear);
		for (uint8_t i = 0; i < 36; i++) {
			leftRear.setPixelColor(i, leftRear.Color(255, 0, 0));
		}
		leftRear.show();
		//colorWipe(rightRear.Color(255, 0, 0), 0, rightRear);
		for (uint8_t i = 0; i < 36; i++) {
			rightRear.setPixelColor(i, rightRear.Color(255, 0, 0));
		}
		rightRear.show();
		//colorWipe(rightRear.Color(255, 0, 0), 1, rightRear);
	}
}

void SetStatus(String statString) {
	Serial.println("GOT DATA");

	Serial.println(statString);
	if (statString.length() < UNIQUE_KEY.length() + 1) return;
	char inCommand = statString[statString.length() - 1];
	strStatus = String(inCommand);
	Serial.println(inCommand);
	if (inCommand == 'O') GoToSleep();
	//SerialBT.println(strStatus);
	//char* buffer;
	//Serial1.readBytes(buffer, Serial1.available());
}

void GoToSleep() {
	digitalWrite(BRAKE_POWER, LOW);
	digitalWrite(LED_POWER, HIGH);
	delay(1000);
	esp_deep_sleep_start();
}