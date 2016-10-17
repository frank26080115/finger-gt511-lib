/*!
 * finger-gt511-lib
 * https://github.com/frank26080115/finger-gt511-lib/
 * portable library for the GT-511C1R fingerprint sensor written in C
 *
 * this file contains an example of usage, written for a Teensy 2.0 and using Arduino
 *
 * Copyright (C) 2016 Frank Zhao
 * https://github.com/frank26080115/finger-gt511-lib/blob/master/LICENSE
 */

#include "finger.h"
#include <avr/eeprom.h>

#define PIN_LED_GREEN 1
#define PIN_LED_RED 2
#define PIN_LED_BLUE 4
#define PIN_BUTTON 3

#define LED_RED_OFF() digitalWrite(PIN_LED_RED, HIGH)
#define LED_GREEN_OFF() digitalWrite(PIN_LED_GREEN, HIGH)
#define LED_RED_ON() digitalWrite(PIN_LED_RED, LOW)
#define LED_GREEN_ON() digitalWrite(PIN_LED_GREEN, LOW)
#define LED_BLUE_OFF() analogWrite(PIN_LED_BLUE, 0)
#define LED_BLUE_ON() analogWrite(PIN_LED_BLUE, 0x20)

#define IS_BUTTON_PRESSED() (digitalRead(PIN_BUTTON) == LOW)

#define REPORT_ERROR(x, r, p) do { Serial.print(F(x)); Serial.print(F(" ")); Serial.print((unsigned long)(r), DEC); Serial.print(F(" ")); Serial.print((unsigned long)((p)->param), DEC); Serial.print(F(" ")); Serial.print((unsigned long)((p)->resp), DEC); Serial.println(); Serial.send_now(); } while(0)

#define FINGERHAL_SERIAL Serial1
#define FINGER_IDLE_DELAY 10

finger_resp_t resp;
finger_err_t err;
char ser_pipe = 0;

void setup()
{
	uint8_t attempts = 3;
	int bright;
	char to_enroll = 0;

	Serial.begin(9600); // this is only for debug
	FINGERHAL_SERIAL.begin(9600); // this must be done at some point by the user, the finger-gt511-lib does not do it for you

	// hardware setup
	pinMode(PIN_BUTTON, INPUT_PULLUP);
	pinMode(PIN_LED_RED, OUTPUT);
	pinMode(PIN_LED_GREEN, OUTPUT);
	pinMode(0, INPUT); // older blue LED pin
	pinMode(PIN_LED_BLUE, OUTPUT);
	LED_RED_OFF();
	LED_GREEN_OFF();
	LED_BLUE_OFF();
	delay(100); // forgot why this is here

	finger_close(&resp); // just in case a reset was performed when the sensor was not powered down

	// holding the button at reset is how to trigger the enrollment demo, see below when enroll() is called
	if (IS_BUTTON_PRESSED())
	{
		while (IS_BUTTON_PRESSED()) {
			delay(50);
			LED_BLUE_ON();
			delay(50);
			LED_BLUE_OFF();
		}
		to_enroll = 1;
	}

	// attempt a few times to establish connection to the GT-511C1R
	// this isn't actually required, should work on the first try
	while (attempts > 0)
	{
		err = finger_open(0, &resp);
		if (err != FINGER_ERROR_NONE) {
			attempts--;
			continue;
		}
		err = finger_setLed(1, &resp);
		if (err != FINGER_ERROR_NONE) {
			attempts--;
			continue;
		}
		break;
	}

	// did not succeed, display error blink pattern
	if (attempts <= 0)
	{
		while (1) {
			delay(100);
			LED_RED_ON();
			delay(100);
			LED_RED_OFF();
		}
	}

	// button was pressed, launch enrollment demo, see function below
	if (to_enroll != 0) {
		enroll();
	}

	// this fades in the LED, only for cosmetics
	for (bright = 0; bright <= 0x20; bright++)
	{
		analogWrite(PIN_LED_BLUE, bright);
		delay(50);
	}
}

