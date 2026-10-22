/*
  StreetLight SmartNode - Fault and Energy Monitor (IC26-04-08)
  Board: ESP32 Dev Module
  Libraries: Adafruit INA219, Adafruit SSD1306, Adafruit GFX

  SAFETY: LOW VOLTAGE ONLY (5V). Never connect mains power.

  WIRING
  INA219:  VCC -> 3.3V, GND -> GND, SDA -> GPIO 21, SCL -> GPIO 22
  OLED:    VCC -> 3.3V, GND -> GND, SDA -> GPIO 21, SCL -> GPIO 22
  LDR:     3.3V -> LDR -> GPIO 34 -> 10k resistor -> GND
           (darker = lower reading)
  Lamp power path:
    5V -> INA219 VIN+ -> INA219 VIN- -> 4 parallel branches
          (toggle switch + white LED + 100 ohm) -> transistor collector
    Transistor emitter -> GND. Transistor base -> 1k -> GPIO 25.
    All grounds connected together.
  Green LED  -> GPIO 26 (via 220 ohm) -> GND
  Red LED    -> GPIO 27 (via 220 ohm) -> GND
  Yellow LED -> GPIO 14 (via 220 ohm) -> GND
  Buzzer     -> GPIO 33 -> GND

  SERIAL COMMANDS (115200 baud) for testing:
    a = automatic (lamps follow the LDR)
    f = force lamps ON   (use for the "daylight waste" test)
    o = force lamps OFF  (use for the "stuck on" test)
*/

#include <Wire.h>
#include <Adafruit_INA219.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define LDR_PIN 34
#define LAMP_CTRL 25
#define GREEN_LED 26
#define RED_LED 27
#define YELLOW_LED 14
#define BUZZER 33

Adafruit_INA219 ina219;
Adafruit_SSD1306 oled(128, 64, &Wire, -1);

// ---- CONFIGURE THESE AFTER CALIBRATION ----
const int LAMP_COUNT = 4;
float I_LAMP_MA = 18.0;        // current of ONE working lamp (measure it!)
float ABNORMAL_FACTOR = 1.35;  // above this x expected current = abnormal power
float STANDBY_MA = 5.0;        // current allowed when lamps are commanded OFF
int LDR_DARK_BELOW = 1200;     // reading below this = dark
int LDR_LIGHT_ABOVE = 1800;    // reading above this = light (gap avoids flicker)
// -------------------------------------------

enum Mode { AUTO, FORCE_ON, FORCE_OFF };
enum State { NORMAL, SETTLING, LAMP_FAULT, HIGH_POWER, DAY_WASTE, STUCK_ON };

Mode mode = AUTO;
bool isDark = false;
bool lampsCommanded = false;
bool prevCommand = false;
unsigned long lastSwitchMs = 0;
unsigned long lastLoopMs = 0;
double energyMWh = 0;

const char* stateName(State s) {
  switch (s) {
    case NORMAL:     return "NORMAL";
    case SETTLING:   return "SETTLING";
    case LAMP_FAULT: return "LAMP FAULT";
    case HIGH_POWER: return "HIGH POWER";
    case DAY_WASTE:  return "DAY WASTE";
    case STUCK_ON:   return "STUCK ON";
  }
  return "?";
}

void setup() {
  Serial.begin(115200);
  pinMode(LAMP_CTRL, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.println("StreetLight SmartNode");

  if (!ina219.begin()) {
    oled.println("INA219 NOT FOUND");
    oled.display();
    Serial.println("INA219 not found - check wiring");
    while (true) delay(100);
  }
  ina219.setCalibration_16V_400mA();  // good resolution for small LED loads

  oled.println("Starting...");
  oled.display();
  delay(1500);

  isDark = analogRead(LDR_PIN) < LDR_DARK_BELOW;
  lastLoopMs = millis();
  Serial.println("ms,ldr,mode,cmd,mA,V,workingEst,state,mWh");
}

void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'a') mode = AUTO;
    else if (c == 'f') mode = FORCE_ON;
    else if (c == 'o') mode = FORCE_OFF;
  }
}

