/*!
 * finger-gt511-lib
 * https://github.com/frank26080115/finger-gt511-lib/
 * portable library for the GT-511C1R fingerprint sensor written in C
 *
 * Copyright (C) 2016 Frank Zhao
 * https://github.com/frank26080115/finger-gt511-lib/blob/master/LICENSE
 */

#ifndef _FINGERHAL_H_
#define _FINGERHAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
you, the person using this library, must write the functions below
usually these will become simple wrappers to your existing function calls
*/

extern void fingerHal_sendPacket(uint8_t* buff, uint8_t len);
extern unsigned long fingerHal_millis(void);
extern uint8_t fingerHal_serAvail(void);
extern int16_t fingerHal_serRead(void);

#ifdef __cplusplus
}
#endif

#endif