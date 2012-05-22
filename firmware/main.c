/* Name: main.c
 * Project: hid-custom-rq example
 * Author: Christian Starkjohann
 * Creation Date: 2008-04-07
 * Tabsize: 4
 * Copyright: (c) 2008 by OBJECTIVE DEVELOPMENT Software GmbH
 * License: GNU GPL v2 (see License.txt), GNU GPL v3 or proprietary (CommercialLicense.txt)
 * This Revision: $Id: main.c 790 2010-05-30 21:00:26Z cs $
 */

/*
This example should run on most AVRs with only little changes. No special
hardware resources except INT0 are used. You may have to change usbconfig.h for
different I/O pins for USB. Please note that USB D+ must be the INT0 pin, or
at least be connected to INT0 as well.
We assume that an LED is connected to port B bit 0. If you connect it to a
different port or bit, change the macros below:
*/
#define LED_PORT_DDR        DDRB
#define LED_PORT_OUTPUT     PORTB
#define PTT_LED_BIT         0
#define POWER_LED_BIT       1
#define STATUS_LED_BIT      2

/* Define Button Port and Pins */
#define BUTTON_PORT PORTC
#define BUTTON_PIN PINC
#define BUTTON_PORT1 PORTB
#define BUTTON_PIN1 PINB
#define BUTTON_PORT2 PORTD
#define BUTTON_PIN2 PIND

/* Define Button Bits */
#define BUTTON_BIT1 PC0
#define BUTTON_BIT2 PC1
#define BUTTON_BIT3 PC2
#define BUTTON_BIT4 PC3
#define BUTTON_BIT5 PC4
#define BUTTON_BIT6 PC5
#define BUTTON_BIT7 PB0
#define BUTTON_BIT8 PD7


#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>  /* for sei() */
#include <util/delay.h>     /* for _delay_ms() */

#include <avr/pgmspace.h>   /* required by usbdrv.h */
#include <stdio.h>
#include <stdint.h>
#include "usbdrv.h"
#include "oddebug.h"        /* This is also an example for using debug macros */
#include "requests.h"       /* The custom request numbers we use */

/* ------------------------------------------------------------------------- */
/* ----------------------------- USB interface ----------------------------- */
/* ------------------------------------------------------------------------- */

PROGMEM char usbHidReportDescriptor[22] = {    /* USB report descriptor */
    0x06, 0x00, 0xff,              // USAGE_PAGE (Generic Desktop)
    0x09, 0x01,                    // USAGE (Vendor Usage 1)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //   LOGICAL_MAXIMUM (255)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x09, 0x00,                    //   USAGE (Undefined)
    0xb2, 0x02, 0x01,              //   FEATURE (Data,Var,Abs,Buf)
    0xc0                           // END_COLLECTION
};
/* The descriptor above is a dummy only, it silences the drivers. The report
 * it describes consists of one byte of undefined data.
 * We don't transfer our data through HID reports, we use custom requests
 * instead.
 */

/* ------------------------------------------------------------------------- */

uint16_t ReadADC(uint8_t __channel)
{
   ADMUX &= 0xE0;
   ADMUX |= __channel;                // Channel selection
   ADCSRA |= _BV(ADSC);               // Start conversion
   while(!bit_is_set(ADCSRA,ADIF));   // Loop until conversion is complete
   ADCSRA |= _BV(ADIF);               // Clear ADIF by writing a 1 (this sets the value to 0)
 
   return(ADC);

   

}

void init_io() 
{
	LED_PORT_DDR |= _BV(STATUS_LED_BIT);   /* make the LED bit an output */
	LED_PORT_DDR |= _BV(POWER_LED_BIT);   /* make the LED bit an output */
	/* turn on internal pull-up resistor for the switch */
	BUTTON_PORT  |= _BV(BUTTON_BIT1);
	BUTTON_PORT  |= _BV(BUTTON_BIT2);
	BUTTON_PORT  |= _BV(BUTTON_BIT3);
	BUTTON_PORT  |= _BV(BUTTON_BIT4);
	BUTTON_PORT  |= _BV(BUTTON_BIT5);
	BUTTON_PORT  |= _BV(BUTTON_BIT6);
	BUTTON_PORT1 |= _BV(BUTTON_BIT7);
	BUTTON_PORT2 |= _BV(BUTTON_BIT8);
}

