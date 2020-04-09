/*
 * K4815 Pattern Generator - Panel Controller
 *
 * Copyright 2010: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 * Version: 1.1
 *
 */
#define PANEL_GATE_POT 0
#define PANEL_CLOCK_POT 1
#define PANEL_PLAY_LEN_POT 2
#define PANEL_PATTERN_POT 3
#define PANEL_OUTPUT_POT 4

#define PANEL_CLOCK_SW 0
#define PANEL_DIR_SW 1
#define PANEL_TONALITY_SW 2
#define PANEL_SPAN_SW 3
#define PANEL_OUTPUT_SW 4

#ifdef EURORACK
#define PANEL_DAC_LEVEL_LOW 4080  // eurorack
#define PANEL_GATE_LEVEL_OFF 2047  // eurorack
#define PANEL_GATE_LEVEL_ON 0		 // eurorack
#endif
#ifdef BUCHLA
#define PANEL_DAC_LEVEL_LOW 4085  // buchla  
#define PANEL_GATE_LEVEL_OFF 4085  // buchla
#define PANEL_GATE_LEVEL_SUSTAIN 1935  // buchla
#define PANEL_GATE_LEVEL_ON  0  // buchla
#endif

// functions
void panel_init(void);
void panel_timer_task(void);
void panel_clear_ol(void);
void panel_clear_fg(void);
void panel_clear_bg(void);
void panel_hard_stop(void);
void panel_draw_ol(unsigned char, unsigned char);
void panel_draw_fg(unsigned char, unsigned char);
void panel_draw_bg(unsigned char, unsigned char);
void panel_set_ol_timeout(unsigned int);
unsigned char panel_get_pot(unsigned char);
unsigned char panel_get_switch(unsigned char);
unsigned char panel_set_clock_led(unsigned char);
signed char panel_get_encoder(void);
unsigned char panel_get_encoder_sw(void);
unsigned char panel_get_dir_in(void);
unsigned char panel_get_reset_in(void);
void panel_set_dac0(unsigned int);
void panel_set_dac1(unsigned int);
#ifdef BUCHLA
void panel_set_dac1_gate_pulse_len(unsigned char timeout);
#endif
void panel_set_popup_num(unsigned char);
