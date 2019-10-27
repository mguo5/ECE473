/*****************************************************************************
* Author: Matthew Guo
* Class: ECE 473
* Date Due: 10/29/2019
* Lab Number: Lab 3
* School: Oregon State University
* Description: This is Lab 2 for ECE 473. A seven segment display is used to
*              display a count number. This is done by parsing the count number
*              from the ones place, tens place, hundreds place, and thousands place.
*              There cannot be any leading 0s. A push button board with 8 buttons
*              is used to increase the count, thereby increasing the number displayed
*              on the LED board. The first button increases the number by 1, the second
*              button increases the count by 2, the third button increases the count by 4,
*              and so on.
*****************************************************************************/

//  HARDWARE SETUP:
//  PORTA is connected to the segments of the LED display. and to the pushbuttons.
//  PORTA.0 corresponds to segment a, PORTA.1 corresponds to segement b, etc.
//  PORTB bits 4-6 go to a,b,c inputs of the 74HC138.
//  PORTB bit 7 goes to the PWM transistor base.


//  WIRING SETUP:
//  Button Board:
//      - COM_EN is tied to SEG7 on the LED Board to enable/disable tristate buffer
//      - COM_LVL is tied to GND
//      - SW_COM is left floating
//      - J1 - J8 is connected to A-G and DP
//  LED Board:
//      - Y7 is connected to the COM_EN
//      - PORTA is connected to the 7 segments
//      - PORTB bits 4-6 is used to control the digit select

#define F_CPU 16000000 // cpu speed in hertz 
#define TRUE 1
#define FALSE 0
#include <avr/io.h>
#include <util/delay.h>

//holds data to be sent to the segments. logic zero turns segment on
int8_t segment_data[5]; 

//decimal to 7-segment LED display encodings, logic "0" turns on segment
uint8_t dec_to_7seg[12]; 