void adc_init()
{
       // ADCSRA = _BV(ADEN) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0); //Enable ADC and set 128 prescale
 uint16_t result;
 
  ADMUX = (0<<REFS1) | (1<<REFS0);      // AVcc als Referenz benutzen
  //ADMUX = (1<<REFS1) | (1<<REFS0);      // interne Referenzspannung nutzen
  // Bit ADFR ("free running") in ADCSRA steht beim Einschalten
  // schon auf 0, also single conversion
  ADCSRA = (1<<ADPS1) | (1<<ADPS0);     // Frequenzvorteiler
  ADCSRA |= (1<<ADEN);                  // ADC aktivieren
 
  /* nach Aktivieren des ADC wird ein "Dummy-Readout" empfohlen, man liest
     also einen Wert und verwirft diesen, um den ADC "warmlaufen zu lassen" */
 
  ADCSRA |= (1<<ADSC);                  // eine ADC-Wandlung 
  while (ADCSRA & (1<<ADSC) ) {}        // auf Abschluss der Konvertierung warten
  /* ADCW muss einmal gelesen werden, sonst wird Ergebnis der nächsten
     Wandlung nicht übernommen. */
  result = ADCW;
}


usbMsgLen_t usbFunctionSetup(uchar data[8])
{
usbRequest_t    *rq = (void *)data;

    if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_VENDOR){
        DBG1(0x50, &rq->bRequest, 1);   /* debug output: print our request */
        if(rq->bRequest == RQ_SET_PTT_LED){
            if(rq->wValue.bytes[0] & 1){    /* set LED */
                LED_PORT_OUTPUT |= _BV(PTT_LED_BIT);
            }else{                          /* clear LED */
                LED_PORT_OUTPUT &= ~_BV(PTT_LED_BIT);
            }	
		}else if(rq->bRequest == RQ_SET_STATUS_LED){
            if(rq->wValue.bytes[0] & 1){    /* set LED */
				LED_PORT_OUTPUT |= _BV(STATUS_LED_BIT);
            }else{                          /* clear LED */
				LED_PORT_OUTPUT &= ~_BV(STATUS_LED_BIT);
            }	
		}else if(rq->bRequest == RQ_SET_POWER_LED){
            if(rq->wValue.bytes[0] & 1){    /* set LED */
				LED_PORT_OUTPUT |= _BV(POWER_LED_BIT);
            }else{                          /* clear LED */
				LED_PORT_OUTPUT &= ~_BV(POWER_LED_BIT);
            }				
        }else if(rq->bRequest == CUSTOM_RQ_GET_STATUS){
            static uchar dataBuffer[1];     /* buffer must stay valid when usbFunctionSetup returns */
            dataBuffer[0] = ((LED_PORT_OUTPUT & _BV(PTT_LED_BIT)) != 0);
            usbMsgPtr = dataBuffer;         /* tell the driver which data to return */
            return 1;                       /* tell the driver to send 1 byte */
        }else if(rq->bRequest == CUSTOM_RQ_GET_POTI) {
			static uchar dataBuffer[16];     /* buffer must stay valid when usbFunctionSetup returns */
			sprintf(dataBuffer,"%d   ", ReadADC(0));
			usbMsgPtr = dataBuffer;         /* tell the driver which data to return */
            return 16;//sizeof(dataBuffer);      /* tell the driver to send 1 byte */
		}else if(rq->bRequest == RQ_GET_POTI_CHANNEL){
		
			static uchar dataBuffer[16];     /* buffer must stay valid when usbFunctionSetup returns */
			/* the button is pressed when BUTTON_BIT is clear */
			sprintf(dataBuffer,"%d   ", 0);
			switch (rq->wValue.bytes[0]) {
				case 0:
					if (bit_is_clear(BUTTON_PIN, BUTTON_BIT1)) sprintf(dataBuffer,"%d   ", 1);
					break;
				case 1:
					if (bit_is_clear(BUTTON_PIN, BUTTON_BIT2)) sprintf(dataBuffer,"%d   ", 1);
					break;
				case 2:
					if (bit_is_clear(BUTTON_PIN, BUTTON_BIT3)) sprintf(dataBuffer,"%d   ", 1);
					break;
				case 3:
					if (bit_is_clear(BUTTON_PIN, BUTTON_BIT4)) sprintf(dataBuffer,"%d   ", 1);
					break;
				case 4:
					if (bit_is_clear(BUTTON_PIN, BUTTON_BIT5)) sprintf(dataBuffer,"%d   ", 1);
					break;
				case 5:
					if (bit_is_clear(BUTTON_PIN, BUTTON_BIT6)) sprintf(dataBuffer,"%d   ", 1);
					break;
				case 6:
					if (bit_is_clear(BUTTON_PIN1, BUTTON_BIT7)) sprintf(dataBuffer,"%d   ", 1); 
					break;
				case 7:
					if (bit_is_clear(BUTTON_PIN2, BUTTON_BIT8)) sprintf(dataBuffer,"%d   ", 1);
					break;
				default:
					sprintf(dataBuffer,"%d   ", 0);
					break;
			}
			//if (bit_is_clear(BUTTON_PIN, BUTTON_BIT1))
			//{
				//delay_ms(DEBOUNCE_TIME);
				
			//}
			usbMsgPtr = dataBuffer;         /* tell the driver which data to return */
            return 16;//sizeof(dataBuffer);      /* tell the driver to send 1 byte */
		}else if(rq->bRequest == RQ_GET_POTI_CHANNEL1){
			adc_init();
			_delay_ms(10);
			static uchar dataBuffer[16];     /* buffer must stay valid when usbFunctionSetup returns */
			sprintf(dataBuffer,"%d   ", ReadADC(MUX0));
			usbMsgPtr = dataBuffer;         /* tell the driver which data to return */
            return 16;//sizeof(dataBuffer);      /* tell the driver to send 1 byte */
		}else if(rq->bRequest == RQ_GET_POTI_CHANNEL2){
			adc_init();
			_delay_ms(10);
			static uchar dataBuffer[16];     /* buffer must stay valid when usbFunctionSetup returns */
			sprintf(dataBuffer,"%d   ", ReadADC(MUX1));
			usbMsgPtr = dataBuffer;         /* tell the driver which data to return */
            return 16;//sizeof(dataBuffer);      /* tell the driver to send 1 byte */
		}
    }else{
        /* calss requests USBRQ_HID_GET_REPORT and USBRQ_HID_SET_REPORT are
         * not implemented since we never call them. The operating system
         * won't call them either because our descriptor defines no meaning.
         */
    }
    return 0;   /* default for not implemented requests: return no data back to host */
}

