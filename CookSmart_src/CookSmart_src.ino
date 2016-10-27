#include <Adafruit_MotorShield.h>  //Needed for motorshield (stepper motor and DC motor)
#include <Servo.h>  //Needed for servo (foodpod servo)
#include <Wire.h>  //Needed for motorshield (stepper motor and DC motor)
#include <SPI.h>  //Needed for...actually, I don't think this is needed but I'm leaving it
//#include <TimerOne.h>  //Don't think I use this anymore, but I'm not changing it
#include "utility/Adafruit_MS_PWMServoDriver.h"  //Needed for motorshield (stepper motor and DC motor)
//#include "Adafruit_MAX31855.h"  //Don't think I need this anymore, but I'm leaving it
#include <Adafruit_MAX31856.h>
#include <stdint.h>  //This allows me to use explicit definitions like "uint8_t"--its just better code practices
#include "CookSmart.h"

#define MESSAGE_TESTING          (true)
#define TEMPERATURE_TESTING      (false)
#define SYSTEM_TESTING           (false)

//Motorshield Init Functions
Adafruit_MotorShield AFMS = Adafruit_MotorShield();
Adafruit_StepperMotor *foodPodMotor = AFMS.getStepper(200, 2); //200=number of steps (its a 200 step motor, but we use both coils to make half-steps, 2=the port on the motor shield it is connected to)
Adafruit_DCMotor *stirringMotor = AFMS.getMotor(1);  //1=motor port its connected to
Adafruit_MAX31856 thermocouple = Adafruit_MAX31856(10);
Servo foodPodServo;

//Local variables to arduino code
bool concurrent = true;

//These are extra precautionary variables to prevent the instruction loops from executing more than once. I *think* they are unnecessary and should be removed after further (extensive) testing. This would be 
//a nasty bug to try and debug otherwise
bool stirringOneShot = false;
bool heatingOneShot = false;
bool pumpingOneShot = false;

int8_t parseInstructionReturn = 0;
//uint8_t foodPodToTurnTo = -1;
uint32_t statusTimeStamp = 0;
uint32_t tempTimeStamp = 0;
uint32_t foodPodTimeStamp = 0;
uint8_t dummyMessageArray[1][3] = {{0x10,0x04,0xb0}};

void setup() {
  if (VERBOSE)
    Serial.begin(115200);  //allows print statements
  sei();  //This enables interrupts (idk if its necessary anymore, but I'm afraid to take it out)
  AFMS.begin();  //Begins motorshield
  Wire.begin(ADDRESS);  //All Wire stuff *should* be unecessary for you
  
  thermocouple.begin();
  thermocouple.setThermocoupleType(MAX31856_TCTYPE_K);
  
  /*
  float test_temp = thermocouple.readThermocoupleTemperature();
  uint8_t fault = thermocouple.readFault();
  while(test_temp == 0){
    test_temp = thermocouple.readThermocoupleTemperature();
    fault = thermocouple.readFault();
    if (fault) {
      if (fault & MAX31856_FAULT_CJRANGE) Serial.println("Cold Junction Range Fault");
      if (fault & MAX31856_FAULT_TCRANGE) Serial.println("Thermocouple Range Fault");
      if (fault & MAX31856_FAULT_CJHIGH)  Serial.println("Cold Junction High Fault");
      if (fault & MAX31856_FAULT_CJLOW)   Serial.println("Cold Junction Low Fault");
      if (fault & MAX31856_FAULT_TCHIGH)  Serial.println("Thermocouple High Fault");
      if (fault & MAX31856_FAULT_TCLOW)   Serial.println("Thermocouple Low Fault");
      if (fault & MAX31856_FAULT_OVUV)    Serial.println("Over/Under Voltage Fault");
      if (fault & MAX31856_FAULT_OPEN)    Serial.println("Thermocouple Open Fault");
    }else{
      Serial.println("No faults detected");
    }
    Serial.print("Test temp: ");
    Serial.println(test_temp);
    
    delay(500);
  }

  
  Serial.println(test_temp);
  
  */
  Wire.onReceive(ReceiveEvent);
  Wire.onRequest(RequestEvent);

  pinMode(HEATING_ELEMENT_RELAY, OUTPUT);
  pinMode(WATER_PUMP_RELAY, OUTPUT);
  
  pinMode(PHOTO_GATE, INPUT);
  
  pinMode(WATER_PUMP_LED, OUTPUT);
  pinMode(FOOD_POD_LED, OUTPUT);
  pinMode(STIRRING_LED, OUTPUT);
  pinMode(HEATING_LED, OUTPUT);

  foodPodServo.attach(FOOD_POD_SERVO);
  stirringMotor->run(FORWARD);  //Sets direction of stirring motor

  digitalWrite(HEATING_ELEMENT_RELAY, LOW);
  digitalWrite(WATER_PUMP_RELAY, LOW);
  
  
  if(MESSAGE_TESTING){
    SendDummyMessage(3, dummyMessageArray);
  }
  //PrintInstructions();
  
  
}

