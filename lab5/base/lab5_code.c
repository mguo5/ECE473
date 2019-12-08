/*****************************************************************************
* Author: Matthew Guo
* Class: ECE 473
* Date Due: 11/12/2019
* Lab Number: Lab 4
* School: Oregon State University
* Description: This is Lab 4 for ECE 473. This program completes the alarm clock
*				lab, which keeps track of time, down to the minute, of actual time
*				and displays that time to the seven segment display. The alarm clock
*				can be both 24 hours or 12 hours, and is user adjustable. The seven
*				segment brightness is auto adjusted using a photocell. A alarm can be
*				set up to buzz for one minute after the time falls onto a set time.
*				The user can then either snooze the alarm for 10 minutes (in this case,
*				10 seconds) or turn off the alarm.
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
//	BAR GRAPH:
//		- RCLK is connected to PB0
//		- SRCLK is connected to PB1
//		- SER is connected to PB2
//		- nOE is connected to PWM to PB7
//	Encoders:
//		- Shift Load is connected to PE6
//		- Clk INH is connected to PD2
//		- SRCLK is connected to PB1
//	OP Amp:
//		- The AVR oscillation outputs on PC3
//	Audio Amp:
//		- The Volume PWM outputs on PE3
//	Photoresistor:
//		- The photoresistor volage divider is sampled at PF7



//  TIMER COUNTER USAGE:
//		TCO - Keeps track of time
//		TC1 - Oscillates for the alarm
//		TC2 - Fast PWM for brightness control
//		TC3 - Fast PWM for volume control

#define F_CPU 16000000 // cpu speed in hertz 
#define TRUE 1
#define FALSE 0
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include "hd44780.h"
#include "twi_master.h"
#include "lm73_functions.h"
#include "uart_functions.h"

//holds data to be sent to the segments. logic zero turns segment on
int8_t segment_data[5]; 

//decimal to 7-segment LED display encodings, logic "0" turns on segment
uint8_t dec_to_7seg[12]; 

uint8_t read_i2c_buffer[2];

volatile int count = 0;

uint8_t encoder_left = 0;
uint8_t encoder_right = 0;

uint8_t adjust_flag = 0;
uint8_t input_flag = 0;
uint8_t pm_flag = 0;
uint8_t hour24_flag = 0;
uint8_t lcd_flag = 0;
uint8_t manual_brightness = 0;
uint8_t trigger_alarm = 0;
uint8_t adjust_alarm = 0;
uint8_t alarm_match_count = 0;
uint8_t alarm_is_set = 0;
uint8_t alarm_set_once = 0;
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
uint8_t ten_sec_start = 0;
uint8_t ten_sec_count = 0;

uint8_t temp_read_flag = 0x01;
uint8_t uart_send_flag = 0;
char temp_digits[3];
char *temp_string = " B:    R:   "; 
volatile uint8_t f_not_c = 0;

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
hour_count = (__TIME__[0]-48)*10 + (__TIME__[1]-48);	//get real time hours in 24 hour format


//check if it is am or pm, set pm_flag if necessary
if(hour_count > 12){
//	hour_count -= 12;
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

	//enable fast PWM mode for TC2 for the seven segment adjust
	//prescale of 8
	//clear bit during output compare
	TCCR2 |= (1 << WGM21) | (1 << WGM20) | (1 << COM21) | (0 << COM20) | (1 << CS20) | (0 << CS21) | (0 << CS22);

	TCNT1 = 40000;				//set TCNT1 to obtain approximately 300Hz for beep
	TIMSK |= (1 << TOIE1);		//enable TC1 interrupt
	TCCR1A = 0x00;				// normal mode
	TCCR1B |= (1 << CS10) | (0 << CS11) | (0 << CS12);		//no prescale

	//8-bit fast PWM for TC3 at PE3	 
	TCCR3A |= (0 << WGM31) | (1 << WGM30) | (1 << COM3A1) | (0 << COM3A0);
	TCCR3B |= (1 << WGM32) | (0 << WGM33) | (0 << CS30) | (1 << CS31) | (0 << CS32);	//8 prescaler
	
	ADMUX = 0x67; //single-ended, input PORTF bit 7, left adjusted, 10 bits
	//ADC enabled, start the conversion, single shot mode, interrupts enabled 
	ADCSRA = (1 << ADEN)| (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0) | (1 << ADIE); 

}//initialization

/******************************************************************************
* Function: init_lm73_sensor()
* Description: Initializes the lm73 temperature sensor via i2c by calling the
* twi_start_wr() from twi_master.c at location LM73_ADDRESS.
******************************************************************************/
void init_lm73_sensor(){

	twi_start_wr(LM73_ADDRESS, 0x00, 1);		//called from twi_master.c
	asm volatile("nop");	

}//init_lm73_sensor()

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
  
	//parse out the two digits for the minutes
	ones = minute % 10;
	tens = minute / 10;

	//parse out the (potential) two digits for hours
	hundreds = hour % 10;
	if(hour > 9)
		thousands = hour / 10;
	//check to see if the 24 hour flag is set, since it MUST show the leading 0
	else if(hour24_flag == 0x01)
		thousands = 0;
	//toggle the middle colon every second
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
	int8_t encoder_left_prev = encoder_left;
	uint8_t encoder_right_prev = encoder_right;

	//obtain the left and right encoder values from the SPDR
	encoder_left = encoder & 0x03;
	encoder_right = (encoder & (0x03 << 2)) >> 2;

	//check right encoder:
	//transistion is 3 -> 2 -> 0 -> 1 -> 3
	//if current state is 3 and its previous is 1, then we know
	//that this was turned to the right
	if(encoder_right == 0x03 && encoder_right_prev == 0x01){
		if(adjust_flag == 0x00 && adjust_alarm == 0x00 && manual_brightness == 0x01){
			if((OCR2 + 5) >= 254)
				OCR2 = 250;
			else	
				OCR2 += 5;

		}
		else
		{
			if(temp_min + 1 > 59)		//bound the count to 0 and 59
				temp_min = 0;
			else
				temp_min++;				//increment minute when right encoder turned to the right
		}
		
		

	}
	//if current state is 3 and its previous is 2, then we know
	//that this was turned to the left
	else if (encoder_right == 0x03 && encoder_right_prev == 0x02){
		if(adjust_flag == 0x00 && adjust_alarm == 0x00 && manual_brightness == 0x01){
			if((OCR2 - 5) <= 0)
				OCR2 = 3;
			else
				OCR2 -= 5;
		}
		else
		{
			if(temp_min - 1 < 0){		//bound the count to 0 and 59
				temp_min = 59;			
			}
			else
				temp_min--;				//decrement minute when right encoder turned to left
		}

	}

	//check left encoder:
	//transistion is 3 -> 2 -> 0 -> 1 -> 3
	//if current state is 3 and its previous is 1, then we know
	//that this was turned to the right
	
	if(encoder_left == 0x03 && encoder_left_prev == 0x01){
		//increment hours during time set mode or alarm adjustment mode
		//appropriately binds the count depending on 24 hour flag
		if((adjust_flag == 0x01 || adjust_alarm == 0x01) && hour24_flag == 0){
			if(temp_hour + 1 > 12)		//if 24 hour flag not set, bound count to 1 and 12
				temp_hour = 1;
			else
				temp_hour++;			//increment hour when left encoder turned right
		}
		else if((adjust_flag == 0x01 || adjust_alarm == 0x01) && hour24_flag == 0x01){
			if(temp_hour + 1 > 23)		//if 24 hour flag is set, bound the count from 0 to 24
				temp_hour = 0;
			else
				temp_hour++;			//increment hour when left encoder turned right

		}
		//else meaning that either time set modes are not set, thus default to volume adjust		
		else{
			if((OCR3A + 10) > 255)		//binds volume to 255 (~5V via PWM to DC converter)
				OCR3A = 255;
			else
				OCR3A += 10;			//increment resolution by 10s to avoid spinning a lot

		}
	}
	//if current state is 3 and its previous is 2, then we know
	//that this was turned to the left
	else if (encoder_left == 0x03 && encoder_left_prev == 0x02){
		//decrement hours during time set mode or alarm adjustment mode
		//appropriately binds the count depending on 24 hour flag
		if((adjust_flag == 0x01 || adjust_alarm == 0x01) && hour24_flag == 0){
			if(temp_hour - 1 < 1){		//if 24 hour flag not set, bound count to 1 and 12
				temp_hour = 12;
			}
			else
				temp_hour--;	 		//decrememnt hour when left encoder turned left
		}
		else if((adjust_flag == 0x01 || adjust_alarm == 0x01) && hour24_flag == 0x01){
			if(temp_hour - 1 < 0)		//if 24 hour flag is set, bound count to 0 and 23
				temp_hour = 23;
			else
				temp_hour--;			//decrement hour when left encoder turned left
		}
		//else meaning that either time set modes are not set, thus default to volume adjust
		else{
			if((OCR3A - 10) <= 0)		//binds volume to 0 (~0V)
				OCR3A = 0;
			else
				OCR3A -= 10;			//decrement resolution by 10s to avoid spinning a lot

		}
	}

	//if time adjustment flag is set, set the actual time to the
	//temporary variables used to increment/decrement
	if(adjust_flag == 0x01){
		hour_count = temp_hour;
		min_count = temp_min;
	}

	//if the alarm adjustment flag is set, set the alarm time
	//to the temporary variables used to increment/decrement
	if(adjust_alarm == 0x01){
		alarm_time_min = temp_min;
		alarm_time_hour = temp_hour;
		temp_pm_flag = pm_flag;			//also save the pm_flag
		
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

	//poll if button 7 is pressed
	//this toggles if the time adjustment flag is set or not
	if(chk_buttons(7))
		adjust_flag ^= 0x01;

	//poll if button 6 is pressed
	//this toggles the 24 hour flag
	if(chk_buttons(6)){
		hour24_flag ^= 0x01;
		if(pm_flag == 0x01 && hour24_flag == 0x01){
			pm_flag = 0;			//pm_flag should not be set when in 24 hour mode
			if(hour_count != 12)	//if it is afternoon in 12 hour format, add 12 to obtain 24 hour
				hour_count += 12;
		}
		if(hour24_flag == 0 && hour_count >= 12){
			pm_flag = 0x01;			//set the pm_flag when coming from 24 hours and time is in afternoon
			if(hour_count != 12)	//edge case of when the time is 12 for 24 hour time
				hour_count -= 12;
		}
	}

	//poll if button 5 is pressed
	//this toggles the pm_flag for adjustment
	//this only works when in time set mode
	if(chk_buttons(5) && adjust_flag == 0x01 && hour24_flag == 0)
		pm_flag ^= 0x01;
	
	//poll if button 4 is pressed
	//this toggles the alarm adjustment mode
	if(chk_buttons(4))
		adjust_alarm ^= 0x01;
	
	if(chk_buttons(3)){
		alarm_is_set ^= 0x01;
		lcd_flag = 0x01;
	}

	if(chk_buttons(2))
		manual_brightness ^= 0x01;

	//poll if button 1 is pressed
	//this activates the snooze feature
	if(chk_buttons(1) && trigger_alarm == 0x01){
		trigger_alarm = 0;			//if pressed, alarm should turn off
		ten_sec_start = 0x01;		//start the count for 10 second delay
		ten_sec_count = 0;			//the count variable starts at 0
		lcd_flag = 0x01;			//tell lcd to update
		
	}

	//poll if button 0 is pressed
	//this silences all alarms, no snooze
	if(chk_buttons(0) && trigger_alarm == 0x01){
		trigger_alarm = 0;			//alarm turns off
		lcd_flag = 0x01;			//update lcd
	}
	
  //disable tristate buffer for pushbutton switches
    PORTB = 0x60;

	asm volatile ("nop");

	//set CLK_INH low and SH/nLD high to shift encoder values through
	//its shift register
	PORTD = (0 << PD2);
	PORTE = (1 << PE6);

	asm volatile ("nop");

	//send out state of flags to the bar graph display
	SPDR = (adjust_flag << 7) | (hour24_flag << 6) | (adjust_alarm << 5) | (manual_brightness << 2);
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

	//after 128 jumps in TC0 ISR, one second has passed
	if(isr_count == 128){
	  	sec_count++;				//increment 1 second
		isr_count = 0;				//reset isr_count
		uart_send_flag ^= 0x01;
		temp_read_flag ^= 0x01;
	//	set_LCD();
		//if snooze is activated, also increment count for snooze
		if(ten_sec_start == 0x01)
			ten_sec_count++;
  	}
	//after 60 seconds, 1 minute is incremented
  	if(sec_count == 60){
		if(trigger_alarm == 0x01){
			lcd_flag = 0x01;
		}
	  	min_count++;
		sec_count = 0;
  	}
	//after 60 minutes, 1 hour is incremented
  	if(min_count == 60){
	  	hour_count++;
		//if in 12 hour mode, set pm when necessary
		if(hour_count == 12 && hour24_flag == 0){
			pm_flag ^= 0x01;
		}

		min_count = 0;
  	}
	//bind the hour to 1 and 12 if 24 hour flag is not set
	if(hour_count >= 13 && hour24_flag == 0){
		hour_count -= 12;	
	}
	//bind the hour to 0 and 24 if 24 hour flag is set
	else if(hour_count >= 24 && hour24_flag == 0x01){
		hour_count -= 24;
	}
	
	//if current time matches saved alarm set time, then start the beeping
	if(min_count == alarm_time_min && hour_count == alarm_time_hour && temp_pm_flag == pm_flag && adjust_alarm == 0){
		if(alarm_match_count == 0 && alarm_is_set == 0x01){			//a check so that this only goes in once
			trigger_alarm = 0x01;			//start the beeping
			alarm_match_count = 0x01;
			lcd_flag = 0x01;				//update lcd
		}
	}
	//else meaning that the actual time does not equal saved alarm time
	else{
		trigger_alarm = 0;
		alarm_match_count = 0;
	}

	//if snooze is pressed, check to see if 10 seconds has elapsed
	if(ten_sec_count == 10){
		trigger_alarm = 0x01;			//initiate beep again after 10 second snooze
		ten_sec_start = 0;				//reset the start variable
		ten_sec_count = 0;				//reset the count variable
		lcd_flag = 0x01;				//update lcd
		alarm_time_min = temp_min;		//beep for 1 minute
		alarm_time_hour = temp_hour;		
	}
	
}//clock_count

