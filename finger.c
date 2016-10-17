/*!
 * finger-gt511-lib
 * https://github.com/frank26080115/finger-gt511-lib/
 * portable library for the GT-511C1R fingerprint sensor written in C
 *
 * Copyright (C) 2016 Frank Zhao
 * https://github.com/frank26080115/finger-gt511-lib/blob/master/LICENSE
 */

#include "finger.h"
#include "fingerhal.h"
#include <stdint.h>

#define FINGER_PKT_LEN 12
#define FINGER_BUFF_LEN (FINGER_PKT_LEN * 4)
#define FINGER_RESPONSE_TIMEOUT 1000

// these are defined from the documentation
enum
{
	FINGER_CMD_OPEN = 0x01,
	FINGER_CMD_CLOSE = 0x02,
	FINGER_CMD_CMOS_LED = 0x12,
	FINGER_CMD_GET_ENROLL_COUNT = 0x20,
	FINGER_CMD_CHECK_ENROLLED = 0x21,
	FINGER_CMD_ENROLL_START = 0x22,
	FINGER_CMD_ENROLL_1 = 0x23,
	FINGER_CMD_ENROLL_2 = 0x24,
	FINGER_CMD_ENROLL_3 = 0x25,
	FINGER_CMD_IS_PRESS_FINGER = 0x26,
	FINGER_CMD_DELETE_ID = 0x40,
	FINGER_CMD_DELETE_ALL = 0x41,
	FINGER_CMD_VERIFY_1_1 = 0x50,
	FINGER_CMD_VERIFY_1_N = 0x51,
	FINGER_CMD_CAPTURE_FINGER = 0x60,
	FINGER_CMD_START_CODE_1 = 0x55,
	FINGER_CMD_START_CODE_2 = 0xAA,
	FINGER_CMD_DEVICE_ID_1 = 0x01,
	FINGER_CMD_DEVICE_ID_2 = 0x00,
};

// function prototypes for private internal functions
// do not call these from the higher level application
finger_err_t finger_sendCommandGetResponse(uint16_t cmd, uint32_t param, finger_resp_t* resp);
void finger_makePacket(uint8_t* buff, uint16_t cmd, uint32_t param);
uint16_t finger_calcChecksum(uint8_t* buff);
char finger_parseResponse(uint8_t* buff, finger_resp_t* resp);
char finger_getResponse(uint8_t* buff);
finger_err_t finger_xlateVerifyError(finger_err_t r, finger_resp_t* resp);

// required for the fingerprint sensor to start communcating and accepting other commands
// leaving the baud parameter 0 will keep the previous baud
// baud rate can be set to one of the acceptable values (see documentation)
// returns FINGER_ERROR_NONE if successful
// otherwise, check the resp struct to find out more details
finger_err_t finger_open(uint32_t baud, finger_resp_t* resp)
{
	return finger_sendCommandGetResponse(FINGER_CMD_OPEN, baud, resp);
}

// closes the connection
// untested
// returns FINGER_ERROR_NONE if successful
// otherwise, check the resp struct to find out more details
finger_err_t finger_close(finger_resp_t* resp)
{
	return finger_sendCommandGetResponse(FINGER_CMD_CLOSE, 0, resp);
}

// this causes the camera sensor to take a snapshot into memory
// the parameter "hq" indicates if the image taken should be high quality
// high quality is required for all stages of the enrollment process (not before "enrollStart")
// high quality is not required for verification or identification
// returns FINGER_ERROR_NONE if successful
// returns FINGER_ERROR_NOFINGER or FINGER_ERROR_BADFINGER for their perspective meanings
// otherwise, check the resp struct to find out more details
finger_err_t finger_captureFinger(char hq, finger_resp_t* resp)
{
	finger_err_t r = finger_sendCommandGetResponse(FINGER_CMD_CAPTURE_FINGER, hq != 0 ? 1 : 0, resp);
	if ((r == FINGER_ERROR_NONE && resp->param != 0) || (r == FINGER_ERROR_NACK && resp->error == FINGER_ERRCODE_FINGER_IS_NOT_PRESSED)) {
		return FINGER_ERROR_NOFINGER;
	}
	if ((r == FINGER_ERROR_NONE && resp->param != 0) || (r == FINGER_ERROR_NACK && resp->error == FINGER_ERRCODE_BAD_FINGER)) {
		return FINGER_ERROR_BADFINGER;
	}
	return r;
}

