#ifndef _FINGER_H_
/*!
 * finger-gt511-lib
 * https://github.com/frank26080115/finger-gt511-lib/
 * portable library for the GT-511C1R fingerprint sensor written in C
 *
 * this is header file for this library
 *
 * Copyright (C) 2016 Frank Zhao
 * https://github.com/frank26080115/finger-gt511-lib/blob/master/LICENSE
 */

#define _FINGER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// general return codes used for all functions of this library, simplifying the user application layer code
typedef enum
{
	FINGER_ERROR_NONE = 0,
	FINGER_ERROR_INVALID,
	FINGER_ERROR_TIMEOUT,
	FINGER_ERROR_PARSING,
	FINGER_ERROR_CHECKSUM,
	FINGER_ERROR_NACK,
	FINGER_ERROR_NOFINGER, // used for verify_1_1 and identify_1_N and captureFinger and isFingerPressed
	FINGER_ERROR_BADFINGER, // used for verify_1_1 and identify_1_N and captureFinger
	FINGER_ERROR_WRONGFINGER, // used for verify_1_1 and identify_1_N
	FINGER_ERROR_OTHER,
}
finger_err_t;

typedef struct
{
	uint32_t param; // check documentation, usually contains a finger's ID from the database, or an error code
	uint16_t resp; // check documentation, usually contains ACK or NACK
	uint16_t error; // should contain one of the FINGER_ERRCODE_* if a NACK is received
}
finger_resp_t;

// obtained from documentation
enum
{
	FINGER_ERRCODE_NONE					 = 0x0000,
	FINGER_ERRCODE_TIMEOUT				 = 0x1001,
	FINGER_ERRCODE_INVALID_BAUDRATE		 = 0x1002,
	FINGER_ERRCODE_INVALID_POS			 = 0x1003,
	FINGER_ERRCODE_IS_NOT_USED			 = 0x1004,
	FINGER_ERRCODE_IS_ALREADY_USED		 = 0x1005,
	FINGER_ERRCODE_COMM_ERR				 = 0x1006,
	FINGER_ERRCODE_VERIFY_FAILED		 = 0x1007,
	FINGER_ERRCODE_IDENTIFY_FAILED		 = 0x1008,
	FINGER_ERRCODE_DB_IS_FULL			 = 0x1009,
	FINGER_ERRCODE_DB_IS_EMPTY			 = 0x100A,
	FINGER_ERRCODE_TURN_ERR				 = 0x100B,
	FINGER_ERRCODE_BAD_FINGER			 = 0x100C,
	FINGER_ERRCODE_ENROLL_FAILED		 = 0x100D,
	FINGER_ERRCODE_IS_NOT_SUPPORTED		 = 0x100E,
	FINGER_ERRCODE_DEV_ERR				 = 0x100F,
	FINGER_ERRCODE_CAPTURE_CANCELED		 = 0x1010,
	FINGER_ERRCODE_INVALID_PARAM		 = 0x1011,
	FINGER_ERRCODE_FINGER_IS_NOT_PRESSED = 0x1012,
};

// public functions for user application layer code
// documentation is in the comments of finger.c
finger_err_t finger_open(uint32_t baud, finger_resp_t* resp);
finger_err_t finger_close(finger_resp_t* resp);
finger_err_t finger_captureFinger(char hq, finger_resp_t* resp);
finger_err_t finger_setLed(char led, finger_resp_t* resp);
finger_err_t finger_getEnrollCount(finger_resp_t* resp);
finger_err_t finger_checkEnrolled(uint32_t id, finger_resp_t* resp);
finger_err_t finger_enrollStart(uint32_t id, finger_resp_t* resp);
finger_err_t finger_enrollStage(uint32_t stage, finger_resp_t* resp);
finger_err_t finger_isFingerPressed(finger_resp_t* resp);
finger_err_t finger_deleteId(uint32_t id, finger_resp_t* resp);
finger_err_t finger_deleteAll(finger_resp_t* resp);
finger_err_t finger_verify_1_1(uint32_t id, finger_resp_t* resp);
finger_err_t finger_identify_1_N(finger_resp_t* resp);

#ifdef __cplusplus
}
#endif

#endif