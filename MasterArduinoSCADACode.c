// UPDATE 17/06/25

#include <ArduinoJson.h>
#include <Wire.h>

struct DataPacket {
  int currentStep;
  float phValue;
};

DataPacket receivedData;

int currentStep = 0;
float phValue = 0.0;

int hmiStartButtonState = 0;

const int trigPin = 9;
const int echoPin = 10;
const int ledPin = 7;
const int sourcePumpPin = 6;

const float HIGH_THRESHOLD = 9.0; // Start filling when water drops below this (in cm)
const float LOW_THRESHOLD = 4.0;   // Stop filling when water reaches this level (in cm)

bool pumpRunning = false;

void setup() {
  Wire.begin(); // Master

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(sourcePumpPin, OUTPUT);


  Serial.begin(9600);
}

void loop() {

  float distance = measureDistance();

  Serial.print("DEBUG_Distance = ");
  Serial.print(distance);
  Serial.println(" cm");

  // Schmitt trigger logic
  if (!pumpRunning && distance > HIGH_THRESHOLD) {
    digitalWrite(ledPin, HIGH);  // Turn on pump
    digitalWrite(sourcePumpPin, LOW); // Reverse Relais Pump = HIGH
    pumpRunning = true;
    Serial.println("Pump ON");
  } else if (pumpRunning && distance < LOW_THRESHOLD) {
    digitalWrite(ledPin, LOW);   // Turn off pump
    digitalWrite(sourcePumpPin, HIGH); // Reverse Relais Pump = LOW
    pumpRunning = false;
    Serial.println("Pump OFF");
  }

  Wire.requestFrom(8, sizeof(DataPacket));

  if (Wire.available() == sizeof(DataPacket)) {
    Wire.readBytes((byte *)&receivedData, sizeof(DataPacket));

    // Print received data
    Serial.print("Current step: ");
    Serial.println(receivedData.currentStep);
    Serial.print("Current pH Value: ");
    Serial.println(receivedData.phValue);
  }

  // Create a JSON string manually
  currentStep = receivedData.currentStep;
  phValue = receivedData.phValue;
  String json = "{";
  json += "\"currentStep\":" + String(currentStep) + ",";
  json += "\"phValue\":" + String(phValue);
  json += "}";

  // Send JSON to node-red over serial
  Serial.println(json);

  hmiStartButtonState = 0;

  hmiStartButtonState = getHmiStartButtonState();

  Serial.print("DEBUG hmiStartButtonState: ");
  Serial.println(hmiStartButtonState);

  if(hmiStartButtonState == 1) {

    Wire.beginTransmission(8); // Address of slave
    Wire.write(1); // Send command: 1 = LED ON
    Wire.endTransmission();
  } else if(hmiStartButtonState == 0) {
    Wire.beginTransmission(8); // Address of slave
    Wire.write(0); // Send command: 1 = LED ON
    Wire.endTransmission();
  } else if(hmiStartButtonState == 2) {
    Wire.beginTransmission(8); // Address of slave
    Wire.write(2); // Send command: 1 = LED ON
    Wire.endTransmission();
  }

  delay(1000);
}

// My functions

int getHmiStartButtonState() {

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

// Function to measure distance in cm
float measureDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000); // 30ms timeout (~5 meters max)
  
  if (duration == 0) {
    return -1.0; // Indicates "Out of range"
  }

  float distance = duration * 0.034 / 2;
  return distance;
}
