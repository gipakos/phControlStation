// UPDATE 17/06/25

#include <ArduinoJson.h>
#include <Wire.h>
#include <avr/wdt.h>

// I2C comms variables---------
//char buffer[32];

byte command = 0;

struct DataPacket {
  int currentStep;
  float phValue;
};

DataPacket dataToSend;
// ---------------------------

int ledPin = 10; // to be connected 
int ledFlotterPin = 11;
int ledTurbineMotorPin = 12;

int startButtonPin = 2; // na to allaksw se startButtonPin kai na ftiaksw kwdika
int emergencyStopButtonPin = 13; // Not used now, just make a reset button

int flotterPin = 3;

int pump1Pin = 6;
int pump2Pin = 7;
int peristalticPumpPin = 8;
int mixingMotorPin = 9;

bool startButtonState = 0;
bool stopButtonState = 0;
bool isReactorTankFilled = 0;
bool flotterState = 0;
bool startCycle = 0;
bool hmiStartButtonState = 0;
bool printed = 0;

//==== Mixing motor variables =====
unsigned long mixingMotorStartTime = 0;
bool mixingMotorRunning = false;

//===PH CONTROL VARIABLES============
float phNeutralValue = 6.5;
float phValue = 0;
float calibration_value = 31.34;
unsigned long int avgval;
int buffer_arr[10], temp;
//===================================
int otherValue = 0;

String led1Status = "";


// SFC Steps
enum Step {
  STEP_STANDBY,
  STEP_ACTIVE,
  STEP_RELEASE,
  STEP_CHECK_REACTOR_TANK_LEVEL,
  STEP_STOP_PUMP1,
  STEP_START_PUMP1,
  STEP_START_MIXING_MOTOR,
  STEP_STOP_MIXING_MOTOR,
  STEP_CHECK_PH_LEVEL,
  STEP_START_PERISTALTIC_PUMP,
  STEP_STOP_PERISTALTIC_PUMP,
  STEP_START_PUMP2,
  STEP_STOP_PUMP2
};

Step currentStep = STEP_STANDBY;
unsigned long stepStartTime = 0;

unsigned long currentMillis = 0;
unsigned long previousMillis = 0;


struct JSONFromNodeData {
  int hmiStartCycle;
  int humidity;
};

void setup() {
  // put your setup code here, to run once:

  Wire.begin(8);
  Wire.onRequest(sendData);
  Wire.onReceive(receiveEvent); // Callback when master sends data

  pinMode(startButtonPin, INPUT);
  pinMode(emergencyStopButtonPin, INPUT);

  pinMode(ledPin, OUTPUT);
  pinMode(ledFlotterPin, OUTPUT);
  pinMode(ledTurbineMotorPin, OUTPUT);
  pinMode(flotterPin, INPUT);
  pinMode(pump1Pin, OUTPUT);
  pinMode(pump2Pin, OUTPUT);
  pinMode(peristalticPumpPin, OUTPUT);
  pinMode(mixingMotorPin, OUTPUT);

  Serial.begin(9600);

  // Clear serial buffer
  while (Serial.available() > 0) {
    Serial.read(); // Discard all incoming bytes
  }

  startButtonState = 0;

}

