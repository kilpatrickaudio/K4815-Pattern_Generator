/*
 * MIDI Receiver/Parser/Transmitter Code - ver. 1.1
 *
 * Copyright 2011: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 *
 */
// configuration
#define PIC18
//#define PIC32
#define SYSEX_RX_BUFSIZE 128
// TX and RX bufs must be size is a power of 2
#define MIDI_RX_BUFSIZE 128
#define MIDI_TX_BUFSIZE 128

// machine includes
#ifdef PIC32
#include <plib.h>  // MCC18 / C32
#endif
#ifdef PIC18
#include <system.h>  // BoostC
#endif

#include "midi.h"
#include "midi_callbacks.h"

// sysex commands
unsigned char dev_type;
#define CMD_DEVICE_TYPE_QUERY 0x7c
#define CMD_DEVICE_TYPE_RESPONSE 0x7d
#define CMD_RESTART_DEVICE 0x7e

// status bytes
#define MIDI_NOTE_OFF 0x80
#define MIDI_NOTE_ON 0x90
#define MIDI_KEY_PRESSURE 0xa0
#define MIDI_CONTROL_CHANGE 0xb0
#define MIDI_PROG_CHANGE 0xc0
#define MIDI_CHAN_PRESSURE 0xd0
#define MIDI_PITCH_BEND 0xe0

// channel mode messages
#define MIDI_CHANNEL_MODE_ALL_SOUNDS_OFF 120
#define MIDI_CHANNEL_MODE_RESET_ALL_CONTROLLERS 121
#define MIDI_CHANNEL_MODE_LOCAL_CONTROL 122
#define MIDI_CHANNEL_MODE_ALL_NOTES_OFF 123
#define MIDI_CHANNEL_MODE_OMNI_OFF 124
#define MIDI_CHANNEL_MODE_OMNI_ON 125
#define MIDI_CHANNEL_MODE_MONO_ON 126
#define MIDI_CHANNEL_MODE_POLY_ON 127

// system common messages
#define MIDI_MTC_QFRAME 0xf1
#define MIDI_SONG_POSITION 0xf2
#define MIDI_SONG_SELECT 0xf3
#define MIDI_TUNE_REQUEST 0xf6

// sysex exclusive messages
#define MIDI_SYSEX_START 0xf0
#define MIDI_SYSEX_END 0xf7

// system realtime messages
#define MIDI_TIMING_TICK 0xf8
#define MIDI_START_SONG 0xfa
#define MIDI_CONTINUE_SONG 0xfb
#define MIDI_STOP_SONG 0xfc
#define MIDI_ACTIVE_SENSING 0xfe
#define MIDI_SYSTEM_RESET 0xff

// state
#define RX_STATE_IDLE 0
#define RX_STATE_DATA0 1
#define RX_STATE_DATA1 2
#define RX_STATE_SYSEX_DATA 3
unsigned char rx_state;  // receiver state
unsigned char midi_learn_mode;

// RX message
unsigned char rx_status_chan;  // current message channel
unsigned char rx_status;  // current status byte
unsigned char rx_data0;  // data0 byte
unsigned char rx_data1;  // data1 byte

// RX buffer
unsigned char rx_msg[MIDI_RX_BUFSIZE];  // receive msg buffer
unsigned char rx_in_pos;
unsigned char rx_out_pos;
#define MIDI_RX_BUF_MASK (MIDI_RX_BUFSIZE - 1)

// TX message
unsigned char tx_msg[MIDI_TX_BUFSIZE];  // transmit msg buffer
unsigned char tx_in_pos;
unsigned char tx_out_pos;
#define MIDI_TX_BUF_MASK (MIDI_TX_BUFSIZE - 1)
#define TX_IN_INC tx_in_pos = (tx_in_pos + 1) & MIDI_TX_BUF_MASK

// sysex buffer
unsigned char sysex_lib_rx_buf[SYSEX_RX_BUFSIZE];
unsigned char sysex_lib_rx_buf_count;

