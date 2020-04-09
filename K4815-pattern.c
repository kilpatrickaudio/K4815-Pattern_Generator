/*
 * K4815 Pattern Generator
 *
 * Copyright 2010: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 * Version: 1.1
 *
 * Hardware I/O:
 *
 *  RA0/AN0		- gate time pot			- analog
 *  RA1/AN1		- clock speed pot		- analog
 *  RA2/AN2 	- motion length pot		- analog
 *  RA3/AN3		- density pot			- analog
 *  RA4			- clock int/ext sw.		- input
 *  RA5/AN4		- output offset pot		- analog
 *  RA6			- crystal
 *  RA7			- crystal
 *
 *  RB0/INT0	- clock input			- input - interrupt
 *  RB1			- motion enc phase A	- input - active low
 *  RB2			- motion enc phase B	- input - active low
 *  RB3			- motion reset push sw.	- input - active low
 *  RB4/AN11	- output mode sw.		- input
 *  RB5			- dir sw.		 		- input
 *  RB6 		- tonality sw.			- input, program clock
 *	RB7			- span sw.				- input, program data
 *
 *  RC0			- DAC !CS				- output - active low
 *  RC1			- LED load				- output - rising edge triggered
 *  RC2			- clock LED				- PWM output
 *  RC3			- SPI CLK				- SPI
 *  RC4			- SPI MISO (unused)		- SPI
 *  RC5			- SPI MOSI				- SPI
 *  RC6			- MIDI TX				- USART
 *  RC7			- MIDI RX				- USART
 *
 *  RD0			- LED matrix col 0		- output - active high
 *  RD1			- LED matrix col 1		- output - active high
 *  RD2			- LED matrix col 2		- output - active high
 *  RD3			- LED matrix col 3		- output - active high
 *  RD4			- LED matrix col 4		- output - active high
 *  RD5			- LED matrix col 5		- output - active high
 *  RD6			- LED matrix col 6		- output - active high
 *  RD7			- LED matrix col 7		- output - active high
 *
 *  RE0/AN5		- direction input		- input - active low
 *  RE1/AN6		- reset input			- input - active low
 *  //  RE0/AN5		- direction input		- analog input - input range hack
 *  //  RE1/AN6		- reset input			- analog input - input range hack
 *  RE2/AN7		- test input			- input - active low
 *  RE3			-
 */
#include <system.h>
#include "panel.h"
#include "seq.h"
#include "sysex.h"
#include "midi.h"
#include "pattern_midi.h"
#include "clock_ctrl.h"
#include "config_store.h"

// master clock frequency
#pragma CLOCK_FREQ 32000000

// PIC18F4520 config fuses
//#pragma DATA 	_CONFIG1H, 0x08  // internal osc, RA6,7 IO
#pragma DATA 	_CONFIG1H, 0x06  // HS + PLL 
#pragma DATA 	_CONFIG2L, 0x00  // BOR disable, PWRTEN enable
#pragma DATA 	_CONFIG2H, 0x0f  // WDT disable
#pragma DATA 	_CONFIG3H, 0x03  // MCLR disable, PORTB digital
#pragma DATA 	_CONFIG4L, 0x81  // DEBUG disable, LVP disable, STVREN enable
#pragma DATA 	_CONFIG5L, 0xff
#pragma DATA 	_CONFIG5H, 0xff
#pragma DATA 	_CONFIG6L, 0xff
#pragma DATA 	_CONFIG6H, 0xff
#pragma DATA 	_CONFIG7L, 0xff
#pragma DATA 	_CONFIG7H, 0xff

unsigned char task_div;

// main!
void main(void) {
	// analog inputs
	adcon0 = 0x81;
	adcon1 = 0x0a;  // internal Vref, AN0-4 analog inputs
	adcon2 = 0x02;

	// I/O pins
	trisa = 0xff;
	porta = 0x00;
	trisb = 0xff;
	portb = 0x00;

	trisc = 0xc0;
	portc = 0x00;
	trisd = 0x00;
	portd = 0x00;
	trise = 0x07;
	porte = 0x00;

	// set up USART
	trisc.6 = 0;
	trisc.7 = 1;
	spbrg = 63;  // 31250bps with 32MHz oscillator
	txsta.BRGH = 1; // high speed
	txsta.SYNC = 0;  // async mode
	rcsta.SPEN = 1;  // serial port enable
	pie1.RCIE = 0;  // no interrupts
	rcsta.RX9 = 0;  // 8 bit reception
	txsta.TX9 = 0;  // 8 bit transmit
	rcsta.CREN = 1;  // enable receiver
	txsta.TXEN = 1; // enable transmit

	// timer 1 - task timer - 1us per count
	t1con = 0x31;
	tmr1h = 0xff;
	tmr1l = 0x00;
	task_div = 0;

	// set up modules
	config_store_init();
	panel_init();
	seq_init();
	pattern_midi_init();
	midi_init(0x41);
	sysex_init();
	clock_ctrl_init();

	// set up interrupts
	intcon2.INTEDG0 = 0;  // needed for transistor INT input

	intcon = 0x00;
	intcon.PEIE = 1;
	pie1.TMR1IE = 1;
	pie1.RCIE = 1;
	intcon.INT0IE = 1;
	intcon.TMR0IE = 1;
	intcon.GIE = 1;

	while(1) {
		clear_wdt();
		midi_tx_task();
	}
}

// interrupt
void interrupt(void) {
	// external clock input
	if(intcon.INT0IF) {
		intcon.INT0IF = 0;
		clock_ctrl_ext_pulse();
	}

 	// timer 1 task timer - 256us interval
	if(pir1.TMR1IF) {
		pir1.TMR1IF = 0;
		tmr1h = 0xff;
		tmr1l = 0x00;
		panel_timer_task();
		midi_rx_task();
		task_div ++;
		// call this stuff every 1024us
		if(task_div == 4) {
			task_div = 0;
			clock_ctrl_timer_task();
			seq_timer_task();
			config_store_timer_task();
		}
	}

	// timer 0 - clock pulse timer
	if(intcon.T0IF) {
		intcon.T0IF = 0;
		clock_ctrl_int();
	}

	// MIDI receive
	if(pir1.RCIF) {
		midi_rx_byte(rcreg);
		// clear errors
		if(rcsta.FERR || rcsta.OERR) {
			rcsta.CREN = 0;
			rcsta.CREN = 1;
		}
	}
}

