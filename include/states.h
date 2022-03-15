/*
 * states.h
 *
 *  Created on: 25 mar 2020
 *      Author: renzo
 */

#ifndef INCLUDE_STATES_H_
#define INCLUDE_STATES_H_

enum OPERATION_MODE {
	OPERATION_NORMAL = 0,
	OPERATION_PING = 1,
	OPERATION_DISABLED = 2
};
enum ACION_MODE {
	ACTION_AUTO = 0,
	ACTION_START = 1,
	ACTION_STOP = 2
};

enum SCREEN {
	SCREEN_MAIN,
	SCREEN_PUMP_TIMER,
	SCREEN_PING_TIMER,
	SCREEN_STOP,
	SCREEN_STATUS_DEVICE,
	SCREEN_STATUS_DEVICE_DETAILED,
	SCREEN_ERROR
};
#define SCREEN_SIZE 7

enum MENU_MAIN_STATE {
	PUMP_START_MENU = 0,
	PUMP_START_PING_MENU,
	PUMP_STOP_MENU,
	STATUS_MENU,
	STATUS_MENU_DETAILED
};
#define MENU_MAIN_STATE_SIZE 5

uint8_t screenEncoderSize[] = {MENU_MAIN_STATE_SIZE, 0, 0, 0, 2, 2, 2};

enum PACKET_TYPE {
	PACKET_PUMP_ACTIVATION = 0,
	PACKET_PUMP_LEVEL = 1,
	BATTERY_LEVEL = 2
};

#endif /* INCLUDE_STATES_H_ */
