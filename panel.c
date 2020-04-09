/*
 * K4815 Pattern Generator - Panel Controller
 *
 * Copyright 2010: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 * Version: 1.1	
 *
 * - LED matrix row data is LSB to the left
 * - DAC0 = CV/X out
 * - DAC1 = gate/Y out
 * - analog input assignments:
 *   - AN0 	- gate time pot			- channel 0
 *   - AN1 	- clock speed pot		- channel 1
 *   - AN2 	- motion length pot		- channel 2
 *   - AN3 	- density pot			- channel 3
 *   - AN4 	- output offset pot		- channel 4
 * - switch input assignments:
 *   - RA4 	- clock int/ext sw.		- channel 0
 *   - RB5 	- dir sw.				- channel 1
 *   - RB6 	- tonality sw.			- channel 2
 *   - RB7 	- span sw.				- channel 3
 *   - RB4 	- output mode sw.		- channel 4
 * - encoder inpu assignments:
 *   - RB1 	- motion enc phase A
 *   - RB2 	- motion enc phase B
 *   - RB3 	- motion reset push sw.
 * - clock control
 *   - RE0	- direction
 *   - RE1	- reset
 * 	 - RE2  - test input (active low)
 * - clock LED = RC2
 */
#include <system.h>
#include "panel.h"
#include "midi.h"

#ifdef EURORACK
#define CV_TEST_VAL 408  // +4.000V - test mode - eurorack
#define CV_ZERO_VAL 2040  // 0.000V - zero val - eurorack
#endif
#ifdef BUCHLA
#define CV_TEST_VAL 1935  // +5.000V - test mode - buchla
#define CV_ZERO_VAL 4085  // 0.000V - zero val - buchla
#endif

#define ENCODER_LOCKOUT_TIME 10
#define ENCODER_PHASE_IDLE 0
#define ENCODER_PHASE_UP1 1
#define ENCODER_PHASE_UP2 2
#define ENCODER_PHASE_DOWN1 3
#define ENCODER_PHASE_DOWN2 4

// hardware defines
#define DAC_CS portc.0
#define LED_CS portc.1
#define LED_COLS portd
#define CLOCK_LED portc.2
#define ENC_A portb.1
#define ENC_B portb.2
#define ENC_SW portb.3
#define CLOCK_INTEXT_SW porta.4
#define DIR_SW portb.5
#define TONALITY_SW portb.6
#define SPAN_SW portb.7
#define OUTPUT_MODE_SW portb.4
#define DIR_IN porte.0
#define RESET_IN porte.1
#define TEST_IN porte.2

// local variables
unsigned char panel_phase;			// the current panel phase
unsigned int dac0_val;				// current DAC0 value
unsigned int dac1_val;				// current DAC1 value
unsigned int dac0_val_new;			// desired DAC0 value
unsigned int dac1_val_new;			// desired DAC1 value
unsigned char dac_count;			// DAC counter of which DAC to output
unsigned char led_fg_row_count;		// foreground LED row counter
unsigned char led_fg_row_ctrl;		// foreground LED row bit
unsigned char led_bg_row_count;		// background LED row counter
unsigned char led_bg_row_ctrl;		// background LED row bit
unsigned char led_ol[8];			// overlay LED pixel data
unsigned int ol_timeout;			// a timer for showing the overlay data for n x 2 ms
unsigned char led_fg[8];			// foreground LED pixel data
unsigned char led_bg[8];			// background LED pixel data
unsigned char input_ch_count;		// input channel counter
unsigned char pot_in[5];			// pot input values
unsigned char sw_in[5];				// switch input values
unsigned char encoder_lockout;		// locks out the encoder
signed char encoder_pos;			// the encoder position since last read
unsigned char encoder_phase;		// encoder phase
unsigned char test_active;			// 1 = test mode active, 0 = test mode inactive
unsigned char test_counter;
unsigned char sustain_pulse_counter;  // counter to sustain level
#define POPUP_TIMEOUT 500

// local functions
void panel_spi_send(unsigned char);

