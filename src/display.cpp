/********************************************************************************
 Includes
 ********************************************************************************/
#include "display.h"

/********************************************************************************
 LCD Layout
*********************************************************************************
**12345678901234
****************
1*F=2.345Mhz 100
2*B=3.0V
3*
4*
5*
6*
7*
*/

/********************************************************************************
 Global Variables
 ********************************************************************************/
bool refreshLCD = true;
bool show_menu = false;
bool enableBackLight = false;
uint8_t rx_show_mode = MENU_MODE_DEC;
uint8_t menu_sel_item = 0;

Port _DIN(&DDRC, &PORTC, PC4);
Port _SCKL(&DDRC, &PORTC, PC5);
Port _DC(&DDRC, &PORTC, PC3);
Port _RST(&DDRB, &PORTB, PB0);
Port _CS(&DDRC, &PORTC, PC2);

Adafruit_PCD8544_Ports pcd8544Ports(&_DIN, &_SCKL, &_DC, &_RST, &_CS);
Adafruit_PCD8544 display(&pcd8544Ports);

char buffer[100];

const char items[MENU_ITEMS][14] = {MENU_ITEM_1_TEXT, MENU_ITEM_2_TEXT, MENU_ITEM_3_TEXT, MENU_ITEM_4_TEXT};

/********************************************************************************
 Functions
 ********************************************************************************/
void displayInit() {
    display.begin();

    display.setRotation(2);
    display.setContrast(55);

    display.display(); // show splashscreen
    _delay_ms(1000);
}

void displayLoop() {
    if (refreshLCD) {
        refreshLCD = false;
        displayWriteData();
    }
}

void displayWriteData() {
    display.clearDisplay();

    sprintf(buffer, "F=%dMhz  %03dB=%d P=%05d",
            (2400 + radio_channel),
            radio_channel,
            battery_level,
            receive_count);

    display.textWrap(buffer, 0, 0);

    if (show_menu) {

        for (uint8_t i = 0; i < MENU_ITEMS; i++) {
            if (i == 0) {
                sprintf(buffer, "%s%s\n", ( (menu_sel_item == i) ? "->" : "  " ), items[i]);
            } else {
                sprintf(buffer, "%s%s%s\n", buffer, ( (menu_sel_item == i) ? "->" : "  " ), items[i]);
            }
        }

        display.textWrap(buffer, 0, 16);

    } else if (radio_has_data) {
        if (rx_show_mode == MENU_MODE_ASCII) {
            display.setCursor(0, 16);

            for(uint8_t i = 0; i < RF_PAYLOAD_SIZE; i++) {
                display.write(radio_data[i]);
            }

        } else if (rx_show_mode == MENU_MODE_DEC) {
            sprintf(buffer, "%d", radio_data[0]);
            for(uint8_t i = 1; i < RF_PAYLOAD_SIZE; i++) {
                sprintf(buffer, "%s %d", buffer, radio_data[i]);
            }

            display.textWrap(buffer, 0, 16);
        }
    }

    display.display();
}

void displayHasChanged() {
    refreshLCD = true;
}

void menuNext() {
    if (menu_sel_item == (MENU_ITEMS - 1)) {
        menu_sel_item = 0;
    } else {
        menu_sel_item++;
    }
}

void menuPrev() {
    if (menu_sel_item > 0) {
        menu_sel_item--;
    } else {
        menu_sel_item = MENU_ITEMS - 1;
    }
}

void displayChangeMode() {
    if (rx_show_mode == MENU_MODE_ASCII) {
        rx_show_mode = MENU_MODE_DEC;
    } else {
        rx_show_mode = MENU_MODE_ASCII;
    }
}

void displayOnOffBacklight() {
    enableBackLight = !enableBackLight;

    if (enableBackLight) {
        _off(PC1, PORTC);
    } else {
        _on(PC1, PORTC);
    }
}

void displayIdle() {
    _on(PC1, PORTC);
    display.sleep();
}

void displayWake() {
    if (enableBackLight) {
        _off(PC1, PORTC);
    }
    display.wake();
    displayHasChanged();
}
