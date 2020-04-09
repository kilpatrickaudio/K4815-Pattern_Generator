/*
 * K4815 Pattern Generator - Sequencer
 *
 * Copyright 2010: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 * Version: 1.1
 *
 */
#include <system.h>
#include <flash.h>
#include "seq.h"
#include "panel.h"
#include "font_sym.h"
#include "pattern_map.h"
#include "scale_map.h"
#include "motion_map.h"
#include "midi.h"
#include "pattern_midi.h"
#include "clock_ctrl.h"
#include "note_lookup.h"
#include "random.h"

// pattern
unsigned char motion_step_loc;		// the current step location (looked up from motion data)
unsigned char motion_type;			// the current motion type
unsigned char motion_step;			// the current motion step (0-63)
unsigned char motion_data[64];		// the grid pos for each of 64 steps
//unsigned char motion_data_len;		// the length of motion data
unsigned char clock_div_count;		// clock divider temp variable
unsigned char clock_div;			// clock divider time
unsigned char clock_div_new;		// clock divider time for next beat
unsigned char pattern_type;			// the pattern type
unsigned char pattern_data[8];		// the pattern data (one entry per row)
unsigned char tonality;				// tonality - major or minor
unsigned char span;					// span - large or small
unsigned char scale_data[64];		// note lookup for each of the 64 grid positions
unsigned char xy_scale_data[8];		// level lookup for each of the X or Y positions
unsigned char current_note;			// current note
unsigned char gate_time;			// the desired length of the note
unsigned char gate_time_count;		// the length of the current note
unsigned char output_offset;		// the output offset - 0-64 = 32 = 0
unsigned char output_mode;			// 1 = CV/gate, 0 = X/Y
unsigned char dir;					// 1 = forward, 0 = backward
unsigned char midi_dir_sw;			// 1 = MIDI dir swap, 0 = normal
unsigned char play_len;				// the length of play for the pattern
unsigned char play_len_pot;			// the value of the play len pot
signed char midi_base_note;			// the note offset sent by MIDI
unsigned char keyboard_trigger;		// 1 = trigger pattern from keyboard
unsigned char keyboard_cur_note;	// the current keyboard note
unsigned char random_seed_count;	// counts whether steps are seeded
unsigned char random_seed_repeat;	// 1 = seed continuously, 0 = seed once
unsigned char random_mask;			// mask out certain spots in X and Y
unsigned char pattern_type_override;  // mod pattern type override
unsigned char motion_start_override;  // motion start override
unsigned char motion_len_override;  // motion len override

// clock divisions
unsigned char clock_div_table[] = { 24, 12, 8, 6, 4, 3, 2, 1 };

// sound definitions
#define TONALITY_MINOR 0
#define TONALITY_MAJOR 1
#define SPAN_SMALL 0
#define SPAN_LARGE 1
#define OUTPUT_MODE_XY 0
#define OUTPUT_MODE_CV 1
#define DIR_BACKWARD 0
#define DIR_FORWARD 1

// MIDI defines
#define MIDI_CC_X 16
#define MIDI_CC_Y 17

// local functions
void seq_render_ball(void);
void seq_draw_ol_symbol(unsigned char, unsigned int);
void seq_note_on(unsigned char);
void seq_note_off(void);

// init the sequencer
void seq_init(void) {
	motion_step_loc = 0;
	motion_type = 0;
	motion_step = 0;
	clock_div_count = 0;
	clock_div = clock_div_table[3];
	clock_div_new = clock_div;
	pattern_type = 0;
	tonality = TONALITY_MAJOR;
	span = SPAN_LARGE;
	current_note = 0;
	gate_time = 0;
	gate_time_count = 0;
	output_offset = 0;
	output_mode = OUTPUT_MODE_CV;
	dir = DIR_FORWARD;
	midi_dir_sw = 0;
	play_len = 64;
	play_len_pot = 64;
	midi_base_note = 0;
	keyboard_trigger = 0;
	keyboard_cur_note = 255;
	random_seed_count = 255;
	random_seed_repeat = 0;
	random_mask = 0x77;
	gate_time_count = 0;
	pattern_type_override = 0;
	motion_start_override = 0;
	motion_len_override = 0;
	seq_load_motion();
	seq_set_pattern();
	seq_set_scale();
	motion_step_loc = motion_data[motion_step];
	seed_rand(100);
	seq_reset_song();
}

