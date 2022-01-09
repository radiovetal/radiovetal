/*
 * main.c
 *
 * Created: 04.04.2012 16:31:34
 *  Author: Freeflyer
 *  Based on program by Guido Socher, http://www.tuxgraphics.org/electronics/   
 * Chip type           : ATMEGA16
 * Clock frequency     : External clock 16 Mhz
 */ 

#include <avr/io.h>
#include <inttypes.h>
#include <avr/interrupt.h>
#define F_CPU 16000000UL  // 16 MHz
#include <util/delay.h>
#include <stdlib.h> 
#include <string.h> 
#include <avr/eeprom.h> 
#include "lcd.h"
#include "dac.h"
#include "encoder.h"
#include "uart.h"
#include "analog.h"
#include "hardware_settings.h"

// change this version string when you compile:
#define SWVERSION "ver: ddcp-0.0.0"

// the units are display units and work as follows: 100mA=10 5V=50
// The function int_to_dispstr is used to convert the intenal values
// into strings for the display
static int16_t measured_val[2]={0,0};
static int16_t set_val[2];
static int16_t divider[2]={I_RESISTOR,U_DIVIDER};
static float filter_val[2]={0,0};
// the set values but converted to ADC steps
static int16_t set_val_adcUnits[2];

static uint8_t change_mode=0;//mode = 0 (nothing),1(current changing),2(current changing)

static uint32_t blink_counter = 0;
static uint32_t last_encoder_touch = 0;
static uint32_t last_display = 0;
static uint8_t blink_flag = 0;

#define UARTSTRLEN 10

static char uartstr[UARTSTRLEN+1];
static uint8_t uartstrpos=0;
static uint8_t uart_has_one_line=0;

// Convert an integer which is representing a float into a string.
// Our display is always 4 digits long (including one
// decimal point position). decimalpoint_pos defines
// after how many positions from the right we set the decimal point.
// The resulting string is fixed width and padded with leading space.
//
// decimalpoint_pos=2 sets the decimal point after 2 pos from the right: 
// e.g 74 becomes "0.74"
// The integer should not be larger than 999.
// The integer must be a positive number.
// decimalpoint_pos can be 0, 1 or 2
static void int_to_dispstr(uint16_t inum,char *outbuf,int8_t decimalpoint_pos){
        int8_t i,j;
        char chbuf[8];
        itoa(inum,chbuf,10); // convert integer to string
        i=strlen(chbuf);
        if ((i>3)&&(decimalpoint_pos!=0)) i=3; //overflow protection
        strcpy(outbuf,"   0"); //decimalpoint_pos==0
        if (decimalpoint_pos==1) strcpy(outbuf," 0.0");
        if (decimalpoint_pos==2) strcpy(outbuf,"0.00");
        j=4;
        while(i){
                outbuf[j-1]=chbuf[i-1];
                i--;
                j--;
                if (j==4-decimalpoint_pos){
                        // jump over the pre-set dot
                        j--;
                }
        }
}

// convert voltage values to adc values, disp=10 is 1.0V
// ADC for voltage is 11bit:
static int16_t disp_u_to_adc(int16_t disp){
        return((int16_t)(((float)disp * 204.7) / (ADC_REF *( (float)divider[1]*0.01))));
}
// calculate the needed adc offset for voltage drop on the
// current measurement shunt (the shunt has about 0.55 Ohm =1/1.82 Ohm)
// use 1/1.8 instead of 1/1.82 because cables and connectors have as well
// a loss.
static int16_t disp_i_to_u_adc_offset(int16_t disp){
        return(disp_u_to_adc(disp/18));
}
// convert adc values to voltage values, disp=10 is 1.0V
// disp_i_val is needed to calculate the offset for the voltage drop over
// the current measurement shunt, voltage measurement is 11bit
static int16_t adc_u_to_disp(int16_t adcunits,int16_t disp_i_val){
        int16_t adcdrop;
        adcdrop=disp_i_to_u_adc_offset(disp_i_val);
        if (adcunits < adcdrop){
                return(0);
        }
        adcunits=adcunits-adcdrop;
        return((int16_t)((((float)adcunits /204.7)* ADC_REF * (float)divider[1]*0.010)+0.5)); 
}
// convert adc values to current values, disp=10 needed to be printed
// by the printing function as 0.10 A, current measurement is 10bit
static int16_t disp_i_to_adc(int16_t disp){
        return((int16_t) (((float)disp * 0.1023* (float)divider[0]) / ADC_REF));
}
// convert adc values to current values, disp=10 needed to be printed
// by the printing function as 0.10 A, current measurement is 10bit
static int16_t adc_i_to_disp(int16_t adcunits){
       return((int16_t) (((float)adcunits* ADC_REF)/(0.1023 * (float)divider[0])+0.5));
}

