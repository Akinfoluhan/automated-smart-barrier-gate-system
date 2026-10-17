#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <IRremote.hpp>
#include <Stepper.h>

// ---------------- PIN DEFINITIONS ----------------
const int RED_LED_PIN    = 2;
const int BUZZER_PIN     = 3;
const int GREEN_LED_PIN  = 4;
const int SERVO_PIN      = 5;
const int TRIG_PIN       = 6;
const int ECHO_PIN       = 7;
const int RFID_RST_PIN   = 9;
const int RFID_SS_PIN    = 10;
const int IR_RECEIVE_PIN = A0;

// ---------------- STEPPER PINS ----------------
const int STEPPER_IN1 = 8;
const int STEPPER_IN2 = A1;
const int STEPPER_IN3 = A2;
const int STEPPER_IN4 = A3;

// ---------------- REMOTE COMMAND ----------------
const uint8_t REMOTE_OVERRIDE_CMD = 0x44;

// ---------------- OBJECTS ----------------
Servo gateServo;
MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);   // if blank, try 0x3F

// 28BYJ-48 stepper
const int STEPS_PER_REVOLUTION = 2048;
const int BANNER_HALF_SWEEP = STEPS_PER_REVOLUTION / 2;

// Same pin order as your working test
Stepper bannerStepper(STEPS_PER_REVOLUTION, STEPPER_IN1, STEPPER_IN3, STEPPER_IN2, STEPPER_IN4);

// ---------------- SETTINGS ----------------
const int GATE_CLOSED_ANGLE = 0;
const int GATE_OPEN_ANGLE   = 90;
const int CAR_THRESHOLD_CM  = 15;

const unsigned long OPEN_TIME_MS  = 4000;
const unsigned long EXTRA_TIME_MS = 3000;   // change this to whatever extra open time you want
const unsigned long DENY_TIME_MS  = 1500;

// ---------------- AUTHORIZED UIDs ----------------
// EXAMPLE UID ONLY. Replace with the UID of your own test card before use.
// RFID UIDs alone are not secure authentication for real access-control deployments.
struct AuthorizedCard {
  byte uid[10];
  byte len;
};

AuthorizedCard allowedCards[] = {
  {{0xDE, 0xAD, 0xBE, 0xEF}, 4},
  // {{0x13, 0x29, 0xC9, 0x06}, 4}
};

const int NUM_ALLOWED_CARDS = sizeof(allowedCards) / sizeof(allowedCards[0]);

// ---------------- LCD MEMORY ----------------
String lastLine1 = "";
String lastLine2 = "";

// ---------------- BANNER STATE ----------------
long bannerPosition = 0;
int bannerDirection = 1;

// ---------------- HELPER FUNCTIONS ----------------
void updateBanner() {
  bannerStepper.step(bannerDirection);
  bannerPosition += bannerDirection;

  if (bannerPosition >= BANNER_HALF_SWEEP) {
    bannerDirection = -1;
  } 
  else if (bannerPosition <= 0) {
    bannerDirection = 1;
  }
}

void smartDelay(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    updateBanner();
  }
}

void showLCD(const String &line1, const String &line2 = "") {
  if (line1 == lastLine1 && line2 == lastLine2) return;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);

  lastLine1 = line1;
  lastLine2 = line2;
}

void setClosedState() {
  gateServo.write(GATE_CLOSED_ANGLE);
  digitalWrite(RED_LED_PIN, HIGH);
  digitalWrite(GREEN_LED_PIN, LOW);
}

void setOpenState() {
  gateServo.write(GATE_OPEN_ANGLE);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, HIGH);
}

// ---------------- LED BLINK PATTERNS ----------------
void blinkRed(int times, int onTime = 150, int offTime = 120) {
  digitalWrite(GREEN_LED_PIN, LOW);
  for (int i = 0; i < times; i++) {
    digitalWrite(RED_LED_PIN, HIGH);
    smartDelay(onTime);
    digitalWrite(RED_LED_PIN, LOW);
    smartDelay(offTime);
  }
  digitalWrite(RED_LED_PIN, HIGH);
}

void blinkGreen(int times, int onTime = 150, int offTime = 120) {
  digitalWrite(RED_LED_PIN, LOW);
  for (int i = 0; i < times; i++) {
    digitalWrite(GREEN_LED_PIN, HIGH);
    smartDelay(onTime);
    digitalWrite(GREEN_LED_PIN, LOW);
    smartDelay(offTime);
  }
}

void blinkBoth(int times, int onTime = 120, int offTime = 100) {
  for (int i = 0; i < times; i++) {
    digitalWrite(RED_LED_PIN, HIGH);
    digitalWrite(GREEN_LED_PIN, HIGH);
    smartDelay(onTime);
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(GREEN_LED_PIN, LOW);
    smartDelay(offTime);
  }
}

