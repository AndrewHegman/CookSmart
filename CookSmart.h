
//test
#ifndef MAIN_H
#define MAIN_H

#include <Arduino.h>
#include <stdint.h> 
#include "Adafruit_MotorShield.h"
#include <Servo.h>
#include <Wire.h>
#include <SPI.h>
//#include "Adafruit_MAX31855.h"
#include <Adafruit_MAX31856.h>
#include "utility/Adafruit_MS_PWMServoDriver.h"


//Testing definitions
#define VERBOSE							(true)

#define ADDRESS							(0x05)
#define MSG_IS_ESTOP(INSTR)				(INSTR == 0x40 ? 1:0)
#define GET_HEAT(MSB)                   (MSB & 0x07)
#define GET_TIME(Middle, LSB)           (((Middle & 0xFF)<<7)+((LSB & 0xFE)>>1))
#define IS_CONCURRENT(LSB)              (LSB & 0x01)
#define GET_CARTRIDGE(MSB, Middle)      (((MSB & 0x07)<<1)+((Middle>>7)&0x01))
#define GET_INSTR_TYPE(BYTE)            ((BYTE & 0x38) >> 3)
#define GET_WATER_AMT(MSB, Middle)      ((((MSB & 0x07)<<2)+((Middle & 0xC0)>>6))+1)
//#define INSTR_ACTIVE_PUMPING			((!instructionIsConcurrent && (!instr_stirring && !stirring) && (!instr_heating && !heating) && (!instr_foodPod && !turningFoodPods)) || instructionIsConcurrent)
#define INSTR_ACTIVE_PUMPING			((!instructionIsConcurrent && (!instr_stirring && !stirring) && (!instr_heating && !heating) && (!instr_foodPod && !turningFoodPods)) || instructionIsConcurrent)
#define INSTR_ACTIVE_STIRRING			((!instructionIsConcurrent && (!instr_pumping && !pumping) && (!instr_heating && !heating) && (!instr_foodPod && !turningFoodPods)) || instructionIsConcurrent)
#define INSTR_ACTIVE_HEATING			((!instructionIsConcurrent && (!instr_stirring && !stirring) && (!instr_pumping && !pumping) && (!instr_foodPod && !turningFoodPods)) || instructionIsConcurrent)
#define INSTR_ACTIVE_FOODPOD			((!instructionIsConcurrent && (!instr_stirring && !stirring) && (!instr_pumping && !pumping) && (!instr_heating && !heating)) || instructionIsConcurrent)

//Relay/misc peripherals definitions
#define PHOTO_GATE						(7)
#define WATER_PUMP_RELAY 				(6)
#define HEATING_ELEMENT_RELAY			(8)
#define FOOD_POD_SERVO                  (9)
#define PUMP_SPEED                      (133L)
#define TEMP_SENSOR_DO					(A2)
#define TEMP_SENSOR_CS					(A3)
#define TEMP_SENSOR_CLK					(A4)
#define TEMP_SENSOR_DIN					(A5)


//LED Definitions
#define WATER_PUMP_LED					(10)
#define FOOD_POD_LED					(12)
#define STIRRING_LED                    (13)
#define HEATING_LED						(11)
#define ESTOP_LED						(2)
#define MANUAL_LED_CONTROL				(false)

//Temperature constants (Farhenheit or Celsius????)
#define HEAT_LEVEL_0                    (15)  
#define HEAT_LEVEL_1                    (237)
#define HEAT_LEVEL_2                    (287)
#define HEAT_LEVEL_3                    (350)
#define HEAT_LEVEL_4                    (412)
#define HEAT_LEVEL_5                    (550)

//Logic for instructions, because I'm lazy and my code is confusing..
#define NO_ACTIVE_INSTRUCTIONS			(!instructionIsConcurrent && !instr_heating && !instr_pumping && !instr_stirring && !turningFoodPods)
#define CHECK_INSTR_HEATING													(instr_heating && !heating)
#define CHECK_TEMP_BELOW_TARGET(thermocouple, targetTemperature)			(GetHeatingElementTemperature(thermocouple) <= targetTemperature * 1.1L)
#define CHECK_TEMP_ABOVE_TARGET(thermocouple, targetTemperature)	        (GetHeatingElementTemperature(thermocouple) > targetTemperature * 0.9L)

#define mod(a, b)           			(((a % b) + b) % b)
#define FOOD_POD_DELAY					(1000)	//milliseconds to wait before turning foodPods after 'closing', and before opening

typedef struct{
	byte MSB;
	byte Middle;
	byte LSB;
}Message;

extern uint8_t instructionCount;
extern bool instructionReceived;
extern Message instructionBuffer[50];
extern int8_t foodPodToTurnTo;
extern bool turningFoodPods;
extern bool globalHeating;
extern bool stirring;
extern bool pumping;
extern bool heating;
extern bool instructionIsConcurrent;
extern uint32_t heatingTime;
extern uint32_t stirringTime;
extern uint32_t pumpingTime;
extern uint32_t foodPodTime;
extern uint32_t waterAmount;
extern bool instr_TurningFoodPods;
extern bool instr_heating;
extern bool instr_stirring;
extern bool instr_pumping;
extern uint8_t instr_foodPod;
extern uint16_t heatingElementTemperature;
extern uint16_t targetTemperature;
extern bool emergencyStop;
extern uint16_t temperatureArray[6];
extern bool foodPodOpen;

void ReceiveEvent(int);
void RequestEvent();
void EmergencyStop(Adafruit_DCMotor*);
void ParseInstruction();
void UpdateStatusLEDs();
void SendDummyMessage(uint8_t, uint8_t[][3]);
void ChangeStirringMotor(Adafruit_DCMotor*, int8_t, bool, uint8_t);
void ChangeFoodPodMotor(Adafruit_StepperMotor*, int8_t, bool);
float CalculateDegressToTurn(uint8_t, uint8_t);
void OpenFoodPod(Servo, bool, uint8_t);
void CloseFoodPod(Servo, bool, uint8_t);
int8_t FindPosZero(Adafruit_StepperMotor&, bool);
void ChangeWaterPump(uint8_t, bool);
uint32_t CalculatePumpingTime(uint32_t);
uint16_t GetHeatingElementTemperature(Adafruit_MAX31856);
void ChangeHeatingElement(bool, bool);
void UpdateInstructionBuffer();
void FlashEstopLED();

#endif

