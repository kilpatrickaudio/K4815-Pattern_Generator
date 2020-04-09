/*
* MIDI RX callbacks to be implemented by RX handlers - ver. 1.1
*
* Copyright 2011: Kilpatrick Audio
* Written by: Andrew Kilpatrick
*
*/
//
// SETUP MESSAGES
//
// learn the MIDI channel
void _midi_learn_channel(unsigned char channel);

//
// CHANNEL MESSAGES
//
// note off - note on with velocity = 0 calls this
void _midi_rx_note_off(unsigned char channel, 
	unsigned char note);

// note on - note on with velocity > 0 calls this
void _midi_rx_note_on(unsigned char channel, 
	unsigned char note, 
	unsigned char velocity);

// key pressure
void _midi_rx_key_pressure(unsigned char channel, 
	unsigned char note,
	unsigned char pressure);

// control change - called for controllers 0-119 only
void _midi_rx_control_change(unsigned char channel,
	unsigned char controller,
	unsigned char value);

// channel mode - all sounds off
void _midi_rx_all_sounds_off(unsigned char channel);

// channel mode - reset all controllers
void _midi_rx_reset_all_controllers(unsigned char channel);

// channel mode - local control
void _midi_rx_local_control(unsigned char channel, unsigned char value);

// channel mode - all notes off
void _midi_rx_all_notes_off(unsigned char channel);

// channel mode - omni off
void _midi_rx_omni_off(unsigned char channel);

// channel mode - omni on
void _midi_rx_omni_on(unsigned char channel);

// channel mode - mono on
void _midi_rx_mono_on(unsigned char channel);

// channel mode - poly on
void _midi_rx_poly_on(unsigned char channel);

// program change
void _midi_rx_program_change(unsigned char channel,
	unsigned char program);

// channel pressure
void _midi_rx_channel_pressure(unsigned char channel,
	unsigned char pressure);

// pitch bend
void _midi_rx_pitch_bend(unsigned char channel,
	unsigned int bend);

//
// SYSTEM COMMON MESSAGES
//
// song position
void _midi_rx_song_position(unsigned int pos);

// song select
void _midi_rx_song_select(unsigned char song);

//
// SYSEX MESSAGES
//
// sysex message received
void _midi_rx_sysex_msg(unsigned char data[], unsigned char len);

//
// SYSTEM REALTIME MESSAGES
//
// timing tick
void _midi_rx_timing_tick(void);

// start song
void _midi_rx_start_song(void);

// continue song
void _midi_rx_continue_song(void);

// stop song
void _midi_rx_stop_song(void);

// active sensing
void _midi_rx_active_sensing(void);

// system reset
void _midi_rx_system_reset(void);

//
// SPECIAL CALLBACKS
//
void _midi_restart_device(void);

