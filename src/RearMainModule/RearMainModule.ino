#include <SoftwareSerial.h>

#include <I2Cdev.h>
#include <MPU6050_6Axis_MotionApps20.h>
#include <Adafruit_NeoPixel.h>
//#include "MPU6050.h" // not necessary if using MotionApps include file

// Arduino Wire library is required if I2Cdev I2CDEV_ARDUINO_WIRE implementation
// is used in I2Cdev.h
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
#include <Wire.h>
#endif

// class default I2C address is 0x68
// specific I2C addresses may be passed as a parameter here
// AD0 low = 0x68 (default for SparkFun breakout and InvenSense evaluation board)
// AD0 high = 0x69
MPU6050 mpu;

#ifdef __AVR__
#include <avr/power.h>
#endif
#include <avr/sleep.h>
#define RESTRICT_PITCH // Comment out to restrict roll to ±90deg instead - please read: http://www.freescale.com/files/sensors/doc/app_note/AN3461.pdf
#define OUTPUT_READABLE_WORLDACCEL;

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
bool ledState = false;
bool blink = false;
unsigned long microsBlink = 0;

int16_t thresholdValue = 600; //accZ brake threshold
unsigned long brakeTime = 250000; //time for accZ to be high to trigger brake

#define LEFT_REAR 5
#define RIGHT_REAR 6
#define INTERRUPT_PIN 7  // use pin 2 on Arduino Uno & most boards
Adafruit_NeoPixel leftRear = Adafruit_NeoPixel(36, LEFT_REAR, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel rightRear = Adafruit_NeoPixel(36, RIGHT_REAR, NEO_GRB + NEO_KHZ800);
unsigned long microsIndicate = 0;
bool indicating = false;
String UNIQUE_KEY = "01ad64hg";
String strStatus = "z";
String strInput = "";

// MPU control/status vars
bool dmpReady = false;  // set true if DMP init was successful
uint8_t mpuIntStatus;   // holds actual interrupt status byte from MPU
uint8_t devStatus;      // return status after each device operation (0 = success, !0 = error)
uint16_t packetSize;    // expected DMP packet size (default is 42 bytes)
uint16_t fifoCount;     // count of all bytes currently in FIFO
uint8_t fifoBuffer[64]; // FIFO storage buffer

						// orientation/motion vars
Quaternion q;           // [w, x, y, z]         quaternion container
VectorInt16 aa;         // [x, y, z]            accel sensor measurements
VectorInt16 aaReal;     // [x, y, z]            gravity-free accel sensor measurements
VectorInt16 aaWorld;    // [x, y, z]            world-frame accel sensor measurements
VectorFloat gravity;    // [x, y, z]            gravity vector
float euler[3];         // [psi, theta, phi]    Euler angle container
float ypr[3];           // [yaw, pitch, roll]   yaw/pitch/roll container and gravity vector

///end invensense stuff

String inputString = "";

SoftwareSerial bluetooth = SoftwareSerial(14, 15);

// ================================================================
// ===               INTERRUPT DETECTION ROUTINE                ===
// ================================================================

volatile bool mpuInterrupt = false;     // indicates whether MPU interrupt pin has gone high
void dmpDataReady() {
	mpuInterrupt = true;
}



void setup() {
	Serial1.begin(2400); //RF Serial
	bluetooth.begin(38400);
	inputString.reserve(200);
	pinMode(9, OUTPUT);
	digitalWrite(9, HIGH);
	// join I2C bus (I2Cdev library doesn't do this automatically)
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
	Wire.begin();
	Wire.setClock(400000); // 400kHz I2C clock. Comment this line if having compilation difficulties
#elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
	Fastwire::setup(400, true);
#endif

	// initialize device
	bluetooth.println(F("Initializing I2C devices..."));
	mpu.initialize();
	pinMode(INTERRUPT_PIN, INPUT);

	// verify connection
	bluetooth.println(F("Testing device connections..."));
	bluetooth.println(mpu.testConnection() ? F("MPU6050 connection successful") : F("MPU6050 connection failed"));
	// load and configure the DMP
	bluetooth.println(F("Initializing DMP..."));
	devStatus = mpu.dmpInitialize();

	// supply your own gyro offsets here, scaled for min sensitivity
	mpu.setXGyroOffset(220);
	mpu.setYGyroOffset(76);
	mpu.setZGyroOffset(-85);
	mpu.setZAccelOffset(1575); // 1688 factory default for my test chip
   // make sure it worked (returns 0 if so)
	if (devStatus == 0) {
		// turn on the DMP, now that it's ready
		bluetooth.println(F("Enabling DMP..."));
		mpu.setDMPEnabled(true);
		// enable Arduino interrupt detection
		bluetooth.println(F("Enabling interrupt detection (Arduino external interrupt 0)..."));
		attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), dmpDataReady, RISING);
		mpuIntStatus = mpu.getIntStatus();

		// set our DMP Ready flag so the main loop() function knows it's okay to use it
		bluetooth.println(F("DMP ready! Waiting for first interrupt..."));
		dmpReady = true;

		// get expected DMP packet size for later comparison
		packetSize = mpu.dmpGetFIFOPacketSize();
	}
	else {
		bluetooth.print(F("DMP Initialization failed (code "));
		bluetooth.print(devStatus);
		bluetooth.println(F(")"));
	}

	// configure LED for output
	WakeUp();
}