// local functions
void process_msg(void);
void sysex_start(void);
void sysex_data(unsigned char data);
void sysex_end(void);
void sysex_parse_msg(void);
void control_change_parse_msg(unsigned char channel, 
		unsigned char controller, unsigned char value);

// init the MIDI receiver module
void midi_init(unsigned char device_type) {
	dev_type = device_type;
	rx_state = RX_STATE_IDLE;
	rx_status = 255;  // no running status yet
 	rx_status_chan = 0;
	tx_in_pos = 0;
	tx_out_pos = 0;
	rx_in_pos = 0;
	rx_out_pos = 0;
	midi_learn_mode = 0;
	sysex_lib_rx_buf_count = 0;
}

// handle a new byte received from the stream
void midi_rx_byte(unsigned char rx_byte) {
	rx_msg[rx_in_pos] = rx_byte;
	rx_in_pos = (rx_in_pos + 1) & MIDI_RX_BUF_MASK;
}

// transmit task - call this on the main loop
void midi_tx_task(void) {
	// check UART ready state
#ifdef PIC18
	if(!pir1.TXIF) return;  // BoostC
#endif
#ifdef PIC32
	if(!UARTTransmitterIsReady(UART2)) return;
#endif

	// check FIFO state
	if(tx_in_pos == tx_out_pos) return;

	// disable interrupts
#ifdef PIC18
	intcon.GIE = 0;  // PIC18
#endif
#ifdef PIC32
	INTDisableInterrupts();  // C32
#endif

	// send data
#ifdef PIC18
	txreg = tx_msg[tx_out_pos];  // BoostC
#endif
#ifdef PIC32
//	UARTSendDataByte(UART1, tx_msg[tx_out_pos]);  // MCC18 / C32
	UARTSendDataByte(UART2, tx_msg[tx_out_pos]);  // MCC18 / C32
#endif
	tx_out_pos = (tx_out_pos + 1) & MIDI_TX_BUF_MASK;

	// enable interrupts
#ifdef PIC18
	intcon.GIE = 1;  // PIC18
#endif
#ifdef PIC32
	INTEnableInterrupts();  // C32
#endif
}

