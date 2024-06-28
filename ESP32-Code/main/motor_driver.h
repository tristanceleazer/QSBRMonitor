

#ifndef MOTOR_DRIVER_H_
#define MOTOR_DRIVER_H_

#include <Arduino.h>
#include "globals.h"
#include "defines.h"

extern double currentPWM;
extern double minPWM;

void setMotorSpeed(double pwmVal);

void motorDisable();

#endif