void loop() {
  handleSerial();

  // ---- Light sensing with hysteresis ----
  int ldr = analogRead(LDR_PIN);
  if (ldr < LDR_DARK_BELOW) isDark = true;
  else if (ldr > LDR_LIGHT_ABOVE) isDark = false;

  // ---- Lamp command ----
  if (mode == FORCE_ON) lampsCommanded = true;
  else if (mode == FORCE_OFF) lampsCommanded = false;
  else lampsCommanded = isDark;

  digitalWrite(LAMP_CTRL, lampsCommanded ? HIGH : LOW);
  if (lampsCommanded != prevCommand) {
    lastSwitchMs = millis();
    prevCommand = lampsCommanded;
  }

  // ---- Measure (average of 5 samples) ----
  float mA = 0, volts = 0;
  for (int i = 0; i < 5; i++) {
    mA += ina219.getCurrent_mA();
    volts += ina219.getBusVoltage_V();
    delay(20);
  }
  mA /= 5.0;
  volts /= 5.0;
  if (mA < 0) mA = 0;

  int workingEst = (int)round(mA / I_LAMP_MA);
  if (workingEst > LAMP_COUNT + 2) workingEst = LAMP_COUNT + 2;

  // ---- Energy (mWh) ----
  unsigned long now = millis();
  double dtMs = now - lastLoopMs;
  lastLoopMs = now;
  energyMWh += (volts * mA) * (dtMs / 3600000.0);

  // ---- Decide state ----
  State state = NORMAL;
  if (millis() - lastSwitchMs < 1500) {
    state = SETTLING;   // ignore the switching transient
  } else if (lampsCommanded) {
    float expectedMA = LAMP_COUNT * I_LAMP_MA;
    if (mA > expectedMA * ABNORMAL_FACTOR) state = HIGH_POWER;
    else if (workingEst < LAMP_COUNT) state = LAMP_FAULT;
    else if (!isDark) state = DAY_WASTE;
  } else {
    if (mA > STANDBY_MA) state = STUCK_ON;
  }

  // ---- Outputs ----
  bool bad = (state == LAMP_FAULT || state == HIGH_POWER || state == STUCK_ON);
  digitalWrite(GREEN_LED, state == NORMAL || state == SETTLING);
  digitalWrite(RED_LED, bad);
  digitalWrite(YELLOW_LED, state == DAY_WASTE);

  unsigned long period = 0;
  if (state == LAMP_FAULT) period = 500;
  else if (state == HIGH_POWER) period = 150;
  else if (state == STUCK_ON) period = 300;
  if (period > 0) digitalWrite(BUZZER, (millis() / period) % 2);
  else digitalWrite(BUZZER, LOW);

  // ---- OLED ----
  oled.clearDisplay();
  oled.setTextSize(2);
  oled.setCursor(0, 0);
  oled.println(stateName(state));
  oled.setTextSize(1);
  oled.setCursor(0, 20);
  oled.print("Lamps ok: ");
  oled.print(lampsCommanded ? min(workingEst, LAMP_COUNT) : 0);
  oled.print("/");
  oled.println(lampsCommanded ? LAMP_COUNT : 0);
  oled.print("I: "); oled.print(mA, 1); oled.print(" mA  V: "); oled.println(volts, 1);
  oled.print("Energy: "); oled.print(energyMWh, 2); oled.println(" mWh");
  oled.print(isDark ? "Dark" : "Light");
  oled.print("  LDR:"); oled.print(ldr);
  oled.print(mode == AUTO ? " AUTO" : mode == FORCE_ON ? " ON" : " OFF");
  oled.display();

  // ---- CSV log (save this as test data) ----
  Serial.print(now); Serial.print(",");
  Serial.print(ldr); Serial.print(",");
  Serial.print(mode == AUTO ? "AUTO" : mode == FORCE_ON ? "FORCE_ON" : "FORCE_OFF"); Serial.print(",");
  Serial.print(lampsCommanded ? 1 : 0); Serial.print(",");
  Serial.print(mA, 1); Serial.print(",");
  Serial.print(volts, 2); Serial.print(",");
  Serial.print(workingEst); Serial.print(",");
  Serial.print(stateName(state)); Serial.print(",");
  Serial.println(energyMWh, 3);

  delay(300);
}
