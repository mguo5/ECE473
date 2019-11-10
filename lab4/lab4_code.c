/*****************************************************************************
* Author: Matthew Guo
* Class: ECE 473
* Date Due: 10/29/2019
* Lab Number: Lab 4
* School: Oregon State University
* Description: This is Lab 3 for ECE 473. A seven segment display is used to
*              display a count number. The displayed number can be a decimal
*				or a hex value. Encoders are read via the SPI bus to increment
*				or decrement the count as necessary. Different modes (add 1, add 2,
*				or add 4) are triggered by the push buttons. A bar graph display
*				is used to indicate the mode of operation.
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
#include <avr/interrupt.h>
#include "hd44780.h"

//holds data to be sent to the segments. logic zero turns segment on
int8_t segment_data[5]; 

//decimal to 7-segment LED display encodings, logic "0" turns on segment
uint8_t dec_to_7seg[12]; 

volatile int count = 0;

uint8_t encoder_left = 0;
uint8_t encoder_right = 0;

uint8_t adjust_flag = 0;
uint8_t input_flag = 0;
uint8_t pm_flag = 0;
uint8_t hour24_flag = 0;
uint8_t trigger_alarm = 0;
uint8_t adjust_alarm = 0;
uint8_t alarm_match_count = 0;
volatile uint8_t isr_count = 0;
volatile uint8_t sec_count = 0;
volatile uint8_t min_count = 0;
volatile uint8_t hour_count = 0;

uint8_t temp_min = 0;
uint8_t temp_hour = 0;
uint8_t temp_pm_flag = 0;
uint8_t alarm_time_sec = 0;
uint8_t alarm_time_min = 0;
uint8_t alarm_time_hour = 0;


/**********************************************************************
* Function: real_time
* Parameters: none
* Description: Call this function to obtain the compile time of the
* program. This is done to initialize sec_count, min_count, and hour_count
* to the right start up time.
**********************************************************************/
void real_time(){

sec_count = (__TIME__[6]-48)*10 + (__TIME__[7]-48);		//get real time seconds
min_count = (__TIME__[3]-48)*10 + (__TIME__[4]-48);		//get real time minutes
hour_count = (__TIME__[0]-24)*10 + (__TIME__[1]-48);	//get real time hours in 24 hour format

//check if it is am or pm, set pm_flag if necessary
if(hour_count > 12){
	hour_count -= 12;
	pm_flag = 0x01;
}
}//real_time()


/************************************************************************
 * Function: initialization
 * Parameters: none
 * Description: Call this function to initialize the Port B, Port D, and Port E,
 * initialize Timer Counter 0 for overflow interrupt, and to initialize the SPI
 * bus for the encoders and bar graph display.
************************************************************************/
void initialization(){
	//initialize port b pins 3 as input, pins 2, 1, and 0 for output (SS, MOSI, SCLK)
	DDRB |= (0 << PB3) | (1 << PB2) | (1 << PB1) | (1 << PB0);
	DDRE = 0xFF;	//initialize port E as output
	DDRD = 0xFF;	//initialize port D as output


	SPCR = (1 << MSTR) | (0 << CPOL) | (0 << CPHA) | (1 << SPE);	//master mode, clk low, and leading edge
	SPSR = (1 << SPI2X);		//double speed operation


	ASSR |= (1 << AS0);			//enables external oscillator
	TIMSK |= (1 << TOIE0);		//enable TC interrupt
	TCCR0 |= (1 << CS00);		//128 prescale on normal mode

	TCCR2 |= (1 << WGM21) | (1 << WGM20) | (1 << COM21) | (0 << COM20) | (0 << CS20) | (1 << CS21) | (0 << CS22);

	TCNT1 = 40000;
	TIMSK |= (1 << TOIE1);		//enable TC1 interrupt
	TCCR1A = 0x00;				// normal mode
	TCCR1B |= (1 << CS10) | (0 << CS11) | (0 << CS12);		//no prescale

	//8-bit fast PWM for TC3 at PE3	 
	TCCR3A |= (0 << WGM31) | (1 << WGM30) | (1 << COM3A1) | (0 << COM3A0);
	TCCR3B |= (1 << WGM32) | (0 << WGM33) | (0 << CS30) | (1 << CS31) | (0 << CS32);	//8 prescaler

}//initialization

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
void segsum(uint8_t hour, uint8_t minute) {
	//initialize variables to be used in this function to -1, which makes LEDs go off
	int8_t ones = -1;
	int8_t tens = -1;
	int8_t hundreds = -1;
	int8_t thousands = -1;
  //determine how many digits there are
	//check to see if the total sum count is less than 10 for parsing
	ones = minute % 10;
	tens = minute / 10;

	hundreds = hour % 10;
	if(hour > 9)
		thousands = hour / 10;
	else if(hour24_flag == 0x01)
		thousands = 0;

	if(sec_count % 2 == 0)
		segment_data[2] = 16;
	else
		segment_data[2] = -1;
	
	//place the variables into the segment_data[] array to be displayed
	segment_data[0] = ones;
	segment_data[1] = tens;
	
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
			output = 0b10100001;
			break;

		case 14:
			//displays E on the seven segment display
			output = 0b10000110;
			break;

		case 15:
			//displays F on the seven segment display
			output = 0b10001110;
			break;

		case 16:
			//display the colon on the seven segment display
			output = 0b11111100;
			break;
			

		default: 
			//defaults to 0xFF, so that nothing shows on seven seg display
			output = 0b11111111;
			break;	
	
	}

	return output;	//return the seven segment display encoding

}//seven_seg_encoding()
//***********************************************************************************


