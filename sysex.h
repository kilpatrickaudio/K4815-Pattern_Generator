/*
 * K4815 MIDI Converter - SYSEX Handler
 *
 * Copyright 2010: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 * Version: 1.1
 *
 */
// init the sysex code
void sysex_init(void);

// handle sysex message received
void sysex_rx_msg(unsigned char data[], unsigned char len);
