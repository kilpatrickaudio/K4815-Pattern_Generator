/*
 * K4815 Pattern Generator - Sequencer Code
 *
 * Copyright 2010: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 * Version: 1.1
 *
 */
// CC mods
#define SEQ_MOD_PATTERN_TYPE 0
#define SEQ_MOD_MOTION_START 1
#define SEQ_MOD_MOTION_LEN 2
#define SEQ_MOD_MAX 2

// pattern / motion geometry
#define SEQ_MAX_PATTERN 32

// flash lookup table offsets
#define MEM_SCALE_MINOR_SMALL 0x6000
#define MEM_SCALE_MINOR_LARGE 0x6040
#define MEM_SCALE_MAJOR_SMALL 0x6080
#define MEM_SCALE_MAJOR_LARGE 0x60c0
#define MEM_XY_SCALE 0x6100
#define MEM_PATTERN 0x6200
#define MEM_MOTION 0x5000

void seq_init(void);
void seq_timer_task(void);
void seq_clock_change(unsigned char phase);
void seq_reset_song(void);
void seq_kill_note(void);
void seq_midi_dir(unsigned char);
void seq_midi_note_on(unsigned char);
void seq_midi_note_off(unsigned char);
void seq_motion_change(unsigned char);
void seq_clock_reset(void);
void seq_control_change(unsigned char mod, unsigned char value);
void seq_load_motion(void);
void seq_set_pattern(void);
void seq_set_scale(void);