void loop()
{
	if (Serial.available() > 0)
	{
		// trigger the firmware to go into serial pipe mode
		// this allows me to play with the PC based GUI made for the GT-511C1R
		if (Serial.read() == '!') {
			ser_pipe = 1;
			Serial.println(F("Serial Pipe Started"));
			Serial.send_now();
			finger_close(&resp); // I think this is required because the GUI software doesn't like it if it tries to connect with an existing open connection
		}
	}

	if (ser_pipe != 0)
	{
		while (Serial.available() > 0) {
			FINGERHAL_SERIAL.write((uint8_t)Serial.read());
		}
		while (FINGERHAL_SERIAL.available() > 0) {
			Serial.write((uint8_t)FINGERHAL_SERIAL.read());
		}
		Serial.send_now();
		return; // do nothing else, the delays below slow everything down too much
	}

	// blink LED if button is pressed
	// this is just for fun
	if (IS_BUTTON_PRESSED())
	{
		FINGERHAL_SERIAL.flush();
		LED_RED_OFF();
		LED_GREEN_ON();
		LED_BLUE_OFF();
		while (IS_BUTTON_PRESSED()) {
			delay(200);
			LED_BLUE_ON();
			delay(200);
			LED_BLUE_OFF();
		}
		LED_GREEN_OFF();
		delay(200);
		LED_BLUE_ON();
	}
	else
	{
		// here is the meat of the fingerprint reading code
		// do everything in this sequence

		if ((err = finger_isFingerPressed(&resp)) == FINGER_ERROR_NONE) 
		{
			if ((err = finger_captureFinger(0, &resp)) == FINGER_ERROR_NONE) // does not need to be high quality capture for identification or verification
			{
				if ((err = finger_identify_1_N(&resp)) == FINGER_ERROR_NONE)
				{
					// finger exists in database
					// notify the user that it worked

					Serial.print("VERIFIED ID: ");
					Serial.println((int)resp.param, DEC);
					Serial.send_now();
					LED_RED_OFF();
					LED_GREEN_ON();
					delay(100);
					LED_GREEN_OFF();
					delay(100);
					LED_GREEN_ON();
					delay(500);
				}
				else
				{
					if (err == FINGER_ERROR_BADFINGER || err == FINGER_ERROR_WRONGFINGER)
					{
						LED_RED_ON();
						LED_GREEN_OFF();
						delay(2000);
						LED_RED_OFF();
					}
					else
					{
						LED_RED_OFF();
						LED_GREEN_OFF();
						if (ser_pipe == 0) {
							REPORT_ERROR("error finger_identify_1_N", err, &resp);
						}
						delay(FINGER_IDLE_DELAY);
					}
				}
			}
			else
			{
				LED_RED_OFF();
				LED_GREEN_OFF();
				if (ser_pipe == 0) {
					REPORT_ERROR("error finger_captureFinger", err, &resp);
				}
				delay(FINGER_IDLE_DELAY);
			}
		}
		else
		{
			if (ser_pipe == 0) {
				REPORT_ERROR("error finger_isFingerPressed", err, &resp);
			}
			delay(FINGER_IDLE_DELAY);
		}
	}
}