// sequencer timer task - called every 1024us
void seq_timer_task(void) {
	signed char stemp;
	unsigned char temp, temp2;

	// pattern
	temp = (panel_get_pot(PANEL_PATTERN_POT) >> 3);
	if(pattern_type_override > temp) {
		temp = pattern_type_override;
	}
	if(temp != pattern_type) {
		pattern_type = temp;
		seq_set_pattern();
	}

	// tonality
	temp = panel_get_switch(PANEL_TONALITY_SW);
	if(temp != tonality) {
		tonality = temp;
		seq_set_scale();
	}

	// span
	temp = panel_get_switch(PANEL_SPAN_SW);
	if(temp != span) {
		span = temp;
		seq_set_scale();
	}

	// direction
	dir = panel_get_switch(PANEL_DIR_SW);
	// possibly flip the direction
	if(panel_get_dir_in()) {
		dir = !dir;
	}
	if(midi_dir_sw) {
		dir = !dir;
	}

	// gate time
	temp = panel_get_pot(PANEL_GATE_POT) >> 2;
	if(gate_time != temp) {
		gate_time = temp;
	}

	// clock_div 
	if(clock_ctrl_is_int()) {
		clock_div_new = clock_div_table[3];  // 4 bpq
	}
	else {
		temp = (panel_get_pot(PANEL_CLOCK_POT) >> 5);
		temp2 = clock_div_table[temp];
		if(temp2 != clock_div_new) {
			clock_div_new = temp2;
			panel_set_popup_num(clock_div_new);
		}
	}

	// output offset
#ifdef EURORACK
	output_offset = panel_get_pot(PANEL_OUTPUT_POT) >> 2;  // range = 64
#endif
#ifdef BUCHLA
	output_offset = panel_get_pot(PANEL_OUTPUT_POT) >> 3;  // range = 32 / buchla
#endif

	// output mode
	if(panel_get_switch(PANEL_OUTPUT_SW)) {
		if(output_mode != OUTPUT_MODE_CV) {
			output_mode = OUTPUT_MODE_CV;
			seq_kill_note();
		}
	}
	else {
		if(output_mode != OUTPUT_MODE_XY) {
			seq_kill_note();  // must be first
			output_mode = OUTPUT_MODE_XY;
		}
	}

	// play len
	temp = (panel_get_pot(PANEL_PLAY_LEN_POT) >> 2) + 1;  // len override
	// pot has changed
	if(play_len_pot != temp) {
		play_len_pot = temp;
		panel_set_popup_num(play_len_pot);
	}
	// override the pot value with CC input
	if(motion_len_override > play_len_pot) {
		play_len = motion_len_override;
	}
	else {
		play_len = play_len_pot;
	}

	// encoder selects new patterns
	stemp = panel_get_encoder();
	if(stemp) {
		seq_motion_change((motion_type + stemp) & 0x3f);
	}
}

