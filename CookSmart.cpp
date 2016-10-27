#include <Arduino.h>
#include <stdint.h> 
#include <CookSmart.h>



uint8_t instructionCount = 0;
bool instructionReceived = false;
Message instructionBuffer[50];

//Current Status
bool turningFoodPods = false;
bool heating = false;
bool stirring = false;
bool pumping = false;
bool foodPodOpen = false;
bool globalHeating = false;
int8_t currentFoodPodPosition = -1;
bool emergencyStop = false;
int8_t foodPodToTurnTo = -1;


//Instruction booleans
bool instr_TurningFoodPods = false;
bool instr_heating = false;
bool instr_stirring = false;
bool instr_pumping = false;
uint8_t instr_foodPod = 0;
bool newMessageReceived = false;
bool instructionIsConcurrent = true;
uint32_t heatingTime = 0;
uint32_t stirringTime = 0;
uint32_t pumpingTime = 0;
uint32_t foodPodTime = 0;
uint32_t waterAmount = 0;

uint16_t targetTemperature = 0;

uint16_t temperatureArray[6] = {HEAT_LEVEL_0, HEAT_LEVEL_1, HEAT_LEVEL_2, HEAT_LEVEL_3, HEAT_LEVEL_4, HEAT_LEVEL_5};

void ReceiveEvent(int useless){  //This variable is literally not used, but it is 'required'
	uint8_t tempBufIndex = 0;
	uint8_t tempBuf[3];
	Message MsgRcvd;
	if(VERBOSE)
  		Serial.println("Received from master: ");
  	while(Wire.available()){
	    tempBuf[tempBufIndex] = (uint8_t)Wire.read();
	    ++tempBufIndex;
	    if(tempBufIndex == 3){
	    	tempBufIndex = 0;
	      	if(MSG_IS_ESTOP(MsgRcvd.MSB)){
	      		MsgRcvd.MSB = tempBuf[0];	//Is this necessary?
	        	emergencyStop = true;
	      	}else{
		      	MsgRcvd.MSB = tempBuf[0];
		        MsgRcvd.Middle = tempBuf[1];
		        MsgRcvd.LSB = tempBuf[2];
		        instructionBuffer[instructionCount] = MsgRcvd;

		        ++instructionCount;
		        instructionReceived = true;
		        if(VERBOSE){
		        	Serial.print("instruction count: ");
		        	Serial.println(instructionCount);
		        }
		    }
		}
	}
}

void RequestEvent(){
	if(VERBOSE)
  		Serial.println("Request from master!");

	uint8_t firstByteToSend = (turningFoodPods<<3) + (globalHeating<<2) + (stirring<<1) + (pumping);
	uint8_t secondByteToSend = 0;
	uint8_t statusToSend[2] = {secondByteToSend, firstByteToSend};

	if(VERBOSE){
		Serial.print("First byte: ");
		Serial.println(firstByteToSend);
		Serial.print("Second byte: ");
		Serial.println(secondByteToSend);
	}

	Wire.write(statusToSend, 2);
}

void EmergencyStop(Adafruit_DCMotor* stirringMotor){
	ChangeStirringMotor(stirringMotor, 0, 0, 0);
	//Turn heating element off
	//Release stepper motor
	FlashEstopLED();
}

void ParseInstruction(){
	Message Instr = instructionBuffer[0];
	switch(GET_INSTR_TYPE(Instr.MSB)){
	case 0x00:
		waterAmount = GET_WATER_AMT(Instr.MSB, Instr.Middle);
		pumpingTime = CalculatePumpingTime(waterAmount);
		instr_pumping = true;
		break;

	case 0x01:
		//Because the stepper motor library is blocking, change LED state in this block
		//digitalWrite(FOOD_POD_LED, HIGH);
		//instr_foodPod = GET_CARTRIDGE(Instr.MSB, Instr.Middle);
		//digitalWrite(FOOD_POD_LED, LOW);
		instr_foodPod = 1;
		foodPodToTurnTo = GET_CARTRIDGE(Instr.MSB, Instr.Middle);
		break;

	case 0x02:
	/*
		heating = true;
		globalHeating = true;
		currentHeatSetting = temperatureArray[GET_HEAT(Instr.MSB)];
		currentHeatTimeMaster = GET_TIME(Instr.Middle, Instr.LSB)*1000L + millis();
		currentHeatTime = GET_HEATING_TIME(currentHeatSetting*1.1F, currentTemperature)*1000L+millis();
		TurnHeatingElementON();
		*/
		targetTemperature = temperatureArray[GET_HEAT(Instr.MSB)];
		heatingTime = GET_TIME(Instr.Middle, Instr.LSB)*((uint32_t)1000);
		instr_heating = true;
		break;

	case 0x03:
		stirringTime = ((uint32_t)(GET_TIME(Instr.Middle, Instr.LSB))*((uint32_t)1000));
		instr_stirring = true;
	break;
	}
	instructionIsConcurrent = IS_CONCURRENT(Instr.LSB);
}

