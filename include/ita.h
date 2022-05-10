/*
 * ita.h
 *
 *  Created on: 25 mar 2020
 *      Author: renzo
 */

#ifndef INCLUDE_ITA_H_
#define INCLUDE_ITA_H_

#include "states.h"

char daysOfTheWeek[7][12] = {"Domenica", "Lunedì", "Martedì", "Mercoledì", "Giovedì", "Venerdì", "Sabato"};

#define SCREEN_MAIN_TITLE ("Tank menu")
#define SCREEN_PUMP_TITLE ("Pompa")
#define SCREEN_STATUS_DEVICE_TITLE ("Stato")
#define SCREEN_CONFIG_TIMER_TITLE ("Timer")
#define SCREEN_ERROR_TITLE ("Errore")

#define PUMP_MENU_LABEL ("Pompa")
#define CHECK_MENU_LABEL ("Controlla\nstato")
#define START_PUMP_MENU_LABEL ("Avvia\npompa")
#define START_PUMP_LABEL ("Avvia\npompa")
#define START_PUMP_PING_LABEL ("Avvia\npompa\nping")
#define STOP_PUMP_MENU_LABEL ("Ferma\npompa")
#define PUMP_TIMER_MENU_LABEL ("Timer\npompa")
#define CONFIG_TIMER_MENU_LABEL ("Conf\ntimer")
#define STATUS_DEVICE_MENU_LABEL ("Stato")
#define STATUS_DETT_MENU_LABEL ("Dett.")

#define STATUS_REFILL ("Riempimento!")
#define STATUS_WAIT_STATUS ("Attesa stato!")
#define STATUS_FULL ("Piena!")
#define STATUS_EMPTY ("Vuota!")

#define TIMER_START_LABEL ("Inizio: ")
#define TIMER_STOP_LABEL ("Fine:   ")

#define BACK_LABEL ("<< Indietro")

String getFormattedDate(int year, int month, int day, int dayOfWeek)  {
    // return String(year)+"/"+String(month)+"/"+String(day) + " (" + String(daysOfTheWeek[dayOfWeek]) + ") "+String(hour)+":"+String(minute)+":"+String(second);
    return String(day)+"/"+String(month)+ "/"+String(year);
}

String getFormattedTime(int hour, int minute, int second)  {
    // return String(year)+"/"+String(month)+"/"+String(day) + " (" + String(daysOfTheWeek[dayOfWeek]) + ") "+String(hour)+":"+String(minute)+":"+String(second);
    return String(hour)+":"+String(minute)+":"+String(second);
}

String getFormattedDateTime(int year, int month, int day, int dayOfWeek, int hour, int minute, int second)  {
    // return String(year)+"/"+String(month)+"/"+String(day) + " (" + String(daysOfTheWeek[dayOfWeek]) + ") "+String(hour)+":"+String(minute)+":"+String(second);
    return getFormattedDate(year, month, day, dayOfWeek) + " " + getFormattedTime(hour, minute, second);
}

#endif /* INCLUDE_ITA_H_ */
