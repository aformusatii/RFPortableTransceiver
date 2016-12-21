/********************************************************************************
 Includes
 ********************************************************************************/
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <util/delay.h>

#include "../rf-common-lib/nrf24l01/RF24.h"
#include "../rf-common-lib/atmega328/mtimer.h"
#include "../rf-common-lib/common/util.h"

#include "display.h"

extern "C" {
#include "../rf-common-lib/atmega328/usart.h"
}

/********************************************************************************
 Macros and Defines
 ********************************************************************************/
#define DEFAULT_CHANNEL  3
#define IDLE_TIMEOUT     10

/********************************************************************************
 Function Prototypes
 ********************************************************************************/
void initGPIO();
void initRadio();
uint16_t adc_read(uint8_t adcx);
void read_battery_level();
void readRxRadio();
void setChannel(uint8_t channel);
void readRadioData();
void sendRadioData();
void checkButton();
void checkJobs();
void powerUpAVR();
void powerDownAVR();
void resetIdleTimer();

/********************************************************************************
 Global Variables
 ********************************************************************************/
uint8_t radio_channel = DEFAULT_CHANNEL;
uint16_t battery_level = 0;
bool rx_received = false;
uint16_t receive_count = 0;
bool radio_has_data = false;
volatile uint64_t idleTimerCicles = 0;

volatile bool button_0_pressed = false;
volatile bool button_1_pressed = false;
volatile bool button_2_pressed = false;

HardwarePlatform radioHP(&DDRB, &PORTB, PB1, PB2);

RF24 radio(&radioHP);
const uint64_t pipes[2] = { 0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL };

uint8_t radio_data[RF_PAYLOAD_SIZE];

/********************************************************************************
 Interrupt Service
 ********************************************************************************/
ISR(USART_RX_vect) {
    handle_usart_interrupt();
}

ISR(TIMER1_OVF_vect) {
    incrementOvf();
}

ISR(TIMER2_OVF_vect) {
    _NOP();
}

ISR(INT0_vect)
{
    _NOP();
}

ISR(INT1_vect)
{
    rx_received = true;
}
/********************************************************************************
 Main
 ********************************************************************************/
int main(void) {
    // initialize code
    usart_init();

    // initialize Timer 1
    initTimer1();

    // init GPIO
    initGPIO();

    // enable interrupts
    sei();

    // Configure Sleep Mode - Power-save
    SMCR = (0 << SM2) | (1 << SM1) | (1 << SM0) | (0 << SE);

    // Output initialization log
    printf("Start...");
    printf(CONSOLE_PREFIX);

    // Init Radio Module
    initRadio();

    // Init LCD display
    displayInit();

    //powerDownAVR();

    idleTimerCicles = convertSecondsToCicles(IDLE_TIMEOUT);

    // main loop
    while (1) {
        checkJobs();

        // main usart loop
        usart_check_loop();

        // sleep_mode();
        displayLoop();

        read_battery_level();

        readRadioData();

        checkButton();
    }
}

/********************************************************************************
 Functions
 ********************************************************************************/
void initRadio() {
    radio.begin();
    radio.setRetries(15, 15);
    radio.setPayloadSize(RF_PAYLOAD_SIZE);
    radio.setPALevel(RF24_PA_HIGH);
    radio.setChannel(radio_channel);

    radio.openWritingPipe(pipes[0]);
    radio.openReadingPipe(1, pipes[1]);

    radio.printDetails();

    _delay_ms(10);

    // need to flush tx buffer, fixed the issue with packet shift...
    radio.startListening();

    _delay_ms(10);
}

void handle_usart_cmd(char *cmd, char* args[], uint8_t arg_count) {
    if (strcmp(cmd, "test") == 0) {
        for (uint8_t i = 0; i < arg_count; i++) {
            printf("\n ARG[%d]=[%s]", i, args[i]);
        }
    }

    if (strcmp(cmd, "channel") == 0) {
        if (arg_count > 0) {
            setChannel(atoi(args[0]));
            log_info("set channel");
            displayHasChanged();
        }
    }

    if (strcmp(cmd, "q") == 0) {

        if ( show_menu && (menu_sel_item != MENU_ITEM_4_CODE) ) {
            switch (menu_sel_item) {
                case MENU_ITEM_1_CODE:
                    sendRadioData();
                    break;
                case MENU_ITEM_2_CODE:
                    displayChangeMode();
                    break;
                case MENU_ITEM_3_CODE:
                    displayOnOffBacklight();
                    break;
            }
        }

        show_menu = !show_menu;
        displayHasChanged();
    }

    if (strcmp(cmd, "w") == 0) {
        if (show_menu) {
            menuNext();
        } else {
            setChannel(radio_channel + 1);
        }

        displayHasChanged();
    }

    if (strcmp(cmd, "e") == 0) {
        if (show_menu) {
            menuPrev();
        } else {
            setChannel(radio_channel - 1);
        }

        displayHasChanged();
    }

}

void initGPIO() {
    _in(DDD2, DDRD); // INT0 input - button
    _in(DDD3, DDRD); // INT1 input - NRF24L01+ IRQ

    _in(DDC0, DDRC); // Analog input 0
    _out(DDC1, DDRC); // LCD back-light
    _on(PC1, PORTC); // Default disable back-light

    // GPIO Interrupt INT0 & INT1
    // The falling edge of INT0 & INT1 generates an interrupt request.
    EICRA = (1<<ISC11)|(0<<ISC10)|(1<<ISC01)|(0<<ISC00);

    // Enable INT0 * INT1
    EIMSK = (1<<INT1)|(1<<INT0);

    _in(DDD6, DDRD); // button 1
    _in(DDD7, DDRD); // button 2

    _off(PD2, PORTD);
    _off(PD6, PORTD);
    _off(PD7, PORTD);
}