/*************************************************************************
* Function: set_LCD()
* Parameters: None
* Description: Update the LCD display to show the current state of the alarm
* clock, such as buzzing an alarm, snoozed, or the alarm is not buzzing.
*************************************************************************/
void set_LCD(){
	//clear current contents in display
  // clear_display();
	//check to see if trigger_alarm is set
   if(trigger_alarm == 0x01){
      	string2lcd("ALARM!!!");			//tell lcd to show "ALARM" message
	  //	line2_col1();
	//	string2lcd(temp_string);

   }
	//check to see if alarm clock is in snooze mode
   else if(ten_sec_start == 0x01){
      	string2lcd("SNOOZED");			//tell lcd to show "SNOOZED" message
      //	line2_col1();
      //	string2lcd(temp_string);
   }
	//check to see if alarm clock is not set
   else if(alarm_is_set == 0){
		string2lcd("ALARM NOT SET");	//tell lcd to show "alarm not set" message
	//	line2_col1();
	//	string2lcd(temp_string);
	}
	//check to see if alarm clock is set
	else if(alarm_is_set == 0x01){
		string2lcd("ALARM SET");		//tell lcd to show "alarm set" message
	//	line2_col1();
	//	string2lcd(temp_string);
	}
   cursor_home();
}

void set_LCD_temp(){

	line2_col1();
	string2lcd(temp_string);

}