// receive task - call this on a timer interrupt
void midi_rx_task(void) {
	unsigned char stat, chan;
	unsigned char rx_byte;
	// get data from RX buffer
	if(rx_in_pos == rx_out_pos) return;
	rx_byte = rx_msg[rx_out_pos];
	rx_out_pos = (rx_out_pos + 1) & MIDI_RX_BUF_MASK;

	// status byte
	if(rx_byte & 0x80) {
		stat = (rx_byte & 0xf0);
	   	chan = (rx_byte & 0x0f);

		// system messages
   		if(stat == 0xf0) {
			// realtime messages - does not reset running status
	     	if(rx_byte == MIDI_TIMING_TICK) {
				_midi_rx_timing_tick();
				return;
    	 	}
	     	else if(rx_byte == MIDI_START_SONG) {
				_midi_rx_start_song();
				return;
    		}	
     		else if(rx_byte == MIDI_CONTINUE_SONG) {
				_midi_rx_continue_song();
				return;
     		}
	     	else if(rx_byte == MIDI_STOP_SONG) {
				_midi_rx_stop_song();
				return;
	     	}
			else if(rx_byte == MIDI_ACTIVE_SENSING) {
				_midi_rx_active_sensing();
				return;
			}
			else if(rx_byte == MIDI_SYSTEM_RESET) {
				rx_status_chan = 255;  // reset running status channel
				rx_status = 0;
				rx_state = RX_STATE_IDLE;
				_midi_rx_system_reset();
				return;
			}
			else if(rx_byte == 0xf9 || rx_byte == 0xfd) {
				// undefined system realtime message
				return;
			}
			// sysex messages
    		else if(rx_byte == MIDI_SYSEX_START) {
				sysex_start();
				rx_status_chan = 255;  // reset running status channel
				rx_status = rx_byte;
				rx_state = RX_STATE_SYSEX_DATA;
				return;
	     	}
	     	else if(rx_byte == MIDI_SYSEX_END) {
				sysex_end();
				rx_status_chan = 255;  // reset running status channel
				rx_status = 0;
				rx_state = RX_STATE_IDLE;
				return;
     		}
			else {
				// are we currently receiving sysex?
				if(rx_state == RX_STATE_SYSEX_DATA) {
					sysex_end();
				}
				// system common messages
    	 		if(rx_byte == MIDI_SONG_POSITION) {
					rx_status_chan = 255;  // reset running status channel
					rx_status = rx_byte;
					rx_state = RX_STATE_DATA0;
					return;
    	 		}
	     		else if(rx_byte == MIDI_SONG_SELECT) {
					rx_status_chan = 255;  // reset running status channel
					rx_status = rx_byte;
					rx_state = RX_STATE_DATA0;
					return;
    	 		}
				// undefined / unsupported system common messages
				// MTC quarter frame, 0xf4, 0xf5, tune request
				rx_status_chan = 255;  // reset running status channel
				rx_status = 0;
				rx_state = RX_STATE_IDLE;
				return;
			}
   		}
	   	// channel messages
   		else {
			// do we have a sysex message current receiving?
			if(rx_state == RX_STATE_SYSEX_DATA) {
				sysex_end();
			}
			// channel status
     		if(stat == MIDI_NOTE_OFF) {
				rx_status_chan = chan;
				rx_status = stat;
				rx_state = RX_STATE_DATA0;
				return;
	     	}
    	 	if(stat == MIDI_NOTE_ON) {
				rx_status_chan = chan;
				rx_status = stat;
				rx_state = RX_STATE_DATA0;
				return;
     		}
	     	if(stat == MIDI_KEY_PRESSURE) {
				rx_status_chan = chan;
				rx_status = stat;
				rx_state = RX_STATE_DATA0;
				return;
    	 	}
	    	if(stat == MIDI_CONTROL_CHANGE) {
				rx_status_chan = chan;
				rx_status = stat;
				rx_state = RX_STATE_DATA0;
				return;
	     	}
    	 	if(stat == MIDI_PROG_CHANGE) {
				rx_status_chan = chan;
				rx_status = stat;
				rx_state = RX_STATE_DATA0;
				return;
	     	}
    	 	if(stat == MIDI_CHAN_PRESSURE) {
				rx_status_chan = chan;
				rx_status = stat;
				rx_state = RX_STATE_DATA0;
				return;
	     	}
    	 	if(stat == MIDI_PITCH_BEND) {
				rx_status_chan = chan;
				rx_status = stat;
				rx_state = RX_STATE_DATA0;
				return;
	    	}
   		}
   		return;  // just in case
	}
 	// data byte 0
 	if(rx_state == RX_STATE_DATA0) {
   		rx_data0 = rx_byte;

		// data length = 1 - process these messages right away
   		if(rx_status == MIDI_SONG_SELECT ||
				rx_status == MIDI_PROG_CHANGE ||
      			rx_status == MIDI_CHAN_PRESSURE) {
     		process_msg();
			// if this message supports running status
     		if(rx_status_chan != 255) {
				rx_state = RX_STATE_DATA0;  // loop back for running status
			}
			else {
				rx_state = RX_STATE_IDLE;
			}
   		}
   		// data length = 2
   		else {
     		rx_state = RX_STATE_DATA1;
   		}
	   return;
 	}

 	// data byte 1
 	if(rx_state == RX_STATE_DATA1) {
   		rx_data1 = rx_byte;
   		process_msg();
		// if this message supports running status
   		if(rx_status_chan != 255) {   		
   			rx_state = RX_STATE_DATA0;  // loop back for running status
		}
		else {
			rx_state = RX_STATE_IDLE;
		}
   		return;
 	}

	// sysex data
	if(rx_state == RX_STATE_SYSEX_DATA) {
		sysex_data(rx_byte);
		return;
	}
}

