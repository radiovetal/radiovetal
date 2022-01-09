/* vim: set sw=8 ts=8 si : */
/*********************************************
* Author: Max Astashev
* Based on Guido Socher dac.c library http://www.tuxgraphics.org/
* Adapted by Max Astashev
* Digital to analog converter using a R-2R leadder (8bit)
* and PWM (4bit)
*/

#include <avr/io.h>

// this dac can do 12 bit resolution: bit 0-3=pwm, bit 4-11=R-2R leadder
void dac(uint16_t value){
        //OCR1AH=0;
        OCR1AL=value&0x0F; // lower 4 bits
	value=value>>4;
	// r2r ladder is PC port
	PORTC=(value&0xFF);
}

void fan(uint8_t value){
        //OCR1BH=0;
        if (value<0x0F) OCR1BL=value&0x0F; // lower 4 bits
		else OCR1BL=0x0F;
}

void init_dac(void) 
{
	// enable port C as output 
	DDRC = 0xFF; // output
	PORTC = 0x00; //  zero volt on port C
	DDRD|= (1<<DDD5);
	PORTD &= ~(1<<PIND5);
	DDRD|= (1<<DDD4);
	PORTD &= ~(1<<PIND4);
	// set up of Pulse Width Modulation (PWM)
	TCNT1H=0; // counter to zero, high byte first
	TCNT1L=0;
        // COM1A1  COM1A0
        //  1       0     Clear OC1A/OC1B on Compare Match (Set output to low level)
        //  1       1     Set OC1A/OC1B on Compare Match (Set output to high level)
        //
        // Fast PWM, ICR1 is top
        // See datasheet page 99 (settings) and 88 (description).
        TCCR1A=(0<<COM1A0)|(1<<COM1A1)|(0<<COM1B0)|(1<<COM1B1)|(0<<WGM10)|(1<<WGM11);
        TCCR1B=(1<<CS10)|(1<<WGM12)|(1<<WGM13); // full clock speed
	// 4 bit resolution:
	ICR1H=0;
	ICR1L=0x0F;
        // At what value to switch on the port (port OC1A=0 -> 0 Volt output)
        OCR1AH=0;
        OCR1AL=0;
		OCR1BH=0;
        OCR1BL=0;
}