/**********************************************************************
* Function: read_lm73_sensor
* Description: calls the twi_start_rd() function from twi_master.c to start
* a reading process from the lm73 temperature sensor at i2c location
* LM73_ADDRESS. The reading is stored in read_i2c_buffer, which is then
* returned via 16 bit value for temp_reading. 
**********************************************************************/
uint16_t read_lm73_sensor(){

	//initialize a 16-bit variable to return
	uint16_t temp_reading = 0;

	//called from twi_master.c to obtain temp reading
	twi_start_rd(LM73_ADDRESS, read_i2c_buffer, 2);

	//stores temp reading to temp_reading
	temp_reading = read_i2c_buffer[0] << 8;
	temp_reading |= read_i2c_buffer[1];

	//returns temp_reading
	return temp_reading;

}//temp_reading

void uart_send_read(){
	
	if(f_not_c == 0x01){
		uart_putc('F');
		temp_string[11] = 'F';
	}
	else{
		uart_putc('C');
		temp_string[11] = 'C';
	}
	//_delay_ms(100);
	
	/*
	while(!(UCSR0A & (1 << UDRE0)));
	if(f_not_c == 0x01)
		UDR0 = 'F';
	else
		UDR0 = 'C';
	*/
	
	asm volatile ("nop");
	temp_string[9] = uart_getc();
	_delay_us(100);
	temp_string[10] = uart_getc();
	_delay_us(100);
	//temp_string[11] = uart_getc();*/

}


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

