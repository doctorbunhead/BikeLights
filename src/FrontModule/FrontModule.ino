#include <Adafruit_NeoPixel.h>

#define LEFT_BUTTON 3
#define RIGHT_BUTTON 4
#define LEFT_BUTTON_LED 5
#define RIGHT_BUTTON_LED 6
#define LEFT_FRONT 7
#define RIGHT_FRONT 8

Adafruit_NeoPixel leftFront = Adafruit_NeoPixel(36, LEFT_FRONT, NEO_GRBW + NEO_KHZ800);
Adafruit_NeoPixel rightFront = Adafruit_NeoPixel(36, RIGHT_FRONT, NEO_GRBW + NEO_KHZ800);
bool blink = false;
unsigned long microsBlink = 0;
unsigned long microsIndicate = 0;
bool lastLeftState = true;
bool leftState = false;
bool lastRightState = true;
bool rightState = false;
bool indicating = false;
bool indicateLeft = false;
bool indicateRight = false;
bool stateChange = false;
unsigned long debounceDelay = 50;
unsigned long millisBounce = 0;
unsigned long tmp = 0;
String UNIQUE_KEY = "01ad64hg";
String strStatus = "z";

unsigned long lastSend = 0;
unsigned int sendDelay = 900;

void setup()
{
	Serial.begin(2400);
	leftFront.begin();
	leftFront.show(); // Initialize all pixels to 'off'
	rightFront.begin();
	rightFront.show(); // Initialize all pixels to 'off'
	pinMode(LEFT_BUTTON, INPUT_PULLUP);
	pinMode(RIGHT_BUTTON, INPUT_PULLUP);
	pinMode(LEFT_BUTTON_LED, OUTPUT);
	pinMode(RIGHT_BUTTON_LED, OUTPUT);
	analogWrite(LEFT_BUTTON_LED, 20);
	analogWrite(RIGHT_BUTTON_LED, 20);
	//Setup and ground all remaining pins.
	pinMode(2, OUTPUT);
	digitalWrite(2, LOW);
	pinMode(9, OUTPUT);
	digitalWrite(9, LOW);
	pinMode(10, OUTPUT);
	digitalWrite(10, LOW);
	pinMode(11, OUTPUT);
	digitalWrite(11, LOW);
	pinMode(12, OUTPUT);
	digitalWrite(12, LOW);
	pinMode(13, OUTPUT);
	digitalWrite(13, LOW);
	pinMode(A0, OUTPUT);
	analogWrite(A0, 0);
	pinMode(A1, OUTPUT);
	analogWrite(A1, 0);
	pinMode(A2, OUTPUT);
	analogWrite(A2, 0);
	pinMode(A3, OUTPUT);
	analogWrite(A3, 0);
	pinMode(A4, OUTPUT);
	analogWrite(A4, 0);
	pinMode(A5, OUTPUT);
	analogWrite(A5, 0);
	pinMode(A6, OUTPUT);
	analogWrite(A6, 0);
	pinMode(A7, OUTPUT);
	analogWrite(A7, 0);

	delay(300);
	WakeUp();
}
void loop() // run over and over
{
	unsigned long now = millis();
	if (now > lastSend + sendDelay) {
		lastSend = now;
		String message = UNIQUE_KEY + strStatus;
		Serial.println(message);
	}

	ProcessButtons();
	UpdateLeds();
}

void WakeUp() {
	int led = 0;
	for (int j = 0; j < 36; j++) {
		for (int i = 0; i < 36; i++) {
			if (i == led) {
				leftFront.setPixelColor(i, 255, 255, 255, 255);
				rightFront.setPixelColor(i, 255, 255, 255, 255);
			}
			else
			{
				leftFront.setPixelColor(i, 0, 0, 0, 0);
				rightFront.setPixelColor(i, 0, 0, 0, 0);
			}
		}
		leftFront.show();
		rightFront.show();
		delay(10);
		led++;
	}
	for (int j = 0; j < 36; j++) {
		for (int i = 35; i > -1; i--) {
			if (i == led) {
				leftFront.setPixelColor(i, 255, 255, 255, 255);
				rightFront.setPixelColor(i, 255, 255, 255, 255);
			}
			else
			{
				leftFront.setPixelColor(i, 0, 0, 0, 0);
				rightFront.setPixelColor(i, 0, 0, 0, 0);
			}
		}
		leftFront.show();
		rightFront.show();
		delay(10);
		led--;
	}
}

