/*****************************************************************************
* Author: Matthew Guo
* Class: ECE 473
* Date Due: 12/13/2019
* Lab Number: Lab 5
* School: Oregon State University
* Description: This is lab 5 for the ATMEGA48 board, in which UART is implemented.
		The 48 waits for command from the 128 board of either F or C, and convert
		the necessary I2C value from the LM73 to 128 board for remote temperature.
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
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include "twi_master.h"
#include "lm73_functions.h"
#include "uart_functions.h"



uint8_t read_i2c_buffer[2];
uint8_t write_i2c_buffer[1];
uint8_t read_temp_flag = 0;
char temp[1];
uint8_t f_not_c = 0x01;
uint8_t flag = 0;


void init_lm73_sensor(){

	twi_start_wr(LM73_ADDRESS, 0x00, 1);		//called from twi_master.c
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
	//grab the UDR0 value from 128
	temp[0] = UDR0;
	//for debugging, toggle PB2
	PORTB ^= (1 << PB2);
	//set the read flag for main function
	read_temp_flag = 0x01;
	
}//ISR


int main()
{
	init_twi();		//called from twi_master.c
	
	DDRC = 0x20;
	uart_init();		//initialized uart

	sei();

	while(1)
	{

		
		//convert temperature from lm73
		volatile char uart_buf[2];
		lm73_temp_convert(uart_buf, read_lm73_sensor(), f_not_c);

		//obtain value from 128 indicating if convert to C or F
		if(temp[0] == 'C'){
			f_not_c = 0;
		}
		else if(temp[0] == 'F'){
			f_not_c = 0x01;
		}

		//if we went through the interrupt, start sending uart to 128
		if(read_temp_flag == 0x01){
			
			
			//send one character at a time
			asm volatile ("nop");
			uart_putc(uart_buf[0]);
			_delay_us(100);

			//send the next character
			uart_putc(uart_buf[1]);
			_delay_us(100);

			//put read flag to zero to avoid spamming
			read_temp_flag = 0x00;
			


		

		}	



	}





}//main
