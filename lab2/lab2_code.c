// lab2_skel.c 
// R. Traylor
// 9.12.08

//  HARDWARE SETUP:
//  PORTA is connected to the segments of the LED display. and to the pushbuttons.
//  PORTA.0 corresponds to segment a, PORTA.1 corresponds to segement b, etc.
//  PORTB bits 4-6 go to a,b,c inputs of the 74HC138.
//  PORTB bit 7 goes to the PWM transistor base.

#define F_CPU 16000000 // cpu speed in hertz 
#define TRUE 1
#define FALSE 0
#include <avr/io.h>
#include <util/delay.h>

//holds data to be sent to the segments. logic zero turns segment on
uint8_t segment_data[5] 

//decimal to 7-segment LED display encodings, logic "0" turns on segment
uint8_t dec_to_7seg[12] 


//******************************************************************************
//                            chk_buttons                                      
//Checks the state of the button number passed to it. It shifts in ones till   
//the button is pushed. Function returns a 1 only once per debounced button    
//push so a debounce and toggle function can be implemented at the same time.  
//Adapted to check all buttons from Ganssel's "Guide to Debouncing"            
//Expects active low pushbuttons on PINA port.  Debounce time is determined by 
//external loop delay times 12. 
//
uint8_t chk_buttons(uint8_t button) {
	static unint16_t state = 0;
	state = (state << 1) | (!bit_is_clear(PINA, button)) | 0xE000;
	if(state == 0xF000) return 1;
	return 0;

}
//******************************************************************************

//***********************************************************************************
//                                   segment_sum                                    
//takes a 16-bit binary input value and places the appropriate equivalent 4 digit 
//BCD segment code in the array segment_data for display.                       
//array is loaded at exit as:  |digit3|digit2|colon|digit1|digit0|
void segsum(uint16_t sum) {
	uint8_t ones = -1;
	uint8_t tens = -1;
	uint8_t hundreds = -1;
	uint8_t thousands = -1;
  //determine how many digits there are
	if(sum < 10 && sum > 0){
		ones = sum;
	
	}
	else if(sum < 100 && sum > 9){
		ones = sum % 10;
		tens = sum / 10;
	
	}
	else if(sum < 1000 && sum > 99){
		ones = (sum % 100) % 10;
		tens = (sum % 100) / 10;
		hundreds = sum / 100;

	}
	else if(sum <= 1023){
		ones = (sum % 1000) % 10;
		tens = (sum % 1000) / 10;
		hundreds = (sum % 1000) / 100;
		thousands = sum / 1000;
	
	}
  //break up decimal sum into 4 digit-segments
  //blank out leading zero digits 
  //now move data to right place for misplaced colon position
	segment_data[0] = thousands;
	segment_data[1] = hundreds;
	segment_data[2] = -1;
	segment_data[3] = tens;
	segment_data[4] = ones;
}//segment_sum
//***********************************************************************************


uint8_t seven_seg_encoding(uint8_t num){

	uint8_t output = 0xFF;
	switch(num){
		
		case -1: output = 0b11111111;
				 break;

		case 0: output = 0b11000000;
				break;

		case 1: output = 0b11111001;
				break;

		case 2: output = 0b10100100;
				break;

		case 3: output = 0b10110000;
				break;

		case 4: output = 0b10011001;
				break;

		case 5: output = 0b10010010;
				break;

		case 6: output = 0b10000010;
				break;

		case 7: output = 0b11111000;
				break;

		case 8: output = 0b10000000;
				break;

		case 9: output = 0b10011000;
				break;

		default: output = 0b11111111;
	
	
	}

	return output;

}

//***********************************************************************************
uint8_t main()
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
    PORTB = (7 << 4);
  //now check each button and increment the count as needed
	for(int i_buttons = 0; i_buttons < 8; i_buttons++){
		if(chk_buttons(i_buttons)){
			count += pow(2, i_buttons);
		}
	
	}
  //disable tristate buffer for pushbutton switches
    PORTB = 0x00;
  //bound the count to 0 - 1023
    while(count > 1023){
		count -= 1024;
	}
  //break up the disp_value to 4, BCD digits in the array: call (segsum)
    segsum(count);
  //bound a counter (0-4) to keep track of digit to display 
  //make PORTA an output
	DDRA = 0xFF;
  //send 7 segment code to LED segments
	for(int i_seg = 0; i_seg < 5, i_seg++){
		encoding = seven_seg_encoding(segment_data[i_seg]);
		PORTB = (i_seg << 4);
		PORTA = encoding;		
	
	}
  //send PORTB the digit to display
  //update digit to display
  }//while
}//main