/***********************************************************************************
 * Function: encoder_process
 * Parameter: uint8_t encoder value
 * Function: process the encoder value to see if turned right or left, then increment
 * or decrement as necessary based on the current bar graph state.
***********************************************************************************/

void encoder_process(uint8_t encoder){

	//initialize variables to store previous encoder state
	uint8_t encoder_left_prev = encoder_left;
	uint8_t encoder_right_prev = encoder_right;

	//obtain the left and right encoder values from the SPDR
	encoder_left = encoder & 0x03;
	encoder_right = (encoder & (0x03 << 2)) >> 2;

	//check right encoder:
	//transistion is 3 -> 2 -> 0 -> 1 -> 3
	//if current state is 3 and its previous is 1, then we know
	//that this was turned to the right
	if(encoder_right == 0x03 && encoder_right_prev == 0x01){
		if(adjust_flag == 0x00 && adjust_alarm == 0x00){
			if((OCR2 + 10) >= 254)
				OCR2 = 254;
			else	
				OCR2 += 10;

		}
		else
		{
			if(temp_min + 1 > 59)
				temp_min = 0;
			else
				temp_min++;
		}
		
		

	}
	//if current state is 3 and its previous is 2, then we know
	//that this was turned to the left
	else if (encoder_right == 0x03 && encoder_right_prev == 0x02){
		if(adjust_flag == 0x00 && adjust_alarm == 0x00){
			if((OCR2 - 10) <= 0)
				OCR2 = 0;
			else
				OCR2 -= 10;
		}
		else
		{
			if(temp_min - 1 < 0){
				temp_min = 59;
			}
			else
				temp_min--;
		}

	}

	//check left encoder:
	//transistion is 3 -> 2 -> 0 -> 1 -> 3
	//if current state is 3 and its previous is 1, then we know
	//that this was turned to the right
	if(encoder_left == 0x03 && encoder_left_prev == 0x01){
		if(adjust_flag == 0x01 || adjust_flag == 0x01)
			temp_hour++;
		else{
			if((OCR3A + 10) > 255)
				OCR3A = 255;
			else
				OCR3A += 10;

		}
	}
	//if current state is 3 and its previous is 2, then we know
	//that this was turned to the left
	else if (encoder_left == 0x03 && encoder_left_prev == 0x02){
		if(adjust_flag == 0x01 || adjust_flag == 0x01)
			if(temp_hour - 1 < 1){
				temp_hour = 12;
			}
			else
				temp_hour--;
		else{
			if((OCR3A - 10) <= 0)
				OCR3A = 0;
			else
				OCR3A -= 10;

		}
	}

	if(adjust_flag == 0x01){
		hour_count = temp_hour;
		min_count = temp_min;
	}

	if(adjust_alarm == 0x01){
		alarm_time_min = temp_min;
		alarm_time_hour = temp_hour;
		temp_pm_flag = pm_flag;
	//	temp_min = min_count;
	//	temp_hour = hour_count;	
	}


}//encoder_process()

/***********************************************************************************
 * Function: button_encoder_read
 * Parameter: None
 * Function: A routinely called cuntion that checks the buttons being pressed and the
 * encoders being read.
***********************************************************************************/
void button_encoder_read(){

 //make PORTA an input port with pullups
	DDRA = 0x00;
	PORTA = 0xFF;	
  //enable tristate buffer for pushbutton switches
    PORTB = 0x70;
  //now check each button and increment the count as needed
	//use a for loop to increment through each button to check

	//asm volatile ("nop");

	_delay_us(5);

	if(chk_buttons(7))
		adjust_flag ^= 0x01;

	if(chk_buttons(6)){
		hour24_flag ^= 0x01;
		if(pm_flag == 0x01 && hour24_flag == 0x01){
			pm_flag = 0;
			if(hour_count != 12)
				hour_count += 12;
		}
		if(hour24_flag == 0 && hour_count >= 12){
			pm_flag = 0x01;
			if(hour_count != 12)
				hour_count -= 12;
		}
	}

	if(chk_buttons(5) && adjust_flag == 0x01 && hour24_flag == 0)
		pm_flag ^= 0x01;
	
	if(chk_buttons(4))
		adjust_alarm ^= 0x01;

	if(chk_buttons(0) && trigger_alarm == 0x01){
		trigger_alarm = 0;
	
		/*
		alarm_time_sec = sec_count + 10;
		if(alarm_time_sec >= 60){
			alarm_time_sec -= 60;
			alarm_time_min++;
		}
		if(alarm_time_min >= 60){
			alarm_time_min -= 60;
			alarm_time_hour++;
		}
		*/
	}
	
  //disable tristate buffer for pushbutton switches
    PORTB = 0x60;

	asm volatile ("nop");

	//set CLK_INH low and SH/nLD high to shift encoder values through
	//its shift register
	PORTD = (0 << PD2);
	PORTE = (1 << PE6);

	asm volatile ("nop");

	SPDR = (adjust_flag + hour24_flag + adjust_alarm);
	while(bit_is_clear(SPSR, SPIF)){}		//continue on while loop until all SPI contents are sent

	//pulse PB0 to send out bar_disp to bar graph
	PORTB |= 0x01;
	PORTB &= 0xFE;

	//store the SPDR encoder value
	uint8_t encoder = SPDR;

	//call function to process that encoder value
	encoder_process(encoder);

	//reset the CLK_INH and SH/nLD
	PORTD = (1 << PD2);
	PORTE = (0 << PE6);

}//button_encoder_read

