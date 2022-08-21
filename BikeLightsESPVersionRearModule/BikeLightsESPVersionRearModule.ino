#include <I2Cdev.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include "BluetoothSerial.h"
#include <WiFi.h>
#include <AsyncUDP.h>
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
#include <Wire.h>
#endif
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <CircularBuffer.h>

CircularBuffer<float, 50> accelX;
CircularBuffer<float, 50> accelY;
CircularBuffer<float, 50> accelZ;
float accelNorm = 0.0f;

Adafruit_MPU6050 mpu;

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
bool disableBrakeLightsForTesting = false;
unsigned long microsBlink = 0;

float thresholdValue = 3.0f; //accZ brake threshold
unsigned long brakeTime = 250000; //time for accZ to be high to trigger brake
const int thresholdAddress = 0;
const int brakeTimeAddress = 10;
const int disableForTestingAddress = 20;

#define LEFT_REAR GPIO_NUM_15
#define RIGHT_REAR GPIO_NUM_2
#define INTERRUPT_PIN GPIO_NUM_13  // use pin 2 on Arduino Uno & most boards
#define POWER_BUTTON GPIO_NUM_4
#define MAIN_POWER GPIO_NUM_16

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
	pinMode(MAIN_POWER, OUTPUT);
	digitalWrite(MAIN_POWER, HIGH);

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
	EEPROM.get(thresholdAddress, thresholdValue);
	EEPROM.get(brakeTimeAddress, brakeTime);
	EEPROM.get(disableForTestingAddress, disableBrakeLightsForTesting);

	if (thresholdValue > 6.0f || thresholdValue < 0.0f) {
		thresholdValue = 3.0f;
		EEPROM.put(thresholdAddress, thresholdValue);
		EEPROM.commit();
	}

	if (brakeTime > 2000000 || brakeTime < 25000) {
		brakeTime = 250000;
		EEPROM.put(brakeTimeAddress, brakeTime);
		EEPROM.commit();
	}

	// join I2C bus (I2Cdev library doesn't do this automatically)
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
	Wire.begin();
	Wire.setClock(400000); // 400kHz I2C clock. Comment this line if having compilation difficulties
#elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
	Fastwire::setup(400, true);
#endif

	SerialBT.println(F("Initializing I2C devices..."));
	//pinMode(INTERRUPT_PIN, INPUT);
	bool devStatus = mpu.begin();
	if (devStatus) {
		// turn on the DMP, now that it's ready
		SerialBT.println(F("Found MPU"));

		//attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), dmpDataReady, RISING);
		mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
		mpu.setGyroRange(MPU6050_RANGE_500_DEG);
		mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
	}
	else {
		SerialBT.print(F("DMP Initialization failed (code "));
		SerialBT.print(devStatus);
		SerialBT.println(F(")"));
	}
	pinMode(POWER_BUTTON, INPUT_PULLDOWN);
	esp_sleep_enable_ext0_wakeup(GPIO_NUM_4, 1);
	delay(500);

	WakeUp();
}

void loop()
{
	MpuRoutine();
	int power;
	power = digitalRead(POWER_BUTTON);
	if (power == 1) {
		digitalWrite(MAIN_POWER, LOW);
		delay(1000);
		esp_deep_sleep_start();
	}

	if (SerialBT.available() > 0) BluetoothRoutine();

	BrakeDetection();
	UpdatePixels();
}

