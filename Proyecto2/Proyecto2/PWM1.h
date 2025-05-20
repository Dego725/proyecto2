#ifndef PWM_1
#define PWM_1


#include <avr/io.h>

void PWM1_init(void);
void servo_writeA(float valADC);
void servo_writeB(float valADC);
float map(float x, float in_min, float in_max, float out_min, float out_max);


#endif