// process a received message
void process_msg(void) {
	if(rx_status == MIDI_SONG_POSITION) {
   		_midi_rx_song_position((rx_data1 << 7) | rx_data0);
   		return;
 	}
 	if(rx_status == MIDI_SONG_SELECT) {
   		_midi_rx_song_select(rx_data0);
   		return;
 	}
	// this is a channel message by this point
	if(midi_learn_mode) {
		_midi_learn_channel(rx_status_chan);
		midi_set_learn_mode(0);  // turn this off
	}
 	if(rx_status == MIDI_NOTE_OFF) {
   		_midi_rx_note_off(rx_status_chan, rx_data0);
   		return;
 	}
 	if(rx_status == MIDI_NOTE_ON) {
   		if(rx_data1 == 0) _midi_rx_note_off(rx_status_chan, rx_data0);
   		else _midi_rx_note_on(rx_status_chan, rx_data0, rx_data1);
   		return;
 	}
 	if(rx_status == MIDI_KEY_PRESSURE) {
   		_midi_rx_key_pressure(rx_status_chan, rx_data0, rx_data1);
   		return;
 	}
 	if(rx_status == MIDI_CONTROL_CHANGE) {
		control_change_parse_msg(rx_status_chan, rx_data0, rx_data1);
   		return;
 	}
 	if(rx_status == MIDI_PROG_CHANGE) {
   		_midi_rx_program_change(rx_status_chan, rx_data0);
   		return;
 	}
 	if(rx_status == MIDI_CHAN_PRESSURE) {
   		_midi_rx_channel_pressure(rx_status_chan, rx_data0);
   		return;
 	}
 	if(rx_status == MIDI_PITCH_BEND) {
   		_midi_rx_pitch_bend(rx_status_chan, (rx_data1 << 7) | rx_data0);
   		return;
 	}
}

// handle sysex start
void sysex_start(void) {
	sysex_lib_rx_buf_count = 0;
}

// handle sysex data
void sysex_data(unsigned char data) {
	if(sysex_lib_rx_buf_count < SYSEX_RX_BUFSIZE) {
		sysex_lib_rx_buf[sysex_lib_rx_buf_count] = data;
		sysex_lib_rx_buf_count ++;
	}
}

// handle sysex end
void sysex_end(void) {
	sysex_parse_msg();
}

// parse a received sysex message for globally supported stuff
void sysex_parse_msg(void) {
	unsigned char cmd;
	if(sysex_lib_rx_buf_count < 4) return;
	// Kilpatrick Audio command
	if(sysex_lib_rx_buf[0] == 0x00 &&
			sysex_lib_rx_buf[1] == 0x01 &&
			sysex_lib_rx_buf[2] == 0x72) {
		cmd = sysex_lib_rx_buf[3];
		// device type query - global command
		if(cmd == CMD_DEVICE_TYPE_QUERY) {
			_midi_tx_sysex1(CMD_DEVICE_TYPE_RESPONSE, dev_type);
		}
		// restart device - global command with addressing
		else if(cmd == CMD_RESTART_DEVICE && sysex_lib_rx_buf_count == 9) {
			// check the device ID and special code ("KILL")
			if(sysex_lib_rx_buf[4] == dev_type && sysex_lib_rx_buf[5] == 'K' &&
					sysex_lib_rx_buf[6] == 'I' && sysex_lib_rx_buf[7] == 'L' &&
					sysex_lib_rx_buf[8] == 'L') {
				_midi_restart_device();
			}
		}
		// non-global Kilpatrick Audio command - pass on to user code
		else {
			_midi_rx_sysex_msg(sysex_lib_rx_buf, sysex_lib_rx_buf_count);
		}
	}
	// non-Kilpatrick Audio command - echo through us
	else {
		_midi_tx_sysex_msg(sysex_lib_rx_buf, sysex_lib_rx_buf_count);
	}
}