// when the clock changes this is called
// the phase runs from 0-23 for each beat
void seq_clock_change(unsigned char phase) {
	unsigned char note_pos;
	unsigned char temp;

	// valid clock phase - adjust the step?
	if(phase != 255) {
		// divide the input clock by the clock_div amount
		if(phase == 0) {
			clock_div_count = 0;
			clock_div = clock_div_new;
		}
		else {
			clock_div_count ++;
		}
		if(clock_div_count >= clock_div) clock_div_count = 0;

		// handle gate time
		// turn off the note that has exceeded the gate time
		if(current_note) {
			gate_time_count ++;
			if(gate_time_count > gate_time || gate_time_count == 255) {
				seq_note_off();
			}
		}

		// motion step
		if(clock_div_count == 0) {
			// time to seed some random data
			if(random_seed_count < 64) {
				motion_data[random_seed_count] = get_rand();
				random_seed_count ++;
				// if we should be doing this over and over
				if(random_seed_count == 64 && random_seed_repeat) {
					random_seed_count = 0;
				}
			}

			// get the note position (grid pos) of the motion step
			motion_step_loc = (motion_data[(motion_step + motion_start_override) & 0x3f] & random_mask);
			// this makes the start point wrap around to the motion length for <64 steps in the motion
//			motion_step_loc = (motion_data[(unsigned char)((motion_step + motion_start_override) % motion_data_len) & 0x3f] & random_mask);
			note_pos = (motion_step_loc & 0x07) | 
				((motion_step_loc & 0x70) >> 1);

			// get the pattern mask for this row
			temp = pattern_data[motion_step_loc >> 4];
			if(motion_step_loc & 0x07) {
				temp = temp >> (motion_step_loc & 0x07);
			}
			temp = (temp & 0x01);

			// does this note have a step?
			if(temp) {
				// is the last note still playing?
				if(current_note) {
					seq_note_off();
				}
				seq_note_on(scale_data[note_pos]);
			}

			// show the note on the display
			seq_render_ball();
			// move forward
			if(dir) {
				motion_step ++;
				// reset after play_len positions
				if(motion_step == play_len || motion_step >= play_len) {
					motion_step = 0;
				}
				// reset if the next position == 0xff
				if(motion_data[motion_step] == 0xff) motion_step = 0;
			}
			// move backward
			else {
				motion_step --;
				if(motion_step == 255 || motion_step >= play_len) {
					motion_step = play_len - 1;
				}
				// reset if the next position == 0xff
				for(; motion_data[motion_step] == 0xff && motion_step > 0;
					motion_step --);
			}
		}
	}
	// invalid clock - maybe stop a note
	else {
		seq_kill_note();
	}
}

// reset the song / pattern position
void seq_reset_song(void) {
	motion_step = 0;
	motion_step_loc = motion_data[motion_step];
	clock_div_count = 0;
	seq_render_ball();
}

// renders the ball on the playfield
void seq_render_ball(void) {
	unsigned char pix = 0x01;
	unsigned char ball_x = motion_step_loc;
	unsigned char ball_y = (ball_x >> 4) & 0x0f;
	ball_x = (ball_x & 0x0f);
	if(ball_x) pix = (pix << ball_x);
	panel_clear_fg();
	panel_draw_fg(ball_y, pix);
}

// draw a symbol to the overlay layer
void seq_draw_motion_overlay(void) {
	unsigned char temp = 1;
	if(motion_type & 0x07) temp = (temp << (motion_type & 0x07));
	panel_clear_ol();
	panel_draw_ol(0, temp);
	panel_draw_ol(1, temp);
	panel_draw_ol(2, temp);
	panel_draw_ol(3, temp);
	panel_draw_ol(4, temp);
	panel_draw_ol(5, temp);
	panel_draw_ol(6, temp);
	panel_draw_ol(7, temp);
	panel_draw_ol(motion_type >> 3, 0xff);
	panel_set_ol_timeout(300);
}

// load a motion into ram
void seq_load_motion(void) {
	// only load preprogrammed stuff
	if(motion_type < 48) {
		random_seed_count = 255;  // prevent motion data randomizing
		flash_read(MEM_MOTION + (motion_type * 64), motion_data);
		random_mask = 0xff;
//		// find the length of the sequence - this is required for motion length wrap around code
//		motion_data_len = 0;
//		while(motion_data[motion_data_len] != 255 && motion_data_len < 64) {
//			motion_data_len ++;
//		}
//		if(motion_data_len == 0) {
//			motion_data_len = 1;
//		}
	}
	// reset random setup stuff
	else {
		motion_step = 0;
		random_seed_count = 0;  // trigger motion data randomizing
		if(motion_type < 56) random_seed_repeat = 0;
		else random_seed_repeat = 1;
		// set up mask
		random_mask = random_masks[motion_type & 0x07];
	}
}

