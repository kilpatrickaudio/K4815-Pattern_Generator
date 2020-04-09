/*
 * K4815 Pattern Generator - Config Storage
 *
 * Copyright 2010: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 * Version: 1.0
 *
 */
#include <system.h>
#include "config_store.h"
#include "sysex.h"

unsigned char config[CONFIG_MAX];
unsigned char config_write;
unsigned char config_check_count;

// local functions
void config_eeprom_write(unsigned char, unsigned char);
unsigned char config_eeprom_read(unsigned char);


// init the config store
void config_store_init(void) {
	unsigned char i;
	// load back all settings from EEPROM
	for(i = 0; i < CONFIG_MAX; i ++) {
		config[i] = config_eeprom_read(i);
	}
	config_write = 0;
	config_check_count = 0;
}

// run the config storage task
void config_store_timer_task(void) {
	// are we writing?
	if(config_write) {
		// write is complete
		if(pir2.EEIF == 1) pir2.EEIF = 0;
		// write is in progress
		else return;
	}

	// check a value and write if changed
	if(config[config_check_count] != 
			config_eeprom_read(config_check_count)) {
		config_eeprom_write(config_check_count, config[config_check_count]);
	}
	config_check_count ++;
	if(config_check_count == CONFIG_MAX) config_check_count = 0;
}

// gets a config byte
unsigned char config_store_get_val(unsigned char addr) {
	if(addr > 0x3f) return 0;
	return config[addr];
}

// sets a config byte
void config_store_set_val(unsigned char addr, unsigned char val) {
	if(addr > 0x3f) return;
	config[addr] = val;
}

// read the EEPROM
unsigned char config_eeprom_read(unsigned char addr) {
	eeadr = addr;
	eecon1.EEPGD = 0;
	eecon1.CFGS = 0;
	eecon1.RD = 1;
	return eedata;
}

// write the EEPROM
void config_eeprom_write(unsigned char addr, unsigned char val) {
	eeadr = addr;
	eedata = val;
	eecon1.EEPGD = 0;
	eecon1.CFGS = 0;
	eecon1.WREN = 1;
//	intcon.GIE = 0;  // disable interrupts
	eecon2 = 0x55;  // required sequence
	eecon2 = 0xaa;  // required sequence
	eecon1.WR = 1;  // begin write
//	intcon.GIE = 1;  // enable interrupts
}
