/* vim: set sw=8 ts=8 si : */
/*********************************************
* Author: Maxim Astashev, Copyright: GPL 
* 
* read the keyboard
**********************************************/
#include <avr/io.h>
#include <avr/interrupt.h>
#include "analog.h"


// it looks like output port settings need time to propagate. Maybe
// caused by input capacitors on the lcd which connect to the same ports.


static uint8_t Encoder_rotate_step=0;
static uint8_t Encoder_rotate_dir=0;
static uint8_t Encoder_Button_Flag=0;
static uint32_t milliseconds=0;
static uint32_t Last_Enc_rot =0xFFFFFFF;//last time when encoder rotor was turned
static uint32_t Last_button =0xFFFFFFF;//last time when buton was pressed

uint32_t millis(void)
{
   return (milliseconds);
}


void init_encoder(void) //prepare encoder 
{
	DDRD&= ~(1<<DDD2); // input line for 1-st encoder phase
	DDRD&= ~(1<<DDD3); // input line for encoder button
    DDRD&= ~(1<<DDD6); // input line for 2-nd encoder phase
	PORTD|= (1<<PIND2); // internal pullup resistor on
	PORTD|= (1<<PIND3); // internal pullup resistor on
	PORTD|= (1<<PIND6); // internal pullup resistor on
    GICR |= (1<<INT1)|(1<<INT0);
	MCUCR|= (1<ISC11)|(1<ISC01);
    MCUCR&=~((1<ISC10)|(1<ISC00));
    TCNT2 = 0;
	OCR2 = 250;
    TCCR2 = (1<<WGM21)|(1<<CS22);
	TIMSK|=(1<<OCIE2);
}

ISR(TIMER2_COMP_vect,ISR_NOBLOCK)
{
   milliseconds++;
   control_loop();
}


ISR(INT0_vect)  
{
  if((Last_Enc_rot>milliseconds)||(milliseconds-Last_Enc_rot)>1){
   Last_Enc_rot=milliseconds;
   Encoder_rotate_step++;
   if (PIND&(1<<PIND6)){
      Encoder_rotate_dir = 1; //Change it to change rotate direction
   }
   else Encoder_rotate_dir = 0; //Change it to change rotate direction
  
  }
}

ISR(INT1_vect) 
{
  if((Last_button>milliseconds)||(milliseconds-Last_button)>200){
  	Last_button=milliseconds;
  	Encoder_Button_Flag = 1;
  }
}
uint8_t check_encoder(int16_t *u) 
{
	if (Encoder_rotate_step>0){
		if (Encoder_rotate_dir==0) {(*u)=(*u)+Encoder_rotate_step;}
		else {(*u)=(*u)-Encoder_rotate_step;}
		if ((*u)<0) (*u) = 0;
		Encoder_rotate_step = 0;
		return(1);
	}
	else return(0);
}

uint8_t check_store_button(void) 
{
	if (Encoder_Button_Flag==1){
	    Encoder_Button_Flag = 0;
		return(1);
	}
	else return(0);
}
