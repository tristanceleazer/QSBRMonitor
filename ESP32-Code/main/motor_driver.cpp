

#include <Arduino.h>
#include "globals.h"
#include "defines.h"

double currentPWM;
double minPWM = MIN_MOTOR_PWM;

void setMotorSpeed(double pwmVal)
{
    currentPWM = pwmVal;

    if (currentPWM < 0)
    {
    analogWrite(PIN_MOTOR_IN1, max(minPWM, abs(currentPWM)));
    analogWrite(PIN_MOTOR_IN2, 0);
    analogWrite(PIN_MOTOR_IN3, max(minPWM, abs(currentPWM)));
    analogWrite(PIN_MOTOR_IN4, 0);
    }

    if (currentPWM > 0)
    {
        analogWrite(PIN_MOTOR_IN1, 0);
        analogWrite(PIN_MOTOR_IN2, max(minPWM, abs(currentPWM)));
        analogWrite(PIN_MOTOR_IN3, 0);
        analogWrite(PIN_MOTOR_IN4, max(minPWM, abs(currentPWM)));
    }
}

void motorDisable()
{
    analogWrite(PIN_MOTOR_IN1, 0);
    analogWrite(PIN_MOTOR_IN2, 0);
    analogWrite(PIN_MOTOR_IN3, 0);
    analogWrite(PIN_MOTOR_IN4, 0); 
}
