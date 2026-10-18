/*
 * ============================================================================
 * Project: Autonomous Fire Fighting Robot Using ESP32
 * Target Board: ESP32 Dev Module / ESP32 WROOM-32
 * ============================================================================
 */

#include <Arduino.h>

// ============================================================================
// 1. HARDWARE CONFIGURATION & PIN ASSIGNMENTS
// ============================================================================

// Flame Sensors (Must be 3.3V safe logic)
// Note: GPIO 35 is an input-only pin (GPI), suitable for reading digital signals.
const uint8_t PIN_FLAME_LEFT   = 32;
const uint8_t PIN_FLAME_CENTER = 33;
const uint8_t PIN_FLAME_RIGHT  = 35;

// L298N Motor Driver Control Pins
const uint8_t PIN_MOTOR_ENA    = 14;  // Speed/Enable Left
const uint8_t PIN_MOTOR_IN1    = 18;  // Direction Left 1
const uint8_t PIN_MOTOR_IN2    = 19;  // Direction Left 2
const uint8_t PIN_MOTOR_IN3    = 25;  // Direction Right 1
const uint8_t PIN_MOTOR_IN4    = 26;  // Direction Right 2
const uint8_t PIN_MOTOR_ENB    = 27;  // Speed/Enable Right

// Pump MOSFET Driver Pin
const uint8_t PIN_PUMP_MOSFET  = 23;

// Onboard Diagnostic LED
const uint8_t PIN_STATUS_LED   = 2;

// ============================================================================
// 2. LOGIC POLARITY & OPERATIONAL CONSTANTS
// ============================================================================

// Sensor Polarity:
// Most LM393 modules pull output LOW when infrared flame is detected.
// Change to HIGH if your sensor module outputs Active-HIGH.
const uint8_t FLAME_DETECTED_LEVEL = LOW;

// Actuator Polarity:
// Standard N-Channel MOSFET drivers activate when gate voltage is pulled HIGH.
const uint8_t PUMP_ACTIVE_LEVEL   = HIGH;
const uint8_t PUMP_INACTIVE_LEVEL = LOW;

// Timing & Safety Safeguards (Milliseconds)
const unsigned long STARTUP_SAFETY_DELAY_MS = 3000; // Stabilize power rails
const unsigned long SENSOR_SAMPLE_INTERVAL  = 50;   // Sensor poll rate (20 Hz)
const unsigned long SENSOR_DEBOUNCE_TIME    = 100;  // Debounce integration window
const unsigned long PUMP_MAX_RUN_TIME_MS    = 6000; // Continuous pump cutoff
const unsigned long PUMP_COOLDOWN_TIME_MS   = 3000; // Cooldown enforcement
const unsigned long TELEMETRY_INTERVAL_MS   = 250;  // Serial streaming rate

// Drive Speed Settings (0 - 255)
const uint8_t SPEED_FORWARD = 200;
const uint8_t SPEED_TURN    = 180;
const uint8_t SPEED_STOP    = 0;

// PWM Channel Properties (ESP32 LEDC Core)
const uint32_t PWM_FREQ       = 5000; // 5 kHz PWM frequency
const uint8_t  PWM_RESOLUTION = 8;    // 8-bit resolution (0-255)
const uint8_t  PWM_CHAN_ENA   = 0;
const uint8_t  PWM_CHAN_ENB   = 1;

// ============================================================================
// 3. SYSTEM STATE & VARIABLE DECLARATIONS
// ============================================================================

enum RobotState {
  STATE_STARTUP_HOLD,
  STATE_SEARCHING,
  STATE_TURNING_LEFT,
  STATE_TURNING_RIGHT,
  STATE_EXTINGUISHING,
  STATE_COOLDOWN,
  STATE_SAFETY_FAULT
};

RobotState currentState = STATE_STARTUP_HOLD;

// Sensor Raw & Filtered Tracking
bool rawLeft   = false;
bool rawCenter = false;
bool rawRight  = false;

bool leftDebounced   = false;
bool centerDebounced = false;
bool rightDebounced  = false;

unsigned long lastLeftDetectTime   = 0;
unsigned long lastCenterDetectTime = 0;
unsigned long lastRightDetectTime  = 0;

// Operational Timers
unsigned long systemStartTime   = 0;
unsigned long lastSensorPollTime= 0;
unsigned long lastTelemetryTime = 0;
unsigned long pumpStartTime     = 0;
unsigned long cooldownStartTime = 0;

// ============================================================================
// 4. LOW-LEVEL ACTUATOR ROUTINES
// ============================================================================

void haltMotors() {
  digitalWrite(PIN_MOTOR_IN1, LOW);
  digitalWrite(PIN_MOTOR_IN2, LOW);
  digitalWrite(PIN_MOTOR_IN3, LOW);
  digitalWrite(PIN_MOTOR_IN4, LOW);
  ledcWrite(PWM_CHAN_ENA, SPEED_STOP);
  ledcWrite(PWM_CHAN_ENB, SPEED_STOP);
}