// turns on or off the LED
// returns FINGER_ERROR_NONE when it is successful
// if an error is returned, check the resp struct to find out more details
finger_err_t finger_setLed(char led, finger_resp_t* resp)
{
	return finger_sendCommandGetResponse(FINGER_CMD_CMOS_LED, led != 0 ? 1 : 0, resp);
}

// checks how many database entries exist
// returns FINGER_ERROR_NONE when it is successful
// when successful, resp->param contains the count
// if an error is returned, check the resp struct to find out more details
finger_err_t finger_getEnrollCount(finger_resp_t* resp)
{
	return finger_sendCommandGetResponse(FINGER_CMD_GET_ENROLL_COUNT, 0, resp);
}

// checks if a ID is in the database
// returns FINGER_ERROR_NONE if ID is in the database
// returns FINGER_ERROR_NOFINGER if ID is not in the database
// otherwise, check the resp struct to find out more details
finger_err_t finger_checkEnrolled(uint32_t id, finger_resp_t* resp)
{
	finger_err_t r = finger_sendCommandGetResponse(FINGER_CMD_CHECK_ENROLLED, id, resp);
	if (r == FINGER_ERROR_NONE || (r == FINGER_ERROR_NACK && (resp->resp & 0x00FF) == 0x30) || (r == FINGER_ERROR_NACK && resp->error == FINGER_ERRCODE_IS_ALREADY_USED)) {
		return FINGER_ERROR_NONE;
	}
	else if (r == FINGER_ERROR_NACK && resp->error == FINGER_ERRCODE_IS_NOT_USED) {
		return FINGER_ERROR_NOFINGER;
	}
	return r;
}

// see documentation on how the enrollment stages work
// the "enrollStart" is used before any of the stages
// capture does not need to happen before enrollStart
// returns FINGER_ERROR_NONE if successful
// otherwise, check the resp struct to find out more details
finger_err_t finger_enrollStart(uint32_t id, finger_resp_t* resp)
{
	return finger_sendCommandGetResponse(FINGER_CMD_ENROLL_START, id, resp);
}

// see documentation on how the enrollment stages work
// the "stage" parameter can be 0, 1, or 2
// and they must be used in that exact sequence or else a NACK can occur with FINGER_ERRCODE_TURN_ERR
// returns FINGER_ERROR_NONE if successful
// otherwise, check the resp struct to find out more details
finger_err_t finger_enrollStage(uint32_t stage, finger_resp_t* resp)
{
	uint16_t cmd;
	if (stage >= 3) {
		return FINGER_ERROR_INVALID;
	}
	switch (stage) {
		case 0:	cmd = FINGER_CMD_ENROLL_1;	break;
		case 1:	cmd = FINGER_CMD_ENROLL_2;	break;
		case 2:	cmd = FINGER_CMD_ENROLL_3;	break;
		default:	return FINGER_ERROR_INVALID;
	}
	return finger_sendCommandGetResponse(cmd, 0, resp);
}

// checks if a finger is currently placed on the sensor
// recommended that this is used prior to captureFinger
// it can also turn the finger print reader into a button
// returns FINGER_ERROR_NONE if a finger is pressed
// returns FINGER_ERROR_NOFINGER if a finger is not pressed
// otherwise, check the resp struct to find out more details
finger_err_t finger_isFingerPressed(finger_resp_t* resp)
{
	finger_err_t r = finger_sendCommandGetResponse(FINGER_CMD_IS_PRESS_FINGER, 0, resp);
	if ((r == FINGER_ERROR_NONE && resp->param != 0) || (r == FINGER_ERROR_NACK && resp->error == FINGER_ERRCODE_FINGER_IS_NOT_PRESSED)) {
		return FINGER_ERROR_NOFINGER;
	}
	return r;
}

