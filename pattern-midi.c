/*
 * K4815 Pattern Generator - MIDI Callbacks
 *
 * Copyright 2010: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 * Version: 1.1
 *
 * MIDI Support:
 * - note on		- select the  note of the sequence - 60 is the centre
 * - pitch bend		- passed through to the MIDI output
 * - CC				- passed through to the MIDI output
 *		- damper pedal (64) is trapped and used to reset motion
 */
#include <system.h>
#include "midi.h"
#include "midi_callbacks.h"
#include "panel.h"
#include "clock_ctrl.h"
#include "seq.h"
#include "sysex.h"
#include "config_store.h"

unsigned char midi_channel;

// init the midi code
void pattern_midi_init(void) {
	midi_channel = config_store_get_val(CONFIG_MIDI_CHANNEL);
	// default to channel 1 if out of range
	if(midi_channel > 0x0f) {
		midi_channel = 0;
		config_store_set_val(CONFIG_MIDI_CHANNEL, midi_channel);
	}
}

// get the current MIDI channel
unsigned char pattern_midi_get_channel(void) {
	return midi_channel;
}

//
// SETUP MESSAGES
//
// learn the MIDI channel
void _midi_learn_channel(unsigned char channel) {
	midi_channel = channel & 0x0f;
	config_store_set_val(CONFIG_MIDI_CHANNEL, midi_channel);
}

//
// CHANNEL MESSAGES
//
// note off - note on with velocity = 0 calls this
void _midi_rx_note_off(unsigned char channel, 
		unsigned char note) {
	if(channel == midi_channel) {
		seq_midi_note_off(note);
	}
}

// note on - note on with velocity > 0 calls this
void _midi_rx_note_on(unsigned char channel, 
		unsigned char note, 
		unsigned char velocity) {
	if(channel == midi_channel) {
		seq_midi_note_on(note);
	}
}

// key pressure
void _midi_rx_key_pressure(unsigned char channel, 
		unsigned char note,
		unsigned char pressure) {
	if(channel == midi_channel) {
		_midi_tx_key_pressure(channel, note, pressure);
	}
}

// control change
void _midi_rx_control_change(unsigned char channel,
		unsigned char controller,
		unsigned char value) {
	// by default channel 16 controllers always respond
	if(channel == midi_channel || channel == 0x0f) {
		// pattern type
		if(controller == 20) {
			seq_control_change(SEQ_MOD_PATTERN_TYPE, value);
			_midi_tx_control_change(midi_channel, controller, value);  // echo
		}
		// motion start
		else if(controller == 21) {
			seq_control_change(SEQ_MOD_MOTION_START, value);
			_midi_tx_control_change(midi_channel, controller, value);  // echo
		}
		// motion len
		else if(controller == 22) {
			seq_control_change(SEQ_MOD_MOTION_LEN, value);
			_midi_tx_control_change(midi_channel, controller, value);  // echo
		}	
		// damper pedal
		else if(controller == 64) {
			if(value > 63) seq_midi_dir(1);
			else seq_midi_dir(0);
			// specifically don't echo this or it will make notes hold in dir flip
		}
		// echo other controllers received on our channel
		else if(channel == midi_channel) {
			_midi_tx_control_change(midi_channel, controller, value);
		}
	}
}

// channel mode - all sounds off
void _midi_rx_all_sounds_off(unsigned char channel) {
	// do not echo
}

// channel mode - reset all controllers
void _midi_rx_reset_all_controllers(unsigned char channel) {
	// do not echo
}

// channel mode - local control
void _midi_rx_local_control(unsigned char channel, unsigned char value) {
	// do not echo
}

// channel mode - all notes off
void _midi_rx_all_notes_off(unsigned char channel) {
	// do not echo
}

// channel mode - omni off
void _midi_rx_omni_off(unsigned char channel) {
	// do not echo
}

// channel mode - omni on
void _midi_rx_omni_on(unsigned char channel) {
	// do not echo
}

// channel mode - mono on
void _midi_rx_mono_on(unsigned char channel) {
	// do not echo
}

// channel mode - poly on
void _midi_rx_poly_on(unsigned char channel) {
	// do not echo
}

// program change
void _midi_rx_program_change(unsigned char channel,
		unsigned char program) {
	if(channel == midi_channel) {
		seq_motion_change(program);
	}
}

// channel pressure
void _midi_rx_channel_pressure(unsigned char channel,
		unsigned char pressure){
	if(channel == midi_channel) {
		_midi_tx_channel_pressure(channel, pressure);
	}	
}

// pitch bend
void _midi_rx_pitch_bend(unsigned char channel,
		unsigned int bend) {
	if(channel == midi_channel) {
		_midi_tx_pitch_bend(channel, bend);
	}
}

//
// SYSTEM COMMON MESSAGES
//
// song position
void _midi_rx_song_position(unsigned int pos) {
	_midi_tx_song_position(pos);
	// set the current clock position
	clock_ctrl_midi_pos((pos & 0x03) * 6);
}

// song select
void _midi_rx_song_select(unsigned char song) {
	// not supported
}

//
// SYSEX MESSAGES
//
// sysex message received
void _midi_rx_sysex_msg(unsigned char data[], unsigned char len) {
	sysex_rx_msg(data, len);
}

//
// SYSTEM REALTIME MESSAGES
//
// timing tick
void _midi_rx_timing_tick(void) {
	clock_ctrl_midi_tick();
}

// start song
void _midi_rx_start_song(void) {
	clock_ctrl_midi_start();
}

// continue song
void _midi_rx_continue_song(void) {
	clock_ctrl_midi_continue();
}

// stop song
void _midi_rx_stop_song(void) {
	clock_ctrl_midi_stop();
}

// active sensing
void _midi_rx_active_sensing(void) {
	_midi_tx_active_sensing();
}

// system reset
void _midi_rx_system_reset(void) {
	clock_ctrl_midi_stop();
	clock_ctrl_midi_pos(0);
}

// restart the device
void _midi_restart_device(void) {
	panel_hard_stop();
	reset();
}


