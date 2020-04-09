/*
 * K4815 Pattern Generator - Clock Controller
 *
 * Copyright 2010: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 * Version: 1.2
 *
 */
#include <system.h>
#include "clock_ctrl.h"
#include "panel.h"
#include "seq.h"
#include "midi.h"

// clock stuff
#define CLOCK_LED_TIME 10
unsigned char clock_led_timeout;
#define CLOCK_HOLDOFF_TIME 10  // ~10ms at 1024us per count
unsigned char clock_tick_count;	// the current subtick - 0-23
unsigned int clock_interval;	// the length of the clock for internal mode
unsigned char clock_int;		// 0 = external, 1 = internal
unsigned int clockin_holdoff;	// create a delay for ignoring input pulses
#define NOTE_KILL_TIME 10000		// ~10s at 1024us per count
#define NOTE_STOP_TIME 50
unsigned int note_kill_timeout;  // the note timeout counter
unsigned char reset_pressed;  // is the reset pressed?
unsigned char clock_slow_override;  // if the clock is slow, turn it off
unsigned char song_playing;  // 1 = playing, 0 = stopped

// MIDI stuff
#define MIDI_OVERRIDE_TIME 976  // 1s at 1024us per count
unsigned int midi_override_timeout;
unsigned char midi_tick_count;

// tempo pot
unsigned char tempo_pot;

// init the clock controller
void clock_ctrl_init(void) {
	// timer 0 - internal clock timer
	t0con = 0x83;  // 8MHz clock * 16 bit, 1:16 prescale = 2us per count

	// reset stuff
	clockin_holdoff = 0;
	clock_interval = 255;
	clock_int = 0;  // start in external mode
	clock_tick_count = 255;
	midi_override_timeout = 0;
	midi_tick_count = 0;
	clock_led_timeout = 0;
	note_kill_timeout = 0;
	reset_pressed = 0;
	clock_slow_override = 0;
	tempo_pot = 0;
	_midi_tx_song_position(0);
	_midi_tx_start_song();
	song_playing = 1;  // start with song playing
}

// clock task - called every 1ms
void clock_ctrl_timer_task(void) {
	unsigned char temp;

	// ignore the clock for some time
	if(clockin_holdoff) {
		clockin_holdoff --;
	}

	// internal clock mode
	if(panel_get_switch(PANEL_CLOCK_SW)) {
		temp = panel_get_pot(PANEL_CLOCK_POT);
		// setting the clock speed pot
		if(tempo_pot != temp || clock_int == 0) {
			// 38 to 1400bpm
			clock_interval = (unsigned int)temp * (unsigned int)245;
			tempo_pot = temp;
			if(temp < 2) {
				note_kill_timeout = NOTE_STOP_TIME;  // cause note to stop sooner
				clock_slow_override = 1;
				if(song_playing) {
					_midi_tx_stop_song();
					song_playing = 0;
				}
			}
			else if(clock_slow_override) {
				clock_slow_override = 0;
				_midi_tx_continue_song();
				song_playing = 1;
			}
			else clock_slow_override = 0;
		}
		// just entering internal mode
		if(clock_int == 0) {
			clock_int = 1;
			clock_tick_count = 0;
			seq_clock_reset();
			seq_kill_note();
			_midi_tx_song_position(0);
			_midi_tx_start_song();
			song_playing = 1;
			// reset the timer interval to the last computed time
			tmr0h = clock_interval >> 8;
			tmr0l = clock_interval & 0xff;
		}
	}
	// entering external clock mode
	else if(clock_int == 1) {
		clock_int = 0;
		midi_tick_count = 0;
		clock_tick_count = 0;
		seq_clock_reset();
		seq_kill_note();
		_midi_tx_song_position(0);
		_midi_tx_start_song();
		song_playing = 1;
	}

	// note kill
	if(note_kill_timeout) {
		note_kill_timeout --;
		if(note_kill_timeout == 0) {
			seq_kill_note();
		}
	}

	// handle MIDI override timeout
	if(midi_override_timeout) {
		midi_override_timeout --;
		// reset the clock if the MIDI clock timed out - otherwise we could be stopped forever
		if(midi_override_timeout == 0) {
			clock_tick_count = 0;
			_midi_tx_start_song();
			song_playing = 1;
		}
	}

	// reset button / reset input
	if(!reset_pressed && (panel_get_encoder_sw() || panel_get_reset_in())) {
		seq_reset_song();
		_midi_tx_song_position(0);
		midi_tick_count = 0;
		clock_tick_count = 0;
		reset_pressed = 1;
	}
	if(reset_pressed && (!panel_get_encoder_sw() && !panel_get_reset_in())) {
		reset_pressed = 0;
	}

	// clock LED
	if(clock_led_timeout) {
		panel_set_clock_led(1);
		clock_led_timeout --;
	}
	else {
		panel_set_clock_led(0);
	}
}