void UpdateStatusLEDs(){
	digitalWrite(WATER_PUMP_LED, pumping);
	digitalWrite(FOOD_POD_LED, turningFoodPods);
	digitalWrite(HEATING_LED, heating);
	digitalWrite(STIRRING_LED, stirring);
}

void ChangeStirringMotor(Adafruit_DCMotor *stirringMotor, int8_t direction, bool testing, uint8_t spd=255){
	
	if(VERBOSE){
		Serial.println("Stirring motor on");
		Serial.print("\tDirection: ");
		Serial.println(direction);
		Serial.print("\tSpeed: ");
		Serial.println(spd);
	}
	
	if(direction==-1){
		if(!testing){
			stirringMotor->setSpeed(spd);
			stirringMotor->run(BACKWARD);
		}
		stirring = true;
	}else if(direction==1){
		if(!testing){
			stirringMotor->setSpeed(spd);
			stirringMotor->run(FORWARD);
		}
		stirring = true;
	}else if(direction==0){
		if(!testing){
			stirringMotor->setSpeed(0);
		}
		stirring = false;
	}else{
		Serial.println("ERROR - invalid DIRECTION parameter passed to ChangeStirringMotor(int, int)");
		Serial.print("\tdirection(int8_t): ");
		Serial.println(direction);
		Serial.print("\ttesting(bool): ");
		Serial.println(testing);
		Serial.print("\tspd(uint8_t): ");
		Serial.println(spd);
	}
}
/*
void ChangeFoodPodMotor(Adafruit_StepperMotor *foodPodMotor, int8_t foodPod, bool testing){
	//turningFoodPods = true;
	float degreesToTurn = CalculateDegressToTurn(currentFoodPodPosition, foodPod);
	if(VERBOSE){
		Serial.print("Turning to food pod: ");
		Serial.println(foodPod);
		Serial.print("Degress to turn: ");
		Serial.println(degreesToTurn);
	}
	
	delay(750);
	if(VERBOSE){
		Serial.print("Current food pod position: ");
		Serial.println(currentFoodPodPosition);
	}
	if(currentFoodPodPosition == -1){
		if(FindPosZero(*foodPodMotor, testing)<0){
			Serial.println("ERROR - unable to find zero position");
			Serial.println("Emergency Stop!");
			emergencyStop = true;
		}  
	}
	if(!emergencyStop){
		delay(750);
	
		if((foodPod-(currentFoodPodPosition*30))>0){	//(currentFoodPodPosition*30) used to be "tempCurrentPos" -- see CalculateDegreesToTurn()
			if(VERBOSE)
				Serial.println("Direction to turn: FORWARD");
			if(!testing)
				foodPodMotor->step((degreesToTurn/0.9), FORWARD, INTERLEAVE);
		}else{
			if(VERBOSE)
				Serial.println("Direction to turn: FORWARD");
			if(!testing)
				foodPodMotor->step((degreesToTurn/0.9), BACKWARD, INTERLEAVE);
		}
		currentFoodPodPosition = foodPod/30;
		delay(500);
		//OpenFoodPod();
		//turningFoodPods = false;
	}
}
*/

void ChangeFoodPodMotor(Adafruit_StepperMotor *foodPodMotor, int8_t destPod, bool testing){
	uint8_t dir = 1;
	uint16_t podsToTurn = 0;
  	if ((abs((currentFoodPodPosition - destPod)) < 12/2 && currentFoodPodPosition < destPod) || 
       (abs((currentFoodPodPosition - destPod)) > 12/2 && currentFoodPodPosition > destPod)){
    	dir = 1;
    
  	}else{
    	dir = 2;
  	}
	//Arduino doesn't handle negative modulo well, so this is how it can be done otherwise:
	if(currentFoodPodPosition == -1){
		if(FindPosZero(*foodPodMotor, testing)<0){
			Serial.println("ERROR - unable to find zero position");
			Serial.println("Emergency Stop!");
			emergencyStop = true;
		}  
	}
	if(!emergencyStop){
		delay(750);
		podsToTurn = min(mod(currentFoodPodPosition-destPod, 12), mod(destPod-currentFoodPodPosition, 12));
		/*
		Serial.print("Pods to turn: ");
		Serial.println(podsToTurn);
		Serial.print("Steps to turn: ");
		Serial.println(podsToTurn * 30);
		Serial.print("Direction: ");
		Serial.println(dir);
		Serial.print("Turning ");
		Serial.print(podsToTurn * 30);
		Serial.println(" steps\n");
		*/
		foodPodMotor->step(podsToTurn * 30, dir, INTERLEAVE);
		Serial.println("Here2");
		currentFoodPodPosition = destPod;
		//delay(500);
		//OpenFoodPod(foodPodServo, testing, 135);
	}
}

