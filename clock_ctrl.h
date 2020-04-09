/*
 * K4815 Pattern Generator - Clock Controller
 *
 * Copyright 2010: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 * Version: 1.1
 *
 */
void clock_ctrl_init(void);
void clock_ctrl_timer_task(void);
void clock_ctrl_int(void);
void clock_ctrl_ext_pulse(void);
void clock_ctrl_midi_tick(void);
void clock_ctrl_midi_pos(unsigned char);
void clock_ctrl_midi_start(void);
void clock_ctrl_midi_continue(void);
void clock_ctrl_midi_stop(void);
unsigned char clock_ctrl_is_int(void);
void clock_ctrl_reset(void);