void loop() {
  while (!emergencyStop) {
    //Serial.println("Here");
    /*******************************************/
    if ((instr_stirring && (!stirring)) && INSTR_ACTIVE_STIRRING){				                //Received instruction to begin stirring, but haven't started yet
      if(MANUAL_LED_CONTROL)
        digitalWrite(STIRRING_LED, HIGH);
      ChangeStirringMotor(stirringMotor, 1, SYSTEM_TESTING, 255);
      
      //if(!stirringOneShot){
        //stirringTime += millis();
      //}
      //Serial.print("Time: ");
      //Serial.println(stirringTime);
      //stirringOneShot = true;
      stirringTime += millis();
    }
    
    if (stirring && ((millis() >= stirringTime) || !instr_stirring)) {		//Currently stirring but time has expired
      ChangeStirringMotor(stirringMotor, 0, SYSTEM_TESTING, 0);                                     //OR currently stirring but no instruction has said to start stirring
      instr_stirring = false;
      stirringOneShot = false;
      if(MANUAL_LED_CONTROL)
        digitalWrite(STIRRING_LED, LOW); 
    }
    /******************************************/

    /******************************************/
    if ((instr_pumping && (!pumping)) && INSTR_ACTIVE_PUMPING){
      if(MANUAL_LED_CONTROL)
        digitalWrite(WATER_PUMP_LED, HIGH);
      ChangeWaterPump(1, SYSTEM_TESTING);
      if(!pumpingOneShot){
        pumpingTime += millis();
      }
      pumpingOneShot = true;
    }

    if (pumping && (millis() >= pumpingTime)) {
      ChangeWaterPump(0, SYSTEM_TESTING);
      instr_pumping = false;
      pumpingOneShot = false;
      if(MANUAL_LED_CONTROL)
        digitalWrite(WATER_PUMP_LED, LOW);
    }
    /******************************************/

    /******************************************/
    if (instr_heating && !heating && CHECK_TEMP_BELOW_TARGET(thermocouple, targetTemperature) && INSTR_ACTIVE_HEATING){      //Heating element has cooled down and should turn back on
      ChangeHeatingElement(true, SYSTEM_TESTING);
      if(MANUAL_LED_CONTROL)
        digitalWrite(HEATING_LED, HIGH);
      if(!heatingOneShot){
        heatingTime += millis();
      }
      heatingOneShot = true;
    }
    if (instr_heating && heating && CHECK_TEMP_ABOVE_TARGET(thermocouple, targetTemperature) && INSTR_ACTIVE_HEATING) {
    //if ((instr_heating && (heating && (GetHeatingElementTemperature(thermocouple) > (targetTemperature * 1.1L)))) && (millis() < heatingTime)){      //Heating element has heated past 1.1 of target temperature
      ChangeHeatingElement(false, SYSTEM_TESTING);
      if(MANUAL_LED_CONTROL)
        digitalWrite(HEATING_LED, LOW);
      if(!heatingOneShot){
        heatingTime += millis();
      }
      heatingOneShot = true;
    }
    if ((millis() >= heatingTime) && instr_heating) {    //Time has expired, turn heater off
      ChangeHeatingElement(false, SYSTEM_TESTING);
      instr_heating = false;
      heatingOneShot = false;
      if(MANUAL_LED_CONTROL)
        digitalWrite(HEATING_LED, LOW);
    }
  /******************************************/
  
  /******************************************/
    if (instr_foodPod){
      if(MANUAL_LED_CONTROL)
        digitalWrite(FOOD_POD_LED, HIGH);
      //Serial.println("Here");
      
      CloseFoodPod(foodPodServo, SYSTEM_TESTING, 90);
      delay(FOOD_POD_DELAY);
      turningFoodPods = true;
      foodPodOpen = false;
      //Serial.println(foodPodToTurnTo);
      ChangeFoodPodMotor(foodPodMotor, foodPodToTurnTo, SYSTEM_TESTING);
      //Serial.println("Here1");
      delay(FOOD_POD_DELAY);
      OpenFoodPod(foodPodServo, SYSTEM_TESTING, 135);
      turningFoodPods = false;
      foodPodOpen = true;
      instr_foodPod = 0;
      if(MANUAL_LED_CONTROL){
        digitalWrite(FOOD_POD_LED, LOW);
      }
    }
    
    if (((instructionReceived && instructionIsConcurrent)
       || (instructionReceived && !instructionIsConcurrent && NO_ACTIVE_INSTRUCTIONS))
       && instructionCount > 0) {
      ParseInstruction();
      if (instructionCount > 0)
        UpdateInstructionBuffer();
      else
        instructionReceived = false;
    }
    
    UpdateStatusLEDs();
    
    if(VERBOSE){
      if (millis() > statusTimeStamp + 100){
        //PrintStatus();
        PrintTempSensor();
        //thermocouple.readThermocoupleTemperature();
        //Serial.println(instructionIsConcurrent);
        statusTimeStamp = millis();
      }
    }
  }
}

