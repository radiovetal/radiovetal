/* vim: set sw=8 ts=8 si : */
/*************************************************************************
 Title	:   include file for the keyboard of the digital power supply
 Copyright: GPL
***************************************************************************/
#ifndef ENCODER_H
#define ENCODER_H

extern void init_encoder(void);
extern uint8_t check_encoder(int16_t *u);
extern uint8_t check_store_button(void);
extern uint32_t millis(void);

#endif 