// init the stuff
void panel_init(void) {
	unsigned char i;

	// set up the SPI port for talking to the DAC and LED register
	trisc.3 = 0;
	trisc.5 = 0;
	DAC_CS = 1;
	LED_CS = 1;
	LED_COLS = 0;
	sspstat = 0x80;
	sspcon1 = 0x32;
	panel_phase = 0;
	dac0_val_new = CV_ZERO_VAL;
	dac1_val_new = CV_ZERO_VAL;
	dac0_val = dac0_val_new + 1;
	dac1_val = dac1_val_new + 1;
	led_fg_row_count = 0;
	led_fg_row_ctrl = 1;
	led_bg_row_count = 0;
	led_bg_row_ctrl = 1;
	pir1.SSPIF = 0;
	sspbuf = 0x00; 
	// clear the framebuffers
	for(i = 0; i < 8; i ++) {
		led_ol[i] = 0x00;
		led_fg[i] = 0x00;
		led_bg[i] = 0x00;
	}
	ol_timeout = 0;
	// clear the panel inputs
	input_ch_count = 0;
	for(i = 0; i < 5; i ++) {
		pot_in[i] = 0;
		sw_in[i] = 0;
	}
	encoder_lockout = 255;
	encoder_pos = 0;
	encoder_phase = 0;
	test_active = 0;
	CLOCK_LED = 0;
	sustain_pulse_counter = 0;
}