/*************************************************************************
* Function: ISR for Timer Counter 1 Overflow
* Description: Triggers the ISR whenever the Timer Counter 1 Overflow
* occurs. This will then check to see if the trigger_alarm state is set.
* If it is, then toggle the PORTC bit 3 pin to drive the annoying beeping
* for the speakers.
*
* NOTE: FREQUENCY IS APPROXIMATELY 300Hz
*************************************************************************/
ISR(TIMER1_OVF_vect){

	//check if trigger alarm is set
	if(trigger_alarm == 0x01){
		
		PORTC ^= (1 << PC3);		//start toggling PC3
		TCNT1 = 40000;				//reset TCN1 to 40000 for ~300Hz

	}

}//ISR

/*************************************************************************
* Function: ISR for ADC Conversion Complete
* Description: Triggers upon the completion of an ADC read and conversion.
* With a defined prescale value of 128, this triggers approximately every 
* 125kHz. It reads the voltage value of the Cds and outputs the 8 bit reading
* to the OCR2 for brightness adjust.
*************************************************************************/

ISR(ADC_vect){

	//checks to see if the manual_brightness flag is cleared
	if(manual_brightness == 0)
		OCR2 = ADCH;		//if cleared, store the 8 bit number to OCR2

}//ISR


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

//initialize I2C
init_twi(); //called from twi_master.c
uart_init();
init_lm73_sensor();