void loop() {
  // put your main code here, to run repeatedly:


  //====Set all pumps and motor relays in initial state ZERO==========================
  digitalWrite(pump1Pin, HIGH); //ANTISTROFI - Relay = LOW Edw kleinw arxika tin 1
  digitalWrite(pump2Pin, HIGH); //ANTISTROFI - Relay = LOW Edw kleinw arxika tin 2
  digitalWrite(peristalticPumpPin, HIGH); //ANTISTROFI - Relay = LOW Edw kleinw arxika tin 3 peristaltiki antlia
  if(!mixingMotorRunning){
    digitalWrite(mixingMotorPin, HIGH); //ANTISTROFI - Relay = LOW Edw kleinw ton anadeutira motor
  }
  

  delay(50);

   //======START CYCLE CONTROL==========================

  //startCycle = getStartCycle(); // POLU MNIMI! Isws na valw allo koumpi gia na kskeinaei to sustima i OXI gia aplopoihsh.

  // Clear serial buffer
  // while (Serial.available() > 0) {
  //   Serial.read(); // Discard all incoming bytes
  // }

  getFlotterState(); // den kserw an tha to kratisw auto edw einai gia na tsekarei to led tou flotter

  sendCurrentPhData();

  currentMillis = millis();

  switch (currentStep) {

  case STEP_STANDBY:

  if (!printed) {
      Serial.println("System ready: Waiting for Cycle Startup...");

      // I2C comms
      setCurrentStepData();

      printed = true;
    }

  startButtonState = digitalRead(startButtonPin);
    //hmiStartButtonState = getHmiStartButtonState(); // Dimiourgei bug sto nodered

  if (startButtonState == HIGH || hmiStartButtonState == HIGH) {
    
    // DEBUG
    Serial.println("Start button pin state: ");
    Serial.println(startButtonState);

    Serial.println("HMI start button state: ");
    Serial.println(hmiStartButtonState);
    
    currentStep = STEP_CHECK_REACTOR_TANK_LEVEL;
    printed = false;
  }

  break;

  case STEP_CHECK_REACTOR_TANK_LEVEL:

  if (!printed) {
      Serial.println("Checking reactor tank level...");
      printed = true;

      // I2C comms
      setCurrentStepData();
    }


  if (currentMillis - previousMillis >= 1000){
    previousMillis = currentMillis;

      if(getFlotterState()){
        Serial.println("Reactor tank is full...");
        isReactorTankFilled = 1;
        currentStep = STEP_STOP_PUMP1;
  
      } else if(!getFlotterState()){
        Serial.println("Reactor tank is empty...");
        isReactorTankFilled = 0;
        currentStep = STEP_START_PUMP1;
      }
      printed = false;
    }
    break;

  case STEP_START_PUMP1:

  if (!printed) {
      Serial.println("Starting pump 1. Filling the reactor tank...");
      printed = true;
      // I2C comms
      setCurrentStepData();
    }

    if (currentMillis - previousMillis >= 5000) {
      previousMillis = currentMillis;

      while(!getFlotterState() && isReactorTankFilled == 0){
        digitalWrite(pump1Pin, LOW); //ATISTROFO -> RELAY = HIGH
      }
      currentStep = STEP_CHECK_REACTOR_TANK_LEVEL;
      printed = false;
    }
    break;

    case STEP_STOP_PUMP1:

    if (!printed) {
      Serial.println("Stopping Pump 1...");
      printed = true;

      // I2C comms
      setCurrentStepData();
    }

    digitalWrite(pump1Pin, HIGH); //ATISTROFO -> RELAY = LOW

    if (currentMillis - previousMillis >= 5000) {
      previousMillis = currentMillis;
      Serial.println("Pump 1 has stopped...");
      currentStep = STEP_START_MIXING_MOTOR;
      printed = false;
    }
    break;

    case STEP_START_MIXING_MOTOR:

    if (!printed) {
      Serial.println("Starting the mixing motor...");
      // I2C comms
      setCurrentStepData();
      printed = true;
    }

    if (!mixingMotorRunning) {
        Serial.println("Starting the mixing motor...");
        digitalWrite(mixingMotorPin, LOW);  // Turn motor ON REALY = HIGH
        mixingMotorStartTime = millis();
        mixingMotorRunning = true;
      }

      // Check if it's time to stop mixing
      if (millis() - mixingMotorStartTime >= 5000) {
        previousMillis = currentMillis;
        printed = false;
        currentStep = STEP_STOP_MIXING_MOTOR;  // Move to next case
      }

    break;

    case STEP_STOP_MIXING_MOTOR:

    if (!printed) {
      Serial.println("Stopping the mixing motor...");
      printed = true;
      // I2C comms
      setCurrentStepData();
    }

    digitalWrite(mixingMotorPin, HIGH);  // Turn motor OFF RELAY = LOW

    if (currentMillis - previousMillis >= 5000) {
      previousMillis = currentMillis;

      if (mixingMotorRunning) {
        mixingMotorRunning = false;
      }

      printed = false;
      currentStep = STEP_CHECK_PH_LEVEL;
    }

    break;

    case STEP_CHECK_PH_LEVEL:

    if (!printed) {
      Serial.println("Checking the pH levels...");
      printed = true;
      // I2C comms
      setCurrentStepData();
    }

    if (currentMillis - previousMillis >= 5000) {
      previousMillis = currentMillis;      

      phValue = getPHValue();

      if(phValue >= phNeutralValue){
        Serial.println("Solution is OK, starting evacuation...");
        currentStep = STEP_START_PUMP2;
      } else if(phValue < phNeutralValue){
        Serial.println("Solution needs correction!!!");
        currentStep = STEP_START_PERISTALTIC_PUMP;
      }
      printed = false;

    }

    break;

    case STEP_START_PERISTALTIC_PUMP:

    if (!printed) {
      Serial.println("Starting the peristaltic pump...");
      printed = true;
      // I2C comms
      setCurrentStepData();
    }

    digitalWrite(peristalticPumpPin, LOW); //ANTISTROFO -> RELAY = HIGH
    delay(5000);

    currentStep = STEP_STOP_PERISTALTIC_PUMP;
    printed = false;

    break;

    case STEP_STOP_PERISTALTIC_PUMP:

    if (!printed) {
      Serial.println("Stopping the peristaltic pump...");
      printed = true;
      // I2C comms
      setCurrentStepData();
    }

    digitalWrite(peristalticPumpPin, HIGH); //ANTISTROFO -> RELAY = LOW
    delay(5000);

    Serial.println("Peristaltic pump is now stopped...");
    
    currentStep = STEP_START_MIXING_MOTOR;
    printed = false;

    break;

    case STEP_START_PUMP2:

    if (!printed) {
      Serial.println("Starting pump 2...Clearing the reactor...");
      printed = true;
      // I2C comms
      setCurrentStepData();
    }

    digitalWrite(pump2Pin, LOW); //ANTISTROFO -> RELAY = HIGH
    delay(5000);

    currentStep = STEP_STOP_PUMP2;
    printed = false;

    break;

    case STEP_STOP_PUMP2:

    if (!printed) {
      Serial.println("Stopping pump 2...");
      printed = true;
      // I2C comms
      setCurrentStepData();
    }

    digitalWrite(pump2Pin, HIGH); //ANTISTROFO -> RELAY = LOW
    delay(5000);

    currentStep = STEP_CHECK_REACTOR_TANK_LEVEL;
    printed = false;

    break;

  } // End Switch

  delay(50); // Small delay for debounce and stability
  
}