static void store_permanent(void){
        int16_t tmp;
        uint8_t changeflag=1;
        lcd_clrscr();
        if (eeprom_read_byte((uint8_t *)0x0) == 19){
                changeflag=0;
                // ok magic number matches accept values
                tmp=eeprom_read_word((uint16_t *)0x04);
                if (tmp != set_val[1]){
                        changeflag=1;
                }
                tmp=eeprom_read_word((uint16_t *)0x02);
                if (tmp != set_val[0]){
                        changeflag=1;
                }
				tmp=	eeprom_read_word((uint16_t *)0x06);
                if (tmp != divider[0]){
                        changeflag=1;
                }
				tmp=	eeprom_read_word((uint16_t *)0x08);
                if (tmp != divider[1]){
                        changeflag=1;
                }
        }
        if (changeflag){
                lcd_puts_P("setting stored");
                eeprom_write_byte((uint8_t *)0x0,19); // magic number
                eeprom_write_word((uint16_t *)0x02,set_val[0]);
                eeprom_write_word((uint16_t *)0x04,set_val[1]);
				eeprom_write_word((uint16_t *)0x06,divider[0]);
                eeprom_write_word((uint16_t *)0x08,divider[1]);
        }else{
                lcd_puts_P(SWVERSION);
                        lcd_gotoxy(0,1);
                        lcd_puts_P("www.datagor.ru");
        }
		for (int i=0;i<10;i++) _delay_ms(100);
        
}

uint8_t uartcheck(void)
{
        char c;
        if(uart_has_one_line==0) 
		  while (uart_getchar(&c)==1){
            
			if (c=='\n') c='\r'; // Make unix scripting easier. A terminal, even under unix, does not send \n
            // ignore any white space and characters we do not use:
            if (~(!(c=='\b'||(c>='0'&&c<='z')||c==0x7f||c=='\r'))){
            if (c=='\r'){
                uartstr[uartstrpos]='\0';
                uart_sendchar('\r'); // the echo line end
                uart_sendchar('\n'); // the echo line end
                uart_has_one_line=1;
				return 1;
            }
			else if (c=='\b'){ // backspace
                if (uartstrpos>0){
                    uartstrpos--;
                    uart_sendchar(c); // echo back
                    uart_sendchar(' '); // clear char on screen
                    uart_sendchar('\b');
                }
            }else if (c==0x7f){ // del
                if (uartstrpos>0){
                    uartstrpos--;
                    uart_sendchar(c); // echo back
                }
            }else{
                uart_sendchar(c); // echo back
                uartstr[uartstrpos]=c;
                uartstrpos++;
            }
            if (uartstrpos>UARTSTRLEN){
                uart_sendstr_P("\r\nERROR\r\n");
                uartstrpos=0; // empty buffer
                uartstr[0]='\0'; // just print prompt
                uart_has_one_line=1;
				return 1; 
           } }
        }
  return 0;
} 