void PrintTempSensor(){
  //if(millis() > tempTimeStamp + 1000){
    tempTimeStamp = millis();
    Serial.print("Current temperature: ");
    Serial.print(thermocouple.readThermocoupleTemperature());
    Serial.println(" (C)");
  //}
}

//void PrintStatus(){}


void PrintStatus(){
  if(VERBOSE){
    Serial.println("Instructions: ");
    Serial.print("\tCurrent time: ");
    Serial.println(millis());
    Serial.println("\tStirring:");
    Serial.print("\t\tInstruction: ");
    Serial.println(instr_stirring);
    Serial.print("\t\tTime: ");
    Serial.println(stirringTime);
    Serial.print("\t\tCurrent status: ");
    Serial.println(stirring);
    
    Serial.println("\tPumping:");
    Serial.print("\t\tInstruction: ");
    Serial.println(instr_pumping);
    Serial.print("\t\tAmount: ");
    Serial.println(waterAmount);
    Serial.print("\t\tTime: ");
    Serial.println(pumpingTime);
    Serial.print("\t\tCurrent status: ");
    Serial.println(pumping);
    
    Serial.println("\tHeating:");
    Serial.print("\t\tInstruction: ");
    Serial.println(instr_heating);
    Serial.print("\t\tTarget temperature: ");
    Serial.print(targetTemperature);
    Serial.println(" (F)");
    Serial.print("\t\tCurrent temperature: ");
    Serial.println(thermocouple.readThermocoupleTemperature());
    Serial.print("\t\tTime: ");
    Serial.println(heatingTime);
    Serial.print("\t\tCurrent status: ");
    Serial.println(heating);
    Serial.print("\t\tInstruction active heating: ");
    Serial.println(INSTR_ACTIVE_HEATING);
    Serial.print("\t\tTemperature below target: ");
    Serial.println(CHECK_TEMP_BELOW_TARGET(thermocouple, targetTemperature));
    Serial.print("\t\tTemperature above target: ");
    Serial.println(CHECK_TEMP_ABOVE_TARGET(thermocouple, targetTemperature));
    
    Serial.println("\tFood pod:");
    Serial.print("\t\tInstruction: ");
    Serial.println(instr_foodPod);
    Serial.println("\t\tCurrent food pod: N/A");
    
    Serial.print("\tInstruction is concurrent: ");
    Serial.println(instructionIsConcurrent);
    
    Serial.print("\tInstruction active pumping: ");
    Serial.println(INSTR_ACTIVE_PUMPING);
    
    Serial.print("\tInstruction active stirring: ");
    Serial.println(INSTR_ACTIVE_STIRRING);
    
    Serial.print("\tInstruction active heating: ");
    Serial.println(INSTR_ACTIVE_HEATING);
    
    Serial.print("\tInstruction active foodpod: ");
    Serial.println(INSTR_ACTIVE_FOODPOD);
    
    Serial.print("\t\tFood pod instruction:");
    Serial.println(instr_foodPod);
    Serial.print("\t\tTurning food pods:");
    Serial.println(turningFoodPods); 
  }
}

/*
void PrintInstructions(){
  if(VERBOSE){
    for(uint8_t i=0; i<instructionCount; ++i){
      Serial.print("Instruction ");
      Serial.print(i);
      Serial.println(": ");
      switch(GET_INSTR_TYPE(instructionBuffer[i].MSB)){
        case 0x00:
          Serial.println("Add water");
          Serial.print("Amount to add: ");
          Serial.print(GET_WATER_AMT(instructionBuffer[i].MSB, instructionBuffer[i].Middle));
          Serial.println("oz");
          break;
        case 0x01:
          Serial.println("Add cartridge");
          Serial.print("Cartridge to add: ");
          Serial.println(GET_CARTRIDGE(instructionBuffer[i].MSB, instructionBuffer[i].Middle));
          break;
        case 0x02:
          Serial.println("Turn heat on");
          Serial.print("Heat level: ");
          Serial.println(GET_HEAT(instructionBuffer[i].MSB));
          Serial.print("Temperature set point: ");
          Serial.println(temperatureArray[GET_HEAT(instructionBuffer[i].MSB)]);
          Serial.print("Total Time: ");
          Serial.println(GET_TIME(instructionBuffer[i].Middle, instructionBuffer[i].LSB));
          break;
        case 0x03:
          Serial.println("Begin stirring");
          Serial.print("Time: ");
          Serial.println(GET_TIME(instructionBuffer[i].Middle, instructionBuffer[i].LSB));
          break;
      }
      Serial.print("Concurrent: ");
      if(IS_CONCURRENT(instructionBuffer[i].LSB)){
        Serial.println("Yes\n\n");
      }else{
        Serial.println("No\n\n");
      }
    }
    Serial.println("Is this correct?");
    //WaitForInput();
  }
}
*/

void PrintStatus_Simple(){
  Serial.print("Stirring: ");
  Serial.println(instr_stirring);
  Serial.print("Heating: ");
  Serial.println(instr_heating);
  Serial.print("Pumping: ");
  Serial.println(instr_pumping);
  Serial.print("Turning: ");
  Serial.println(turningFoodPods);
  Serial.println("\n");
}