void BrakeDetection() {  //detect braking
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

void MpuRoutine() {
	/* Get new sensor events with the readings */
	sensors_event_t a, g, temp;
	mpu.getEvent(&a, &g, &temp);

	accelX.push(a.acceleration.x);
	accelY.push(a.acceleration.y);
	accelZ.push(a.acceleration.z);

	float avgX = 0.0;
	// the following ensures using the right type for the index variable
	using index_t = decltype(accelX)::index_t;
	for (index_t i = 0; i < accelX.size(); i++) {
		avgX += accelX[i] / accelX.size();
	}

	float avgY = 0.0;
	// the following ensures using the right type for the index variable
	using index_t = decltype(accelY)::index_t;
	for (index_t i = 0; i < accelY.size(); i++) {
		avgY += accelY[i] / accelY.size();
	}

	float avgZ = 0.0;
	// the following ensures using the right type for the index variable
	using index_t = decltype(accelZ)::index_t;
	for (index_t i = 0; i < accelZ.size(); i++) {
		avgZ += accelZ[i] / accelZ.size();
	}

	float squares = powf(avgX, 2.0f) + powf(avgY, 2.0f) + powf(avgZ, 2.0f);
	float gsquare = pow(GRAVITY, 2.0f);
	float sumSquares = squares - gsquare;
	//if (sumSquares < 0) sumSquares = -sumSquares;
	accelNorm = sqrtf(sumSquares);

	if (logging) {
		SerialBT.print("avgX");
		SerialBT.print('\t');
		SerialBT.print("avgY");
		SerialBT.print('\t');
		SerialBT.print("avgZ");
		SerialBT.print('\t');
		SerialBT.print("squares");
		SerialBT.print('\t');
		SerialBT.print("gsquare");
		SerialBT.print('\t');
		SerialBT.print("sumSquares");
		SerialBT.print('\t');
		SerialBT.println("accelNorm");
		SerialBT.print(avgX);
		SerialBT.print('\t');
		SerialBT.print(avgY);
		SerialBT.print('\t');
		SerialBT.print(avgZ);
		SerialBT.print('\t');
		SerialBT.print(squares);
		SerialBT.print('\t');
		SerialBT.print(gsquare);
		SerialBT.print('\t');
		SerialBT.print(sumSquares);
		SerialBT.print('\t');
		SerialBT.println(accelNorm);
	}
}

void BluetoothRoutine() {//// blink LED to indicate activity
	char inChar = (char)SerialBT.read();
	// add it to the inputString:
	inputString += inChar;

	//Bluetooth Connected
	if (inputString == "B") {
		btConnected = true;
		SerialBT.println("Bluetooth Connected\t\t\t\t\t\t\t\t\t\t\t\t\t\t");
	}

	if (inputString == "N") {
		btConnected = false;
		SerialBT.println("Bluetooth Disconnected\t\t\t\t\t\t\t\t\t\t\t\t\t\t");
	}

	//Increase brakeTime Value
	if (inputString == "Q") {
		brakeTime += 25000;
		if (brakeTime > 2000000) brakeTime = 2000000;
		EEPROM.put(brakeTimeAddress, brakeTime);
		EEPROM.commit();
		SerialBT.print("brakeTime value inc to ");
		SerialBT.print(brakeTime, DEC);
		SerialBT.println("\t\t\t\t\t\t\t\t\t\t\t\t\t\t");
	}

	//decrease brakeTime Value
	if (inputString == "A") {
		brakeTime -= 25000;
		if (brakeTime < 25000) brakeTime = 25000;
		EEPROM.put(brakeTimeAddress, brakeTime);
		EEPROM.commit();
		SerialBT.print("brakeTime value dec to ");
		SerialBT.print(brakeTime, DEC);
		SerialBT.println("\t\t\t\t\t\t\t\t\t\t\t\t\t\t");
	}

	//Increase Threshold
	if (inputString == "W") {
		thresholdValue += 0.1f;
		if (thresholdValue > 6.0F) thresholdValue = 6.0f;
		EEPROM.put(thresholdAddress, thresholdValue);
		EEPROM.commit();
		SerialBT.print("Threshold increased to ");
		SerialBT.print(thresholdValue, DEC);
		SerialBT.println("\t\t");
	}

	//Decrease thresholdValue Comparison
	if (inputString == "S") {
		thresholdValue -= 0.1f;
		if (thresholdValue < 0.0f) thresholdValue = 0.0f;
		EEPROM.put(thresholdAddress, thresholdValue);
		EEPROM.commit();
		SerialBT.print("Threshold decreased to ");
		SerialBT.print(thresholdValue, DEC);
		SerialBT.println("\t\t");
	}

	//Logging On
	if (inputString == "R") {
		logging = true;
		SerialBT.println("Logging On\t\t");
		//SerialBT.println("accX\taccY\taccZ\tgyrox\tgyroY\tgyroZ\t\tRoll\tgyroXAngle\tCompXAngle\tkalXAngle\t\tpitch\tgyroYAngle\tcompAngleY\tkalAngleY\ttemp");
		SerialBT.println("aveX\taveY\taveZ\taccelNorm");
	}

	//Logging Off
	if (inputString == "F") {
		logging = false;
		SerialBT.println("Logging Off\t\t");
	}

	//Brake Testing On
	if (inputString == "E") {
		disableBrakeLightsForTesting = true;
		EEPROM.put(disableForTestingAddress, disableBrakeLightsForTesting);
		EEPROM.commit();
		SerialBT.println("Brake Light Disabled\t\t");
	}

	//Brake Testing Off
	if (inputString == "D") {
		disableBrakeLightsForTesting = false;
		EEPROM.put(disableForTestingAddress, disableBrakeLightsForTesting);
		EEPROM.commit();
		SerialBT.println("Brake Light Enabled\t\t");
	}
	inputString = "";
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
	if (strStatus == "q" && brakeOn && !disableBrakeLightsForTesting) { //indicate left on , brake on
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
	if (strStatus == "a" && brakeOn && !disableBrakeLightsForTesting) { //indicate right on, brake on
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

	if (strStatus == "z" && (!brakeOn || disableBrakeLightsForTesting)) { //normal
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
	if (strStatus == "z" && brakeOn && !disableBrakeLightsForTesting) {
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
	//SerialBT.println(strStatus);
	//char* buffer;
	//Serial1.readBytes(buffer, Serial1.available());
}