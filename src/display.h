#ifndef _DISPLAY_H_
#define _DISPLAY_H_

/********************************************************************************
 Includes
 ********************************************************************************/
#include <stdbool.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <util/delay.h>

#include "../rf-common-lib/common/util.h"
#include "../rf-common-lib/PCD8544/Adafruit_PCD8544.h"

/********************************************************************************
 Macros and Defines
 ********************************************************************************/
#define MENU_MODE_ASCII  0
#define MENU_MODE_DEC    2
#define MENU_ITEMS       4

#define MENU_ITEM_1_TEXT "Send Data"
#define MENU_ITEM_1_CODE 0

#define MENU_ITEM_2_TEXT "Show D/A"
#define MENU_ITEM_2_CODE 1

#define MENU_ITEM_3_TEXT "Led ON/OFF"
#define MENU_ITEM_3_CODE 2

#define MENU_ITEM_4_TEXT "Exit"
#define MENU_ITEM_4_CODE 3

#define RF_PAYLOAD_SIZE  8

/********************************************************************************
 Global Variables
 ********************************************************************************/
extern uint8_t radio_channel;
extern uint16_t battery_level;
extern uint8_t radio_data[RF_PAYLOAD_SIZE];
extern uint16_t receive_count;
extern bool radio_has_data;
extern uint8_t menu_sel_item;
extern bool show_menu;
extern uint8_t rx_show_mode;

/********************************************************************************
 Function Prototypes
 ********************************************************************************/
void displayInit();
void displayLoop();
void displayWriteData();
void displayHasChanged();
void menuNext();
void menuPrev();
void displayChangeMode();
void displayOnOffBacklight();
void displayIdle();
void displayWake();

#endif /* _DISPLAY_H_ */