// parse a received controller change message
void control_change_parse_msg(unsigned char channel, 
		unsigned char controller, unsigned char value) {
	// controllers
	if(controller < 120) {
		// pass through to user code
		_midi_rx_control_change(rx_status_chan, rx_data0, rx_data1);
	}
	// all sounds off
	else if(controller == MIDI_CHANNEL_MODE_ALL_SOUNDS_OFF) {
		_midi_rx_all_sounds_off(channel);
	}
	// reset all controllers
	else if(controller == MIDI_CHANNEL_MODE_RESET_ALL_CONTROLLERS) {
		_midi_rx_reset_all_controllers(channel);
	}
	// local control
	else if(controller == MIDI_CHANNEL_MODE_LOCAL_CONTROL) {
		_midi_rx_local_control(channel, value);
	}
	// all notes off
	else if(controller == MIDI_CHANNEL_MODE_ALL_NOTES_OFF) {
		_midi_rx_all_notes_off(channel);
	}
	// omni off
	else if(controller == MIDI_CHANNEL_MODE_OMNI_OFF) {
		_midi_rx_omni_off(channel);
	}
	// omni on
	else if(controller == MIDI_CHANNEL_MODE_OMNI_ON) {
		_midi_rx_omni_on(channel);
	}
	// mono on
	else if(controller == MIDI_CHANNEL_MODE_MONO_ON) {
		_midi_rx_mono_on(channel);
	}
	// poly on
	else if(controller == MIDI_CHANNEL_MODE_POLY_ON) {
		_midi_rx_poly_on(channel);
	}	
}

// sets the learn mode - 1 = on, 0 = off
void midi_set_learn_mode(unsigned char mode) {
	midi_learn_mode = (mode & 0x01);
}

// gets the device type configured in the MIDI library
unsigned char midi_get_device_type(void) {
	return dev_type;
}

//
// SENDERS
//
// send note off - sends note on with velocity 0
void _midi_tx_note_off(unsigned char channel,
		unsigned char note) {  
 	tx_msg[tx_in_pos] = MIDI_NOTE_ON | (channel & 0x0f);
	TX_IN_INC;
 	tx_msg[tx_in_pos] = (note & 0x7f);
	TX_IN_INC;
 	tx_msg[tx_in_pos] = 0x00;
	TX_IN_INC;
}

// send note on
void _midi_tx_note_on(unsigned char channel,
		unsigned char note,
		unsigned char velocity) {
	tx_msg[tx_in_pos] = MIDI_NOTE_ON | (channel & 0x0f);
	TX_IN_INC;
 	tx_msg[tx_in_pos] = (note & 0x7f);
	TX_IN_INC;
 	tx_msg[tx_in_pos] = (velocity & 0x7f);	
	TX_IN_INC;
}

// send key pressure
void _midi_tx_key_pressure(unsigned char channel,
			   unsigned char note,
			   unsigned char pressure) {
	tx_msg[tx_in_pos] = MIDI_KEY_PRESSURE | (channel & 0x0f);
	TX_IN_INC;
 	tx_msg[tx_in_pos] = (note & 0x7f);
	TX_IN_INC;
 	tx_msg[tx_in_pos] = (pressure & 0x7f);
	TX_IN_INC;
}

// send control change
void _midi_tx_control_change(unsigned char channel,
		unsigned char controller,
		unsigned char value) {
 	tx_msg[tx_in_pos] = MIDI_CONTROL_CHANGE | (channel & 0x0f);
	TX_IN_INC;
 	tx_msg[tx_in_pos] = (controller & 0x7f);
	TX_IN_INC;
 	tx_msg[tx_in_pos] = (value & 0x7f);
	TX_IN_INC;
}


// send channel mode - all sounds off
void _midi_tx_all_sounds_off(unsigned char channel) {
 	tx_msg[tx_in_pos] = MIDI_CONTROL_CHANGE | (channel & 0x0f);
	TX_IN_INC;
 	tx_msg[tx_in_pos] = MIDI_CHANNEL_MODE_ALL_SOUNDS_OFF;
	TX_IN_INC;
 	tx_msg[tx_in_pos] = 0;
	TX_IN_INC;
}

// send channel mode - reset all controllers
void _midi_tx_reset_all_controllers(unsigned char channel) {
 	tx_msg[tx_in_pos] = MIDI_CONTROL_CHANGE | (channel & 0x0f);
	TX_IN_INC;
 	tx_msg[tx_in_pos] = MIDI_CHANNEL_MODE_RESET_ALL_CONTROLLERS;
	TX_IN_INC;
 	tx_msg[tx_in_pos] = 0;
	TX_IN_INC;
}

