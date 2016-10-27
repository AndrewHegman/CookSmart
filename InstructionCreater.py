instructionCount = 0
instructionBuffer = []

with open("instructions.txt", 'w') as instructionFile:
    pass
def PrintMenu():
    print("Type of Message")
    print("---------------")
    print("1) Instruction")
    print("2) Query")
    print("3) Emergency stop\n")
    print("Instruction type")
    print("----------------")
    print("0) N/A")
    print("1) Add water")
    print("2) Add cartridge")
    print("3) Heat")
    print("4) Stir\n")
    print("If heat, the next number is the heat setting: ")
    print("1) Warm")
    print("2) Low")
    print("3) Med-low")
    print("4) Med")
    print("5) Med-high")
    print("6) High\n")
    print("The next numbers should be one of the following: ")
    print("-------------------------------------------------")
    print("The amount of WATER (in oz) to be added (0-31)")
    print("The amount of TIME (in sec) to heat/stir (0-16,383)")
    print("The CARTRIDGE to open (1-12)\n ")
    print("Concurrency")
    print("-----------")
    print("1) Yes")
    print("2) No")
    print("-----------")
    print("Type \"4\" as the only input to end")

def ConvertToHex(text):
    textStr = str(hex(text))
    if len(textStr) < 4:
        return(textStr[:2]+"0"+textStr[2])
    else:
        return(textStr)

def CreateInstruction(rawInstruction):
    global instructionCount
    packetMSB = 0
    packetMiddle = 0
    packetLSB = 0
    
    if(rawInstruction[1] == "0"):
        print("Invalid instruction!")
        return
    elif(rawInstruction[1] == "1"):   #Water
        packetMSB += 0<<3
        packetMSB += (int(rawInstruction[2:-1])&0x1C)>>2
        packetMiddle += (int(rawInstruction[2:-1])&0x03)<<6
    elif(rawInstruction[1] == "2"):   #Cartridge
        print(rawInstruction[2:-1])
        packetMSB += 1<<3
        packetMSB += (int(rawInstruction[2:-1])&0x0E)>>1
        packetMiddle += ((int(rawInstruction[2:-1])&0x01)<<7)
    elif(rawInstruction[1] == "3"):   #Heat
        packetMSB += 2<<3
        packetMSB += (int(rawInstruction[2])-1)
        packetMiddle += (int(rawInstruction[3:-1])&0x7F80)>>7
        packetLSB += (int(rawInstruction[3:-1])&0x7F)<<1
    elif(rawInstruction[1] == "4"):   #Stir
        packetMSB += 3<<3
        packetMiddle += (int(rawInstruction[2:-1])&0x7F80)>>7
        packetLSB += (int(rawInstruction[2:-1])&0x7F)<<1
    else:
        print("Invalid instruction!")
        return

    if(rawInstruction[-1:] == "1"):
        packetLSB += 1
    print(ConvertToHex(packetMSB), ConvertToHex(packetMiddle), ConvertToHex(packetLSB))
    #with open("instructions.txt", 'a') as instructionFile:
    #    instructionFile.write("instruction"+str(instructionCount)+".MSB = "+str(packetMSB)+";"+"\n")
    #    instructionFile.write("instruction"+str(instructionCount)+".Middle = "+str(packetMiddle)+";"+"\n")
    #    instructionFile.write("instruction"+str(instructionCount)+".LSB = "+str(packetLSB)+";"+"\n")
    #    instructionFile.write("instructionBuffer["+str(instructionCount)+"] = instruction"+str(instructionCount)+";"+"\n\n")
    #instructionCount += 1
    return([packetMSB, packetMiddle, packetLSB])
def CreateQuery(rawInstruction):
    print("Query!")

def CreateESTOP(rawInstruction):
    print("ESTOP!")

def AppendToFile(instructionBuffer, instructionCount):
    with open("instruction.txt", 'w') as instructionFile:
        instructionFile.write("uint8_t dummyMessageArray["+str(instructionCount)+"][3] = {")
        for i in range(len(instructionBuffer)):
            instructionFile.write("{"+ConvertToHex(instructionBuffer[i][0])+
                                  ","+ConvertToHex(instructionBuffer[i][1])+
                                  ","+ConvertToHex(instructionBuffer[i][2])+
                                  "}")
            if i != len(instructionBuffer)-1:
                instructionFile.write(", ")
        instructionFile.write("};")
        #instructionFile.write(instructionBuffer)

def main():
    exit = False
    instructionCount = 0
    while not exit:
        rawInstruction = input("Enter instruction here: ")
        if(rawInstruction[0] == "1"):
            instructionBuffer.append(CreateInstruction(rawInstruction))
            instructionCount += 1
        elif(rawInstruction[0] == "2"):
            CreateQuery(rawInstruction)
        elif(rawInstruction[0] == "3"):
            CreateESTOP(rawInstruction)
        elif(rawInstruction[0] == "4"):
            AppendToFile(instructionBuffer, instructionCount)
            print("Output written to \"instruction.txt\"")
            exit = True
        else:
            print("Invalid input!")


PrintMenu()
main()
