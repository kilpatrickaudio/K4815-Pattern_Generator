/*
 * K1600 MIDI Converter - SYSEX Handler
 *
 * Copyright 2010: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 * Version: 1.1
 *
 */
#include <system.h>
#include <flash.h>
#include "sysex.h"
#include "midi.h"
#include "seq.h"

#define SYSEX_UPDATE_PATTERN 0x02
#define SYSEX_UPDATE_MOTION 0x03
#define SYSEX_UPDATE_SCALE 0x04

// local functions
void sysex_write_flash_buf(int addr, unsigned char buf[], int len);

// init the sysex code
void sysex_init(void) {
	// currently no K4815 sysex commands
	// just echo everything through
	// keep this here for future use
}

// handle sysex message received
void sysex_rx_msg(unsigned char data[], unsigned char len) {
	unsigned char temp, buf[64], buf2[64], i, item;
//	// echo other Kilpatrick Audio messages
//	_midi_tx_sysex_msg(data, len);
	if(len < 5) return;

	// update a pattern
	if(data[4] == SYSEX_UPDATE_PATTERN) {
		if(len != 22) return;
		item = data[5];  // pattern num
		if(item >= 32) return;
		// get bytes
		temp = 0;
		for(i = 0; i < 16; i += 2) {
			buf[temp] = (data[i+6] << 4) & 0xf0;  // high byte
			buf[temp] |= (data[i+7] & 0x0f);  // low byte
			temp ++;
		}
		// update the flash
		sysex_write_flash_buf(MEM_PATTERN + (item << 3), buf, 8);
		// cause current pattern to reload
		seq_set_pattern(); 
	}
	// update a motion
	else if(data[4] == SYSEX_UPDATE_MOTION) {
		if(len != 70) return;
		item = data[5];  // motion num
		if(item >= 48) return;
		for(i = 0; i < 64; i ++) {
			temp = data[i+6] & 0x7f;
			if(temp == 0x7f) {
				temp = 0xff;
			}
			buf[i] = temp;
		}
		// check if the first step is null - it can't be
		if(buf[0] == 0xff) {
			buf[0] = 0;
		}
		sysex_write_flash_buf(MEM_MOTION + (item << 6), buf, 64);
		// cause the current motion to reload
		seq_load_motion();
	}
	else if(data[4] == SYSEX_UPDATE_SCALE) {
		if(len != 14) return;
		item = data[5];  // scale num
		if(item >= 2) return;
		// build "small" from incoming bytes
		for(i = 0; i < 8; i ++) {
			temp = data[i+6] & 0x0f;
			if(temp > 12) temp -= 12;
			buf[i] = temp + 60;
			buf[i+8] = temp + 72;
			buf[i+16] = temp + 60;
			buf[i+24] = temp + 72;
			buf[i+32] = temp + 60;
			buf[i+40] = temp + 72;
			buf[i+48] = temp + 60;
			buf[i+56] = temp + 72;
			buf2[i] = temp + 48;
			buf2[i+8] = temp + 60;
			buf2[i+16] = temp + 72;
			buf2[i+24] = temp + 84;
			buf2[i+32] = temp + 48;
			buf2[i+40] = temp + 60;
			buf2[i+48] = temp + 72;
			buf2[i+56] = temp + 84;
		}
		// minor
		if(item == 0) {
			pir2.EEIF = 0;  // clear write complete flag
			sysex_write_flash_buf(MEM_SCALE_MINOR_SMALL, buf, 64);
			while(pir2.EEIF == 0) clear_wdt();
			pir2.EEIF = 0;  // clear write complete flag
			sysex_write_flash_buf(MEM_SCALE_MINOR_LARGE, buf2, 64);		
			while(pir2.EEIF == 0) clear_wdt();
		}
		// major
		else if(item == 1) {
			pir2.EEIF = 0;  // clear write complete flag
			sysex_write_flash_buf(MEM_SCALE_MAJOR_SMALL, buf, 64);
			while(pir2.EEIF == 0) clear_wdt();
			pir2.EEIF = 0;  // clear write complete flag
			sysex_write_flash_buf(MEM_SCALE_MAJOR_LARGE, buf2, 64);
			while(pir2.EEIF == 0) clear_wdt();
		}
		seq_set_scale();
	}
}

// update some flash mem
void sysex_write_flash_buf(int addr, unsigned char buf[], int len) {
	unsigned char cache[64];
	unsigned char i, outcount;
	unsigned int block_addr = addr & 0xffc0;  // figure out which 64 byte offset we're on
	if(len > 64) return;

	intcon.GIE = 0;

	// read the data from the block - this reads 64 bytes
	flash_read(block_addr, cache);
	// modify the data to write back
	outcount = addr & 0x3f;
	for(i = 0; i < len; i ++) {
		cache[outcount++] = buf[i];
	}
	// erase 64 byte block
	tblptru = (block_addr >> 16) & 0x0f;
	tblptrh = (block_addr >> 8) & 0xff;
	tblptrl = block_addr & 0xff;
	_asm {
		bsf	_eecon1, EEPGD
		bcf _eecon1, CFGS
		bsf _eecon1, WREN
		bsf _eecon1, FREE
//		bcf _intcon, GIE
		movlw 0x55 ; unlock
		movwf _eecon2
		movlw 0xaa
		movwf _eecon2
		bsf _eecon1, WR ; start the write
//		bsf _intcon, GIE
		tblrd*-
	}

	// load data - first block of 32 bytes
	for(i = 0; i < 32; i++) {
		tablat = cache[i];
		_asm tblwt+*
	}

	// write flash
	_asm {
		bsf	_eecon1, EEPGD
		bcf _eecon1, CFGS
		bsf _eecon1, WREN
//		bcf _intcon, GIE
		movlw 0x55 ; Unlock
		movwf _eecon2
		movlw 0xaa
		movwf _eecon2
		bsf _eecon1, WR ; start the write
//		bsf _intcon, GIE
		bcf _eecon1, WREN
	}

	// load data - second block of 32 bytes
	for(i = 0; i < 32; i++) {
		tablat = cache[i+32];
		_asm tblwt+*
	}

	// write flash
	_asm {
		bsf	_eecon1, EEPGD
		bcf _eecon1, CFGS
		bsf _eecon1, WREN
//		bcf _intcon, GIE
		movlw 0x55 ; Unlock
		movwf _eecon2
		movlw 0xaa
		movwf _eecon2
		bsf _eecon1, WR ; start the write
//		bsf _intcon, GIE
		bcf _eecon1, WREN
	}

	intcon.GIE = 1;
}