// deletes one ID from the database
// please check that the ID is within the memory limits of your particular device
// untested
// returns FINGER_ERROR_NONE if successful
// otherwise, check the resp struct to find out more details
finger_err_t finger_deleteId(uint32_t id, finger_resp_t* resp)
{
	return finger_sendCommandGetResponse(FINGER_CMD_DELETE_ID, id, resp);
}

// deletes entire database
// untested
// returns FINGER_ERROR_NONE if successful
// otherwise, check the resp struct to find out more details
finger_err_t finger_deleteAll(finger_resp_t* resp)
{
	return finger_sendCommandGetResponse(FINGER_CMD_DELETE_ALL, 0, resp);
}

// checks if the captured finger exists in the database
// returns FINGER_ERROR_NONE if the finger does exist and everything is successful
// returns FINGER_ERROR_WRONGFINGER if the finger does not exists
// it can return FINGER_ERROR_BADFINGER or FINGER_ERROR_NOFINGER for their respective meanings
// it can also return other error codes but the user will need to examine the resp struct to find out more details
// if the ID does not exist, resp->error may contain something like FINGER_ERRCODE_IS_NOT_USED
finger_err_t finger_verify_1_1(uint32_t id, finger_resp_t* resp)
{
	finger_err_t r = finger_sendCommandGetResponse(FINGER_CMD_VERIFY_1_1, id, resp);
	return finger_xlateVerifyError(r, resp);
}

// checks if the captured finger exists in the database
// returns FINGER_ERROR_NONE if the finger does exist and everything is successful
// returns FINGER_ERROR_WRONGFINGER if the finger does not exists
// it can return FINGER_ERROR_BADFINGER or FINGER_ERROR_NOFINGER for their respective meanings
// it can also return other error codes but the user will need to examine the resp struct to find out more details
finger_err_t finger_identify_1_N(finger_resp_t* resp)
{
	finger_err_t r = finger_sendCommandGetResponse(FINGER_CMD_VERIFY_1_N, 0, resp);
	if (r == FINGER_ERROR_NONE && resp->param >= 200) { // out of the range of available IDs, this should never happen
		return FINGER_ERROR_WRONGFINGER;
	}
	return finger_xlateVerifyError(r, resp);
}

// every command gets a response, so this sequence is wrapped nicely into this function that does everything, including formatting the outbound packet and parsing the reply packet
finger_err_t finger_sendCommandGetResponse(uint16_t cmd, uint32_t param, finger_resp_t* resp)
{
	uint8_t buff[FINGER_BUFF_LEN];
	int8_t ret;
	finger_makePacket(buff, cmd, param);
	fingerHal_sendPacket(buff, FINGER_PKT_LEN);
	ret = finger_getResponse(buff);
	if (ret == 0) {
		return FINGER_ERROR_TIMEOUT;
	}
	ret = finger_parseResponse(buff, resp);
	return ret;
}

// packs the command and parameters into a proper packet and also apply the checksum
void finger_makePacket(uint8_t* buff, uint16_t cmd, uint32_t param)
{
	uint16_t cs;
	buff[0] = FINGER_CMD_START_CODE_1;
	buff[1] = FINGER_CMD_START_CODE_2;
	buff[2] = FINGER_CMD_DEVICE_ID_1;
	buff[3] = FINGER_CMD_DEVICE_ID_2;
	// respect endianess
	buff[4] = (uint8_t)((param & 0x000000FF) >> (8 * 0));
	buff[5] = (uint8_t)((param & 0x0000FF00) >> (8 * 1));
	buff[6] = (uint8_t)((param & 0x00FF0000) >> (8 * 2));
	buff[7] = (uint8_t)((param & 0xFF000000) >> (8 * 3));
	buff[8] = (uint8_t)((cmd & 0x00FF) >> (8 * 0));
	buff[9] = (uint8_t)((cmd & 0xFF00) >> (8 * 1));
	cs = finger_calcChecksum(buff);
	buff[10] = (uint8_t)((cs & 0x00FF) >> (8 * 0));
	buff[11] = (uint8_t)((cs & 0xFF00) >> (8 * 1));
}

// checksum algorithm is described by the documentation
uint16_t finger_calcChecksum(uint8_t* buff)
{
	uint8_t i;
	uint16_t cs = 0;
	for (i = 0; i < FINGER_PKT_LEN - sizeof(uint16_t); i++) {
		cs += buff[i];
	}
	return cs;
}