float CalculateDegressToTurn(uint8_t currentPosition, uint8_t foodPod){
	float degreesBetweenPods = 0;
	float degreesToTurn = 0;
	uint8_t tempCurrentPos;

	tempCurrentPos = currentFoodPodPosition*30;
	foodPod *= 30;
	degreesBetweenPods = abs(foodPod-tempCurrentPos);
	degreesToTurn = abs(foodPod-tempCurrentPos)*0.9;
	while(degreesToTurn < degreesBetweenPods){
		degreesToTurn += 0.9;
	}
	if(degreesToTurn > degreesBetweenPods){
		degreesToTurn -= 0.9;
	}

	return(degreesToTurn);
}

void OpenFoodPod(Servo foodPodServo, bool testing, uint8_t degrees=135){
	Serial.println("Here");
	if(VERBOSE)
		Serial.println("Open food pod");
	if(!testing);
	foodPodServo.write(degrees);
	//currentFoodPodTime = FOOD_POD_TIME + millis();
	foodPodOpen = true;
}

void CloseFoodPod(Servo foodPodServo, bool testing, uint8_t degrees=90){
	Serial.println("here");
	if(VERBOSE)
		Serial.println("Close food pod");
	if(!testing)
		foodPodServo.write(90);
	foodPodOpen = false;
}

int8_t FindPosZero(Adafruit_StepperMotor &foodPodMotor, bool testing){
	if(!testing){
		if(!digitalRead(PHOTO_GATE)){
			if(VERBOSE)
				Serial.println("Foodpods already in position 0!");
			currentFoodPodPosition = 1;
			return(0);
		}
		uint16_t turnCount = 0;
		while(digitalRead(PHOTO_GATE)){
			if(VERBOSE)
				Serial.println("Turning foodpods");
			foodPodMotor.step(1, FORWARD, INTERLEAVE);
			turnCount++;
			if(VERBOSE){
				Serial.print("Turn count: ");
				Serial.println(turnCount);
			}
			if(turnCount > 400){
				return(-1);
			}
		}
		currentFoodPodPosition = 1;
		foodPodMotor.step(10, FORWARD, INTERLEAVE);
	}
	return(0);
}

void ChangeWaterPump(uint8_t mode, bool testing){
	switch(mode){
		case 0:
			if(!testing)
				digitalWrite(WATER_PUMP_RELAY, LOW);
			pumping = false;
			break;
		case 1:
			if(!testing)
				digitalWrite(WATER_PUMP_RELAY, HIGH);
			pumping = true;
			break;
		default:
			if(VERBOSE){
				Serial.println("ERROR - invalid parameter passed to ChangeWaterPump(int, bool)");
				Serial.print("\tmode(uint8_t): ");
				Serial.println(mode);
				Serial.print("\ttesting(bool): ");
				Serial.println(testing);
			}
	}
}

uint32_t CalculatePumpingTime(uint32_t amtOZ){
	uint32_t amtML;
	amtML = (amtOZ*29.574F);
	return(((amtML/PUMP_SPEED)*1000L)+10000L+millis());
}

uint16_t GetHeatingElementTemperature(Adafruit_MAX31856 thermocouple){
	//Read thermocouple
	return(thermocouple.readFarenheit());
}

void ChangeHeatingElement(bool state, bool testing){
	if(!testing)
		digitalWrite(HEATING_ELEMENT_RELAY, state);
	heating = state;
}

void UpdateInstructionBuffer(){
	for(int i=0;i<(instructionCount-1);++i){
		instructionBuffer[i] = instructionBuffer[i+1];
	}
	--instructionCount;
}

void SendDummyMessage(uint8_t numMessages, uint8_t messageArray[][3]){
	
	Message DummyMsg;

	for(uint8_t i=0; i<numMessages; ++i){
		DummyMsg.MSB = messageArray[i][0];
		Serial.println(messageArray[i][0]);

		DummyMsg.Middle = messageArray[i][1];
		Serial.println(messageArray[i][1]);

		DummyMsg.LSB = messageArray[i][2];
		Serial.println(messageArray[i][2]);
		instructionBuffer[instructionCount] = DummyMsg;
		++instructionCount; 
	}
	instructionReceived = true;
	/*
	DummyMsg.MSB = 0x03;
	DummyMsg.Middle = 0xC0;
	DummyMsg.LSB = 0x01;
	instructionBuffer[instructionCount] = DummyMsg;
	++instructionCount;

	DummyMsg.MSB = 0x18;
	DummyMsg.Middle = 0x00;
	DummyMsg.LSB = 0x15;
	instructionBuffer[instructionCount] = DummyMsg;
	++instructionCount;
	instructionReceived = true;
	*/

}

void FlashEstopLED(){
	while(1){
		digitalWrite(ESTOP_LED, HIGH);
		delay(500);
		digitalWrite(ESTOP_LED, LOW);
		delay(500);
	}
}