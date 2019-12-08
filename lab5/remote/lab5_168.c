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

#define F_CPU 8000000 // cpu speed in hertz 
#define TRUE 1
#define FALSE 0
#define TEMP_MEASURE 0xE3
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include "twi_master.h"
#include "lm73_functions.h"
#include "uart_functions.h"



uint8_t read_i2c_buffer[2];
uint8_t write_i2c_buffer[1] = {0xE3};
uint8_t read_temp_flag = 0x01;
char temp[1];
volatile char uart_buf[2];
uint8_t f_not_c = 0x01;
uint8_t flag = 0;


void init_lm73_sensor(){

	twi_start_wr(LM73_ADDRESS, write_i2c_buffer[0], 1);		//called from twi_master.c
	asm volatile("nop");	

}//init_lm73_sensor()

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

ISR(USART_RX_vect){
	temp[0] = UDR0;
	PORTB ^= (1 << PB2);
	flag = 0x01;
	
}


int main()
{
	init_twi();
	init_lm73_sensor();
	uart_init();

	sei();

	DDRB |= (1 << PB2) | (1 << PB1);
	PORTB |= (1 << PB2) | (1 << PB1);

	DDRC |= (1 << PC3);
	PORTC |= (1 << PC3);

	while(1)
	{

		//_delay_ms(100);
		PORTC ^= (1 << PC3);

		if(flag == 0x01){

			uart_putc('G');
			uart_putc('R');
			flag = 0;
			PORTB ^= (1 << PB1);

		}
		/*
		if(temp[0] == 'G')
			PORTB ^= (1 << PB2);
		*/
		/*
		if(temp[0] == 'C'){
			read_temp_flag = 0x01;
			PORTB |= (1 << PB5);
			f_not_c = 0;
		}
		else if(temp[0] == 'F'){
			read_temp_flag = 0x01;
			PORTB |= (1 << PB5);
			f_not_c = 0x01;
		}
		
		if(UDR0 == 0)
			PORTB |= (1 << PB5);

		if(read_temp_flag == 0x01){
			init_lm73_sensor();
			lm73_temp_convert(uart_buf, read_lm73_sensor(), f_not_c);
			
			while(!(UCSR0A & (1 << UDRE0)));
			UDR0 = '5';//uart_buf[0];
			while(!(UCSR0A & (1 << UDRE0)));
			UDR0 = '7';//uart_buf[1];
			while(!(UCSR0A & (1 << UDRE0)));
			if(f_not_c == 0x01){
				UDR0 = 'F';
				while(!(UCSR0A & (1 << UDRE0)));
			}
			else{
				UDR0 = 'C';
				while(!(UCSR0A & (1 << UDRE0)));
			}
			//read_temp_flag = 0x00;
			
			uart_putc('5');
			if(f_not_c == 0x01)
				uart_putc('F');
			else
				uart_putc('C');


		

		}*/	
	/*
	uart_putc('7');
	_delay_ms(500);
	*/


	}





}//main
