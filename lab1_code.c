// lab1_code.c 
// R. Traylor
// 7.21.08

//This program increments a binary display of the number of button pushes on switch 
//S0 on the mega128 board.

#include <avr/io.h>
#include <util/delay.h>

//*******************************************************************************
//                            debounce_switch                                  
// Adapted from Ganssel's "Guide to Debouncing"            
// Checks the state of pushbutton S0 It shifts in ones till the button is pushed. 
// Function returns a 1 only once per debounced button push so a debounce and toggle 
// function can be implemented at the same time.  Expects active low pushbutton on 
// Port D bit zero.  Debounce time is determined by external loop delay times 12. 
//*******************************************************************************
int8_t debounce_switch() {
  static uint16_t state = 0; //holds present state
  state = (state << 1) | (! bit_is_clear(PIND, 0)) | 0xE000;
  if (state == 0xF000) return 1;
  return 0;
}

//*******************************************************************************
// Check switch S0.  When found low for 12 passes of "debounce_switch(), increment
// PORTB.  This will make an incrementing count on the port B LEDS. 
//*******************************************************************************
int main()
{
DDRB = 0xFF;  //set port B to all outputs
uint8_t tens = 0;	//initialize integer variable for parsed tens unit
uint8_t ones = 0;	//initialize integer variable for parsed ones unit
uint8_t counter = 0;	//initialize integer variable to keep track of counts
while(1){     //do forever
 if(debounce_switch()) {counter++;}  //if switch true for 12 passes, increment the counter
  _delay_ms(2);                    //keep in loop to debounce 24ms

  if(counter == 100){				//check if counter is at 100
 	counter = 0;					//if counter is at 100, then reset back to 0, as indicated by the lab instructions
  }

  tens = counter/10;				//parse out the tens digit via integer division
  ones = counter%10;				//parse out the ones digit via mod division

  PORTB = (tens << 4) | (ones);		//output the tens digit to LEDs 4-7, ones digit to LEDs 0-3

  } //while 
} //main