/* ------------------------------------------------------------------------- */

int __attribute__((noreturn)) main(void)
{
uchar   i;
	//adc_init();
	init_io();
    wdt_enable(WDTO_1S);
    /* Even if you don't use the watchdog, turn it off here. On newer devices,
     * the status of the watchdog (on/off, period) is PRESERVED OVER RESET!
     */
    /* RESET status: all port bits are inputs without pull-up.
     * That's the way we need D+ and D-. Therefore we don't need any
     * additional hardware initialization.
     */
    odDebugInit();
    DBG1(0x00, 0, 0);       /* debug output: main starts */
    usbInit();
    usbDeviceDisconnect();  /* enforce re-enumeration, do this while interrupts are disabled! */
    i = 0;
    while(--i){             /* fake USB disconnect for > 250 ms */
        wdt_reset();
        _delay_ms(1);
    }
    usbDeviceConnect();
    //LED_PORT_DDR |= _BV(PTT_LED_BIT);   /* make the LED bit an output */
	LED_PORT_OUTPUT |= _BV(POWER_LED_BIT);
    sei();
    DBG1(0x01, 0, 0);       /* debug output: main loop starts */
    for(;;){                /* main event loop */
#if 0   /* this is a bit too aggressive for a debug output */
        DBG2(0x02, 0, 0);   /* debug output: main loop iterates */
#endif
        wdt_reset();
        usbPoll();
    }
}

/* ------------------------------------------------------------------------- */