// here is the code for the enrollment demo
void enroll()
{
	int i = 0;
	char hasAvail = 0;
	int useId = 0;
	char prevBtn = 0;
	uint8_t stage = 0;
	unsigned long t;

	LED_BLUE_OFF();
	LED_RED_ON();
	LED_GREEN_ON();

	// finds a free spot in the database
	// limit is 20 for the GT-511C1R and 200 for the GT-511C3
	for (i = 0; i < 20; i++)
	{
		err = finger_checkEnrolled(i, &resp);
		if ((err == FINGER_ERROR_NONE || err == FINGER_ERROR_NACK) && (resp.resp & 0x00FF) == 0x31) {
			hasAvail = 1;
			break;
		}
	}

	if (hasAvail != 0) // free space found
	{
		useId = i;
		eeprom_write_byte((uint8_t*)0, useId); // remember for next time
	}
	else
	{
		// no space is available, so we find the oldest entry
		// note, you can use ID 0 but for my own application, I decided to keep 0 as a "master" that never gets forgotten
		useId = eeprom_read_byte((uint8_t*)0);
		useId++;
		useId %= 19;
		err = finger_checkEnrolled(useId + 1, &resp);
		if (err == FINGER_ERROR_NONE || (err == FINGER_ERROR_NACK && (resp.resp & 0x00FF) == 0x30)) {
			err = finger_deleteId(useId + 1, &resp);
		}
		eeprom_write_byte((uint8_t*)0, useId + 1); // remember for next time, this is the most recent, so it won't get overwritten
		useId++;
	}

	// pay attention to the sequence of "enrollStart" and "enrollStage"

	err = finger_enrollStart(useId, &resp);
	if (err != FINGER_ERROR_NONE)
	{
		// something is wrong, error blink, report to serial port
		while (1) {
			delay(50);
			LED_RED_ON();
			delay(50);
			LED_RED_OFF();
			REPORT_ERROR("error finger_enrollStart", err, &resp);
		}
	}

	// go through the stages
	// step 1: the user has to put their finger on the sensor
	// the light will turn red
	// then the user has to press the button when they are ready for a capture
	// the capture will complete and the light will turn green
	// when the light is green, the user must remove the finger, and the light will turn off
	// then goto step 10
	// this happens 3 times (for the 3 stages)
	while (stage < 3)
	{
		// this bit of code makes the LED blink according to what stage you are in
		t = millis();
		t /= 100;
		t %= 20;
		if (t == 0 || (stage <= 1 && t == 10) || (stage == 1 && (t == 2 || t == 12)) || (stage == 2 && (t == 4 || t == 2))) {
			LED_BLUE_ON();
		}
		else {
			LED_BLUE_OFF();
		}

		// pay attention to the sequence of these function calls
		// check if finger is here first, if it is, take a picture in high quality, then perform a single "stage"
		// after that, the user should release the finger so the next picture is slightly different

		if ((err = finger_isFingerPressed(&resp)) == FINGER_ERROR_NONE)
		{
			LED_GREEN_OFF();
			LED_RED_ON();
			if (prevBtn == 0 && IS_BUTTON_PRESSED())
			{
				LED_BLUE_OFF();
				if ((err = finger_captureFinger(1, &resp)) == FINGER_ERROR_NONE) // capture must be high quality for enrollment
				{
					err = finger_enrollStage(stage, &resp);
					if (err == FINGER_ERROR_NONE)
					{
						LED_RED_OFF();
						LED_GREEN_ON();
						Serial.print(F("Eroll stage ")); Serial.print(stage, DEC); Serial.println(F(" done")); Serial.send_now();
						stage++;
						delay(100);
					}
					else
					{
						LED_RED_OFF();
						REPORT_ERROR("error finger_enrollStage", err, &resp);
						delay(100);
						LED_RED_ON();
						delay(100);
						LED_RED_OFF();
					}
				}
				else
				{
					LED_RED_OFF();
					REPORT_ERROR("error finger_captureFinger HQ", err, &resp);
					delay(100);
					LED_RED_ON();
					delay(100);
					LED_RED_OFF();
				}
				while (IS_BUTTON_PRESSED() || ((err = finger_isFingerPressed(&resp)) == FINGER_ERROR_NONE));
				prevBtn = 0;
				LED_GREEN_OFF();
				LED_RED_OFF();
				delay(FINGER_IDLE_DELAY);
			}
		}
		else
		{
			LED_RED_OFF();
			delay(FINGER_IDLE_DELAY);
		}
		prevBtn = IS_BUTTON_PRESSED();
	}

	if (stage == 3) // all done
	{
		while (1)
		{
			Serial.print("Enroll Done ID "); Serial.print(useId, DEC); Serial.println(); Serial.send_now();
			LED_BLUE_OFF();
			LED_RED_OFF();
			LED_GREEN_ON();
			delay(200);
			LED_GREEN_OFF();
			delay(200);
		}
	}
}