/***********************************************************************************
 * Function: clock_count
 * Parameter: None
 * Function: A function called during every main while loop that increments the sec_count,
 * min_count, and hour_count according to how many times the ISR is being called. This function
 * also looks into the hour24_flag to bound the hour count as necessary depending on the time
 * mode.
***********************************************************************************/
void clock_count(){

	if(isr_count == 128){
	  	sec_count++;
		isr_count = 0;
  	}
  	if(sec_count == 60){
	  	min_count++;
		sec_count = 0;
  	}
  	if(min_count == 60){
	  	hour_count++;

		if(hour_count == 12){
			pm_flag ^= 0x01;
		}

		min_count = 0;
  	}
	if(hour_count == 13 && hour24_flag == 0){
		hour_count = 1;	
	}
	else if(hour_count == 24 && hour24_flag == 0x01){
		hour_count = 0;
	}
	
	if(min_count == alarm_time_min && hour_count == alarm_time_hour && temp_pm_flag == pm_flag && adjust_alarm == 0){
		if(alarm_match_count == 0){
			trigger_alarm = 0x01;
			alarm_match_count = 0x01;
		}
	}
	else{
		trigger_alarm = 0;
		alarm_match_count = 0;
	}
	if(min_count == (alarm_time_min + 1) || hour_count == (alarm_time_hour + 1))
		trigger_alarm = 0;
	
}//clock_count

/***********************************************************************
 * Function: ISR for Timer Counter 0 Overflow
 * Description: Triggers this ISR whenever the Timer Counter 0 Overflow
 * occurs. This will then check the push buttons, read the encoder values
 * via the SPI bus, and send the SPI to the bar graph display to show the
 * current state.
 * 
 * NOTE: TRIGGERS EVERY 7.8125ms
 * *********************************************************************/

ISR(TIMER0_OVF_vect){

	input_flag = TRUE;		//subject to change

	isr_count++;

}//ISR


ISR(TIMER1_OVF_vect){

	if(trigger_alarm == 0x01){
		
		PORTC ^= (1 << PC3);
		TCNT1 = 40000;

	}

}


//***********************************************************************************
int main()
{
//set port bits 4-7 B as outputs
DDRB = 0xF0;
DDRC |= (1 << PC3);
PORTC |= (0 << PC3);

//initialize encoding value to be used
uint8_t encoding = 0;

real_time();

//call function to initialize SPI and TC
initialization();

//enable global interrupts
sei();

OCR2 = 0;
OCR3A = 200;

while(1){
  //insert loop delay for debounce
	//PORTB |= (6 << 4);
	//_delay_us(300);

  	if(input_flag == TRUE){
	  	button_encoder_read();
	  	input_flag = FALSE;
  	}

	clock_count();
	
	if(adjust_alarm == 0){
		temp_min = min_count;
		temp_hour = hour_count;
	}
	else{
		temp_min = alarm_time_min;
		temp_hour = alarm_time_hour;
	}
	
	if(adjust_alarm == 0)
		segsum(hour_count, min_count);
	else
		segsum(alarm_time_hour, alarm_time_min);
  //make PORTA an output
	DDRA = 0xFF;
	//uses "nop" to add a little delay
	asm volatile ("nop");
  //send 7 segment code to LED segments
	//calls seven_seg_encoding function to grab seven seg encoding, then displays it
	//based on the parsed number of the overall count
	for(int i_seg = 0; i_seg < 5; i_seg++){
		encoding = seven_seg_encoding(segment_data[i_seg]);
		if(i_seg == 4 && pm_flag == 0x01 && hour24_flag == 0)
			encoding &= 0b01111111;
		PORTA = 0xFF;
		PORTB = (i_seg << 4);			//output onto PORTB to select segment digit
		PORTA = encoding;				//output the encoding value to PORTA for seven seg display
		//asm volatile ("nop");
		_delay_us(80);					//add in tiny delay, but not large enough for flicker
	
	}

	//anti-ghosting protocol
	PORTA = 0xFF;
	PORTB = (5 << 4);

  }//while
}//main