// runs the task on a timer - every 256uS
void panel_timer_task(void) {
	unsigned char sw_temp, new_pot, old_pot;
	unsigned int tempi;

	// foreground / overlay LEDs only
	if((panel_phase & 0x07) < 6) {
		// turn off the data
		LED_COLS = 0;
		// load the new row
		LED_CS = 0;
		delay_us(1);
		panel_spi_send(led_fg_row_ctrl);
		LED_CS = 1;
		// overlay
		if(ol_timeout) {
			LED_COLS = led_ol[led_fg_row_count];			
		}
		// normal foreground
		else {
			LED_COLS = led_fg[led_fg_row_count];
		}
		// move to the next row
		led_fg_row_ctrl = (led_fg_row_ctrl >> 1);
		led_fg_row_count ++;
		if(led_fg_row_count == 8) {
			led_fg_row_count = 0;
			led_fg_row_ctrl = 0x80;
		}
	}
	// do all LEDs
	else {
		// turn off the data
		LED_COLS = 0;
		// load the new row
		LED_CS = 0;
		delay_us(1);
		panel_spi_send(led_bg_row_ctrl);
		LED_CS = 1;
		if(ol_timeout) {
			LED_COLS = (led_fg[led_bg_row_count] | 
				led_bg[led_bg_row_count] |
				led_ol[led_bg_row_count]);
		}
		else {
			LED_COLS = (led_fg[led_bg_row_count] | 
				led_bg[led_bg_row_count]);
		}
		// move to the next row
		led_bg_row_ctrl = (led_bg_row_ctrl >> 1);
		led_bg_row_count ++;
		if(led_bg_row_count == 8) {
			led_bg_row_count = 0;
			led_bg_row_ctrl = 0x80;
		}
	}

	// every 2048us
	// do the pot and switch inputs and overlay timeout
	if((panel_phase & 0x07) == 0) {
		if(!adcon0.GO) {
			// pots and switches
			adcon0.GO = 1;  // start the sampler
			while(adcon0.GO) clear_wdt();  // wait for the conversion
			if(input_ch_count == 0) sw_temp = !CLOCK_INTEXT_SW;
			else if(input_ch_count == 1) sw_temp = !DIR_SW;
			else if(input_ch_count == 2) sw_temp = !TONALITY_SW;
			else if(input_ch_count == 3) sw_temp = !SPAN_SW;
			else if(input_ch_count == 4) sw_temp = !OUTPUT_MODE_SW;
			else sw_temp = 0;
			sw_in[input_ch_count] = sw_temp;  // save the switch result
			// de-jitter the pot input
			new_pot = adresh;
			old_pot = pot_in[input_ch_count];
			if(new_pot > old_pot && (new_pot - old_pot > 2)) {
				pot_in[input_ch_count] = new_pot;
			}
			else if(new_pot < old_pot && (new_pot + old_pot > 2)) {		
				pot_in[input_ch_count] = new_pot;
			}
			else if(new_pot > 253) {
				pot_in[input_ch_count] = new_pot;
			}
			else if(new_pot < 2) {
				pot_in[input_ch_count] = new_pot;
			}
			input_ch_count ++;
			if(input_ch_count == 5) input_ch_count = 0;
			adcon0 &= 0xc3;
			adcon0 |= (input_ch_count & 0x07) << 2;
		}
		if(ol_timeout) {
			ol_timeout --;
		}

		// if test mode is becoming active
		if(!TEST_IN && test_active == 0) {
			test_active = 1;
			midi_set_learn_mode(1);
		}
		// if test mode is becoming inactive
		else if(TEST_IN && test_active == 1) {
			test_active = 0;
			midi_set_learn_mode(0);			
		}

		// do DACs
		if(dac_count & 0x01) {
			if(test_active) {
#ifdef EURORACK
				dac1_val_new = CV_TEST_VAL;
				dac1_val = dac1_val_new + 1;  // force an update
				panel_clear_ol();
				// show a "T"
				panel_draw_ol(0, 0xff);
				panel_draw_ol(1, 0xff);
				panel_draw_ol(2, 0x18);
				panel_draw_ol(3, 0x18);
				panel_draw_ol(4, 0x18);
				panel_draw_ol(5, 0x18);
				panel_draw_ol(6, 0x18);
				panel_draw_ol(7, 0x18);
				panel_set_ol_timeout(300);
#endif
#ifdef BUCHLA
				// no DAC test mode on buchla
				panel_clear_ol();
				// shown an "L"
				panel_draw_ol(0, 0x06);
				panel_draw_ol(1, 0x06);
				panel_draw_ol(2, 0x06);
				panel_draw_ol(3, 0x06);
				panel_draw_ol(4, 0x06);
				panel_draw_ol(5, 0x06);
				panel_draw_ol(6, 0x7e);
				panel_draw_ol(7, 0x7e);
				panel_set_ol_timeout(300);
#endif
			}
#ifdef BUCHLA
			// buchla gate pulse / sustain level switcher
			if(sustain_pulse_counter) {
				sustain_pulse_counter --;
				if(!sustain_pulse_counter) {
					dac1_val_new = PANEL_GATE_LEVEL_SUSTAIN;
				}
			}
#endif
			if(dac1_val != dac1_val_new) {
				dac1_val = dac1_val_new;
				DAC_CS = 0;
		//		delay_us(30);
				panel_spi_send(0xb0 | ((dac1_val >> 8) & 0x0f));
				delay_us(30);
				panel_spi_send(dac1_val & 0xff);
				delay_us(30);
				DAC_CS = 1;
			}
		}
		else {
			if(test_active) {
				dac0_val_new = CV_TEST_VAL;
				dac0_val = dac1_val_new + 1;  // force an update
			}
			if(dac0_val != dac0_val_new) {
				dac0_val = dac0_val_new;
				DAC_CS = 0;
		//		delay_us(30);
				panel_spi_send(0x30 | ((dac0_val >> 8) & 0x0f));
				delay_us(30);
				panel_spi_send(dac0_val & 0xff);
				delay_us(30);
				DAC_CS = 1;
			}
		}
		dac_count ++;
	}

	// every 256us
	// do the encoder input
	if(encoder_lockout) {
		encoder_lockout --;
	}
	else {
		switch(encoder_phase) {
			case ENCODER_PHASE_IDLE:  // idle phase
				if(!ENC_A) {
					encoder_phase = ENCODER_PHASE_UP1;
					encoder_lockout = ENCODER_LOCKOUT_TIME;
				}
				else if(!ENC_B) {
					encoder_phase = ENCODER_PHASE_DOWN1;
					encoder_lockout = ENCODER_LOCKOUT_TIME;					
				}
				break;
			case ENCODER_PHASE_UP1:  // up 1
				if(!ENC_A && !ENC_B) {
					encoder_phase = ENCODER_PHASE_UP2;
					encoder_lockout = ENCODER_LOCKOUT_TIME;					
				}
				break;
			case ENCODER_PHASE_DOWN1:  // down 1
				if(!ENC_A && !ENC_B) {
					encoder_phase = ENCODER_PHASE_DOWN2;
					encoder_lockout = ENCODER_LOCKOUT_TIME;					
				}
				break;
			case ENCODER_PHASE_UP2:  // up 2
				if(ENC_A && ENC_B) {
					encoder_pos += 1;
					encoder_phase = ENCODER_PHASE_IDLE;
					encoder_lockout = ENCODER_LOCKOUT_TIME;					
				}
				break;
			case ENCODER_PHASE_DOWN2:  // down 2
				if(ENC_A && ENC_B) {
					encoder_pos -= 1;
					encoder_phase = ENCODER_PHASE_IDLE;
					encoder_lockout = ENCODER_LOCKOUT_TIME;					
				}
				break;
		}	
	}

/*
	// every 256us
	// do the encoder input
	if(ENC_A && ENC_B) {
		if(encoder_lockout) encoder_lockout --;
	}
	if(!encoder_lockout) {
		if(!ENC_A) {
			encoder_lockout = 20;
			encoder_pos += 1;
		}
		else if(!ENC_B) {
			encoder_lockout = 20;
			encoder_pos -= 1;
		}
	}
*/	
	panel_phase ++;
}