// load a pattern into ram
void seq_set_pattern(void) {
	unsigned char i;
	unsigned short temp;
	for(i = 0; i < 8; i += 2) {
		temp = flash_read(MEM_PATTERN + (pattern_type * 8) + i);
		pattern_data[i] = temp & 0xff;
		pattern_data[i + 1] = temp >> 8;
		panel_draw_bg(i, pattern_data[i]);
		panel_draw_bg(i + 1, pattern_data[i + 1]);
	}
}

// set up the scale
void seq_set_scale(void) {
	unsigned char i;
	unsigned short temp;
	unsigned char xy_scale = 0;
	if(tonality == TONALITY_MINOR && span == SPAN_SMALL) {
		flash_read(MEM_SCALE_MINOR_SMALL, scale_data);
		xy_scale = 0;
	}
	else if(tonality == TONALITY_MINOR && span == SPAN_LARGE) {
		flash_read(MEM_SCALE_MINOR_LARGE, scale_data);
		xy_scale = 1;
	}
	else if(tonality == TONALITY_MAJOR && span == SPAN_SMALL) {
		flash_read(MEM_SCALE_MAJOR_SMALL, scale_data);
		xy_scale = 2;
	}
	else if(tonality == TONALITY_MAJOR && span == SPAN_LARGE) {
		flash_read(MEM_SCALE_MAJOR_LARGE, scale_data);
		xy_scale = 3;
	}
	for(i = 0; i < 8; i += 2) {
		temp = flash_read(MEM_XY_SCALE + (xy_scale * 8) + i);
		xy_scale_data[i] = temp & 0xff;
		xy_scale_data[i + 1] = temp >> 8;
	}
}

// note on / send X/Y
void seq_note_on(unsigned char note) {
//	signed char stemp;
	int temp;
	if(keyboard_trigger && keyboard_cur_note == 255) return;
	// CV/gate mode
	if(output_mode == OUTPUT_MODE_CV) {
#ifdef EURORACK
		// note range = 48-96, shifted range = 16-127, plus note offset
		temp = note + output_offset - 32 + midi_base_note;  // range = 64 - eurorack
#endif
#ifdef BUCHLA
		// note range = 48-96, shifted range = 32-111, plus note offset
		temp = note + output_offset - 16 + midi_base_note;  // range = 32 - buchla
#endif
		// clamp note range
		if(temp < 0) current_note = 0;
		else if(temp > 127) current_note = 127;
		else current_note = temp;
		panel_set_dac0(note_lookup[current_note]);  // CV
		panel_set_dac1(PANEL_GATE_LEVEL_ON);  // gate on
#ifdef BUCHLA
		panel_set_dac1_gate_pulse_len(2);  // 4ms buchla
#endif
		_midi_tx_note_on(pattern_midi_get_channel(), 
		current_note, 100);  // use velocity 100
	}
	// X/Y mode
	else {
#ifdef EURORACK
		// scale data range = 0-127, offset range = -32 - +31, offset range = -64 to +63
		temp = xy_scale_data[(motion_step_loc & 0x07)] + (output_offset << 1) - 64;
#endif
#ifdef BUCHLA
		// scale data range = 0-127, offset range = -16 - +15, offset range = -64 to +63
		temp = xy_scale_data[(motion_step_loc & 0x07)] + (output_offset << 2) - 64;
#endif
		// clamp X/Y range
		if(temp < 0) temp = 0;
		else if(temp > 127) temp = 127;
		// range -5V to +5V / 0-10V nominal (with 0-127 input value)
		panel_set_dac0(PANEL_DAC_LEVEL_LOW - (temp << 5));  // 4064 is max val of temp << 5
		_midi_tx_control_change(pattern_midi_get_channel(), MIDI_CC_X, temp);
#ifdef EURORACK
		// scale data range = 0-127, offset range = -32 - +31, offset range = -64 to +63
		temp = xy_scale_data[((motion_step_loc >> 4) & 0x07)] + (output_offset << 1) - 64;
#endif
#ifdef BUCHLA
		// scale data range = 0-127, offset range = -16 - +15, offset range = -64 to +63
		temp = xy_scale_data[((motion_step_loc >> 4) & 0x07)] + (output_offset << 2) - 64;
#endif
		// clamp X/Y range
		if(temp < 0) temp = 0;
		else if(temp > 127) temp = 127;
		// range -5V to +5V / 0-10V nominal (with 0-127 input value)
		panel_set_dac1(PANEL_DAC_LEVEL_LOW - (temp << 5));  // 4064 is max val of temp << 5
		_midi_tx_control_change(pattern_midi_get_channel(), MIDI_CC_Y, temp);
	}
}