// ===== MY FUNCTIONS =================================

JSONFromNodeData readJSONFromNode() {

  if (Serial.available()) {

    delay(100);

    // Read the incoming serial data
    String jsonString = Serial.readStringUntil('\n'); // Assume each JSON ends with newline

    // Allocate a JSON document (adjust size as needed)
    StaticJsonDocument<200> doc;

    // Parse the JSON
    DeserializationError error = deserializeJson(doc, jsonString);

    // Test if parsing succeeds.
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }
    //Access values
    // const char* status = doc["statusLED1"];
    // led1Status = status;

    JSONFromNodeData data;
    data.hmiStartCycle = doc["hmiStartCycleStatus"];  // Virtual HMI start cycle button
    // data.humidity = analogRead(A1);     // Simulated humidity sensor
    return data;
  }
}

bool getHmiStartButtonState() {

  if (Serial.available()) {

    delay(100);

    // Read the incoming serial data
    String jsonString = Serial.readStringUntil('\n'); // Assume each JSON ends with newline

    // Allocate a JSON document (adjust size as needed)
    StaticJsonDocument<200> doc;

    // Parse the JSON
    DeserializationError error = deserializeJson(doc, jsonString);

    // Test if parsing succeeds.
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }
    //Access values
    // const char* status = doc["statusLED1"];
    // led1Status = status;

    hmiStartButtonState = doc["hmiStartButtonState"];  // Virtual HMI start cycle button
    // data.humidity = analogRead(A1);     // Simulated humidity sensor
    return hmiStartButtonState;
  }
}

bool getStartCycle(){
  startButtonState = digitalRead(startButtonPin);
  hmiStartButtonState = getHmiStartButtonState();

  if(startButtonState == 1 || hmiStartButtonState == 1){
    hmiStartButtonState = 1;
    startButtonState = 1;
    startCycle = 1;
  } else if(startButtonState == 0 || hmiStartButtonState == 0) {
    hmiStartButtonState = 0;
    startButtonState = 0;
    startCycle = 0;
  }

  return startCycle;
}

// Flotter control function

bool getFlotterState(){

  flotterState = digitalRead(flotterPin);

  if(flotterState == 1){
    digitalWrite(ledFlotterPin, HIGH);
  } else {
    digitalWrite(ledFlotterPin, LOW);
  }
  return flotterState;

}

float getPHValue() {

  for (int i = 0; i < 10; i++) {
    buffer_arr[i] = analogRead(A0);
    delay(30);

    }

    for (int i = 0; i < 9; i++) {
      for (int j = i + 1; j < 10; j++) {
        if (buffer_arr[i] > buffer_arr[j]) {
          temp = buffer_arr[i];
          buffer_arr[i] = buffer_arr[j];
          buffer_arr[j] = temp;
        }
      }
    }

    avgval = 0;

    for (int i = 2; i < 8; i++)
      avgval += buffer_arr[i];
    float volt = (float) avgval * 5.0 / 1024 / 6;
    float ph_act = -5.70 * volt + calibration_value;

  return ph_act;

}

// void sendData() {
//   Wire.write(buffer);
// }

void sendData() {
  Wire.write((byte *)&dataToSend, sizeof(dataToSend));
}

void setCurrentStepData() {
  dataToSend.currentStep = currentStep;
  // Debug
  Serial.print("Data to send to the master: ");
  Serial.println(dataToSend.currentStep);
}

void sendCurrentPhData() {
  static unsigned long lastPHReadTime = 0;  // Keeps value across calls

  unsigned long currentPHTime = millis();

  if (currentPHTime - lastPHReadTime >= 2000) {
    lastPHReadTime = currentPHTime;

    float phValue = getPHValue();

    dataToSend.phValue = phValue;

    Serial.print("Data to send to the master: ");
    Serial.println(dataToSend.phValue);
  }
}

void receiveEvent(int bytes) {
  if (Wire.available()) {

    hmiStartButtonState = 0;
    command = Wire.read(); // Read 1 byte command

    if (command == 1) {
      hmiStartButtonState = 1; // hmibutton is pressed
    } else if (command == 0) {
      hmiStartButtonState = 0; // hmibutton NOT pressed
    } else if (command == 2) {
      softwareReset();
    }
  }
}

void softwareReset() {
  wdt_enable(WDTO_15MS); // Set watchdog timer to 15ms
  while (1) {}           // Wait for reset
}



  

