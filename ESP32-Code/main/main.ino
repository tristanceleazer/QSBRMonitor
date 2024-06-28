

#include <Wire.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <Arduino.h>

#include <PID_v2.h>

#include "I2Cdev.h"
#include <MPU6050_6Axis_MotionApps20.h>
#include <ModbusIP_ESP8266.h>

#include "motor_driver.h"
#include "defines.h"
#include "globals.h"

#include <stdio.h>
#include "esp_types.h"
#include "soc/timer_group_struct.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"
#include "driver/ledc.h"
#include "esp32-hal-ledc.h"

int LTval = 0;
int RTval = 0;

#define ENABLE_PID
//#define DEBUG_MODE_PID // See the PID Values
//#define DEBUG_MODE_YPR // See the YPR Values

String WIFI_SSID = "[SSID]";
String WIFI_PASS = "[PASS]";


//Specify the links and initial tuning parameters
PID_v2 stabilityControl(Kp, Ki, Kd, PID::Direct);
MPU6050 mpu;
ModbusIP regData;


bool dmpReady = false;
uint8_t mpuIntStatus;
uint8_t devStatus;
uint16_t packetSize;
uint16_t fifoCount;
uint8_t fifoBuffer[64];

VectorFloat gravity;    // [x, y, z]            gravity vector
float euler[3];
float ypr[3];

Quaternion q;           // [w, x, y, z]         quaternion container
volatile bool mpuInterrupt = false;

unsigned long previousMillis = 0;

void dmpDataReady()
{
  mpuInterrupt = true;
}



void initNetwork()
{
  Serial.println("Starting WiFi Initialization");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while(WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi Connected to");
  Serial.println("IP Address  : ");
  Serial.print(WiFi.localIP()); // To connect to the ModbusTCP server, please write this IP address.

  //Modbus TCP Server Initialization
  regData.server();
  for( int i = 0 ; i <= REG_AMOUNT ; i++)
  {
    regData.addHreg(i);
  }
}



void initMPU6050()
{
  Serial.println("");
  Serial.println("Initializing I2C devices...");
  mpu.initialize();
  pinMode(INTERRUPT_PIN, INPUT);
  Serial.println(F("Testing device connections..."));
  Serial.println(mpu.testConnection() ? F("MPU6050 connection successful") : F("MPU6050 connection failed"));
  
  Serial.println(F("Initializzing DMP..."));
  devStatus = mpu.dmpInitialize();

  mpu.setXGyroOffset(220);
  mpu.setYGyroOffset(76);
  mpu.setZGyroOffset(-85);
  mpu.setZAccelOffset(1788); // 1688 factory default for my test chip

  if(devStatus == 0)
  {
    mpu.CalibrateAccel(6);
    mpu.CalibrateGyro(6);
    mpu.PrintActiveOffsets();

    Serial.println(F("Enabling DMP..."));
    mpu.setDMPEnabled(true);

    // enable Arduino interrupt detection
    Serial.print(F("Enabling interrupt detection (Arduino external interrupt "));
    Serial.print(digitalPinToInterrupt(INTERRUPT_PIN));
    Serial.println(F(")..."));
    attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), dmpDataReady, RISING);
    mpuIntStatus = mpu.getIntStatus();

    Serial.println(F("DMP ready! Waiting for first interrupt..."));
    dmpReady = true;
    packetSize = mpu.dmpGetFIFOPacketSize();
  
  }
  else
  {
    Serial.print(F("DMP Initialization failed (code "));
    Serial.print(devStatus);
    Serial.println(F(")"));
  }

  Serial.println("Sampling MPU for Stable Position...");
  currentSample = 0;
  while( currentSample <= INIT_SAMPLE_SIZE )
  {
    if(!dmpReady) return;
    if(mpu.dmpGetCurrentFIFOPacket(fifoBuffer))
    {
      mpu.dmpGetQuaternion(&q, fifoBuffer);
      mpu.dmpGetGravity(&gravity, &q);
      mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
      ++currentSample;
      delay(5);
      angleSample = angleSample + (ypr[1] * 180/M_PI);
    }
  }
  targetAngle = (angleSample / INIT_SAMPLE_SIZE) - CENTER_OF_MASS_OFFSET;

  Serial.println("Target Angle = ");
  Serial.print(targetAngle);

}

void setup() 
{
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println("ESP32 SBR");
  Serial.println("============================================================");
  pinMode(PIN_MOTOR_IN1, OUTPUT);
  pinMode(PIN_MOTOR_IN2, OUTPUT);
  pinMode(PIN_MOTOR_IN3, OUTPUT);
  pinMode(PIN_MOTOR_IN4, OUTPUT);
  delay(50);

  Wire.begin();
  initMPU6050();

  initNetwork();

  #ifdef ENABLE_PID
  stabilityControl.SetOutputLimits(-250, 250);
  stabilityControl.SetMode(PID::Automatic);
  stabilityControl.SetSampleTime(10);
  stabilityControl.Start(angleSample, 0, targetAngle);
  #endif
  Serial.println("");
  Serial.print("Setup Complete");
}

void loop() 
{
    double targetSpeed = stabilityControl.Run(angleSample);// Raw PID
    regData.Hreg(2, targetSpeed);
    targetSpeed = -targetSpeed;
    double finalSpeed = targetSpeed;
    finalSpeed = constrain(finalSpeed, -250, 250);

    setMotorSpeed(finalSpeed);

    #ifdef DEBUG_MODE_PID
    Serial.println(" ");
    Serial.println(targetSpeed);
    Serial.print("Motor Engage = ");
    Serial.print(finalSpeed);
    Serial.print("  angleMapped = ");
    Serial.print(angleMapped);
    Serial.print("  target = ");
    Serial.print(targetAngle);
    #endif
  
    if(!dmpReady) return;
    if(mpu.dmpGetCurrentFIFOPacket(fifoBuffer))
    {
      mpu.dmpGetQuaternion(&q, fifoBuffer);
      mpu.dmpGetGravity(&gravity, &q);
      mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);

      #ifdef DEBUG_MODE_YPR
        Serial.print("ypr\t");
        Serial.print(ypr[0] * 180/M_PI);
        Serial.print("\t");
        Serial.print(ypr[1] * 180/M_PI);
        Serial.print("\t");
        Serial.println(ypr[2] * 180/M_PI);
      #endif
      angleSample = (ypr[1] * 180/M_PI);
    }
    regData.Hreg(0, angleSample);
    regData.Hreg(1, finalSpeed);
    regData.Hreg(3, targetAngle);
    regData.Hreg(4, INIT_SAMPLE_SIZE);
    regData.Hreg(5, CENTER_OF_MASS_OFFSET);
    regData.Hreg(6, KP);
    regData.Hreg(7, KI);
    regData.Hreg(8, KD);
    LTval = regData.Hreg(9);
    RTval = regData.Hreg(10);
    regData.task();
    
    Serial.print("Left Trigger: ");
    Serial.print(LTval);
    Serial.print(" | Right Trigger: ");
    Serial.println(RTval);

    // Serial.print(angleSample);
    // Serial.print(" ");
    // Serial.print(finalSpeed);
    // Serial.print(" ");
    // Serial.println(targetAngle);
}
