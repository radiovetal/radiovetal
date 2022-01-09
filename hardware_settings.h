/*
 * Hardware settings for the digital dc power supply
 * http://www.tuxgraphics.org/electronics/
 *
 * In this file you can:
 * - calibrate the ampere and voltmeter
 * - choose your hardware type: 22V 2.5A or 30V 2.0A
 *
 *   The ADC has a resolution of 11 bit = values from 0-2047
 */
#ifndef CAL_HW_H
#define CAL_HW_H


/* ================= uncomment this section for the model 22V 2.5A */

#define U_MAX 200
#define I_MAX 350

// internal adc ref voltage (should be 2.56V, can vary from uC to uC)
#define ADC_REF 2.56

// the divider R3/R4 [(R3+R4)/R4] (calibrate the voltmeter, lower value=higher output)
#define U_DIVIDER 2040 // = 10.20

// the shunt for current measurement, you can calibrate here the 
// amperemeter.
// 4*2.2Ohm 3W=0.55:
#define I_RESISTOR 55 // =0.55 Ohm
// short circuit protection limit (do not change unless you know what you do):
// 880=3.5A= (3,5A * 1023 * 0.55 / 2.56 )
#define SH_CIR_PROT 880

#define FAN_COOL_LEV 220 // cool termosensor ADC output value

#define FAN_PROP_KOEFF 8 // coeff proportional fan drive

#endif //CAL_HW_H