//enable global interrupts
sei();

//initially set output compare register for TC2 to 0 (brightness control)
OCR2 = 0;
//initially set output compare register for TC3 to 200 (volume control)
OCR3A = 100;

//initialize LCD
lcd_init();
set_LCD();

while(1){
  //insert loop delay for debounce
	//PORTB |= (6 << 4);
	//_delay_us(300);
	
	ADCSRA |= (1 << ADSC);//poke ADSC and start conversion

	
	if(temp_read_flag == 0x01){
		lm73_temp_convert(temp_digits, read_lm73_sensor(), f_not_c);
		temp_string[3] = temp_digits[0];
		temp_string[4] = temp_digits[1];
		temp_read_flag = 0x00;
		if(f_not_c == 0x01)
			temp_string[5] = 'F';
		else
			temp_string[5] = 'C';		
		//set_LCD_temp();
	}
	
	if(uart_send_flag == 0x01){
		uart_send_read();
		set_LCD_temp();
		uart_send_flag = 0x00;
	}

	
	//Check to see if program went into ISR
  	if(input_flag == TRUE){
	  	button_encoder_read();		//if so, read the encoders/buttons
	  	input_flag = FALSE;
  	}
	
	//update the clock counters
	clock_count();

	//call set_LCD() function if there is a need to update
	if(lcd_flag == 0x01){
		lcd_flag = 0;
		set_LCD();
	}
	
	//if adjustment alarm is set, need to show the alarm set time on the LED display
	//otherwise, show the current time
	if(adjust_alarm == 0){
		temp_min = min_count;
		temp_hour = hour_count;
	}
	else{
		temp_min = alarm_time_min;
		temp_hour = alarm_time_hour;
	}
	
	//parse the alarm set time if necessary (if alarm adjust is set)
	//otherwise, parse the current time
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
		if(i_seg == 0 && alarm_is_set == 0x01)
			encoding &= 0b01111111;			//indicate on the LED display (decimal point for seg 0) that alarm set
		if(i_seg == 4 && pm_flag == 0x01 && hour24_flag == 0)
			encoding &= 0b01111111;			//indicate on the LED display (decimal point for seg 4) that it is pm
		if(i_seg == 2 && trigger_alarm == 0x01)
			encoding &= 0b11111011;			//indicate on the LED display (top decimal point for seg 2) that
											//the alarm is triggered
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