// clock interrupt expired for internal clock
void clock_ctrl_int(void) {
	if(!clock_int) return;
	if(midi_override_timeout) return;
	if(clock_slow_override) return;

	// reset the timer interval to the last computed time
	tmr0h = clock_interval >> 8;
	tmr0l = clock_interval & 0xff;

	_midi_tx_timing_tick();
	if(song_playing) {
		if(clock_tick_count == 0) clock_led_timeout = CLOCK_LED_TIME;
		seq_clock_change(clock_tick_count);
		clock_tick_count ++;
		if(clock_tick_count == 24) {
			clock_tick_count = 0;
		}
		note_kill_timeout = NOTE_KILL_TIME;
	}
}

// external clock pulse was received
void clock_ctrl_ext_pulse(void) {
	if(clock_int) return;
	if(midi_override_timeout) return;
	// if we're not ignoring pulses right now
	if(!clockin_holdoff) {
		_midi_tx_timing_tick();	
		clockin_holdoff = CLOCK_HOLDOFF_TIME;
		if(song_playing) {
			clock_led_timeout = CLOCK_LED_TIME;
			seq_clock_change(clock_tick_count);
			clock_tick_count ++;
			if(clock_tick_count == 24) {
				clock_tick_count = 0;
			}
			note_kill_timeout = NOTE_KILL_TIME;
		}
	}
}

// MIDI RX - clock pulse was received
void clock_ctrl_midi_tick(void) {
	if(clock_int) return;
	_midi_tx_timing_tick();  // echo in ext clock mode
	if(song_playing) {
		if(midi_tick_count == 0) clock_led_timeout = CLOCK_LED_TIME;
		seq_clock_change(midi_tick_count);
		midi_tick_count ++;
		if(midi_tick_count == 24) midi_tick_count = 0;
		note_kill_timeout = NOTE_KILL_TIME;
	}
	midi_override_timeout = MIDI_OVERRIDE_TIME;
}

// MIDI RX - song position
void clock_ctrl_midi_pos(unsigned char pos) {
	midi_tick_count = pos;
}

// MIDI RX - start song
void clock_ctrl_midi_start(void) {
	_midi_tx_start_song();
	midi_tick_count = 0;
	clock_tick_count = 0;
	song_playing = 1;
	seq_reset_song();
	
}

// MIDI RX - continue song
void clock_ctrl_midi_continue(void) {
	song_playing = 1;
	_midi_tx_continue_song();
}

// MIDI RX - clock stop
void clock_ctrl_midi_stop(void) {
	song_playing = 0;
	_midi_tx_stop_song();
	note_kill_timeout = NOTE_STOP_TIME;
}

// return 1 if the clock is set to internal
unsigned char clock_ctrl_is_int(void) {
	return clock_int;
}

// reset the clock (used by the keyboard MIDI trigger)
void clock_ctrl_reset(void) {
	midi_tick_count = 0;
	clock_tick_count = 0;
}