/*
 * K1600 MIDI Converter - Config Storage
 *
 * Copyright 2010: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 * Version: 1.0
 *
 */
#define CONFIG_MAX 1
#define CONFIG_MIDI_CHANNEL 0x00

// init the config store
void config_store_init(void);

// run the config storage task
void config_store_timer_task(void);

// gets a config byte
unsigned char config_store_get_val(unsigned char addr);

// sets a config byte
void config_store_set_val(unsigned char addr, unsigned char val);
