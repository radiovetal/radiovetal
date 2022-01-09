/* vim: set sw=8 ts=8 si : */
/*************************************************************************
 Title	:   C include file for analog conversion
 Target:    any AVR device
 Copyright: GPL
***************************************************************************/
#ifndef ANALOG_H
#define ANALOG_H

/* You must enable interrupt with sei() in the main program */
extern void init_analog(void);
/* get the result of an analog conversion */
extern int16_t getanalogresult(uint8_t channel);
extern void set_target_adc_val(uint8_t item,int16_t val);
extern uint8_t is_current_limit(void); 
extern int16_t get_dacval(void);
extern void control_loop(void);

#endif /* ANALOG_H */
