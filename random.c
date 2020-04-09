/*
 * Random Number Code
 *
 * Copyright 2010: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 * Version: 1.1
 *
 */
#include <system.h>
#include "random.h"

unsigned char random_u;
unsigned char random_h;
unsigned char random_l;

void rand_do(void);

// seed the number generator
void seed_rand(unsigned char seed) {
	random_u = 0xd7;
	random_h = 0xd7;
	random_l = 0xd7;
}

// get the next random number
unsigned char get_rand(void) {
	rand_do();
	return random_l;
}

void rand_do(void) {
	asm {
		BCF     _status, C 
   		RRCF    _random_u,F
   		RRCF    _random_h,F
   		RRCF    _random_l,F
   		BTFSS   _status, C
   		RETURN
   		MOVLW   0xD7
   		XORWF   _random_u, F
   		XORWF   _random_h, F
   		XORWF   _random_l, F
   		RETURN
	} 
}


