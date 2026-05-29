#include <Wire.h>
#include <Servo.h>

// --------------------------------------------------
// Pin configuration
// --------------------------------------------------
const int valvePin    = 3;    // Pressure valve
const int rotServoPin = 9;    // Rotation servo
const int cutServoPin = 10;   // Cutting servo

// --------------------------------------------------
// Servo objects
// --------------------------------------------------
Servo rotServo;
Servo cutServo;

// --------------------------------------------------
// Servo parameters
// --------------------------------------------------
const int rotStartPos = 90;   // Neutral position
const int cutStartPos = 5;
const int cutAngle    = 140;

// --------------------------------------------------
// Pressure sensor
// --------------------------------------------------
#define SENSOR_ADDR 0x28

const float P_MAX_BAR = 4.137;
const int OUTPUT_MIN = 1638;
const int OUTPUT_MAX = 14745;

// --------------------------------------------------
// System parameters
// --------------------------------------------------
const float targetPressureBar = 0.06;

// --------------------------------------------------
// System states
// --------------------------------------------------
enum State {
  WAIT_ROTATION,
  PRESSURISING,
  WAIT_GRIP_CONFIRM,
  WAIT_CUT_COMMAND,
  WAIT_CUT_CONFIRM,
  WAIT_MANUAL_EXHAUST
};

State currentState = WAIT_ROTATION;

// --------------------------------------------------
// Global variables
// --------------------------------------------------
bool valveClosed = true;
unsigned long lastPrint = 0;
float lastValidPressureBar = 0.0;
int sensorFailCount = 0;

// --------------------------------------------------
// Pressure sensor functions
// --------------------------------------------------
float readPressureBarRaw() {
  Wire.requestFrom(SENSOR_ADDR, 4);

  if (Wire.available() < 4) {
    return -1.0;
  }

  byte b1 = Wire.read();
  byte b2 = Wire.read();

  Wire.read();
  Wire.read();

  byte status = (b1 >> 6) & 0x03;

  if (status != 0) {
    return -1.0;
  }

  int rawPressure = ((b1 & 0x3F) << 8) | b2;

  float pressureBar = ((float)(rawPressure - OUTPUT_MIN) *
                       P_MAX_BAR /
                       (OUTPUT_MAX - OUTPUT_MIN));

  pressureBar = constrain(pressureBar, 0.0, P_MAX_BAR);

  return pressureBar;
}

float readPressureBarStable() {
  for (int i = 0; i < 3; i++) {
    float p = readPressureBarRaw();

    if (p >= 0) {
      lastValidPressureBar = p;
      sensorFailCount = 0;
      return p;
    }

    delay(2);
  }

  sensorFailCount++;
  return lastValidPressureBar;
}

// --------------------------------------------------
// Actuation functions
// --------------------------------------------------
void openValve() {
  digitalWrite(valvePin, HIGH);
}

void closeValve() {
  digitalWrite(valvePin, LOW);
}

void moveRotServoTo(int targetPos) {
  targetPos = constrain(targetPos, 0, 180);
  rotServo.write(targetPos);

  Serial.print("Rotation servo moved to angle: ");
  Serial.println(targetPos);

  delay(2000);
}

void runCutServo() {
  cutServo.write(cutAngle);

  Serial.print("Cutting servo moved to angle: ");
  Serial.println(cutAngle);

  delay(2000);

  cutServo.write(cutStartPos);

  Serial.print("Cutting servo returned to start position: ");
  Serial.println(cutStartPos);
}

// --------------------------------------------------
// User interface functions
// --------------------------------------------------
void printInstructions() {
  Serial.println();
  Serial.println("=== System ready ===");
  Serial.println("Commands:");
  Serial.println("hX   = rotate upwards by X degrees, e.g. h75");
  Serial.println("vX   = rotate downwards by X degrees, e.g. v30");
  Serial.println("g    = confirm rotation angle and start pressurisation");
  Serial.println("f    = confirm that the fingers have gripped");
  Serial.println("k    = activate cutting servo");
  Serial.println("c    = confirm successful cut");
  Serial.println("e    = confirm manual exhaust");
  Serial.println("r    = reset system");
  Serial.println();
}

void resetSystem() {
  closeValve();
  valveClosed = true;

  rotServo.write(rotStartPos);
  cutServo.write(cutStartPos);

  currentState = WAIT_ROTATION;

  Serial.println();
  Serial.println("System reset.");
  Serial.println("Test rotation servo using hX or vX.");
}

