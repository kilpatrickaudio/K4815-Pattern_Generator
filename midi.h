/*
 * MIDI Receiver/Parser/Transmitter Code - ver. 1.1
 *
 * Copyright 2011: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 *
 */
// init the MIDI receiver module
void midi_init(unsigned char device_type);

// handle a new byte received from the stream
void midi_rx_byte(unsigned char rx_byte);

// transmit task - call this on the main loop
void midi_tx_task(void);

// receive task - call this on a timer interrupt
void midi_rx_task(void);

// sets the learn mode - 1 = on, 0 = off
void midi_set_learn_mode(unsigned char mode);

// gets the device type configured in the MIDI library
unsigned char midi_get_device_type(void);

//
// SENDERS
//
// send note on
void _midi_tx_note_off(unsigned char channel,
		       unsigned char note);

// send note off - sends a note on with velocity 0
void _midi_tx_note_on(unsigned char channel,
		      unsigned char note,
		      unsigned char velocity);			  
			   
// send key pressture
void _midi_tx_key_pressure(unsigned char channel,
			   unsigned char note,
			   unsigned char pressure);

// send control change
void _midi_tx_control_change(unsigned char channel,
		unsigned char controller,
		unsigned char value);

// send channel mode - all sounds off
void _midi_tx_all_sounds_off(unsigned char channel);

// send channel mode - reset all controllers
void _midi_tx_reset_all_controllers(unsigned char channel);

// send channel mode - local control
void _midi_tx_local_control(unsigned char channel, unsigned char value);

// send channel mode - all notes off
void _midi_tx_all_notes_off(unsigned char channel);

// send channel mode - omni off
void _midi_tx_omni_off(unsigned char channel);

// send channel mode - omni on
void _midi_tx_omni_on(unsigned char channel);

// send channel mode - mono on
void _midi_tx_mono_on(unsigned char channel);

// send channel mode - poly on
void _midi_tx_poly_on(unsigned char channel);

// send program change
void _midi_tx_program_change(unsigned char channel,
		unsigned char program);

// send channel pressure
void _midi_tx_channel_pressure(unsigned char channel,
			       unsigned char pressure);

// send pitch bend
void _midi_tx_pitch_bend(unsigned char channel,
			 unsigned int bend);
			 
// send sysex start
void _midi_tx_sysex_start(void);

// send sysex data
void _midi_tx_sysex_data(unsigned char data_byte);

// send sysex end
void _midi_tx_sysex_end(void);

// send a sysex packet with CMD and DATA - default MMA ID and dev type
void _midi_tx_sysex1(unsigned char cmd, unsigned char data);

// send a sysex packet with CMD and 2 DATA bytes - default MMA ID and dev type
void _midi_tx_sysex2(unsigned char cmd, unsigned char data0, unsigned char data1);

// send a sysex message with some number of bytes
void _midi_tx_sysex_msg(unsigned char data[], unsigned char len);

// send song position
void _midi_tx_song_position(unsigned int position);

// send song select
void _midi_tx_song_select(unsigned char song);

// send timing tick
void _midi_tx_timing_tick(void);

// send start song
void _midi_tx_start_song(void);

// send continue song
void _midi_tx_continue_song(void);

// send stop song
void _midi_tx_stop_song(void);

// send active sensing
void _midi_tx_active_sensing(void);

// send system reset
void _midi_tx_system_reset(void);

// send a debug string
void _midi_tx_debug(char *str);

