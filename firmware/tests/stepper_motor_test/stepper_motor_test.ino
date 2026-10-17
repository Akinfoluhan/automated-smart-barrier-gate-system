#include <Stepper.h>

const int stepsPerRevolution = 2048;

// ULN2003 inputs: IN1, IN3, IN2, IN4 order often works best
Stepper bannerStepper(stepsPerRevolution, 8, A2, A1, A3);

void setup() {
  bannerStepper.setSpeed(8);   // lower = slower
}

void loop() {
  // Move about 180 degrees one way
  bannerStepper.step(stepsPerRevolution / 2);
  delay(500);

  // Move 180 degrees back
  bannerStepper.step(-(stepsPerRevolution / 2));
  delay(500);
}