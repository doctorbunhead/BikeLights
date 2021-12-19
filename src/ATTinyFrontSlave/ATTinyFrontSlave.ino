

#include <Adafruit_NeoPixel.h>

#define LEFT_FRONT 2
#define RIGHT_FRONT 3


Adafruit_NeoPixel leftFront = Adafruit_NeoPixel(36, LEFT_FRONT, NEO_GRBW + NEO_KHZ800);
Adafruit_NeoPixel rightFront = Adafruit_NeoPixel(36, RIGHT_FRONT, NEO_GRBW + NEO_KHZ800);
bool blink = false;
bool led = false;
unsigned long microsBlink = 0;
unsigned long microsIndicate = 0;
bool indicating=false;
String strStatus="z";
void setup()
{
  // Open serial communications and wait for port to open:

  // set the data rate for the SoftwareSerial port
  Serial.begin(4800);

  leftFront.begin();
  leftFront.show(); // Initialize all pixels to 'off'
  rightFront.begin();
  rightFront.show(); // Initialize all pixels to 'off'
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() // run over and over
{
	/* strings are
	 *  q - indicate left on
	 *  w - indicate left off
	 *  a - indicate right on
	 *  s - indicate right off
	 *  z - normal
	 */
	if (Serial.available() > 0) {
		String strRead = Serial.read();
		strStatus = strRead;
		led = !led;
		digitalWrite(LED_BUILTIN, led);
	}
	if (strStatus == "q") {
		if (indicating == false) {
			colorWipe(rightFront.Color(64, 64, 64), 0, rightFront);
			colorWipe(leftFront.Color(255, 100, 0), 10,leftFront);
			microsIndicate = micros();
			indicating = true;
		}
		if (micros() > microsIndicate + 400000) {
			colorWipe(rightFront.Color(64, 64, 64), 0, rightFront);
			colorWipe(leftFront.Color(64, 64, 64), 0,leftFront);
		}
		if (micros() > microsIndicate + 600000) {
			indicating = false;
		}
	}
	if (strStatus == "a") {
		if (indicating == false) {
			colorWipe(leftFront.Color(64, 64, 64), 0, leftFront);
			colorWipe(rightFront.Color(255, 100, 0), 10, rightFront);
			microsIndicate = micros();
			indicating = true;
		}
		if (micros() > microsIndicate + 400000) {
			colorWipe(leftFront.Color(64, 64, 64), 0, leftFront);
			colorWipe(rightFront.Color(64, 64, 64), 0, rightFront);
		}
		if (micros() > microsIndicate + 600000) {
			indicating = false;
		}
	}

	if(strStatus=="z") {

		if ((micros() > microsBlink + 100000 && micros() < microsBlink + 150000) || micros() > microsBlink + 300000) {
			blink = true;

		}
		if ((micros() > microsBlink + 150000 && micros() < microsBlink + 200000) || micros() > microsBlink + 350000) {
			blink = false;
		}
		if (micros() > microsBlink + 1000000) microsBlink = micros();
		for (int j = 0; j < 36; j++)
		{


			leftFront.setPixelColor(j, 192, 192, 192);
			rightFront.setPixelColor(j, 192, 192, 192);

			if (blink) {
				//if (j % 5 == 0) {
				  //if (j != 0 && j != 35) {
				leftFront.setPixelColor(j, 255, 255, 255);
				rightFront.setPixelColor(j, 255, 255, 255);
				//}
			  //}
			}

		}
	}
  leftFront.show();
  rightFront.show();
}

void colorWipe(uint32_t c, uint8_t wait, Adafruit_NeoPixel pixel) {
  for (uint16_t i = 0; i < pixel.numPixels(); i++) {
    pixel.setPixelColor(i, c);
    pixel.show();
    delay(wait);
  }
}