//******************************************************************************
//                            chk_buttons                                      
//Checks the state of the button number passed to it. It shifts in ones till   
//the button is pushed. Function returns a 1 only once per debounced button    
//push so a debounce and toggle function can be implemented at the same time.  
//Adapted to check all buttons from Ganssel's "Guide to Debouncing"            
//Expects active low pushbuttons on PINA port.  Debounce time is determined by 
//external loop delay times 12. 
//NOTE: This function was taken from lab1 for the class, modified so that it can be
//      used for all of the buttons on PORTA
//
uint8_t chk_buttons(uint8_t button) {
	//make state an array to hold different state values for different buttons
	static uint16_t state[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	state[button] = (state[button] << 1) | (bit_is_clear(PINA, button)) | 0xE000;
	if(state[button] == 0xF000) return 1;
	return 0;

}
//******************************************************************************

//***********************************************************************************
//                                   segment_sum                                    
//takes a 16-bit binary input value and places the appropriate equivalent 4 digit 
//BCD segment code in the array segment_data for display.                       
//array is loaded at exit as:  |digit3|digit2|colon|digit1|digit0|
//NOTE: -1 is used as a way to determine leading 0s necessary to turn them off in the case statement
//       for the seven_seg_encoding() function
void segsum(uint16_t sum) {
	//initialize variables to be used in this function to -1, which makes LEDs go off
	int8_t ones = -1;
	int8_t tens = -1;
	int8_t hundreds = -1;
	int8_t thousands = -1;
  //determine how many digits there are
	//check to see if the total sum count is less than 10 for parsing
	if(sum < 10){
		ones = sum;
	
	}
	//check to see if the total sum count less than 100 but at or greater than 10 for parsing
	else if(sum < 100 && sum > 9){
		ones = sum % 10;
		tens = sum / 10;
	
	}
	//check to see if the total sum count is less than 100 but at or greater than 100 for parsing
	else if(sum < 1000 && sum > 99){
		ones = (sum % 100) % 10;
		tens = (sum % 100) / 10;
		hundreds = sum / 100;

	}
	//check to see if the total sum count is less than 1024 but at or greater than 1000 for parsing
	else if(sum <= 1023){
		ones = (sum % 1000) % 10;
		tens = (sum % 1000) / 10;
		hundreds = (sum % 1000) / 100;
		thousands = sum / 1000;
	
	} 
	//place the variables into the segment_data[] array to be displayed
	segment_data[0] = ones;
	segment_data[1] = tens;
	segment_data[2] = -1;
	segment_data[3] = hundreds;
	segment_data[4] = thousands;
}//segment_sum
//***********************************************************************************
									//seven_seg_encoding
//This function is used to implement the binary encodings for the seven segment
//display by using a case statement. It takes in each individual ones, tens, hundreds,
//thousands place integers, and from there, use the number to display the needed
//segments on the LED display to represent those numbers.
//
//***********************************************************************************

uint8_t seven_seg_encoding(int8_t num){
	//initially initialize the output to be 0xFF, which turns off everything for the display
	uint8_t output = 0xFF;
	switch(num){
		
		case -1: 
			//displays nothing on the seven segment display
			output = 0b11111111;
			break;

		case 0:
			//displays 0 on the seven segment display
			output = 0b11000000;
			break;

		case 1: 
			//displays 1 on the seven segment display
			output = 0b11111001;
			break;

		case 2: 
			//displays 2 on the seven segment display
			output = 0b10100100;
			break;

		case 3: 
			//displays 3 on the seven segment display
			output = 0b10110000;
			break;

		case 4: 
			//displays 4 on the seven segment display
			output = 0b10011001;
			break;

		case 5: 
			//displays 5 on the seven segment display
			output = 0b10010010;
			break;

		case 6: 
			//displays 6 on the seven segment display
			output = 0b10000010;
			break;

		case 7: 
			//displays 7 on the seven segment display
			output = 0b11111000;
			break;

		case 8: 
			//displays 8 on the seven segment display
			output = 0b10000000;
			break;

		case 9: 
			//displays 9 on the seven segment display
			output = 0b10011000;
			break;

		case 10:
			//displays A on the seven segment display
			output = 0b10001000;
			break;

		case 11:
			//displays B on the seven segment display
			output = 0b10000011;
			break;

		case 12:
			//displays C on the seven segment display
			output = 0b11000110;
			break;

		case 13:
			//displays D on the seven segment display
			output = 0b00100001;
			break;

		case 14:
			//displays E on the seven segment display
			output = 0b10000110;
			break;

		case 15:
			//displays F on the seven segment display
			output = 0b10001110;
			break;
			

		default: 
			//defaults to 0xFF, so that nothing shows on seven seg display
			output = 0b11111111;
			break;	
	
	}

	return output;	//return the seven segment display encoding

}//seven_seg_encoding()
//***********************************************************************************

//***********************************************************************************
int main()
{
//set port bits 4-7 B as outputs
DDRB = 0xF0;
uint16_t count = 0;
uint8_t encoding = 0;
while(1){
  //insert loop delay for debounce
	_delay_ms(2);
  //make PORTA an input port with pullups
	DDRA = 0x00;
	PORTA = 0xFF;	
  //enable tristate buffer for pushbutton switches
    PORTB = 0x70;
  //now check each button and increment the count as needed
	//use a for loop to increment through each button to check
	for(uint8_t i_buttons = 0; i_buttons < 8; i_buttons++){
		if(chk_buttons(i_buttons)){
			count += (1 << (i_buttons));		//makes S1 add 1, S2 add 2, S3 add 4, etc, using binary shift
		}
	
	}
  //disable tristate buffer for pushbutton switches
    PORTB = 0x60;
  //bound the count to 0 - 1023
	//uses while loop to check if count is greater than 1024, and subtract 1024 to it, making 1024 -> 0
    while(count > 1023){
		count -= 1023;
	}
  //break up the disp_value to 4, BCD digits in the array: call (segsum)
    segsum(count);
  //make PORTA an output
	DDRA = 0xFF;
	//uses "nop" to add a little delay
	asm volatile ("nop");
  //send 7 segment code to LED segments
	//calls seven_seg_encoding function to grab seven seg encoding, then displays it
	//based on the parsed number of the overall count
	for(int i_seg = 0; i_seg < 5; i_seg++){
		encoding = seven_seg_encoding(segment_data[i_seg]);
		PORTB = (i_seg << 4);			//output onto PORTB to select segment digit
		PORTA = encoding;				//output the encoding value to PORTA for seven seg display
		_delay_ms(2);					//add in tiny delay, but not large enough for flicker
	
	}

  }//while
}//main
