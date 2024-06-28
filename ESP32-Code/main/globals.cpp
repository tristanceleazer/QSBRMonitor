



#include <Arduino.h>
#include "defines.h"
#include "esp32-hal-timer.h"

String MAC;
int regNum;

double Kp = KP;
double Ki = KI;
double Kd = KD;


double currentAngle;
int16_t currentSample = 0;
double angleSample;
double targetAngle = 0;

double motorPWM;