void parse_uartStr(void){
  uint8_t uartprint_ok=0;
  uint8_t cmdok=0;
  char buf[21];
  if (uart_has_one_line){
                        //Set or check current
                        if (uartstr[0]=='i'){
						    if (uartstr[1]=='=' && uartstr[2]!='\0'){
                                set_val[0]=atoi(&uartstr[2]);
                                if(set_val[0]>I_MAX){
                                        set_val[0]=I_MAX;
                                }
                                if(set_val[0]<0){
                                        set_val[0]=0;
                                }
                                uartprint_ok=1;
							}
							else if (uartstr[1]=='\0'){
							    int_to_dispstr(measured_val[0],buf,2);
                                uart_sendstr(buf);
                                uart_sendstr_p(P("\r\n"));
								uartprint_ok=1;
							} 
                        }
                        // version
                        if (uartstr[0]=='v' && uartstr[1]=='e'){
                                uart_sendstr_p(P(SWVERSION));
                                uart_sendstr_p(P("\r\n"));
                                uartprint_ok=1;
                        }
						//mode
						if (uartstr[0]=='m' && uartstr[1]=='\0'){
                                if (is_current_limit()){
                                uart_sendchar('I');
                                }else{
                                uart_sendchar('U');
                                }
                                uart_sendchar('>');
						        uart_sendstr_p(P("\r\n"));
                                uartprint_ok=1;
                        }
                        // store
                        if (uartstr[0]=='s' && uartstr[1]=='t'){
                                store_permanent();
                                uartprint_ok=1;
                        }
						//Set or check voltage
                        if (uartstr[0]=='u' ){
						   if (uartstr[1]=='=' && uartstr[2]!='\0'){
                                set_val[1]=atoi(&uartstr[2]);
                                if(set_val[1]>U_MAX){
                                        set_val[1]=U_MAX;
                                }
                                if(set_val[1]<0){
                                        set_val[1]=0;
                                }  
								uartprint_ok=1;  
							}
							else if (uartstr[1]=='\0'){
							    int_to_dispstr(measured_val[1],buf,1);
                                uart_sendstr(buf);
                                uart_sendstr_p(P("\r\n"));
								uartprint_ok=1;
							} 
							
                        }
						//Set or check voltage divider
 						if (uartstr[0]=='d'){
						   if (uartstr[1]=='=' && uartstr[2]!='\0'){
                                divider[1]=atoi(&uartstr[2]);
                                
                                if(divider[1]<=0){
                                        divider[1]=U_DIVIDER;
                                }
								eeprom_write_word((uint16_t *)0x08,divider[1]);
                                uartprint_ok=1;
							}
							else if (uartstr[1]=='\0'){
							    int_to_dispstr(divider[1],buf,0);
                                uart_sendstr(buf);
                                uart_sendstr_p(P("\r\n"));
								uartprint_ok=1;
							} 
                        }
						//Set or check current shunt
						if (uartstr[0]=='r'){
						    if (uartstr[1]=='=' && uartstr[2]!='\0'){
                                divider[0]=atoi(&uartstr[2]);
                                
                                if(divider[0]<=0){
                                        divider[0]=I_RESISTOR;
                                }
								eeprom_write_word((uint16_t *)0x06,divider[0]);
                                uartprint_ok=1;
							}
							else if (uartstr[1]=='\0'){
							    int_to_dispstr(divider[0],buf,0);
                                uart_sendstr(buf);
                                uart_sendstr_p(P("\r\n"));
								uartprint_ok=1;
							} 
                        }
                        // help
                        if (uartstr[0]=='h' || uartstr[0]=='H'){
                                uart_sendstr_p(P("Usage: u[=V*10]|i=[mA/10]|d[=100*(R7+R8)/R8]|\r\n"));
								_delay_ms(1);
								uart_sendstr_p(P("       r[=100*R4]|store|help|version\r\n"));
								_delay_ms(1);
                                uart_sendstr_p(P("Examples:\r\n"));
								_delay_ms(1);
                                uart_sendstr_p(P("set 6V: u=60\r\n"));
								_delay_ms(1);
                                uart_sendstr_p(P("max 200mA: i=20\r\n"));
								
                                cmdok=1;
                        }
                        if (uartprint_ok){
                                cmdok=1;
                                uart_sendstr_p(P("ok\r\n"));
                        }
                        if (uartstr[0]!='\0' && cmdok==0){
                                uart_sendstr_p(P("command unknown\r\n"));
                        }
                        uart_sendstr_p(P("\r\n"));
                uartstrpos=0;
                uart_has_one_line=0;
        }
}

// check the keyboard
static uint8_t check_buttons(void){
        switch (change_mode){
        case 0:{ if (check_store_button()) change_mode = 1;
		         return(0);  
			   } break;
  		case 1:{ if (check_encoder(&(set_val[1]))){
				    if(set_val[1]>U_MAX){
                        set_val[1]=U_MAX;
                    }
				 	return(1);
				 }
				 else {
				    if (check_store_button()) change_mode = 2;
		            return(0); 	
				 }
				}break; 		
		case 2:{ if (check_encoder(&(set_val[0]))){
				    if(set_val[0]>I_MAX){
                        set_val[0]=I_MAX;
                    }
				 	return(1);
				 }
				 else {
				    if (check_store_button()) {
					   change_mode = 0;
					   store_permanent();
                       return(2);
					}
				 }
				}break;			
		}
        return(0);
}