void driveForward(uint8_t speed) {
  digitalWrite(PIN_MOTOR_IN1, HIGH);
  digitalWrite(PIN_MOTOR_IN2, LOW);
  digitalWrite(PIN_MOTOR_IN3, HIGH);
  digitalWrite(PIN_MOTOR_IN4, LOW);
  ledcWrite(PWM_CHAN_ENA, speed);
  ledcWrite(PWM_CHAN_ENB, speed);
}

void turnLeft(uint8_t speed) {
  // Skid-steer: Left reverse, Right forward
  digitalWrite(PIN_MOTOR_IN1, LOW);
  digitalWrite(PIN_MOTOR_IN2, HIGH);
  digitalWrite(PIN_MOTOR_IN3, HIGH);
  digitalWrite(PIN_MOTOR_IN4, LOW);
  ledcWrite(PWM_CHAN_ENA, speed);
  ledcWrite(PWM_CHAN_ENB, speed);
}

void turnRight(uint8_t speed) {
  // Skid-steer: Left forward, Right reverse
  digitalWrite(PIN_MOTOR_IN1, HIGH);
  digitalWrite(PIN_MOTOR_IN2, LOW);
  digitalWrite(PIN_MOTOR_IN3, LOW);
  digitalWrite(PIN_MOTOR_IN4, HIGH);
  ledcWrite(PWM_CHAN_ENA, speed);
  ledcWrite(PWM_CHAN_ENB, speed);
}

void setPump(bool active) {
  if (active) {
    digitalWrite(PIN_PUMP_MOSFET, PUMP_ACTIVE_LEVEL);
    digitalWrite(PIN_STATUS_LED, HIGH);
  } else {
    digitalWrite(PIN_PUMP_MOSFET, PUMP_INACTIVE_LEVEL);
    digitalWrite(PIN_STATUS_LED, LOW);
  }
}

// ============================================================================
// 5. SENSOR ACQUISITION & INTEGRATION
// ============================================================================

void updateSensors() {
  unsigned long now = millis();

  // Read raw digital inputs and evaluate against active logic level
  rawLeft   = (digitalRead(PIN_FLAME_LEFT)   == FLAME_DETECTED_LEVEL);
  rawCenter = (digitalRead(PIN_FLAME_CENTER) == FLAME_DETECTED_LEVEL);
  rawRight  = (digitalRead(PIN_FLAME_RIGHT)  == FLAME_DETECTED_LEVEL);

  // Left Channel Integration
  if (rawLeft) {
    lastLeftDetectTime = now;
    leftDebounced = true;
  } else if (now - lastLeftDetectTime > SENSOR_DEBOUNCE_TIME) {
    leftDebounced = false;
  }

  // Center Channel Integration
  if (rawCenter) {
    lastCenterDetectTime = now;
    centerDebounced = true;
  } else if (now - lastCenterDetectTime > SENSOR_DEBOUNCE_TIME) {
    centerDebounced = false;
  }

  // Right Channel Integration
  if (rawRight) {
    lastRightDetectTime = now;
    rightDebounced = true;
  } else if (now - lastRightDetectTime > SENSOR_DEBOUNCE_TIME) {
    rightDebounced = false;
  }
}

// ============================================================================
// 6. TELEMETRY OUTPUT
// ============================================================================

const char* getStateName(RobotState state) {
  switch (state) {
    case STATE_STARTUP_HOLD:   return "STARTUP_HOLD";
    case STATE_SEARCHING:      return "SEARCHING";
    case STATE_TURNING_LEFT:   return "TURNING_LEFT";
    case STATE_TURNING_RIGHT:  return "TURNING_RIGHT";
    case STATE_EXTINGUISHING:  return "EXTINGUISHING";
    case STATE_COOLDOWN:       return "COOLDOWN";
    case STATE_SAFETY_FAULT:   return "SAFETY_FAULT";
    default:                   return "UNKNOWN";
  }
}

void streamTelemetry() {
  Serial.print("[");
  Serial.print(millis() / 1000.0, 2);
  Serial.print("s] State: ");
  Serial.print(getStateName(currentState));
  Serial.print(" | Sensors [L C R]: ");
  Serial.print(leftDebounced   ? "FLAME " : "CLEAR ");
  Serial.print(centerDebounced ? "FLAME " : "CLEAR ");
  Serial.print(rightDebounced  ? "FLAME " : "CLEAR ");
  Serial.print("| Pump: ");
  Serial.println(digitalRead(PIN_PUMP_MOSFET) == PUMP_ACTIVE_LEVEL ? "ACTIVE" : "OFF");
}