// ---------------- BUZZER PATTERNS ----------------
void grantedBeep() {
  tone(BUZZER_PIN, 1200);
  smartDelay(150);
  noTone(BUZZER_PIN);
  IrReceiver.restartTimer();
}

void deniedBeep() {
  tone(BUZZER_PIN, 700);
  smartDelay(120);
  noTone(BUZZER_PIN);
  smartDelay(80);
  tone(BUZZER_PIN, 700);
  smartDelay(120);
  noTone(BUZZER_PIN);
  IrReceiver.restartTimer();
}

void closingBeep() {
  tone(BUZZER_PIN, 900);
  smartDelay(100);
  noTone(BUZZER_PIN);
  IrReceiver.restartTimer();
}

// ---------------- SENSOR FUNCTIONS ----------------
long readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return 999;

  long distance = duration * 0.0343 / 2.0;
  return distance;
}

bool isCarPresent() {
  long d1 = readDistanceCm();
  smartDelay(30);
  long d2 = readDistanceCm();
  smartDelay(30);
  long d3 = readDistanceCm();

  long avg = (d1 + d2 + d3) / 3;
  return avg <= CAR_THRESHOLD_CM;
}

bool remoteOverridePressed() {
  if (IrReceiver.decode()) {
    uint8_t cmd = IrReceiver.decodedIRData.command;
    IrReceiver.resume();

    if (cmd == REMOTE_OVERRIDE_CMD) {
      return true;
    }
  }
  return false;
}

bool isAuthorized(MFRC522::Uid *uid) {
  for (int i = 0; i < NUM_ALLOWED_CARDS; i++) {
    if (uid->size != allowedCards[i].len) continue;

    bool match = true;
    for (byte j = 0; j < uid->size; j++) {
      if (uid->uidByte[j] != allowedCards[i].uid[j]) {
        match = false;
        break;
      }
    }
    if (match) return true;
  }
  return false;
}

// returns:
//  1 = authorized
// -1 = unauthorized
//  0 = no card scanned
int checkRFID() {
  if (!rfid.PICC_IsNewCardPresent()) return 0;

  Serial.println("Card detected by reader.");

  for (int attempt = 0; attempt < 3; attempt++) {
    smartDelay(20);

    if (rfid.PICC_ReadCardSerial()) {
      Serial.print("Scanned UID: ");
      for (byte i = 0; i < rfid.uid.size; i++) {
        if (rfid.uid.uidByte[i] < 0x10) Serial.print("0");
        Serial.print(rfid.uid.uidByte[i], HEX);
        Serial.print(" ");
      }
      Serial.println();

      bool ok = isAuthorized(&rfid.uid);

      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();

      return ok ? 1 : -1;
    }
  }

  Serial.println("Card present, but UID not read.");
  return 0;
}

// ---------------- ACCESS FUNCTIONS ----------------
void grantAccess(const String &msg) {
  showLCD(msg, "");
  grantedBeep();
  blinkGreen(2);
  setOpenState();
  smartDelay(300);

  unsigned long startTime = millis();
  while (millis() - startTime < OPEN_TIME_MS) {
    updateBanner();
  }

  if (isCarPresent()) {
    showLCD("Please Proceed", "");
    smartDelay(EXTRA_TIME_MS);
  }

  showLCD("Gate Closing", "");
  closingBeep();
  blinkBoth(1);
  setClosedState();
}

void denyAccess() {
  showLCD("Access Denied", "");
  deniedBeep();
  blinkRed(3);
  setClosedState();
  smartDelay(DENY_TIME_MS);
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(9600);

  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  gateServo.attach(SERVO_PIN);
  setClosedState();

  lcd.init();
  lcd.backlight();
  showLCD("System Starting", "");

  SPI.begin();
  rfid.PCD_Init();

  IrReceiver.begin(IR_RECEIVE_PIN, DISABLE_LED_FEEDBACK);

  bannerStepper.setSpeed(8);

  smartDelay(1000);
  showLCD("No Car Detected", "");
}

// ---------------- LOOP ----------------
void loop() {
  updateBanner();

  // Remote override works anytime
  if (remoteOverridePressed()) {
    grantAccess("Manual Override");
    smartDelay(500);
    return;
  }

  if (!isCarPresent()) {
    setClosedState();
    showLCD("No Car Detected", "");
    smartDelay(10);
    return;
  }

  setClosedState();
  showLCD("Car Detected", "Scan Card");

  while (isCarPresent()) {
    updateBanner();

    if (remoteOverridePressed()) {
      grantAccess("Manual Override");
      smartDelay(500);
      return;
    }

    int cardStatus = checkRFID();

    if (cardStatus == 1) {
      grantAccess("Access Granted");
      smartDelay(500);
      return;
    } 
    else if (cardStatus == -1) {
      denyAccess();
      showLCD("Car Detected", "Scan Card");
    }

    smartDelay(10);
  }

  showLCD("No Car Detected", "");
}