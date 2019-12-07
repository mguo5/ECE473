// lm73_functions.c       
// Roger Traylor 11.28.10

#include <util/twi.h>
#include "lm73_functions.h"
#include <util/delay.h>
#include <stdlib.h>

//TODO: remove volatile type modifier?  I think so.
//TODO: initalize with more resolution and disable the smb bus timeout
//TODO: write functions to change resolution, alarm etc.

volatile uint8_t lm73_wr_buf[2];
volatile uint8_t lm73_rd_buf[2];

//********************************************************************************

//******************************************************************************
void lm73_temp_convert(char temp_digits[], uint16_t lm73_temp, uint8_t f_not_c){
//given a temperature reading from an LM73, the address of a buffer
//array, and a format (deg F or C) it formats the temperature into ascii in 
//the buffer pointed to by the arguement.
//TODO:Returns what???(uint8_t)??? Probably a BUG?

	uint16_t temperature = lm73_temp / 128;

	if(f_not_c == 0x01){
		temperature = (temperature * 9)/5 + 32;
	}

	itoa((int)temperature, temp_digits, 10);

}//lm73_temp_convert
//******************************************************************************