void handleRotationCommand(String cmd) {
  if (cmd.startsWith("h")) {
    int value = cmd.substring(1).toInt();
    int target = rotStartPos + value;
    target = constrain(target, 0, 180);

    Serial.print("Testing upward rotation by +");
    Serial.print(value);
    Serial.println(" degrees");

    moveRotServoTo(target);
  }

  else if (cmd.startsWith("v")) {
    int value = cmd.substring(1).toInt();
    int target = rotStartPos - value;
    target = constrain(target, 0, 180);

    Serial.print("Testing downward rotation by -");
    Serial.print(value);
    Serial.println(" degrees");

    moveRotServoTo(target);
  }
}

// --------------------------------------------------
// Setup
// --------------------------------------------------
void setup() {
  Wire.begin();
  Serial.begin(115200);

  pinMode(valvePin, OUTPUT);
  closeValve();

  rotServo.attach(rotServoPin);
  cutServo.attach(cutServoPin);

  rotServo.write(rotStartPos);
  cutServo.write(cutStartPos);

  delay(500);

  printInstructions();
  resetSystem();
}

// --------------------------------------------------
// Main loop
// --------------------------------------------------
void loop() {
  float pressureBar = readPressureBarStable();

  // Periodic state and pressure output
  if (millis() - lastPrint > 300) {
    lastPrint = millis();

    Serial.print("State: ");

    switch (currentState) {
      case WAIT_ROTATION:       Serial.print("WAIT_ROTATION"); break;
      case PRESSURISING:        Serial.print("PRESSURISING"); break;
      case WAIT_GRIP_CONFIRM:   Serial.print("WAIT_GRIP_CONFIRM"); break;
      case WAIT_CUT_COMMAND:    Serial.print("WAIT_CUT_COMMAND"); break;
      case WAIT_CUT_CONFIRM:    Serial.print("WAIT_CUT_CONFIRM"); break;
      case WAIT_MANUAL_EXHAUST: Serial.print("WAIT_MANUAL_EXHAUST"); break;
    }

    Serial.print(" | Pressure: ");
    Serial.print(pressureBar, 4);
    Serial.print(" bar");

    if (sensorFailCount > 0) {
      Serial.print(" | Sensor fail count: ");
      Serial.print(sensorFailCount);
    }

    Serial.println();
  }

  // Automatic valve closing when target pressure is reached
  if (currentState == PRESSURISING &&
      !valveClosed &&
      pressureBar >= targetPressureBar) {

    closeValve();
    valveClosed = true;
    currentState = WAIT_GRIP_CONFIRM;

    Serial.println();
    Serial.print("Target pressure reached: ");
    Serial.print(pressureBar, 4);
    Serial.println(" bar");
    Serial.println("Confirm grip using 'f'");
  }

  // Read serial command
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd.length() == 0) {
      return;
    }

    // Reset is always available
    if (cmd == "r") {
      resetSystem();
      return;
    }

    switch (currentState) {
      case WAIT_ROTATION:
        if (cmd.startsWith("h") || cmd.startsWith("v")) {
          handleRotationCommand(cmd);
        }
        else if (cmd == "g") {
          currentState = PRESSURISING;
          openValve();
          valveClosed = false;

          Serial.println("Rotation confirmed.");
          Serial.println("Pressurising fingers to 0.0525 bar...");
        }
        else {
          Serial.println("Invalid command. Use hX, vX, or g.");
        }
        break;

      case PRESSURISING:
        Serial.println("Automatic pressurisation in progress...");
        break;

      case WAIT_GRIP_CONFIRM:
        if (cmd == "f") {
          currentState = WAIT_CUT_COMMAND;
          Serial.println("Grip confirmed.");
          Serial.println("Send 'k' to activate the cutting servo.");
        }
        else {
          Serial.println("Invalid command. Use 'f' when grip is confirmed.");
        }
        break;

      case WAIT_CUT_COMMAND:
        if (cmd == "k") {
          Serial.println("Activating cutting servo...");
          runCutServo();

          currentState = WAIT_CUT_CONFIRM;
          Serial.println("Confirm cut using 'c'");
        }
        else {
          Serial.println("Invalid command. Use 'k' to cut.");
        }
        break;

      case WAIT_CUT_CONFIRM:
        if (cmd == "c") {
          currentState = WAIT_MANUAL_EXHAUST;
          Serial.println("Cut confirmed.");
          Serial.println("Manually release air through exhaust and confirm using 'e'.");
        }
        else {
          Serial.println("Invalid command. Use 'c' when cut is confirmed.");
        }
        break;

      case WAIT_MANUAL_EXHAUST:
        if (cmd == "e") {
          Serial.println("Exhaust confirmed.");
          Serial.println("Cycle complete.");
          resetSystem();
        }
        else {
          Serial.println("Invalid command. Use 'e' when air has been released.");
        }
        break;
    }
  }
}