// ============================================================================
// 7. SETUP & INITIALIZATION
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println("\n==============================================");
  Serial.println(" Autonomous Fire Fighting Robot Firmware Initializing");
  Serial.println("==============================================");

  // Configure Output Pins to Safe State FIRST
  pinMode(PIN_PUMP_MOSFET, OUTPUT);
  digitalWrite(PIN_PUMP_MOSFET, PUMP_INACTIVE_LEVEL);

  pinMode(PIN_STATUS_LED, OUTPUT);
  digitalWrite(PIN_STATUS_LED, LOW);

  pinMode(PIN_MOTOR_IN1, OUTPUT);
  pinMode(PIN_MOTOR_IN2, OUTPUT);
  pinMode(PIN_MOTOR_IN3, OUTPUT);
  pinMode(PIN_MOTOR_IN4, OUTPUT);
  digitalWrite(PIN_MOTOR_IN1, LOW);
  digitalWrite(PIN_MOTOR_IN2, LOW);
  digitalWrite(PIN_MOTOR_IN3, LOW);
  digitalWrite(PIN_MOTOR_IN4, LOW);

  // Attach ESP32 Hardware PWM to Motor Enable lines
  ledcSetup(PWM_CHAN_ENA, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(PWM_CHAN_ENB, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(PIN_MOTOR_ENA, PWM_CHAN_ENA);
  ledcAttachPin(PIN_MOTOR_ENB, PWM_CHAN_ENB);
  haltMotors();

  // Configure Sensor Inputs
  // Note: GPIO 35 has no internal pullup/pulldown; module must provide driven logic.
  pinMode(PIN_FLAME_LEFT, INPUT);
  pinMode(PIN_FLAME_CENTER, INPUT);
  pinMode(PIN_FLAME_RIGHT, INPUT);

  systemStartTime = millis();
  Serial.println("[SYSTEM] Hardware IO configured. Entering safety hold...");
}

// ============================================================================
// 8. MAIN ARBITRATION LOOP
// ============================================================================

void loop() {
  unsigned long currentMillis = millis();

  // 1. Periodic Sensor Filtering
  if (currentMillis - lastSensorPollTime >= SENSOR_SAMPLE_INTERVAL) {
    lastSensorPollTime = currentMillis;
    updateSensors();
  }

  // 2. Finite State Machine
  switch (currentState) {

    case STATE_STARTUP_HOLD:
      haltMotors();
      setPump(false);
      if (currentMillis - systemStartTime >= STARTUP_SAFETY_DELAY_MS) {
        Serial.println("[SYSTEM] Startup hold complete. System operational.");
        currentState = STATE_SEARCHING;
      }
      break;

    case STATE_SEARCHING:
      setPump(false);

      // Arbitration Rules for Multiple Sensor Hits:
      // Priority 1: Center sensor hit -> Stop immediately and start extinguishing
      if (centerDebounced) {
        haltMotors();
        pumpStartTime = currentMillis;
        currentState = STATE_EXTINGUISHING;
      }
      // Priority 2: Left and Right simultaneous conflict -> Halt to avoid oscillation
      else if (leftDebounced && rightDebounced) {
        haltMotors();
      }
      // Priority 3: Directional tracking
      else if (leftDebounced) {
        turnLeft(SPEED_TURN);
        currentState = STATE_TURNING_LEFT;
      }
      else if (rightDebounced) {
        turnRight(SPEED_TURN);
        currentState = STATE_TURNING_RIGHT;
      }
      else {
        // No targets: Remain halted in safe observation mode
        haltMotors();
      }
      break;

    case STATE_TURNING_LEFT:
      // Abort turning if center aligns or signal drops
      if (centerDebounced) {
        haltMotors();
        pumpStartTime = currentMillis;
        currentState = STATE_EXTINGUISHING;
      } else if (!leftDebounced) {
        haltMotors();
        currentState = STATE_SEARCHING;
      } else {
        turnLeft(SPEED_TURN);
      }
      break;

    case STATE_TURNING_RIGHT:
      // Abort turning if center aligns or signal drops
      if (centerDebounced) {
        haltMotors();
        pumpStartTime = currentMillis;
        currentState = STATE_EXTINGUISHING;
      } else if (!rightDebounced) {
        haltMotors();
        currentState = STATE_SEARCHING;
      } else {
        turnRight(SPEED_TURN);
      }
      break;

    case STATE_EXTINGUISHING:
      haltMotors(); // Zero velocity during discharge
      setPump(true);

      // Enforce Safety Cutoff: Prevent pump runaway or thermal driver failure
      if (currentMillis - pumpStartTime >= PUMP_MAX_RUN_TIME_MS) {
        setPump(false);
        cooldownStartTime = currentMillis;
        Serial.println("[WARNING] Pump run limit reached. Forcing thermal cooldown.");
        currentState = STATE_COOLDOWN;
      } 
      // Flame extinguished early
      else if (!centerDebounced) {
        setPump(false);
        currentState = STATE_SEARCHING;
      }
      break;

    case STATE_COOLDOWN:
      haltMotors();
      setPump(false);
      if (currentMillis - cooldownStartTime >= PUMP_COOLDOWN_TIME_MS) {
        Serial.println("[SYSTEM] Cooldown period expired. Resuming search.");
        currentState = STATE_SEARCHING;
      }
      break;

    case STATE_SAFETY_FAULT:
    default:
      haltMotors();
      setPump(false);
      break;
  }

  // 3. Serial Telemetry Broadcast
  if (currentMillis - lastTelemetryTime >= TELEMETRY_INTERVAL_MS) {
    lastTelemetryTime = currentMillis;
    streamTelemetry();
  }
}
