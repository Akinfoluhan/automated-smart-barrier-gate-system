#include <IRremote.hpp>

#define IR_RECEIVE_PIN A0

void setup() {
  Serial.begin(115200);
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
  Serial.println("Press remote buttons...");
}

void loop() {
  if (IrReceiver.decode()) {
    Serial.print("Command: 0x");
    Serial.println(IrReceiver.decodedIRData.command, HEX);

    IrReceiver.printIRResultShort(&Serial);
    Serial.println();

    IrReceiver.resume();
  }
}