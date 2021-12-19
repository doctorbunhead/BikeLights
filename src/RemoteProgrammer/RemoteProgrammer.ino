#include <VirtualWire\VirtualWire.h>

#define txToFront 3 //rf transmit pin
#define rxFromButtons 4 //rf receive pin
#define txPtt 5 //enable transmitter

bool left = false;
bool right = false;

void setup() {
	Serial.begin(9600);
	vw_set_rx_pin(rxFromButtons);
	vw_set_tx_pin(txToFront);
	vw_setup(4096);	 // Bits per sec

}

void loop() {
	if (Serial.available() > 0) {
		String message;
		char inChar = (char)Serial.read();
		message = inChar;
		if (message == "l") {
			left = true;
			right = false;
		}
		if (message == "r") {
			left = false;
			right = true;
		}
		if (message == "x") {
			left = false;
			right = false;
		}
	}
	if (left) {
		Serial.println("GHEI8576DHEYRT5L");
		transmit("GHEI8576DHEYRT5L");
		delay(50);
	}
	if (right) {
		Serial.println("GHEI8576DHEYRT5R");
		transmit("GHEI8576DHEYRT5R");
		delay(50);
	}

}

void transmit(String message) {
	for (int i = 0; i < 4; i++) {
		const char *msg = message.c_str();
		digitalWrite(LED_BUILTIN, HIGH);
		digitalWrite(txPtt, true);
		vw_send((uint8_t *)msg, strlen(msg));
		vw_wait_tx(); // Wait until the whole message is gone
		digitalWrite(13, false);
		digitalWrite(txPtt, false);
		digitalWrite(LED_BUILTIN, LOW);
	}
}
