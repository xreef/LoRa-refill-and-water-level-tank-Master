/*
 * eng.h
 *
 *  Created on: 25 mar 2020
 *      Author: renzo
 */

#ifndef INCLUDE_ENG_H_
#define INCLUDE_ENG_H_

#include "states.h"

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

#define SCREEN_MAIN_TITLE ("Tank menu")
#define SCREEN_PUMP_TITLE ("Pump")
#define SCREEN_STATUS_DEVICE_TITLE ("State")
#define SCREEN_CONFIG_TIMER_TITLE ("Timer")
#define SCREEN_ERROR_TITLE ("Error")

#define PUMP_MENU_LABEL ("Pump")
#define CHECK_MENU_LABEL ("Check\nstate")
#define START_PUMP_MENU_LABEL ("Start\npump")
#define START_PUMP_LABEL ("Start\npump")
#define START_PUMP_PING_LABEL ("Start\npump\nping")
#define STOP_PUMP_MENU_LABEL ("Stop\npump")
#define PUMP_TIMER_MENU_LABEL ("Timer\npump")
#define CONFIG_TIMER_MENU_LABEL ("Conf\ntimer")
#define STATUS_DEVICE_MENU_LABEL ("State")
#define STATUS_DETT_MENU_LABEL ("Detail")

#define STATUS_REFILL ("On load!")
#define STATUS_WAIT_STATUS ("Wait status!")
#define STATUS_FULL ("Tank full!")
#define STATUS_EMPTY ("Tank empty!")

#define TIMER_START_LABEL ("Start:  ")
#define TIMER_STOP_LABEL ("End:    ")

#define BACK_LABEL ("<< Indietro")

String getFormattedDate(int year, int month, int day, int dayOfWeek)  {
    // return String(year)+"/"+String(month)+"/"+String(day) + " (" + String(daysOfTheWeek[dayOfWeek]) + ") "+String(hour)+":"+String(minute)+":"+String(second);
    return String(year)+"/"+String(month)+"/"+String(day);
}

String getFormattedTime(int hour, int minute, int second)  {
    // return String(year)+"/"+String(month)+"/"+String(day) + " (" + String(daysOfTheWeek[dayOfWeek]) + ") "+String(hour)+":"+String(minute)+":"+String(second);
    return String(hour)+":"+String(minute)+":"+String(second);
}

String getFormattedDateTime(int year, int month, int day, int dayOfWeek, int hour, int minute, int second)  {
    // return String(year)+"/"+String(month)+"/"+String(day) + " (" + String(daysOfTheWeek[dayOfWeek]) + ") "+String(hour)+":"+String(minute)+":"+String(second);
    return getFormattedDate(year, month, day, dayOfWeek) + " " + getFormattedTime(hour, minute, second);
}

#endif /* INCLUDE_ENG_H_ */