void Form_Display(void){
    static char out_buf[21];
    static uint8_t i=0;
    static uint8_t ilimit=0;
	
	i++;
    // due to electrical interference we can get some
    // garbage onto the display especially if the power supply
    // source is not stable enough. We can remedy it a bit in
    // software with an ocasional reset:
    if (i==50){ // not every round to avoid flicker
                lcd_reset();
                i=0;
              }
	lcd_home();
	// current
	measured_val[0]=adc_i_to_disp(((int16_t)filter_val[0]));
    set_val_adcUnits[0]=disp_i_to_adc(set_val[0]);
    set_target_adc_val(0,set_val_adcUnits[0]);
    // voltage
    measured_val[1]=adc_u_to_disp(((int16_t)filter_val[1]),measured_val[0]);
    set_val_adcUnits[1]=disp_u_to_adc(set_val[1])+disp_i_to_u_adc_offset(measured_val[0]);
    set_target_adc_val(1,set_val_adcUnits[1]);
    ilimit=is_current_limit();
		//volatge
	int_to_dispstr(measured_val[1],out_buf,1);
    lcd_puts(out_buf);
    lcd_puts("V [");
    int_to_dispstr(set_val[1],out_buf,1);
	if ((change_mode ==1)&&(((millis() - blink_counter)>500)||(blink_counter>millis()))){
	   blink_counter = millis();
	   blink_flag ^=1;
	  }
    if ((change_mode !=1)||(blink_flag==0))	lcd_puts(out_buf);
	else lcd_puts("    "); 
    lcd_putc(']');
    if (!ilimit){
          // put a marker to show which value is currenlty limiting
          lcd_puts("<- ");
    }else{
          lcd_puts("   ");
    }
    // current
    lcd_gotoxy(0,1);
    int_to_dispstr(measured_val[0],out_buf,2);
    lcd_puts(out_buf);
	lcd_puts("A [");
    int_to_dispstr(set_val[0],out_buf,2);
    if ((change_mode ==2)&&(((millis() - blink_counter)>500)||(blink_counter>millis()))){
	   blink_counter = millis();
	   blink_flag ^=1;
	  }
    if ((change_mode !=2)||(blink_flag==0))	lcd_puts(out_buf);
	else lcd_puts("    "); 
    lcd_putc(']');
    if (ilimit){
        // put a marker to show which value is currenlty limiting
         lcd_puts("<- ");
    }else{
         lcd_puts("   ");
    }
   
}

int main(void) //Work case
{
        init_dac();
        lcd_init();
        init_encoder();
        set_val[0]=15;set_val[1]=50; // 150mA and 5V
        if (eeprom_read_byte((uint8_t *)0x0) == 19){
                // ok magic number matches accept values
                set_val[1]=eeprom_read_word((uint16_t *)0x04);
                set_val[0]=eeprom_read_word((uint16_t *)0x02);
				divider[0] =eeprom_read_word((uint16_t *)0x06);
				divider[1] =eeprom_read_word((uint16_t *)0x08);
                // sanity check:
                if (set_val[0]<0) set_val[0]=0;
				if (set_val[1]<0) set_val[1]=0;
                if (divider[0]<=0) divider[0]=I_RESISTOR;
                if (divider[1]<=0) divider[1]=U_DIVIDER;
        }
        uart_init();
        init_analog();
		sei();
        while (1) {
                if (millis()>last_display+40) {
				   filter_val[0]=(filter_val[0]*0.9)+(float)getanalogresult(0)*0.1;
				   filter_val[1]=(filter_val[1]*0.9)+(float)getanalogresult(1)*0.1;
				   Form_Display();
				   last_display = millis();
				}
				if ((millis()-last_encoder_touch)>20000){
				   last_encoder_touch = millis();
				   change_mode = 0;
				}
				
				if(uartcheck()){
				  parse_uartStr();
				}
                if (check_buttons()){
                        // button press
						blink_counter=millis();
						last_encoder_touch = blink_counter;
						blink_flag = 0;
                        
                }
        }
        return(0);
}
  
 
