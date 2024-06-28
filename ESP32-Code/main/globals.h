



#ifndef GLOBALS_H_
#define GLOBALS_H_

#include <Arduino.h>
#include "esp32-hal-timer.h"

extern String MAC;
extern int regNum;

extern double Kp;
extern double Ki;
extern double Kd;


extern double currentAngle;
extern int16_t currentSample;
extern double angleSample;
extern double targetAngle;


extern double motorPWM;


#endif