uint16_t adc_read(uint8_t adcx) {
    /* adcx is the analog pin we want to use.  ADMUX's first few bits are
     * the binary representations of the numbers of the pins so we can
     * just 'OR' the pin's number with ADMUX to select that pin.
     * We first zero the four bits by setting ADMUX equal to its higher
     * four bits. */
    ADMUX = adcx;
    ADMUX |= (1 << REFS1) | (1 << REFS0) | (0 << ADLAR);

    _delay_us(300);

    /* This starts the conversion. */
    ADCSRA |= _BV(ADSC);

    /* This is an idle loop that just wait around until the conversion
     * is finished.  It constantly checks ADCSRA's ADSC bit, which we just
     * set above, to see if it is still set.  This bit is automatically
     * reset (zeroed) when the conversion is ready so if we do this in
     * a loop the loop will just go until the conversion is ready. */
    while ((ADCSRA & _BV(ADSC)))
        ;

    /* Finally, we return the converted value to the calling function. */
    return ADC;
}

void read_battery_level() {
    // enable pull-up resistor for ADC battery level
    _on(PC0, PORTC);

    // Enable the ADC
    ADCSRA |= _BV(ADEN);
    _delay_ms(5);

    uint16_t new_battery_level = adc_read(MUX0);

    // If battery level has changed refresh it on display
    if (battery_level != new_battery_level) {
        displayHasChanged();
    }

    battery_level = new_battery_level;

    // Disable ADC
    ADCSRA &= ~_BV(ADEN);

    // disable pull-up resistor for ADC battery level
    _off(PC0, PORTC);
}

void readRxRadio() {
    bool tx_ok, tx_fail, rx_ok;
    radio.whatHappened(tx_ok, tx_fail, rx_ok);

    if (rx_ok) {
        radio.read(radio_data, RF_PAYLOAD_SIZE);
        radio.flush_rx();

        log_inline("DATA,");

        for (uint8_t i = 0; i < RF_PAYLOAD_SIZE; i++) {
            log_inline("%d,", radio_data[i]);
        }

        log_inline("DATA\n");

        receive_count++;
        radio_has_data = true;

    } else {
        log_error("RX is not ok");
    }
}

void setChannel(uint8_t channel) {
    if (channel == 126) {
        channel = 0;
    } else if (channel > 126) {
        channel = 125;
    }

    if (radio_channel != channel) {
        radio_has_data = false;
        receive_count = 0;
        radio_channel = channel;
        radio.setChannel(radio_channel);
        radio.stopListening();
        radio.startListening();
    }
}

void readRadioData() {
    if (rx_received) {
        rx_received = false;
        readRxRadio();
        displayHasChanged();
    }
}

void sendRadioData() {

}

void checkButton() {
    // ===== check button 0 =====
    if (_check(PD2, PIND)) {
        if (!button_0_pressed) {
            //button_0_pressed = true;
            if (show_menu && (menu_sel_item != MENU_ITEM_4_CODE)) {
                switch (menu_sel_item) {
                case MENU_ITEM_1_CODE:
                    sendRadioData();
                    break;
                case MENU_ITEM_2_CODE:
                    displayChangeMode();
                    break;
                case MENU_ITEM_3_CODE:
                    displayOnOffBacklight();
                    break;
                }
            }

            show_menu = !show_menu;
            displayHasChanged();
        }
    } else {
        button_0_pressed = false;
    }

    // ===== check button 1 =====
    if (_check(PD6, PIND)) {
        if (!button_1_pressed) {
            //button_1_pressed = true;
            if (show_menu) {
                menuNext();
            } else {
                setChannel(radio_channel + 1);
            }

            displayHasChanged();
        }
    } else {
        button_1_pressed = false;
    }

    // ===== check button 2 =====
    if (_check(PD7, PIND)) {
        if (!button_2_pressed) {
            //button_2_pressed = true;
            if (show_menu) {
                menuPrev();
            } else {
                setChannel(radio_channel - 1);
            }

            displayHasChanged();
        }
    } else {
        button_2_pressed = false;
    }

    if (PIND & (1 << PD2 | 1 << PD6 | 1 << PD7)) {
        _delay_ms(100);
        resetIdleTimer();
    }
}

void checkJobs() {
    uint64_t currentTimeCicles = getCurrentTimeCicles();

    if ((idleTimerCicles != 0) && (currentTimeCicles >= idleTimerCicles)) {

        log_inline("sleep");
        powerDownAVR();
        _delay_ms(50);

        sleep_mode();

        _delay_ms(50);
        log_inline("wake");
        powerUpAVR();
        resetIdleTimer();
    }
}

void powerUpAVR() {
    radio.powerUp();
    displayWake();

    // Wait a little after wake up
    _delay_ms(10);
}

void powerDownAVR() {
    radio.powerDown();
    displayIdle();

    // Wait a little before going to sleep again
    _delay_ms(10);
}

void resetIdleTimer() {
    idleTimerCicles = convertSecondsToCicles(IDLE_TIMEOUT);
}