void ProcessButtons() {
	bool leftReading = !digitalRead(LEFT_BUTTON);
	bool rightReading = !digitalRead(RIGHT_BUTTON);

	if (!(leftReading && rightReading)) { //check both buttons are not pressed
		if (leftReading != lastLeftState) {
			millisBounce = millis();
		}

		if (rightReading != lastRightState) {
			millisBounce = millis();
		}
		tmp = millis() - millisBounce;
		if (tmp > 50) {
			if (leftReading != leftState) {
				leftState = leftReading;

				stateChange = true;
			}
			if (rightReading != rightState)
			{
				rightState = rightReading;
				stateChange = true;
			}
		}
		lastLeftState = leftReading;
		lastRightState = rightReading;

		if (leftState && stateChange) {
			indicateLeft = !indicateLeft;
			if (indicateLeft) indicateRight = false;

			stateChange = false;

			if (indicateLeft) {
				strStatus = "q";
			}
			else
			{
				strStatus = "z";
			}

			Serial.println(UNIQUE_KEY + strStatus);
			//delay(500);
		}
		if (rightState && stateChange) {
			indicateRight = !indicateRight;
			if (indicateRight) indicateLeft = false;
			stateChange = false;
			if (indicateRight) {
				strStatus = "a";
			}
			else
			{
				strStatus = "z";
			}

			Serial.println(UNIQUE_KEY + strStatus);
			//delay(500);
		}
	}
}

void UpdateLeds() {
	if (indicateLeft) {
		if (indicating == false) {
			analogWrite(LEFT_BUTTON_LED, 255);
			analogWrite(RIGHT_BUTTON_LED, 20);
			for (uint16_t i = 0; i < 36; i++) {
				rightFront.setPixelColor(i, rightFront.Color(0, 0, 0, 100));
			}
			rightFront.show();
			for (uint16_t i = 0; i < 36; i++) {
				leftFront.setPixelColor(i, leftFront.Color(255, 60, 0, 0));
				leftFront.show();
				ProcessButtons();
				delay(10);
			}
			microsIndicate = millis();
			indicating = true;
		}
		if (millis() > microsIndicate + 400) {
			analogWrite(LEFT_BUTTON_LED, 20);
			for (uint16_t i = 0; i < 36; i++) {
				rightFront.setPixelColor(i, rightFront.Color(0, 0, 0, 100));
			}
			rightFront.show();
			for (uint16_t i = 0; i < 36; i++) {
				leftFront.setPixelColor(i, leftFront.Color(0, 0, 0, 100));
			}
			leftFront.show();
		}
		if (millis() > microsIndicate + 600) {
			indicating = false;
		}
	}
	if (indicateRight) {
		if (indicating == false) {
			analogWrite(RIGHT_BUTTON_LED, 255);
			analogWrite(LEFT_BUTTON_LED, 20);
			for (uint16_t i = 0; i < 36; i++) {
				leftFront.setPixelColor(i, leftFront.Color(0, 0, 0, 100));
			}
			rightFront.show();
			for (uint16_t i = 0; i < 36; i++) {
				rightFront.setPixelColor(i, rightFront.Color(255, 60, 0, 0));
				rightFront.show();
				ProcessButtons();
				delay(10);
			}
			microsIndicate = millis();
			indicating = true;
		}
		if (millis() > microsIndicate + 400) {
			analogWrite(RIGHT_BUTTON_LED, 20);
			for (uint16_t i = 0; i < 36; i++) {
				leftFront.setPixelColor(i, leftFront.Color(0, 0, 0, 100));
			}
			rightFront.show();
			for (uint16_t i = 0; i < 36; i++) {
				rightFront.setPixelColor(i, rightFront.Color(0, 0, 0, 100));
			}
			rightFront.show();
		}
		if (millis() > microsIndicate + 600) {
			indicating = false;
		}
	}

	if (!indicateLeft && !indicateRight) {
		indicating = false;
		if ((millis() > microsBlink + 100 && millis() < microsBlink + 150) || millis() > microsBlink + 300) {
			blink = true;
		}
		if ((millis() > microsBlink + 150 && millis() < microsBlink + 200) || millis() > microsBlink + 350) {
			blink = false;
		}
		if (millis() > microsBlink + 400) microsBlink = millis();
		for (int j = 0; j < 36; j++)
		{
			leftFront.setPixelColor(j, 0, 0, 0, 100); //192
			rightFront.setPixelColor(j, 0, 0, 0, 100);

			if (blink) {
				if (j % 5 == 0) {
					leftFront.setPixelColor(j, 255, 255, 255, 255); //255
					rightFront.setPixelColor(j, 255, 255, 255, 255);
				}
			}
		}
		analogWrite(LEFT_BUTTON_LED, 20);
		analogWrite(RIGHT_BUTTON_LED, 20);
	}
	leftFront.show();
	rightFront.show();
}