// send channel mode - local control
void _midi_tx_local_control(unsigned char channel, unsigned char value) {
 	tx_msg[tx_in_pos] = MIDI_CONTROL_CHANGE | (channel & 0x0f);
	TX_IN_INC;
 	tx_msg[tx_in_pos] = MIDI_CHANNEL_MODE_LOCAL_CONTROL;
	TX_IN_INC;
 	tx_msg[tx_in_pos] = (value & 0x7f);
	TX_IN_INC;
}

// send channel mode - all notes off
void _midi_tx_all_notes_off(unsigned char channel) {
 	tx_msg[tx_in_pos] = MIDI_CONTROL_CHANGE | (channel & 0x0f);
	TX_IN_INC;
 	tx_msg[tx_in_pos] = MIDI_CHANNEL_MODE_ALL_NOTES_OFF;
	TX_IN_INC;
 	tx_msg[tx_in_pos] = 0;
	TX_IN_INC;
}

// send channel mode - omni off
void _midi_tx_omni_off(unsigned char channel) {
 	tx_msg[tx_in_pos] = MIDI_CONTROL_CHANGE | (channel & 0x0f);
	TX_IN_INC;
 	tx_msg[tx_in_pos] = MIDI_CHANNEL_MODE_OMNI_OFF;
	TX_IN_INC;
 	tx_msg[tx_in_pos] = 0;
	TX_IN_INC;
}

// send channel mode - omni on
void _midi_tx_omni_on(unsigned char channel) {
 	tx_msg[tx_in_pos] = MIDI_CONTROL_CHANGE | (channel & 0x0f);
	TX_IN_INC;
 	tx_msg[tx_in_pos] = MIDI_CHANNEL_MODE_OMNI_ON;
	TX_IN_INC;
 	tx_msg[tx_in_pos] = 0;
	TX_IN_INC;
}

// send channel mode - mono on
void _midi_tx_mono_on(unsigned char channel) {
 	tx_msg[tx_in_pos] = MIDI_CONTROL_CHANGE | (channel & 0x0f);
	TX_IN_INC;
 	tx_msg[tx_in_pos] = MIDI_CHANNEL_MODE_MONO_ON;
	TX_IN_INC;
 	tx_msg[tx_in_pos] = 0;
	TX_IN_INC;
}

// send channel mode - poly on
void _midi_tx_poly_on(unsigned char channel) {
 	tx_msg[tx_in_pos] = MIDI_CONTROL_CHANGE | (channel & 0x0f);
	TX_IN_INC;
 	tx_msg[tx_in_pos] = MIDI_CHANNEL_MODE_POLY_ON;
	TX_IN_INC;
 	tx_msg[tx_in_pos] = 0;
	TX_IN_INC;
}

// send program change
void _midi_tx_program_change(unsigned char channel,
		unsigned char program) {
 	tx_msg[tx_in_pos] = MIDI_PROG_CHANGE | (channel & 0x0f);
	TX_IN_INC;
 	tx_msg[tx_in_pos] = (program & 0x7f);
	TX_IN_INC;
}

// send channel pressure
void _midi_tx_channel_pressure(unsigned char channel,
			       unsigned char pressure) {
 	tx_msg[tx_in_pos] = MIDI_CHAN_PRESSURE | (channel & 0x0f);
	TX_IN_INC;
 	tx_msg[tx_in_pos] = (pressure & 0x7f);
	TX_IN_INC;
}

// send pitch bend
void _midi_tx_pitch_bend(unsigned char channel,
		unsigned int bend) {
 	tx_msg[tx_in_pos] = MIDI_PITCH_BEND | (channel & 0x0f);
	TX_IN_INC;
 	tx_msg[tx_in_pos] = (bend & 0x7f);
	TX_IN_INC;
 	tx_msg[tx_in_pos] = (bend & 0x3f80) >> 7;
	TX_IN_INC;
}

// sysex message start
void _midi_tx_sysex_start(void) {
	tx_msg[tx_in_pos] = MIDI_SYSEX_START;
	TX_IN_INC;
}