// note off - current note reset to 0
void seq_note_off(void) {
	// CV/gate mode
	if(output_mode == OUTPUT_MODE_CV) {
		panel_set_dac1(PANEL_GATE_LEVEL_OFF);  // gate off
		_midi_tx_note_off(pattern_midi_get_channel(), current_note);
		current_note = 0;
		gate_time_count = 0;
	}
}

// kill a note externally
void seq_kill_note(void) {
	if(current_note) seq_note_off();
	// send all notes off
	_midi_tx_control_change(pattern_midi_get_channel(), 123, 0);
}

// switch direction from MIDI
void seq_midi_dir(unsigned char dir_sw) {
	if(dir_sw) midi_dir_sw = 1;
	else midi_dir_sw = 0;
}

// MIDI note on
void seq_midi_note_on(unsigned char note) {
	// change the base note
	midi_base_note = (signed char)note - 60;
	if(keyboard_trigger) {
		// no note was playing so we want to reset the song
		if(keyboard_cur_note == 255) {
			clock_ctrl_reset();
			seq_reset_song();
		}
		keyboard_cur_note = note;
	}
}

// MIDI note off
void seq_midi_note_off(unsigned char note) {
	if(note == keyboard_cur_note) {
		keyboard_cur_note = 255;  // reset to default
	}
}

// change the motion pattern
void seq_motion_change(unsigned char motion) {
	motion_type = (motion & 0x3f);
	if(motion & 0x40) keyboard_trigger = 1;
	else keyboard_trigger = 0;
	seq_draw_motion_overlay();
	seq_load_motion();
}

// reset the clock because we changed modes
void seq_clock_reset(void) {
	unsigned char temp;
	clock_div_count = 0;
	// clock_div 
	if(clock_ctrl_is_int()) {
		clock_div_new = clock_div_table[3];  // 4 bpq
		clock_div = clock_div_new;
	}
	else {
		temp = (panel_get_pot(PANEL_CLOCK_POT) >> 5);
		clock_div_new = clock_div_table[temp];
		clock_div = clock_div_new;
	}
}

// change a modulation parameter
void seq_control_change(unsigned char mod, unsigned char value) {
	if(mod > SEQ_MOD_MAX) return;
	if(mod == SEQ_MOD_PATTERN_TYPE) {
		pattern_type_override = (value & 0x7f) >> 2;
	}
	else if(mod == SEQ_MOD_MOTION_START) {
		motion_start_override = (value & 0x7f) >> 1;
	}
	else if(mod == SEQ_MOD_MOTION_LEN) {
		motion_len_override = (value & 0x7f) >> 1;
	}
}

// live update the pattern
void seq_pattern_live_update(unsigned char buf[]) {
	unsigned char i;
	for(i = 0; i < 8; i ++) {
		pattern_data[i] = buf[i] & 0xff;
		panel_draw_bg(i, pattern_data[i]);
	}
}

