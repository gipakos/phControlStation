#include <Wire.h>

struct DataPacket {
  int currentStep;
};

DataPacket receivedData;

int currentStep = 0;

void setup() {
  Wire.begin(); // Master
  Serial.begin(9600);
}

void loop() {
  Wire.requestFrom(8, sizeof(DataPacket));

  if (Wire.available() == sizeof(DataPacket)) {
    Wire.readBytes((byte *)&receivedData, sizeof(DataPacket));

    // Print received data
    Serial.print("Current step: ");
    Serial.println(receivedData.currentStep);  
  }

  // Create a JSON string manually
  currentStep = receivedData.currentStep;
  String json = "{";
  json += "\"currentStep\":" + String(currentStep);
  json += "}";

  // Send JSON to node-red over serial
  Serial.println(json);

  delay(1000);
}