void loop()
{
	MpuRoutine();

	if (bluetooth.available() > 0) BluetoothRoutine();

	if (Serial1.available() > 0) RfComms();

	BrakeDetection();
	UpdatePixels();
}

void BrakeDetection() {  //detect braking
	if (aaReal.z > thresholdValue && !brakeDetect) {
		microsBrake = micros();
		brakeDetect = true;
		if (btConnected && !brakeTrigOnSent) {
			bluetooth.println("Brake Trigger On\t\t");
			brakeTrigOnSent = true;
			brakeTrigOffSent = false;
		}
	}
	if (aaReal.z < thresholdValue) {
		brakeDetect = false;
		if (btConnected && !brakeTrigOffSent) {
			bluetooth.println("Brake Trigger Off\t\t");
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
				bluetooth.println("Brake On\t\t");
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
				bluetooth.print("Brake Off\t\t");
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
	bluetooth.println("Wagons roll");
}

void MpuRoutine() {
	if (!dmpReady) return;

	// wait for MPU interrupt or extra packet(s) available
	while (!mpuInterrupt && fifoCount < packetSize) {

	}

	// reset interrupt flag and get INT_STATUS byte
	mpuInterrupt = false;
	mpuIntStatus = mpu.getIntStatus();

	// get current FIFO count
	fifoCount = mpu.getFIFOCount();

	// check for overflow (this should never happen unless our code is too inefficient)
	if ((mpuIntStatus & 0x10) || fifoCount == 1024) {
		// reset so we can continue cleanly
		mpu.resetFIFO();
		//mpu.reset();
		//bluetooth.println(F("FIFO overflow!"));

		// otherwise, check for DMP data ready interrupt (this should happen frequently)
	}
	else if (mpuIntStatus & 0x02) {
		// wait for correct available data length, should be a VERY short wait
		while (fifoCount < packetSize) fifoCount = mpu.getFIFOCount();

		// read a packet from FIFO
		mpu.getFIFOBytes(fifoBuffer, packetSize);

		// track FIFO count here in case there is > 1 packet available
		// (this lets us immediately read more without waiting for an interrupt)
		fifoCount -= packetSize;

		// display initial world-frame acceleration, adjusted to remove gravity
		// and rotated based on known orientation from quaternion
		mpu.dmpGetQuaternion(&q, fifoBuffer);
		mpu.dmpGetAccel(&aa, fifoBuffer);
		mpu.dmpGetGravity(&gravity, &q);
		mpu.dmpGetLinearAccel(&aaReal, &aa, &gravity);
		mpu.dmpGetLinearAccelInWorld(&aaWorld, &aaReal, &q);
		//logging = true;
		if (logging) {
			bluetooth.print(aaReal.x);
			bluetooth.print("\t");
			bluetooth.print(aaReal.y);
			bluetooth.print("\t");
			bluetooth.println(aaReal.z);
		}



	}
	///************* END MPU6050 *************
}

void BluetoothRoutine() {//// blink LED to indicate activity
	char inChar = (char)bluetooth.read();
	// add it to the inputString:
	inputString += inChar;

	//Bluetooth Connected
	if (inputString == "B") {
		btConnected = true;
		bluetooth.println("Bluetooth Connected\t\t\t\t\t\t\t\t\t\t\t\t\t\t");
		ledState = !ledState;
		digitalWrite(13, ledState);
	}

	if (inputString == "N") {
		btConnected = false;
		bluetooth.println("Bluetooth Disconnected\t\t\t\t\t\t\t\t\t\t\t\t\t\t");
		ledState = !ledState;
		digitalWrite(13, ledState);
	}

	//Increase brakeTime Value
	if (inputString == "Q") {
		brakeTime += 25000;
		if (brakeTime > 2000000) brakeTime = 2000000;
		bluetooth.print("brakeTime value inc to ");
		bluetooth.print(brakeTime, DEC);
		bluetooth.println("\t\t\t\t\t\t\t\t\t\t\t\t\t\t");
		ledState = !ledState;
		digitalWrite(13, ledState);
	}

	//decrease brakeTime Value
	if (inputString == "A") {
		brakeTime -= 25000;
		if (brakeTime < 25000) brakeTime = 25000;
		bluetooth.print("brakeTime value dec to ");
		bluetooth.print(brakeTime, DEC);
		bluetooth.println("\t\t\t\t\t\t\t\t\t\t\t\t\t\t");
		ledState = !ledState;
		digitalWrite(13, ledState);
	}

	//Increase Threshold
	if (inputString == "W") {
		thresholdValue += 100;
		if (thresholdValue > 32000) thresholdValue = 32000;
		bluetooth.print("Threshold increased to ");
		bluetooth.print(thresholdValue, DEC);
		bluetooth.println("\t\t");
		ledState = !ledState;
		digitalWrite(13, ledState);
	}

	//Decrease thresholdValue Comparison
	if (inputString == "S") {
		thresholdValue -= 100;
		if (thresholdValue < 100) thresholdValue = 100;
		bluetooth.print("Threshold decreased to ");
		bluetooth.print(thresholdValue, DEC);
		bluetooth.println("\t\t");
		ledState = !ledState;
		digitalWrite(13, ledState);
	}

	//Logging On
	if (inputString == "R") {
		logging = true;
		bluetooth.println("Logging On\t\t");
		//bluetooth.println("accX\taccY\taccZ\tgyrox\tgyroY\tgyroZ\t\tRoll\tgyroXAngle\tCompXAngle\tkalXAngle\t\tpitch\tgyroYAngle\tcompAngleY\tkalAngleY\ttemp");
		bluetooth.println("aworld-x\taworld-y\taworld-z");
		ledState = !ledState;
		digitalWrite(13, ledState);
	}

	//Logging Off
	if (inputString == "F") {
		logging = false;
		bluetooth.println("Logging Off\t\t");
		ledState = !ledState;
		digitalWrite(13, ledState);
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

	if (strStatus == "z" && !brakeOn) { //normal
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

void RfComms() {
	//bluetooth.println("GOT DATA");
	String inStr = Serial1.readStringUntil(13);
	//bluetooth.println(inStr);
	if (inStr.length() < UNIQUE_KEY.length() + 1) return;
	char inCommand = inStr[inStr.length()-1];
	strStatus = String(inCommand);
	//bluetooth.println(strStatus);
	//char* buffer;
	//Serial1.readBytes(buffer, Serial1.available());
}