// sysex message data byte
void _midi_tx_sysex_data(unsigned char data_byte) {
 	tx_msg[tx_in_pos] = data_byte;
	TX_IN_INC;
}

// sysex message end
void _midi_tx_sysex_end(void) {
	tx_msg[tx_in_pos] = MIDI_SYSEX_END;
	TX_IN_INC;
}

// send a sysex packet with CMD and DATA - default MMA ID and dev type
void _midi_tx_sysex1(unsigned char cmd, unsigned char data) {
	_midi_tx_sysex_start();
	_midi_tx_sysex_data(0x00);
	_midi_tx_sysex_data(0x01);
	_midi_tx_sysex_data(0x72);
	_midi_tx_sysex_data(dev_type);
	_midi_tx_sysex_data(cmd & 0x7f);
	_midi_tx_sysex_data(data & 0x7f);
	_midi_tx_sysex_end();
}

// send a sysex packet with CMD and 2 DATA bytes - default MMA ID and dev type
void _midi_tx_sysex2(unsigned char cmd, unsigned char data0, unsigned char data1) {
	_midi_tx_sysex_start();
	_midi_tx_sysex_data(0x00);
	_midi_tx_sysex_data(0x01);
	_midi_tx_sysex_data(0x72);
	_midi_tx_sysex_data(dev_type);
	_midi_tx_sysex_data(cmd & 0x7f);
	_midi_tx_sysex_data(data0 & 0x7f);
	_midi_tx_sysex_data(data1 & 0x7f);
	_midi_tx_sysex_end();
}

// send a sysex message with some number of bytes - no default MMA ID and dev type
void _midi_tx_sysex_msg(unsigned char data[], unsigned char len) {
	int i;
	_midi_tx_sysex_start();
	for(i = 0; i < len; i ++) {
		_midi_tx_sysex_data(data[i]);
	}
	_midi_tx_sysex_end();
}

// send song position
void _midi_tx_song_position(unsigned int position) {
	tx_msg[tx_in_pos] = MIDI_SONG_POSITION;
	TX_IN_INC;
	tx_msg[tx_in_pos] = (position & 0x7f);
	TX_IN_INC;
	tx_msg[tx_in_pos] = (position & 0x3f80) >> 7;
	TX_IN_INC;
}

// send song select
void _midi_tx_song_select(unsigned char song) {
	tx_msg[tx_in_pos] = MIDI_SONG_SELECT;
	TX_IN_INC;
	tx_msg[tx_in_pos] = (song & 0x7f);
	TX_IN_INC;
}

// send timing tick
void _midi_tx_timing_tick(void) {
	tx_msg[tx_in_pos] = MIDI_TIMING_TICK;
	TX_IN_INC;
}

// send start song
void _midi_tx_start_song(void) {
 	tx_msg[tx_in_pos] = MIDI_START_SONG;
	TX_IN_INC;
}

// send continue song
void _midi_tx_continue_song(void) {
 	tx_msg[tx_in_pos] = MIDI_CONTINUE_SONG;
	TX_IN_INC;
}

// send stop song
void _midi_tx_stop_song(void) {
 	tx_msg[tx_in_pos] = MIDI_STOP_SONG;
	TX_IN_INC;
}

// send active sensing
void _midi_tx_active_sensing(void) {
 	tx_msg[tx_in_pos] = MIDI_ACTIVE_SENSING;
	TX_IN_INC;
}

// send system reset
void _midi_tx_system_reset(void) {
 	tx_msg[tx_in_pos] = MIDI_SYSTEM_RESET;
	TX_IN_INC;
}

// send a debug string
void _midi_tx_debug(char *str) {
	_midi_tx_sysex_start();
	_midi_tx_sysex_data(0x00);
	_midi_tx_sysex_data(0x01);
	_midi_tx_sysex_data(0x72);
	_midi_tx_sysex_data(0x01);
	while(*str) {
		_midi_tx_sysex_data((*str) & 0x7f);
		*str++;
	}
	_midi_tx_sysex_end();
}
