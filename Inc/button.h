/*
 * button.h
 *
 *  Created on: 10 апр. 2020 г.
 *      Author: Arh
 */

#ifndef BUTTON_H_
#define BUTTON_H_
#include "stm32f1xx_hal.h"

#define GetTimeMs(x)	HAL_GetTick()

#define BUT_DEBOUNSE 	1
#define BUT_DOUBLE		200
#define BUT_LONG		700
#define BUT_REPEAT		300


typedef enum {
	RELISED = 0,
	DOWN,
	PRESSED,
	CLICKED,
	LONG_PRESSED,
	DOUBLE_CLICKED,
	REPEAT_PRESS
}t_button_state;

typedef struct {
	unsigned long time_stamp;
	unsigned short state;
}t_button_var;

t_button_state Button_rutine (t_button_var * var, int pressed);

#endif /* BUTTON_H_ */
