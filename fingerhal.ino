/*!
 * finger-gt511-lib
 * https://github.com/frank26080115/finger-gt511-lib/
 * portable library for the GT-511C1R fingerprint sensor written in C
 *
 * this file is only an example, you will need to edit it
 * this is the HAL wrappers used with finger_example.ino
 *
 * Copyright (C) 2016 Frank Zhao
 * https://github.com/frank26080115/finger-gt511-lib/blob/master/LICENSE
 */

#include "fingerhal.h"

/*
this is an example of how integrate the library with Arduino
"FINGERHAL_SERIAL" needs to be defined as something like Serial, Serial1, Serial2, etc
*/

void fingerHal_sendPacket(uint8_t* buff, uint8_t len)
{
	FINGERHAL_SERIAL.write(buff, len);
}

uint8_t fingerHal_serAvail(void)
{
	return FINGERHAL_SERIAL.available();
}

uint16_t fingerHal_serRead(void)
{
	return FINGERHAL_SERIAL.read();
}

unsigned long fingerHal_millis(void)
{
	return millis();
}