char finger_parseResponse(uint8_t* buff, finger_resp_t* resp)
{
	uint16_t cs;
	// check for the right start sequence
	if (buff[0] != FINGER_CMD_START_CODE_1
		|| buff[1] != FINGER_CMD_START_CODE_2
		|| buff[2] != FINGER_CMD_DEVICE_ID_1
		|| buff[3] != FINGER_CMD_DEVICE_ID_2
		) {
		return FINGER_ERROR_PARSING;
	}

	// this byte is always zero, because the command word is 16 bits but there's no command that has the upper 8 bits being used
	if (buff[9] != 0) {
		return FINGER_ERROR_PARSING;
	}

	// either a ACK or a NACK, nothing else is defined in the documentation
	if (buff[8] != 0x30 && buff[8] != 0x31) {
		return FINGER_ERROR_PARSING;
	}

	// validate the checksum
	cs = finger_calcChecksum(buff);
	if (buff[10] != (uint8_t)((cs & 0x00FF) >> (8 * 0)) || buff[11] != (uint8_t)((cs & 0xFF00) >> (8 * 1))) {
		return FINGER_ERROR_CHECKSUM;
	}

	// copy into the correct sizes while respecting endianess
	resp->param  = ((uint32_t)buff[4]) << (8 * 0);
	resp->param += ((uint32_t)buff[5]) << (8 * 1);
	resp->param += ((uint32_t)buff[6]) << (8 * 2);
	resp->param += ((uint32_t)buff[7]) << (8 * 3);
	resp->resp   = ((uint16_t)buff[8]) << (8 * 0);
	resp->resp  += ((uint16_t)buff[9]) << (8 * 1);

	// translate into the correct error if a NACK is received
	if (buff[8] == 0x31 && buff[5] != 0) {
		if (buff[6] == 0) {
			return FINGER_ERROR_NONE; // NACK but the NACK code is NONE
		}
		else if (buff[6] > 0x12) {
			return FINGER_ERROR_PARSING; // out of range of known NACK codes
		}
		else {
			// a proper NACK, copy into error field and return
			resp->error = resp->param;
			return FINGER_ERROR_NACK;
		}
	}
	return FINGER_ERROR_NONE; // all good
}

// this helps the user accessible functions return something that can be used more immediately
finger_err_t finger_xlateVerifyError(finger_err_t r, finger_resp_t* resp)
{
	if (r == FINGER_ERROR_NACK) {
		if (resp->error == FINGER_ERRCODE_VERIFY_FAILED || resp->error == FINGER_ERRCODE_IDENTIFY_FAILED) {
			return FINGER_ERROR_WRONGFINGER;
		}
		else if (resp->error == FINGER_ERRCODE_FINGER_IS_NOT_PRESSED) {
			return FINGER_ERROR_NOFINGER;
		}
		else if (resp->error == FINGER_ERRCODE_BAD_FINGER) {
			return FINGER_ERROR_BADFINGER;
		}
	}
	return r;
}

char finger_getResponse(uint8_t* buff)
{
	unsigned long stopwatch;
	char hasSync = 0;
	uint8_t first;
	uint8_t i;
	stopwatch = fingerHal_millis();
	// first wait for the sync character to arrive
	while (hasSync == 0 && (fingerHal_millis() - stopwatch) < FINGER_RESPONSE_TIMEOUT)
	{
		if (fingerHal_serAvail() > 0) {
			first = fingerHal_serRead();
			if (first == FINGER_CMD_START_CODE_1) {
				hasSync = 1;
			}
		}
	}
	if (hasSync == 0) {
		return 0; // did not arrive, failed
	}
	buff[0] = first; // store the sync character because the parser needs it
	i = 1; // increment index
	while (i < FINGER_PKT_LEN && (fingerHal_millis() - stopwatch) < FINGER_RESPONSE_TIMEOUT)
	{
		// read the entire packet
		if (fingerHal_serAvail() > 0) {
			buff[i] = fingerHal_serRead();
			i++;
		}
	}
	return (i == FINGER_PKT_LEN); // whole packet received is good, otherwise, the return 0 means timeout happened
}