// clear the LED overlay
void panel_clear_ol(void) {
	unsigned char i;
	// clear the framebuffer
	for(i = 0; i < 8; i ++) {
		led_ol[i] = 0x00;
	}
}

// clear the LED foreground
void panel_clear_fg(void) {
	unsigned char i;
	// clear the framebuffer
	for(i = 0; i < 8; i ++) {
		led_fg[i] = 0x00;
	}
}

// clear the LED background
void panel_clear_bg(void) {
	unsigned char i;
	// clear the framebuffer
	for(i = 0; i < 8; i ++) {
		led_fg[i] = 0x00;
	}
}

// turn off the panel before a reset
void panel_hard_stop(void) {
	LED_COLS = 0;
}


// write a row to the overlay
void panel_draw_ol(unsigned char row, unsigned char data) {
	if(row > 7) return;
	led_ol[row] = data;
}

// write a row to the foreground
void panel_draw_fg(unsigned char row, unsigned char data) {
	if(row > 7) return;
	led_fg[row] = data;
}

// write a row to the background
void panel_draw_bg(unsigned char row, unsigned char data) {
	if(row > 7) return;
	led_bg[row] = data;
}

// sets the overlay timeout
void panel_set_ol_timeout(unsigned int time) {
	ol_timeout = time;
}

// get an analog value
unsigned char panel_get_pot(unsigned char input) {
	if(input > 4) return 0;
	return pot_in[input];
}

// get a switch input
unsigned char panel_get_switch(unsigned char input) {
	if(input > 4) return 0;
	return sw_in[input];
}

// turn on the clock led
unsigned char panel_set_clock_led(unsigned char state) {
	if(state) CLOCK_LED = 1;
	else CLOCK_LED = 0;
}

// get the position of the encoder since the last read
// reading this resets the counter
signed char panel_get_encoder(void) {
	signed char temp;
	temp = encoder_pos;
	encoder_pos = 0;
	return temp;
}

// get the state of the encoder push switch
unsigned char panel_get_encoder_sw(void) {
	return !ENC_SW;
}

// get the state of the direction input - 1 = reverse current dir
unsigned char panel_get_dir_in(void) {
	return !DIR_IN;
}

// get the state of the reset input - 1 = reset
unsigned char panel_get_reset_in(void) {
	return !RESET_IN;
}

// send a byte on the SPI bus and wait it to be sent
void panel_spi_send(unsigned char data) {
	pir1.SSPIF = 0;
	sspbuf = data;
	while(!pir1.SSPIF) clear_wdt();
}

// set the dac0 value
void panel_set_dac0(unsigned int val) {
	dac0_val_new = val;
}

// set the dac1 value
void panel_set_dac1(unsigned int val) {
	dac1_val_new = val;
}

#ifdef BUCHLA
// set the pulse timeout in ms x 2
void panel_set_dac1_gate_pulse_len(unsigned char timeout) {
	sustain_pulse_counter = timeout;
}
#endif

// set a popup number
void panel_set_popup_num(unsigned char num) {
	unsigned char i, temp;
	temp = 0;	
	for(i = 0; i < 8; i ++) {
		if(num > temp + 7) {
			panel_draw_ol(i, 0xff);
		}
		else if(num > temp + 6) {
			panel_draw_ol(i, 0x7f);
		}
		else if(num > temp + 5) {
			panel_draw_ol(i, 0x3f);
		}
		else if(num > temp + 4) {
			panel_draw_ol(i, 0x1f);
		}
		else if(num > temp + 3) {
			panel_draw_ol(i, 0x0f);
		}
		else if(num > temp + 2) {
			panel_draw_ol(i, 0x07);
		}
		else if(num > temp + 1) {
			panel_draw_ol(i, 0x03);
		}
		else if(num > temp + 0) {
			panel_draw_ol(i, 0x01);
		}
		else {
			panel_draw_ol(i, 0x00);
		}
		temp += 8;
	}


	ol_timeout = POPUP_TIMEOUT;
}

