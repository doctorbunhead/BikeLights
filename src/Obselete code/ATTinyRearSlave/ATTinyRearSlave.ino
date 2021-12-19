/*
	This slave will do all radio comms for the rear. It will take a brake output
	from the main module and receive commands from the front module for indicating.
*/

#include <cc1101.h>
#include <ccpacket.h>


// Attach CC1101 pins to their corresponding SPI pins
// Uno pins:
// CSN (SS) => 10
// MOSI => 11
// MISO => 12
// SCK => 13
// GD0 => A valid interrupt pin for your platform (defined below this)  PIN 2

#define CC1101Interrupt 0 // Pin 2
#define CC1101_GDO0 2



#define BRAKE_IN 9



CC1101 radio;
byte syncWord[2] = { 199, 10 };
bool packetWaiting;



void messageReceived() {
	packetWaiting = true;
}
void setup()
{
}



void loop()
{
	



	/* strings are
	   q 113 - indicate left on
	   a  97 - indicate right on
	   z 122 - normal
	*/
	///************ check rf for data ***********
	

	brakeIn = !digitalRead(BRAKE_IN);

}